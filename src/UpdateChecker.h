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
class QNetworkReply;

// Checks GitHub Releases for a newer tagged version than the running plugin,
// and can download + launch the matching platform installer. Installing
// only ever launches the downloaded installer; it never touches the running
// plugin's own files, so OBS must be restarted afterward to load the result.
class UpdateChecker : public QObject {
	Q_OBJECT

public:
	explicit UpdateChecker(QObject *parent = nullptr);

	void checkForUpdate();
	void downloadAndLaunchInstaller();

signals:
	void upToDate();
	void updateAvailable(const QString &newVersion);
	void checkFailed(const QString &plainMessage, const QString &technicalDetail);
	void downloadStarted();
	void installerLaunched();
	void downloadFailed(const QString &plainMessage, const QString &technicalDetail);

private slots:
	void handleCheckReply();
	void handleDownloadReply();

private:
	QNetworkAccessManager *m_network = nullptr;
	QString m_latestVersion;
	QString m_downloadUrl;
	QString m_downloadFileName;
};
