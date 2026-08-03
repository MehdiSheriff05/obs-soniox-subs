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

#include "UpdateChecker.h"

#include <plugin-support.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>

#include <algorithm>

namespace {

const char *kReleasesLatestUrl = "https://api.github.com/repos/MehdiSheriff05/obs-soniox-subs/releases/latest";

#if defined(_WIN32)
const char *kAssetSuffix = "windows-x64-Installer.exe";
#elif defined(__APPLE__)
const char *kAssetSuffix = "macos-universal.pkg";
#else
const char *kAssetSuffix = nullptr;
#endif

// Compares dotted numeric version strings (e.g. "1.2.0"). Any non-numeric
// suffix (like "-rc1") is ignored for comparison purposes; this only needs
// to distinguish real releases since GitHub's "latest" endpoint already
// excludes drafts and prereleases.
bool isNewerVersion(const QString &candidate, const QString &current)
{
	QStringList candidateParts = candidate.split(QLatin1Char('.'));
	QStringList currentParts = current.split(QLatin1Char('.'));

	int partCount = std::max(candidateParts.size(), currentParts.size());
	for (int i = 0; i < partCount; ++i) {
		int candidateValue = i < candidateParts.size() ? candidateParts[i].toInt() : 0;
		int currentValue = i < currentParts.size() ? currentParts[i].toInt() : 0;

		if (candidateValue != currentValue)
			return candidateValue > currentValue;
	}

	return false;
}

} // namespace

UpdateChecker::UpdateChecker(QObject *parent) : QObject(parent)
{
	m_network = new QNetworkAccessManager(this);
}

void UpdateChecker::checkForUpdate()
{
	QNetworkRequest request{QUrl(QString::fromUtf8(kReleasesLatestUrl))};
	request.setRawHeader("User-Agent", "obs-soniox-subs-update-checker");
	request.setRawHeader("Accept", "application/vnd.github+json");

	QNetworkReply *reply = m_network->get(request);
	connect(reply, &QNetworkReply::finished, this, &UpdateChecker::handleCheckReply);
}

void UpdateChecker::handleCheckReply()
{
	auto *reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply)
		return;
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError) {
		emit checkFailed(tr("Could not check for updates"), reply->errorString());
		return;
	}

	QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
	QJsonObject root = doc.object();

	QString tagName = root.value(QStringLiteral("tag_name")).toString();
	if (tagName.isEmpty()) {
		emit checkFailed(tr("Could not check for updates"),
				  QStringLiteral("GitHub API response had no tag_name"));
		return;
	}

	if (!isNewerVersion(tagName, QString::fromUtf8(PLUGIN_VERSION))) {
		emit upToDate();
		return;
	}

	if (!kAssetSuffix) {
		// No installer built for this platform (e.g. Linux) — still report
		// the version so the UI can point people at the Releases page.
		m_latestVersion = tagName;
		m_downloadUrl.clear();
		emit updateAvailable(tagName);
		return;
	}

	QString suffix = QString::fromUtf8(kAssetSuffix);
	for (const QJsonValue assetValue : root.value(QStringLiteral("assets")).toArray()) {
		QJsonObject asset = assetValue.toObject();
		QString name = asset.value(QStringLiteral("name")).toString();
		if (name.endsWith(suffix)) {
			m_downloadUrl = asset.value(QStringLiteral("browser_download_url")).toString();
			m_downloadFileName = name;
			break;
		}
	}

	m_latestVersion = tagName;
	emit updateAvailable(tagName);
}

void UpdateChecker::downloadAndLaunchInstaller()
{
	if (m_downloadUrl.isEmpty()) {
		emit downloadFailed(tr("No installer available for this platform"),
				     QStringLiteral("No release asset matched this platform's suffix"));
		return;
	}

	emit downloadStarted();

	QNetworkRequest request{QUrl(m_downloadUrl)};
	request.setRawHeader("User-Agent", "obs-soniox-subs-update-checker");
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

	QNetworkReply *reply = m_network->get(request);
	connect(reply, &QNetworkReply::finished, this, &UpdateChecker::handleDownloadReply);
}

void UpdateChecker::handleDownloadReply()
{
	auto *reply = qobject_cast<QNetworkReply *>(sender());
	if (!reply)
		return;
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError) {
		emit downloadFailed(tr("Could not download the update"), reply->errorString());
		return;
	}

	QString path = QDir::temp().filePath(m_downloadFileName);
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		emit downloadFailed(tr("Could not save the downloaded update"),
				     QStringLiteral("Could not open %1 for writing").arg(path));
		return;
	}
	file.write(reply->readAll());
	file.close();

#if defined(__APPLE__)
	bool launched = QProcess::startDetached(QStringLiteral("open"), {path});
#else
	bool launched = QProcess::startDetached(path, {});
#endif

	if (!launched) {
		emit downloadFailed(tr("Could not launch the installer"), QStringLiteral("Downloaded to %1").arg(path));
		return;
	}

	emit installerLaunched();
}
