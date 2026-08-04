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

#include "TelemetryReporter.h"

#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/config-file.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

namespace {

// Public, write-only project token -- safe to embed in a distributed app by
// PostHog's own design (it can only submit events, not read account data).
const char *kPostHogApiKey = "phc_AdobASscunnW79B8j3FHuTbmXxVts8Up8Vbm4VW4JoDTA";
const char *kPostHogCaptureUrl = "https://eu.i.posthog.com/i/v0/e/";

QString currentPlatform()
{
#if defined(_WIN32)
	return QStringLiteral("windows");
#elif defined(__APPLE__)
	return QStringLiteral("macos");
#else
	return QStringLiteral("linux");
#endif
}

} // namespace

TelemetryReporter::TelemetryReporter(QObject *parent) : QObject(parent)
{
	m_network = new QNetworkAccessManager(this);
}

QString TelemetryReporter::distinctId()
{
	if (!m_distinctId.isEmpty())
		return m_distinctId;

	config_t *config = obs_frontend_get_user_config();
	if (!config)
		return QString();

	const char *existing = config_get_string(config, "SonioxTelemetry", "AnonId");
	if (existing && *existing) {
		m_distinctId = QString::fromUtf8(existing);
		return m_distinctId;
	}

	m_distinctId = QUuid::createUuid().toString(QUuid::WithoutBraces);
	config_set_string(config, "SonioxTelemetry", "AnonId", m_distinctId.toUtf8().constData());
	config_save(config);
	return m_distinctId;
}

void TelemetryReporter::reportSessionEnded(qint64 durationSeconds, const QString &speechLanguage,
					    const QString &captionLanguage, bool autoDetect)
{
	// Skip near-instant start/stop clicks -- not a real session, just noise.
	if (durationSeconds < 5)
		return;

	QString id = distinctId();
	if (id.isEmpty())
		return;

	QJsonObject properties;
	properties["app"] = QStringLiteral("obs-soniox-subs");
	properties["platform"] = currentPlatform();
	properties["plugin_version"] = QString::fromUtf8(PLUGIN_VERSION);
	properties["duration_seconds"] = static_cast<double>(durationSeconds);
	properties["speech_language"] = speechLanguage.isEmpty() ? QStringLiteral("auto") : speechLanguage;
	properties["caption_language"] = captionLanguage;
	properties["auto_detect"] = autoDetect;

	QJsonObject body;
	body["api_key"] = QString::fromUtf8(kPostHogApiKey);
	body["event"] = QStringLiteral("session_ended");
	body["distinct_id"] = id;
	body["properties"] = properties;

	QNetworkRequest request{QUrl(QString::fromUtf8(kPostHogCaptureUrl))};
	request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

	QNetworkReply *reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
	connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
