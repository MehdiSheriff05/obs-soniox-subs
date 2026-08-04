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

#include "Localization.h"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QCoreApplication>
#include <QHash>
#include <QMessageBox>
#include <QPushButton>
#include <QTranslator>

namespace {

class FrenchTranslator : public QTranslator {
public:
	QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
			   int n = -1) const override
	{
		Q_UNUSED(context);
		Q_UNUSED(disambiguation);
		Q_UNUSED(n);
		auto it = table().constFind(QString::fromUtf8(sourceText));
		return it != table().constEnd() ? it.value() : QString();
	}

	bool isEmpty() const override { return false; }

private:
	static const QHash<QString, QString> &table();
};

const QHash<QString, QString> &FrenchTranslator::table()
{
	static const QHash<QString, QString> kTable = {
		// Tab titles
		{QStringLiteral("Captions"), QStringLiteral("Sous-titres")},
		{QStringLiteral("Stats"), QStringLiteral("Statistiques")},
		{QStringLiteral("Settings"), QStringLiteral("Paramètres")},

		// Footer
		{QStringLiteral("<a href=\"https://github.com/MehdiSheriff05/obs-soniox-subs/releases\">"
				 "Soniox Live Captions Plugin v%1</a>"),
		 QStringLiteral("<a href=\"https://github.com/MehdiSheriff05/obs-soniox-subs/releases\">"
				"Plugin Sous-titres en direct Soniox v%1</a>")},
		{QStringLiteral("<a href=\"https://github.com/MehdiSheriff05\">by MehdiSheriff05</a>"),
		 QStringLiteral("<a href=\"https://github.com/MehdiSheriff05\">par MehdiSheriff05</a>")},

		// Captions tab
		{QStringLiteral("Refresh"), QStringLiteral("Actualiser")},
		{QStringLiteral("Audio Source:"), QStringLiteral("Source audio :")},
		{QStringLiteral("How much translated text accumulates on screen before it clears and starts a "
				 "fresh line."),
		 QStringLiteral("Quantité de texte traduit qui s'accumule à l'écran avant de s'effacer et de "
				"recommencer une nouvelle ligne.")},
		{QStringLiteral("Max caption length:"), QStringLiteral("Longueur maximale des sous-titres :")},
		{QStringLiteral("Start"), QStringLiteral("Démarrer")},
		{QStringLiteral("Stop"), QStringLiteral("Arrêter")},
		{QStringLiteral("Audio level:"), QStringLiteral("Niveau audio :")},
		{QStringLiteral("Live captions:"), QStringLiteral("Sous-titres en direct :")},
		{QStringLiteral("Idle"), QStringLiteral("Inactif")},
		{QStringLiteral("Pick an audio source first"), QStringLiteral("Choisissez d'abord une source audio")},
		{QStringLiteral("Enter a Soniox API key first"), QStringLiteral("Entrez d'abord une clé API Soniox")},
		{QStringLiteral("Could not use that audio source"),
		 QStringLiteral("Impossible d'utiliser cette source audio")},
		{QStringLiteral("Connecting..."), QStringLiteral("Connexion en cours...")},

		// Stats tab
		{QStringLiteral("Elapsed time:"), QStringLiteral("Temps écoulé :")},
		{QStringLiteral("Estimated using Soniox's published real-time rate ($%1/hour, translation "
				 "included). This is an estimate for your own awareness, not an exact bill — "
				 "actual Soniox billing is token-based."),
		 QStringLiteral("Estimation basée sur le tarif temps réel publié par Soniox ($%1/heure, "
				"traduction incluse). Il s'agit d'une estimation pour votre information, pas "
				"d'une facture exacte — la facturation réelle de Soniox est basée sur des "
				"jetons.")},
		{QStringLiteral("Estimated cost:"), QStringLiteral("Coût estimé :")},
		{QStringLiteral("How many times the connection to Soniox dropped and had to retry during this "
				 "session — a high count may indicate a shaky network."),
		 QStringLiteral("Nombre de fois où la connexion à Soniox a été interrompue et a dû être "
				"relancée pendant cette session — un nombre élevé peut indiquer un réseau "
				"instable.")},
		{QStringLiteral("Reconnects:"), QStringLiteral("Reconnexions :")},

		// Settings tab
		{QStringLiteral("Interface language:"), QStringLiteral("Langue de l'interface :")},
		{QStringLiteral("Change interface language?"), QStringLiteral("Changer la langue de l'interface ?")},
		{QStringLiteral("Switch the interface language to %1? OBS needs to restart for this to take "
				 "effect."),
		 QStringLiteral("Passer la langue de l'interface à %1 ? OBS doit redémarrer pour que ce "
				"changement prenne effet.")},
		{QStringLiteral("Restart required"), QStringLiteral("Redémarrage requis")},
		{QStringLiteral("The interface language has been changed. Restart OBS for it to take effect."),
		 QStringLiteral("La langue de l'interface a été modifiée. Redémarrez OBS pour que ce changement "
				"prenne effet.")},
		{QStringLiteral("Soniox API key"), QStringLiteral("Clé API Soniox")},
		{QStringLiteral("Change"), QStringLiteral("Modifier")},
		{QStringLiteral("Cancel"), QStringLiteral("Annuler")},
		{QStringLiteral("API Key:"), QStringLiteral("Clé API :")},
		{QStringLiteral("The language being spoken. Auto-detect is recommended if the speaker switches "
				 "languages mid-session (e.g. Urdu to Arabic) — Soniox identifies the spoken "
				 "language itself instead of being locked to one. You can also type any "
				 "Soniox-supported language code directly."),
		 QStringLiteral("La langue parlée. La détection automatique est recommandée si l'orateur change "
				"de langue en cours de session (par ex. de l'ourdou à l'arabe) — Soniox "
				"identifie lui-même la langue parlée au lieu d'être limité à une seule. Vous "
				"pouvez aussi saisir directement tout code de langue pris en charge par "
				"Soniox.")},
		{QStringLiteral("Auto-detect (any language)"), QStringLiteral("Détection automatique (toute langue)")},
		{QStringLiteral("Arabic"), QStringLiteral("Arabe")},
		{QStringLiteral("Urdu"), QStringLiteral("Ourdou")},
		{QStringLiteral("Urdu + Arabic (mixed)"), QStringLiteral("Ourdou + Arabe (mixte)")},
		{QStringLiteral("English"), QStringLiteral("Anglais")},
		{QStringLiteral("French"), QStringLiteral("Français")},
		{QStringLiteral("Bengali"), QStringLiteral("Bengali")},
		{QStringLiteral("Hindi"), QStringLiteral("Hindi")},
		{QStringLiteral("Punjabi"), QStringLiteral("Pendjabi")},
		{QStringLiteral("Pashto"), QStringLiteral("Pachto")},
		{QStringLiteral("Persian/Farsi"), QStringLiteral("Persan/Farsi")},
		{QStringLiteral("Turkish"), QStringLiteral("Turc")},
		{QStringLiteral("Indonesian"), QStringLiteral("Indonésien")},
		{QStringLiteral("Malay"), QStringLiteral("Malais")},
		{QStringLiteral("Somali"), QStringLiteral("Somali")},
		{QStringLiteral("Speech language:"), QStringLiteral("Langue parlée :")},
		{QStringLiteral("The language captions are translated into. You can also type any "
				 "Soniox-supported language code directly."),
		 QStringLiteral("La langue vers laquelle les sous-titres sont traduits. Vous pouvez aussi saisir "
				"directement tout code de langue pris en charge par Soniox.")},
		{QStringLiteral("Spanish"), QStringLiteral("Espagnol")},
		{QStringLiteral("German"), QStringLiteral("Allemand")},
		{QStringLiteral("Caption language:"), QStringLiteral("Langue des sous-titres :")},
		{QStringLiteral("Only takes effect if this font is actually installed on this computer — Poppins "
				 "is a Google font, not preinstalled on macOS or Windows."),
		 QStringLiteral("Ne s'applique que si cette police est réellement installée sur cet ordinateur — "
				"Poppins est une police Google, non préinstallée sur macOS ou Windows.")},
		{QStringLiteral("Font:"), QStringLiteral("Police :")},
		{QStringLiteral("Font size:"), QStringLiteral("Taille de police :")},
		{QStringLiteral("Font color:"), QStringLiteral("Couleur de la police :")},
		{QStringLiteral("Choose font color"), QStringLiteral("Choisir la couleur de la police")},
		{QStringLiteral("Show text outline / border"),
		 QStringLiteral("Afficher le contour / la bordure du texte")},
		{QStringLiteral("Check for Updates"), QStringLiteral("Vérifier les mises à jour")},
		{QStringLiteral("Checking for updates..."), QStringLiteral("Vérification des mises à jour...")},
		{QStringLiteral("Install Update"), QStringLiteral("Installer la mise à jour")},

		// Status / error messages (CaptionsDock, SonioxClient, UpdateChecker)
		{QStringLiteral("Live — captions are being translated"),
		 QStringLiteral("En direct — les sous-titres sont en cours de traduction")},
		{QStringLiteral("Connection lost, retrying..."), QStringLiteral("Connexion perdue, nouvelle tentative...")},
		{QStringLiteral("Invalid API key"), QStringLiteral("Clé API invalide")},
		{QStringLiteral("Authentication failed. Check the Soniox API key."),
		 QStringLiteral("Échec de l'authentification. Vérifiez la clé API Soniox.")},
		{QStringLiteral("No audio detected from selected source"),
		 QStringLiteral("Aucun audio détecté depuis la source sélectionnée")},
		{QStringLiteral("Update available: v%1"), QStringLiteral("Mise à jour disponible : v%1")},
		{QStringLiteral("You're up to date (v%1)"), QStringLiteral("Vous êtes à jour (v%1)")},
		{QStringLiteral("Downloading update..."), QStringLiteral("Téléchargement de la mise à jour...")},
		{QStringLiteral("Installer launched — finish the install, then restart OBS."),
		 QStringLiteral("Programme d'installation lancé — terminez l'installation, puis redémarrez OBS.")},
		{QStringLiteral("No API key set"), QStringLiteral("Aucune clé API définie")},
		{QStringLiteral("Too many requests to translation service, retrying..."),
		 QStringLiteral("Trop de requêtes vers le service de traduction, nouvelle tentative...")},
		{QStringLiteral("Translation service timed out, retrying..."),
		 QStringLiteral("Le service de traduction a expiré, nouvelle tentative...")},
		{QStringLiteral("Translation service error, retrying..."),
		 QStringLiteral("Erreur du service de traduction, nouvelle tentative...")},
		{QStringLiteral("Connection problem, retrying..."),
		 QStringLiteral("Problème de connexion, nouvelle tentative...")},
		{QStringLiteral("Could not check for updates"), QStringLiteral("Impossible de vérifier les mises à jour")},
		{QStringLiteral("No installer available for this platform"),
		 QStringLiteral("Aucun programme d'installation disponible pour cette plateforme")},
		{QStringLiteral("Could not download the update"),
		 QStringLiteral("Impossible de télécharger la mise à jour")},
		{QStringLiteral("Could not save the downloaded update"),
		 QStringLiteral("Impossible d'enregistrer la mise à jour téléchargée")},
		{QStringLiteral("Could not launch the installer"),
		 QStringLiteral("Impossible de lancer le programme d'installation")},
	};
	return kTable;
}

} // namespace

