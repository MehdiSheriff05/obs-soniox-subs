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

#include <QDateTime>
#include <QWidget>

#include "AudioBridge.h"
#include "SonioxClient.h"

#include <obs-frontend-api.h>
#include <obs.h>

class QComboBox;
class QLineEdit;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QSpinBox;
class QTimer;
class QNetworkAccessManager;
class QNetworkReply;

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
	void refreshCostEstimate();
	void onCostReply(QNetworkReply *reply);

private:
	void buildUi();
	void refreshSourceList();
	void loadSettings();
	void saveSettings();
	void setRunningUiState(bool running);
	void setApiKeyLocked(bool locked);
	void setStatusText(const QString &plain, const QString &tooltip = QString());
	void ensureCaptionTextSource();
	void updateCaptionTextSource(const QString &text);
	void clearCaptionText();
	void updatePreviewCurrentLine(const QString &text);
	static void frontendEventCallback(enum obs_frontend_event event, void *privateData);

	QComboBox *m_sourceCombo = nullptr;
	QPushButton *m_refreshSourcesButton = nullptr;
	QLineEdit *m_apiKeyEdit = nullptr;
	QPushButton *m_apiKeyChangeButton = nullptr;
	QSpinBox *m_maxLineCharsSpin = nullptr;
	QPushButton *m_startStopButton = nullptr;
	QLabel *m_statusLabel = nullptr;
	QPlainTextEdit *m_captionPreview = nullptr;
	QProgressBar *m_levelMeter = nullptr;
	QLabel *m_costLabel = nullptr;
	QPushButton *m_costRefreshButton = nullptr;
	QTimer *m_watchdogTimer = nullptr;
	QTimer *m_costRefreshTimer = nullptr;
	QNetworkAccessManager *m_networkManager = nullptr;

	AudioBridge m_audioBridge;
	SonioxClient m_sonioxClient;

	obs_source_t *m_captionTextSource = nullptr;
	QString m_lastFinalizedText;
	QString m_savedApiKey;
	QDateTime m_sessionStartUtc;
	int m_maxLineChars = 60;
	bool m_running = false;
	bool m_noAudioWarned = false;
	bool m_apiKeyLocked = false;

	static constexpr const char *kCaptionSourceName = "Soniox Live Captions";
};
