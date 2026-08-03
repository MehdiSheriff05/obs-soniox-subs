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

#pragma once

#include <QElapsedTimer>
#include <QWidget>

#include "AudioBridge.h"
#include "SonioxClient.h"
#include "UpdateChecker.h"

#include <obs-frontend-api.h>
#include <obs.h>

class QCheckBox;
class QComboBox;
class QFontComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QSpinBox;
class QTabWidget;
class QTimer;

class CaptionsDock : public QWidget {
	Q_OBJECT

public:
	explicit CaptionsDock(QWidget *parent = nullptr);
	~CaptionsDock() override;

private slots:
	void onStartStopClicked();
	void onApiKeyChangeClicked();
	void onLevelChanged(float peakLevel);
	void onCaptionReady(const QString &text, bool isFinal);
	void onSonioxStatusChanged(SonioxClient::Status status);
	void onSonioxError(const QString &plainMessage, const QString &technicalDetail);
	void onWatchdogTick();
	void onMaxLineCharsChanged(int value);
	void onStatsTick();
	void onCheckForUpdatesClicked();
	void onInstallUpdateClicked();
	void onUpdateAvailable(const QString &newVersion);
	void onUpdateUpToDate();
	void onUpdateCheckFailed(const QString &plainMessage, const QString &technicalDetail);
	void onUpdateDownloadStarted();
	void onInstallerLaunched();
	void onUpdateDownloadFailed(const QString &plainMessage, const QString &technicalDetail);
	void onAppearanceSettingChanged();

private:
	void buildUi();
	QWidget *buildCaptionsTab();
	QWidget *buildStatsTab();
	QWidget *buildSettingsTab();
	static QWidget *wrapInScrollArea(QWidget *content);
	void refreshSourceList();
	void loadSettings();
	void saveSettings();
	void setRunningUiState(bool running);
	void setApiKeyLocked(bool locked);
	void setStatusText(const QString &plain, const QString &tooltip = QString());
	void ensureCaptionTextSource();
	void applyCaptionStyleSettings();
	void updateCaptionTextSource(const QString &text);
	void clearCaptionText();
	void updatePreviewCurrentLine(const QString &text);
	static void frontendEventCallback(enum obs_frontend_event event, void *privateData);
	static QString languageComboValue(const QComboBox *combo);
	static void setLanguageComboValue(QComboBox *combo, const QString &value);

	QTabWidget *m_tabWidget = nullptr;

	// Captions tab
	QComboBox *m_sourceCombo = nullptr;
	QPushButton *m_refreshSourcesButton = nullptr;
	QComboBox *m_speechLanguageCombo = nullptr;
	QComboBox *m_captionLanguageCombo = nullptr;
	QSpinBox *m_maxLineCharsSpin = nullptr;
	QPushButton *m_startStopButton = nullptr;
	QLabel *m_statusLabel = nullptr;
	QPlainTextEdit *m_captionPreview = nullptr;
	QProgressBar *m_levelMeter = nullptr;
	QLabel *m_updateBannerLabel = nullptr;
	QPushButton *m_updateInstallButton = nullptr;

	// Stats tab
	QLabel *m_elapsedTimeLabel = nullptr;
	QLabel *m_sessionCostLabel = nullptr;
	QLabel *m_reconnectCountLabel = nullptr;
	QTimer *m_statsTimer = nullptr;
	QElapsedTimer m_sessionElapsedTimer;
	int m_reconnectCount = 0;

	// Settings tab
	QLineEdit *m_apiKeyEdit = nullptr;
	QPushButton *m_apiKeyChangeButton = nullptr;
	QPushButton *m_checkUpdatesButton = nullptr;
	QLabel *m_updateStatusLabel = nullptr;

	// Appearance tab
	QFontComboBox *m_fontComboBox = nullptr;
	QSpinBox *m_fontSizeSpin = nullptr;
	QCheckBox *m_outlineCheckBox = nullptr;

	QTimer *m_watchdogTimer = nullptr;

	AudioBridge m_audioBridge;
	SonioxClient m_sonioxClient;
	UpdateChecker m_updateChecker;

	obs_source_t *m_captionTextSource = nullptr;
	QString m_lastFinalizedText;
	QString m_savedApiKey;
	int m_maxLineChars = 40;
	bool m_running = false;
	bool m_noAudioWarned = false;
	bool m_apiKeyLocked = false;

	static constexpr const char *kCaptionSourceName = "Soniox Live Captions";
	// Soniox's published flat real-time rate (transcription + translation
	// bundled, no extra charge) as of Aug 2026 — see soniox.com/pricing.
	// This is an estimate for the volunteer's own awareness, not a real
	// billing figure (actual billing is token-based, not pure wall-clock).
	static constexpr double kEstimatedCostPerHour = 0.12;
};