QString Localization::savedLanguageCode()
{
	config_t *config = obs_frontend_get_user_config();
	if (!config)
		return QStringLiteral("en");

	const char *code = config_get_string(config, "SonioxCaptions", "UiLanguage");
	return (code && *code) ? QString::fromUtf8(code) : QStringLiteral("en");
}

void Localization::saveLanguageCode(const QString &code)
{
	config_t *config = obs_frontend_get_user_config();
	if (!config)
		return;

	config_set_string(config, "SonioxCaptions", "UiLanguage", code.toUtf8().constData());
	config_save(config);
}

bool Localization::isFirstRun()
{
	config_t *config = obs_frontend_get_user_config();
	if (!config)
		return false;

	return !config_has_user_value(config, "SonioxCaptions", "UiLanguage");
}

void Localization::installTranslatorFromSavedLanguage()
{
	if (savedLanguageCode() != QStringLiteral("fr"))
		return;

	static FrenchTranslator *translator = new FrenchTranslator();
	QCoreApplication::installTranslator(translator);
}

void Localization::showFirstRunLanguageDialog(QWidget *parent)
{
	QMessageBox box(parent);
	box.setIcon(QMessageBox::Question);
	box.setWindowTitle(QStringLiteral("Choose your language / Choisissez votre langue"));
	box.setText(QStringLiteral("Choose your language for Live Captions.\n"
				    "Choisissez votre langue pour Live Captions."));

	QPushButton *englishButton = box.addButton(QStringLiteral("English"), QMessageBox::AcceptRole);
	QPushButton *frenchButton = box.addButton(QStringLiteral("Français"), QMessageBox::AcceptRole);
	box.setDefaultButton(englishButton);
	box.exec();

	saveLanguageCode(box.clickedButton() == frenchButton ? QStringLiteral("fr") : QStringLiteral("en"));
}
