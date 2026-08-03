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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>

#include <cstddef>
#include <cstdint>
#include <memory>

namespace ix {
class WebSocket;
}

class QTimer;

class SonioxClient : public QObject {
	Q_OBJECT

public:
	enum class Status {
		Disconnected,
		Connecting,
		Connected,
		Reconnecting,
		AuthError,
	};
	Q_ENUM(Status)

	explicit SonioxClient(QObject *parent = nullptr);
	~SonioxClient() override;

	void setApiKey(const QString &apiKey);
	// Empty sourceLanguageHints means "auto-detect" — no language_hints key
	// is sent at all, letting Soniox identify the spoken language itself
	// (useful when a speaker switches languages mid-session).
	void setLanguages(const QStringList &sourceLanguageHints, const QString &targetLanguage);

	void start();
	void stop();

	void sendAudio(const uint8_t *data, size_t byteCount);

	bool isRunning() const;
	Status status() const { return m_status; }

signals:
	void statusChanged(SonioxClient::Status status);
	void captionReady(const QString &text, bool isFinal);
	void errorOccurred(const QString &plainMessage, const QString &technicalDetail);

private slots:
	void handleOpen();
	void handleMessage(const QString &json);
	void handleClose(int code, const QString &reason);
	void handleTransportError(const QString &message);

private:
	QByteArray buildConfigMessage() const;
	void scheduleReconnect();
	void setStatus(Status status);

	std::unique_ptr<ix::WebSocket> m_ws;
	QTimer *m_reconnectTimer = nullptr;

	QString m_apiKey;
	QStringList m_sourceLanguageHints = {QStringLiteral("ur")};
	QString m_targetLanguage = QStringLiteral("en");

	Status m_status = Status::Disconnected;
	bool m_userRequestedStop = true;
	bool m_authFailed = false;
	int m_reconnectAttempts = 0;
};
