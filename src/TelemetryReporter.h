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

class QNetworkAccessManager;

// Sends anonymous, aggregate-only usage events to PostHog (EU Cloud) — see
// the "Anonymous usage stats" section in README.md for exactly what this
// sends and why. Never sends audio, API keys, or anything identifying a
// specific person; only a random per-install ID, session duration, chosen
// languages, platform, and plugin version.
class TelemetryReporter : public QObject {
	Q_OBJECT

public:
	explicit TelemetryReporter(QObject *parent = nullptr);

	void reportSessionEnded(qint64 durationSeconds, const QString &speechLanguage, const QString &captionLanguage,
				 bool autoDetect);

private:
	QString distinctId();

	QNetworkAccessManager *m_network = nullptr;
	QString m_distinctId;
};
