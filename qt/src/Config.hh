/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.hh
 * Copyright (C) 2013-2025 Sandro Mani <manisandro@gmail.com>
 *
 * gImageReader is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * gImageReader is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CONFIG_HH
#define CONFIG_HH

#include "common.hh"
#include "ui_ConfigDialog.h"

#include <QFontDialog>
#include <QMap>

class Config : public QDialog {
	Q_OBJECT
public:
	struct Lang {
		QString prefix, code, name;
	};

	Config(QWidget* parent = nullptr);

	bool searchLangSpec(Lang& lang) const;
	QStringList searchLangCultures(const QString& code) const;
	void showDialog();

	bool useUtf8() const;
	bool useSystemDataLocations() const;
	QString tessdataLocation() const;
	QString spellingLocation() const;

	static void openTessdataDir();
	static void openSpellingDir();
	static QString lookupLangCode(const QString& prefix) { return LANG_LOOKUP[prefix]; }
	static QStringList getAvailableLanguages();

public slots:
	void disableDictInstall();
	void disableUpdateCheck();

private:
	enum Location {SystemLocation = 0, UserLocation = 1};
	static const QList<Lang> LANGUAGES;
	static const QMap<QString, QString> LANG_LOOKUP;
	static const QMultiMap<QString, QString> LANGUAGE_CULTURES;

	Ui::ConfigDialog ui;
	QFontDialog m_fontDialog;

	static QString spellingLocation(Location location);
	static QString tessdataLocation(Location location);

private slots:
	void addLanguage();
	void removeLanguage();
	void updateFontButton(const QFont& font);
	void langTableSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);
	void clearLineEditErrorState();
	void setDataLocations(int idx);
	void toggleAddLanguage(bool forceHide = false);
};

Q_DECLARE_METATYPE(Config::Lang)

#endif // CONFIG_HH
