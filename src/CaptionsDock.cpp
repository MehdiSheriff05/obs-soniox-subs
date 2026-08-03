/*
Plugin Name
Copyright (C) 2026 Mehdi Sheriff mehdirsheriff@gmail.com

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "CaptionsDock.h"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/config-file.h>

#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace {

#if defined(_WIN32)
const char *kCaptionSourceId = "text_gdiplus";
#else
const char *kCaptionSourceId = "text_ft2_source";
#endif

const int kMaxPreviewLines = 10;
const qint64 kNoAudioWarningMs = 3000;

bool enumAudioSourceCallback(void *param, obs_source_t *source)
{
	auto *combo = static_cast<QComboBox *>(param);

	uint32_t flags = obs_source_get_output_flags(source);
	if (!(flags & OBS_SOURCE_AUDIO))
		return true;

	const char *name = obs_source_get_name(source);
	if (name && *name)
		combo->addItem(QString::fromUtf8(name));

	return true;
}

QString formatElapsed(qint64 elapsedMs)
{
	qint64 totalSeconds = elapsedMs / 1000;
	int hours = static_cast<int>(totalSeconds / 3600);
	int minutes = static_cast<int>((totalSeconds % 3600) / 60);
	int seconds = static_cast<int>(totalSeconds % 60);
	return QStringLiteral("%1:%2:%3")
		.arg(hours, 2, 10, QLatin1Char('0'))
		.arg(minutes, 2, 10, QLatin1Char('0'))
		.arg(seconds, 2, 10, QLatin1Char('0'));
}

} // namespace

CaptionsDock::CaptionsDock(QWidget *parent) : QWidget(parent)
{
	buildUi();
	refreshSourceList();
	loadSettings();

	connect(&m_audioBridge, &AudioBridge::levelChanged, this, &CaptionsDock::onLevelChanged);
	connect(&m_sonioxClient, &SonioxClient::captionReady, this, &CaptionsDock::onCaptionReady);
	connect(&m_sonioxClient, &SonioxClient::statusChanged, this, &CaptionsDock::onSonioxStatusChanged);
	connect(&m_sonioxClient, &SonioxClient::errorOccurred, this, &CaptionsDock::onSonioxError);

	connect(&m_updateChecker, &UpdateChecker::updateAvailable, this, &CaptionsDock::onUpdateAvailable);
	connect(&m_updateChecker, &UpdateChecker::upToDate, this, &CaptionsDock::onUpdateUpToDate);
	connect(&m_updateChecker, &UpdateChecker::checkFailed, this, &CaptionsDock::onUpdateCheckFailed);
	connect(&m_updateChecker, &UpdateChecker::downloadStarted, this, &CaptionsDock::onUpdateDownloadStarted);
	connect(&m_updateChecker, &UpdateChecker::installerLaunched, this, &CaptionsDock::onInstallerLaunched);
	connect(&m_updateChecker, &UpdateChecker::downloadFailed, this, &CaptionsDock::onUpdateDownloadFailed);

	m_audioBridge.setAudioReadyCallback(
		[this](const uint8_t *data, size_t byteCount) { m_sonioxClient.sendAudio(data, byteCount); });

	m_watchdogTimer = new QTimer(this);
	m_watchdogTimer->setInterval(1000);
	connect(m_watchdogTimer, &QTimer::timeout, this, &CaptionsDock::onWatchdogTick);

	m_statsTimer = new QTimer(this);
	m_statsTimer->setInterval(1000);
	connect(m_statsTimer, &QTimer::timeout, this, &CaptionsDock::onStatsTick);

	obs_frontend_add_event_callback(&CaptionsDock::frontendEventCallback, this);

	setStatusText(tr("Idle"));

	m_updateChecker.checkForUpdate();
}

CaptionsDock::~CaptionsDock()
{
	obs_frontend_remove_event_callback(&CaptionsDock::frontendEventCallback, this);

	saveSettings();

	m_sonioxClient.stop();
	m_audioBridge.stop();
	clearCaptionText();

	if (m_captionTextSource) {
		obs_source_release(m_captionTextSource);
		m_captionTextSource = nullptr;
	}
}

void CaptionsDock::frontendEventCallback(enum obs_frontend_event event, void *privateData)
{
	auto *self = static_cast<CaptionsDock *>(privateData);

	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING || event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED)
		self->refreshSourceList();
}

QWidget *CaptionsDock::wrapInScrollArea(QWidget *content)
{
	auto *scrollArea = new QScrollArea();
	scrollArea->setWidgetResizable(true);
	scrollArea->setFrameShape(QFrame::NoFrame);
	scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scrollArea->setWidget(content);
	return scrollArea;
}

void CaptionsDock::buildUi()
{
	auto *outerLayout = new QVBoxLayout(this);
	outerLayout->setContentsMargins(0, 0, 0, 0);

	m_tabWidget = new QTabWidget(this);
	m_tabWidget->addTab(wrapInScrollArea(buildCaptionsTab()), tr("Captions"));
	m_tabWidget->addTab(wrapInScrollArea(buildStatsTab()), tr("Stats"));
	m_tabWidget->addTab(wrapInScrollArea(buildSettingsTab()), tr("Settings"));
	m_tabWidget->addTab(wrapInScrollArea(buildAppearanceTab()), tr("Appearance"));

	outerLayout->addWidget(m_tabWidget);
	setLayout(outerLayout);
}

QWidget *CaptionsDock::buildCaptionsTab()
{
	auto *content = new QWidget();
	auto *layout = new QVBoxLayout(content);

	m_updateBannerLabel = new QLabel(content);
	m_updateBannerLabel->setWordWrap(true);
	m_updateBannerLabel->setVisible(false);
	layout->addWidget(m_updateBannerLabel);

	m_updateInstallButton = new QPushButton(tr("Install Update"), content);
	m_updateInstallButton->setVisible(false);
	connect(m_updateInstallButton, &QPushButton::clicked, this, &CaptionsDock::onInstallUpdateClicked);
	layout->addWidget(m_updateInstallButton);

	auto *form = new QFormLayout();
	m_sourceCombo = new QComboBox(content);
	m_refreshSourcesButton = new QPushButton(tr("Refresh"), content);
	connect(m_refreshSourcesButton, &QPushButton::clicked, this, &CaptionsDock::refreshSourceList);

	auto *sourceRow = new QHBoxLayout();
	sourceRow->addWidget(m_sourceCombo);
	sourceRow->addWidget(m_refreshSourcesButton);
	form->addRow(tr("Audio Source:"), sourceRow);

	m_maxLineCharsSpin = new QSpinBox(content);
	m_maxLineCharsSpin->setRange(20, 200);
	m_maxLineCharsSpin->setValue(m_maxLineChars);
	m_maxLineCharsSpin->setToolTip(
		tr("How much translated text accumulates on screen before it clears and starts a fresh line."));
	connect(m_maxLineCharsSpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
		&CaptionsDock::onMaxLineCharsChanged);
	form->addRow(tr("Max caption length:"), m_maxLineCharsSpin);

	layout->addLayout(form);

	m_startStopButton = new QPushButton(tr("Start"), content);
	connect(m_startStopButton, &QPushButton::clicked, this, &CaptionsDock::onStartStopClicked);
	layout->addWidget(m_startStopButton);

	m_statusLabel = new QLabel(content);
	m_statusLabel->setWordWrap(true);
	layout->addWidget(m_statusLabel);

	auto *levelRow = new QHBoxLayout();
	levelRow->addWidget(new QLabel(tr("Audio level:"), content));
	m_levelMeter = new QProgressBar(content);
	m_levelMeter->setRange(0, 100);
	m_levelMeter->setValue(0);
	m_levelMeter->setTextVisible(false);
	levelRow->addWidget(m_levelMeter);
	layout->addLayout(levelRow);

	auto *captionGroup = new QVBoxLayout();
	captionGroup->setSpacing(2);
	captionGroup->addWidget(new QLabel(tr("Live captions:"), content));
	m_captionPreview = new QPlainTextEdit(content);
	m_captionPreview->setReadOnly(true);
	m_captionPreview->setMaximumBlockCount(kMaxPreviewLines);
	m_captionPreview->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	m_captionPreview->setFixedHeight(100);
	captionGroup->addWidget(m_captionPreview);
	layout->addLayout(captionGroup);

	auto *creditLabel = new QLabel(
		tr("<a href=\"https://github.com/MehdiSheriff05/obs-soniox-subs/releases\">"
		   "Soniox Live Captions Plugin v%1</a>")
			.arg(QString::fromUtf8(PLUGIN_VERSION)),
		content);
	creditLabel->setAlignment(Qt::AlignCenter);
	creditLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 10px;"));
	creditLabel->setTextFormat(Qt::RichText);
	creditLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
	creditLabel->setOpenExternalLinks(true);
	layout->addWidget(creditLabel);

	layout->addStretch(1);

	return content;
}

QWidget *CaptionsDock::buildStatsTab()
{
	auto *content = new QWidget();
	auto *layout = new QVBoxLayout(content);

	auto *form = new QFormLayout();

	m_elapsedTimeLabel = new QLabel(tr("00:00:00"), content);
	form->addRow(tr("Elapsed time:"), m_elapsedTimeLabel);

	m_sessionCostLabel = new QLabel(tr("$0.0000"), content);
	m_sessionCostLabel->setToolTip(tr("Estimated using Soniox's published real-time rate ($%1/hour, "
					   "translation included). This is an estimate for your own awareness, "
					   "not an exact bill — actual Soniox billing is token-based.")
						.arg(kEstimatedCostPerHour, 0, 'f', 2));
	form->addRow(tr("Estimated cost:"), m_sessionCostLabel);

	m_reconnectCountLabel = new QLabel(tr("0"), content);
	m_reconnectCountLabel->setToolTip(tr("How many times the connection to Soniox dropped and had to retry "
					      "during this session — a high count may indicate a shaky network."));
	form->addRow(tr("Reconnects:"), m_reconnectCountLabel);

	layout->addLayout(form);
	layout->addStretch(1);

	return content;
}

QWidget *CaptionsDock::buildSettingsTab()
{
	auto *content = new QWidget();
	auto *layout = new QVBoxLayout(content);

	auto *form = new QFormLayout();

	m_apiKeyEdit = new QLineEdit(content);
	m_apiKeyEdit->setEchoMode(QLineEdit::Password);
	m_apiKeyEdit->setPlaceholderText(tr("Soniox API key"));

	m_apiKeyChangeButton = new QPushButton(tr("Change"), content);
	m_apiKeyChangeButton->setVisible(false);
	connect(m_apiKeyChangeButton, &QPushButton::clicked, this, &CaptionsDock::onApiKeyChangeClicked);

	auto *apiKeyRow = new QHBoxLayout();
	apiKeyRow->addWidget(m_apiKeyEdit);
	apiKeyRow->addWidget(m_apiKeyChangeButton);
	form->addRow(tr("API Key:"), apiKeyRow);

	layout->addLayout(form);

	m_checkUpdatesButton = new QPushButton(tr("Check for Updates"), content);
	connect(m_checkUpdatesButton, &QPushButton::clicked, this, &CaptionsDock::onCheckForUpdatesClicked);
	layout->addWidget(m_checkUpdatesButton);

	m_updateStatusLabel = new QLabel(tr("Checking for updates..."), content);
	m_updateStatusLabel->setWordWrap(true);
	layout->addWidget(m_updateStatusLabel);

	layout->addStretch(1);

	return content;
}

QWidget *CaptionsDock::buildAppearanceTab()
{
	auto *content = new QWidget();
	auto *layout = new QVBoxLayout(content);

	auto *form = new QFormLayout();
	m_fontComboBox = new QFontComboBox(content);
	m_fontComboBox->setCurrentFont(QFont(QStringLiteral("Poppins")));
	m_fontComboBox->setToolTip(tr("Only takes effect if this font is actually installed on this computer — "
				       "Poppins is a Google font, not preinstalled on macOS or Windows."));
	connect(m_fontComboBox, &QFontComboBox::currentFontChanged, this, &CaptionsDock::onAppearanceSettingChanged);
	form->addRow(tr("Font:"), m_fontComboBox);
	layout->addLayout(form);

	m_outlineCheckBox = new QCheckBox(tr("Show text outline / border"), content);
	m_outlineCheckBox->setChecked(true);
	connect(m_outlineCheckBox, &QCheckBox::toggled, this, &CaptionsDock::onAppearanceSettingChanged);
	layout->addWidget(m_outlineCheckBox);

	layout->addStretch(1);

	return content;
}

void CaptionsDock::refreshSourceList()
{
	QString previous = m_sourceCombo->currentText();

	m_sourceCombo->clear();
	obs_enum_sources(enumAudioSourceCallback, m_sourceCombo);

	int idx = m_sourceCombo->findText(previous);
	if (idx >= 0)
		m_sourceCombo->setCurrentIndex(idx);
}

void CaptionsDock::loadSettings()
{
	config_t *config = obs_frontend_get_user_config();
	if (!config)
		return;

	const char *apiKey = config_get_string(config, "SonioxCaptions", "ApiKey");
	if (apiKey)
		m_savedApiKey = QString::fromUtf8(apiKey);
	setApiKeyLocked(!m_savedApiKey.isEmpty());

	const char *sourceName = config_get_string(config, "SonioxCaptions", "AudioSource");
	if (sourceName) {
		int idx = m_sourceCombo->findText(QString::fromUtf8(sourceName));
		if (idx >= 0)
			m_sourceCombo->setCurrentIndex(idx);
	}

	long long maxLineChars = config_get_int(config, "SonioxCaptions", "MaxLineChars");
	if (maxLineChars > 0) {
		m_maxLineChars = static_cast<int>(maxLineChars);
		m_maxLineCharsSpin->setValue(m_maxLineChars);
	}

	if (config_has_user_value(config, "SonioxCaptions", "FontFace")) {
		const char *fontFace = config_get_string(config, "SonioxCaptions", "FontFace");
		if (fontFace && *fontFace)
			m_fontComboBox->setCurrentFont(QFont(QString::fromUtf8(fontFace)));
	}

	if (config_has_user_value(config, "SonioxCaptions", "OutlineEnabled"))
		m_outlineCheckBox->setChecked(config_get_bool(config, "SonioxCaptions", "OutlineEnabled"));
}

void CaptionsDock::saveSettings()
{
	config_t *config = obs_frontend_get_user_config();
	if (!config)
		return;

	config_set_string(config, "SonioxCaptions", "ApiKey", m_apiKeyEdit->text().toUtf8().constData());
	config_set_string(config, "SonioxCaptions", "AudioSource",
			   m_sourceCombo->currentText().toUtf8().constData());
	config_set_int(config, "SonioxCaptions", "MaxLineChars", m_maxLineChars);
	config_save(config);
}

void CaptionsDock::onMaxLineCharsChanged(int value)
{
	m_maxLineChars = value;

	// Persist just this value directly rather than calling saveSettings(),
	// which also writes the API key field's current text — and that field
	// may be mid-edit (unlocked, cleared) when this fires.
	config_t *config = obs_frontend_get_user_config();
	if (config) {
		config_set_int(config, "SonioxCaptions", "MaxLineChars", m_maxLineChars);
		config_save(config);
	}
}

void CaptionsDock::onAppearanceSettingChanged()
{
	applyCaptionStyleSettings();

	config_t *config = obs_frontend_get_user_config();
	if (config) {
		config_set_string(config, "SonioxCaptions", "FontFace",
				   m_fontComboBox->currentFont().family().toUtf8().constData());
		config_set_bool(config, "SonioxCaptions", "OutlineEnabled", m_outlineCheckBox->isChecked());
		config_save(config);
	}
}

void CaptionsDock::onStartStopClicked()
{
	if (m_running) {
		m_sonioxClient.stop();
		m_audioBridge.stop();
		m_watchdogTimer->stop();
		m_statsTimer->stop();
		clearCaptionText();
		setRunningUiState(false);
		setStatusText(tr("Idle"));
		return;
	}

	QString sourceName = m_sourceCombo->currentText();
	QString apiKey = m_apiKeyEdit->text();

	if (sourceName.isEmpty()) {
		setStatusText(tr("Pick an audio source first"));
		return;
	}
	if (apiKey.isEmpty()) {
		setStatusText(tr("Enter a Soniox API key first"));
		return;
	}

	saveSettings();
	m_savedApiKey = apiKey;
	setApiKeyLocked(true);

	if (!m_audioBridge.start(sourceName)) {
		setStatusText(tr("Could not use that audio source"),
			      QStringLiteral("obs_get_source_by_name(\"%1\") returned null").arg(sourceName));
		return;
	}

	ensureCaptionTextSource();

	m_lastFinalizedText.clear();
	m_captionPreview->clear();

	m_sonioxClient.setApiKey(apiKey);
	m_sonioxClient.setLanguages(QStringLiteral("ur"), QStringLiteral("en"));
	m_sonioxClient.start();

	m_noAudioWarned = false;
	m_watchdogTimer->start();

	m_reconnectCount = 0;
	m_reconnectCountLabel->setText(QStringLiteral("0"));
	m_sessionElapsedTimer.start();
	m_elapsedTimeLabel->setText(formatElapsed(0));
	m_sessionCostLabel->setText(tr("$0.0000"));
	m_statsTimer->start();

	setRunningUiState(true);
	setStatusText(tr("Connecting..."));
}

void CaptionsDock::setRunningUiState(bool running)
{
	m_running = running;
	m_startStopButton->setText(running ? tr("Stop") : tr("Start"));
	m_sourceCombo->setEnabled(!running);
	m_refreshSourcesButton->setEnabled(!running);
	m_apiKeyEdit->setEnabled(!running);
	m_apiKeyChangeButton->setEnabled(!running);
}

void CaptionsDock::setApiKeyLocked(bool locked)
{
	m_apiKeyLocked = locked;
	m_apiKeyEdit->setReadOnly(locked);
	m_apiKeyChangeButton->setVisible(!m_savedApiKey.isEmpty());
	m_apiKeyChangeButton->setText(locked ? tr("Change") : tr("Cancel"));

	if (locked)
		m_apiKeyEdit->setText(m_savedApiKey);
}

void CaptionsDock::onApiKeyChangeClicked()
{
	if (m_apiKeyLocked) {
		setApiKeyLocked(false);
		m_apiKeyEdit->clear();
		m_apiKeyEdit->setFocus();
	} else {
		setApiKeyLocked(true);
	}
}

void CaptionsDock::setStatusText(const QString &plain, const QString &tooltip)
{
	m_statusLabel->setText(plain);
	m_statusLabel->setToolTip(tooltip.isEmpty() ? plain : tooltip);
}

void CaptionsDock::onLevelChanged(float peakLevel)
{
	int percent = static_cast<int>(std::min(1.0f, peakLevel) * 100.0f);
	m_levelMeter->setValue(percent);
}

void CaptionsDock::onCaptionReady(const QString &text, bool isFinal)
{
	if (isFinal)
		m_lastFinalizedText += text;

	QString display = isFinal ? m_lastFinalizedText : m_lastFinalizedText + text;

	updateCaptionTextSource(display);
	updatePreviewCurrentLine(display);

	if (isFinal && m_lastFinalizedText.length() >= m_maxLineChars) {
		m_lastFinalizedText.clear();
		m_captionPreview->appendPlainText(QString());
	}
}

void CaptionsDock::updatePreviewCurrentLine(const QString &text)
{
	QTextCursor cursor(m_captionPreview->document()->lastBlock());
	cursor.select(QTextCursor::LineUnderCursor);
	cursor.insertText(text);
}

void CaptionsDock::onSonioxStatusChanged(SonioxClient::Status status)
{
	switch (status) {
	case SonioxClient::Status::Connecting:
		setStatusText(tr("Connecting..."));
		break;
	case SonioxClient::Status::Connected:
		setStatusText(tr("Live — captions are being translated"));
		break;
	case SonioxClient::Status::Reconnecting:
		setStatusText(tr("Connection lost, retrying..."));
		m_reconnectCount++;
		m_reconnectCountLabel->setText(QString::number(m_reconnectCount));
		break;
	case SonioxClient::Status::AuthError:
		setStatusText(tr("Invalid API key"), tr("Authentication failed. Check the Soniox API key."));
		m_watchdogTimer->stop();
		m_statsTimer->stop();
		m_audioBridge.stop();
		setRunningUiState(false);
		break;
	case SonioxClient::Status::Disconnected:
		if (m_running) {
			m_watchdogTimer->stop();
			m_statsTimer->stop();
			m_audioBridge.stop();
			setRunningUiState(false);
			setStatusText(tr("Idle"));
		}
		break;
	}
}

void CaptionsDock::onSonioxError(const QString &plainMessage, const QString &technicalDetail)
{
	setStatusText(plainMessage, technicalDetail);
	obs_log(LOG_WARNING, "Soniox error: %s", technicalDetail.toUtf8().constData());
}

void CaptionsDock::onWatchdogTick()
{
	if (!m_running)
		return;

	if (m_audioBridge.msSinceLastAudio() > kNoAudioWarningMs) {
		if (!m_noAudioWarned) {
			m_noAudioWarned = true;
			setStatusText(tr("No audio detected from selected source"),
				      tr("No audio_capture_callback invocations received recently"));
		}
	} else {
		m_noAudioWarned = false;
	}
}

void CaptionsDock::onStatsTick()
{
	qint64 elapsedMs = m_sessionElapsedTimer.elapsed();
	m_elapsedTimeLabel->setText(formatElapsed(elapsedMs));

	double estimatedCost = (static_cast<double>(elapsedMs) / 3600000.0) * kEstimatedCostPerHour;
	m_sessionCostLabel->setText(QStringLiteral("$%1").arg(estimatedCost, 0, 'f', 4));
}

void CaptionsDock::onCheckForUpdatesClicked()
{
	m_checkUpdatesButton->setEnabled(false);
	m_updateStatusLabel->setText(tr("Checking for updates..."));
	m_updateChecker.checkForUpdate();
}

void CaptionsDock::onUpdateAvailable(const QString &newVersion)
{
	m_checkUpdatesButton->setEnabled(true);

	QString message = tr("Update available: v%1").arg(newVersion);
	m_updateStatusLabel->setText(message);

	m_updateBannerLabel->setText(message);
	m_updateBannerLabel->setVisible(true);
	m_updateInstallButton->setVisible(true);
	m_updateInstallButton->setEnabled(true);
}

void CaptionsDock::onUpdateUpToDate()
{
	m_checkUpdatesButton->setEnabled(true);
	m_updateStatusLabel->setText(tr("You're up to date (v%1)").arg(QString::fromUtf8(PLUGIN_VERSION)));
	m_updateBannerLabel->setVisible(false);
	m_updateInstallButton->setVisible(false);
}

void CaptionsDock::onUpdateCheckFailed(const QString &plainMessage, const QString &technicalDetail)
{
	m_checkUpdatesButton->setEnabled(true);
	m_updateStatusLabel->setText(plainMessage);
	m_updateStatusLabel->setToolTip(technicalDetail);
	m_updateBannerLabel->setVisible(false);
	m_updateInstallButton->setVisible(false);
}

void CaptionsDock::onInstallUpdateClicked()
{
	m_updateInstallButton->setEnabled(false);
	m_updateChecker.downloadAndLaunchInstaller();
}

void CaptionsDock::onUpdateDownloadStarted()
{
	m_updateBannerLabel->setText(tr("Downloading update..."));
}

void CaptionsDock::onInstallerLaunched()
{
	m_updateBannerLabel->setText(tr("Installer launched — finish the install, then restart OBS."));
	m_updateInstallButton->setVisible(false);
}

void CaptionsDock::onUpdateDownloadFailed(const QString &plainMessage, const QString &technicalDetail)
{
	m_updateBannerLabel->setText(plainMessage);
	m_updateBannerLabel->setToolTip(technicalDetail);
	m_updateInstallButton->setEnabled(true);
}

void CaptionsDock::applyCaptionStyleSettings()
{
	if (!m_captionTextSource)
		return;

	obs_data_t *styleSettings = obs_data_create();

	obs_data_t *fontSettings = obs_data_create();
	obs_data_set_string(fontSettings, "face", m_fontComboBox->currentFont().family().toUtf8().constData());
	obs_data_set_int(fontSettings, "size", 48);
	obs_data_set_obj(styleSettings, "font", fontSettings);
	obs_data_release(fontSettings);

	// Same setting applied identically on every platform, deliberately —
	// text_ft2_source (macOS/Linux) and text_gdiplus (Windows) render this
	// "outline" toggle differently under the hood (a real black stroke on
	// Windows; a softer same-color edge on macOS/Linux, since that source
	// has no separate outline color), but that's OBS's own engine
	// difference, not something this plugin should paper over by sending
	// different settings per platform.
	obs_data_set_bool(styleSettings, "outline", m_outlineCheckBox->isChecked());

	obs_source_update(m_captionTextSource, styleSettings);
	obs_data_release(styleSettings);
}

void CaptionsDock::ensureCaptionTextSource()
{
	if (m_captionTextSource)
		return;

	obs_video_info ovi;
	bool haveVideoInfo = obs_get_video_info(&ovi);
	uint32_t canvasWidth = haveVideoInfo ? ovi.base_width : 1920;
	uint32_t canvasHeight = haveVideoInfo ? ovi.base_height : 1080;

	obs_source_t *existing = obs_get_source_by_name(kCaptionSourceName);
	if (existing) {
		m_captionTextSource = existing;
	} else {
		m_captionTextSource = obs_source_create(kCaptionSourceId, kCaptionSourceName, nullptr, nullptr);
	}

	if (!m_captionTextSource)
		return;

	// text_ft2_source/text_gdiplus have no native center-align option, so a
	// fixed-width word-wrapped box (the previous approach) always rendered
	// text flush to its left edge — fine for a full line, but short captions
	// looked stuck on the left instead of centered. Letting the source
	// auto-size to its actual text extent and centering the scene item's
	// anchor point below (which OBS keeps fixed as the source's width
	// changes with each caption) achieves real centering instead.
	obs_data_t *sizingSettings = obs_data_create();
	obs_data_set_bool(sizingSettings, "word_wrap", false);
	obs_data_set_int(sizingSettings, "custom_width", 0);
	obs_source_update(m_captionTextSource, sizingSettings);
	obs_data_release(sizingSettings);

	applyCaptionStyleSettings();

	obs_source_t *sceneSource = obs_frontend_get_current_scene();
	if (!sceneSource)
		return;

	obs_scene_t *scene = obs_scene_from_source(sceneSource);
	if (scene) {
		obs_sceneitem_t *item = obs_scene_find_source(scene, kCaptionSourceName);
		if (!item) {
			obs_scene_add(scene, m_captionTextSource);
			item = obs_scene_find_source(scene, kCaptionSourceName);
		}

		// Re-applied on every Start (not just first creation) so a source
		// left over from an earlier plugin version, positioned for the old
		// fixed-width left-aligned box, gets re-anchored to the new centered
		// layout too.
		if (item) {
			obs_sceneitem_set_alignment(item, OBS_ALIGN_BOTTOM);
			struct vec2 pos;
			pos.x = static_cast<float>(canvasWidth) / 2.0f;
			pos.y = static_cast<float>(canvasHeight) * 0.9f;
			obs_sceneitem_set_pos(item, &pos);
		}
	}

	obs_source_release(sceneSource);
}

void CaptionsDock::updateCaptionTextSource(const QString &text)
{
	if (!m_captionTextSource)
		return;

	obs_data_t *settings = obs_data_create();
	obs_data_set_string(settings, "text", text.toUtf8().constData());
	obs_source_update(m_captionTextSource, settings);
	obs_data_release(settings);
}

void CaptionsDock::clearCaptionText()
{
	if (!m_captionTextSource)
		return;

	updateCaptionTextSource("");
}
