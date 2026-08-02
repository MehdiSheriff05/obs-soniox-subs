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

#include "SonioxClient.h"

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketMessage.h>
#include <ixwebsocket/IXWebSocketMessageType.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>
#include <QTimer>

#include <algorithm>

namespace {
const char *kSonioxUrl = "wss://stt-rt.soniox.com/transcribe-websocket";
const int kBaseReconnectDelayMs = 1000;
const int kMaxReconnectDelayMs = 16000;
} // namespace

SonioxClient::SonioxClient(QObject *parent) : QObject(parent)
{
	ix::initNetSystem();

	m_reconnectTimer = new QTimer(this);
	m_reconnectTimer->setSingleShot(true);
	connect(m_reconnectTimer, &QTimer::timeout, this, &SonioxClient::start);
}

SonioxClient::~SonioxClient()
{
	stop();
	ix::uninitNetSystem();
}

void SonioxClient::setApiKey(const QString &apiKey)
{
	m_apiKey = apiKey;
}

void SonioxClient::setLanguages(const QString &sourceLanguageHint, const QString &targetLanguage)
{
	m_sourceLanguageHint = sourceLanguageHint;
	m_targetLanguage = targetLanguage;
}

bool SonioxClient::isRunning() const
{
	return m_status == Status::Connected || m_status == Status::Connecting || m_status == Status::Reconnecting;
}

void SonioxClient::setStatus(Status status)
{
	m_status = status;
	emit statusChanged(m_status);
}

QByteArray SonioxClient::buildConfigMessage() const
{
	QJsonObject translation;
	translation["type"] = QStringLiteral("one_way");
	translation["target_language"] = m_targetLanguage;

	QJsonArray hints;
	hints.append(m_sourceLanguageHint);

	QJsonObject config;
	config["api_key"] = m_apiKey;
	config["model"] = QStringLiteral("stt-rt-v5");
	config["audio_format"] = QStringLiteral("s16le");
	config["sample_rate"] = 16000;
	config["num_channels"] = 1;
	config["language_hints"] = hints;
	config["enable_endpoint_detection"] = true;
	config["translation"] = translation;

	return QJsonDocument(config).toJson(QJsonDocument::Compact);
}

void SonioxClient::start()
{
	m_userRequestedStop = false;
	m_authFailed = false;

	if (m_apiKey.isEmpty()) {
		emit errorOccurred(tr("No API key set"), QStringLiteral("SonioxClient::start called with empty api_key"));
		setStatus(Status::AuthError);
		return;
	}

	setStatus(m_reconnectAttempts > 0 ? Status::Reconnecting : Status::Connecting);

	m_ws = std::make_unique<ix::WebSocket>();
	m_ws->setUrl(kSonioxUrl);

	m_ws->setOnMessageCallback([this](const ix::WebSocketMessagePtr &msg) {
		switch (msg->type) {
		case ix::WebSocketMessageType::Open:
			QMetaObject::invokeMethod(this, "handleOpen", Qt::QueuedConnection);
			break;
		case ix::WebSocketMessageType::Message:
			if (!msg->binary) {
				QString text = QString::fromStdString(msg->str);
				QMetaObject::invokeMethod(this, "handleMessage", Qt::QueuedConnection,
							  Q_ARG(QString, text));
			}
			break;
		case ix::WebSocketMessageType::Error: {
			QString reason = QString::fromStdString(msg->errorInfo.reason);
			QMetaObject::invokeMethod(this, "handleTransportError", Qt::QueuedConnection,
						  Q_ARG(QString, reason));
			break;
		}
		case ix::WebSocketMessageType::Close: {
			int code = msg->closeInfo.code;
			QString reason = QString::fromStdString(msg->closeInfo.reason);
			QMetaObject::invokeMethod(this, "handleClose", Qt::QueuedConnection, Q_ARG(int, code),
						  Q_ARG(QString, reason));
			break;
		}
		default:
			break;
		}
	});

	m_ws->start();
}

void SonioxClient::stop()
{
	m_userRequestedStop = true;
	m_reconnectAttempts = 0;
	m_reconnectTimer->stop();

	if (m_ws) {
		m_ws->stop();
		m_ws.reset();
	}

	setStatus(Status::Disconnected);
}

void SonioxClient::sendAudio(const uint8_t *data, size_t byteCount)
{
	if (!m_ws || m_status != Status::Connected)
		return;

	m_ws->sendBinary(std::string(reinterpret_cast<const char *>(data), byteCount));
}

void SonioxClient::handleOpen()
{
	m_reconnectAttempts = 0;
	setStatus(Status::Connected);

	if (m_ws) {
		QByteArray config = buildConfigMessage();
		m_ws->send(std::string(config.constData(), static_cast<size_t>(config.size())));
	}
}

void SonioxClient::handleMessage(const QString &json)
{
	QJsonParseError parseError;
	QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject())
		return;

	QJsonObject obj = doc.object();

	if (obj.contains(QStringLiteral("error_code"))) {
		QString errorType = obj.value(QStringLiteral("error_type")).toString();
		QString errorMessage = obj.value(QStringLiteral("error_message")).toString();
		int errorCode = obj.value(QStringLiteral("error_code")).toInt();

		QString plain;
		bool retryable = true;

		if (errorType == QStringLiteral("unauthenticated") || errorCode == 401 || errorCode == 403) {
			plain = tr("Invalid API key");
			retryable = false;
			m_authFailed = true;
		} else if (errorCode == 429) {
			plain = tr("Too many requests to translation service, retrying...");
		} else if (errorCode == 408) {
			plain = tr("Translation service timed out, retrying...");
		} else {
			plain = tr("Translation service error, retrying...");
		}

		QString technical = QStringLiteral("[%1] %2: %3").arg(errorCode).arg(errorType, errorMessage);
		emit errorOccurred(plain, technical);

		if (!retryable) {
			setStatus(Status::AuthError);
			m_ws.reset();
		}
		return;
	}

	if (obj.value(QStringLiteral("finished")).toBool())
		return;

	QJsonArray tokens = obj.value(QStringLiteral("tokens")).toArray();
	QString finalText;
	QString partialText;

	for (const QJsonValue v : tokens) {
		QJsonObject tok = v.toObject();
		if (tok.value(QStringLiteral("translation_status")).toString() != QStringLiteral("translation"))
			continue;

		QString text = tok.value(QStringLiteral("text")).toString();
		if (tok.value(QStringLiteral("is_final")).toBool())
			finalText += text;
		else
			partialText += text;
	}

	if (!finalText.isEmpty())
		emit captionReady(finalText, true);
	if (!partialText.isEmpty())
		emit captionReady(partialText, false);
}

void SonioxClient::handleClose(int code, const QString &reason)
{
	m_ws.reset();

	if (m_userRequestedStop || m_authFailed) {
		setStatus(m_authFailed ? Status::AuthError : Status::Disconnected);
		return;
	}

	emit errorOccurred(tr("Connection lost, retrying..."),
			   QStringLiteral("WebSocket closed (code %1): %2").arg(code).arg(reason));
	scheduleReconnect();
}

void SonioxClient::handleTransportError(const QString &message)
{
	if (m_authFailed)
		return;

	emit errorOccurred(tr("Connection problem, retrying..."), message);
}

void SonioxClient::scheduleReconnect()
{
	if (m_userRequestedStop || m_authFailed)
		return;

	setStatus(Status::Reconnecting);

	m_reconnectAttempts++;
	int delay = std::min(kBaseReconnectDelayMs * (1 << std::min(m_reconnectAttempts, 4)), kMaxReconnectDelayMs);
	m_reconnectTimer->start(delay);
}
