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

#include <QString>

class QWidget;

// OBS's bundled Qt6 (the "Pre-Built Qt6" obs-deps package) ships the
// lrelease.prf qmake feature file but no actual lrelease/lupdate binaries,
// so the standard Qt Linguist .ts/.qm workflow isn't available at build
// time. This installs a QTranslator subclass that supplies French strings
// from an in-code table instead, wired up through the same tr() calls
// already used throughout the UI.
namespace Localization {

// The saved UI language code ("en" or "fr"), defaulting to "en" if never set.
QString savedLanguageCode();

// Persists the chosen UI language code to OBS's user config. Takes effect
// on next OBS restart (existing widgets' already-resolved text isn't
// retranslated live).
void saveLanguageCode(const QString &code);

// True if the user has never chosen a UI language before.
bool isFirstRun();

// Installs the French translator if the saved language is "fr". Call once,
// before any tr() calls happen (i.e. before building the dock's UI).
void installTranslatorFromSavedLanguage();

// Shows a modal "Choose your language / Choisissez votre langue" dialog and
// persists the choice. Only call when isFirstRun() is true.
void showFirstRunLanguageDialog(QWidget *parent);

} // namespace Localization
