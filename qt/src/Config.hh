/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Config.hh
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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
#include "ConfigSettings.hh"

#include <QMap>

class Config : public QDialog {
	Q_OBJECT
public:
	struct Lang {
		QString prefix, code, name;
	};

	Config(QWidget* parent = nullptr);
	~Config();

	void addSetting(AbstractSetting* setting) {
		m_settings.insert(setting->key(), setting);
	}
	template<class T>
	T* getSetting(const QString& key) const{
		if(m_settings.contains(key)){
			return static_cast<T*>(m_settings[key]);
		}
		return 0;
	}

	bool searchLangSpec(Lang& lang) const;
	void showDialog();

public slots:
	void disableDictInstall();
#if ENABLE_VERSIONCHECK
	void disableUpdateCheck();
#endif

private:
	static const QList<Lang> LANGUAGES;

	Ui::ConfigDialog ui;
	QFontDialog m_fontDialog;
	QMap<QString, AbstractSetting*> m_settings;


private slots:
	void addLanguage();
	void removeLanguage();
	void updateFontButton(const QFont& font);
	void langTableSelectionChanged(const QItemSelection & selected, const QItemSelection & deselected);
	void clearLineEditErrorState();
	void toggleAddLanguage(bool forceHide = false);
};

Q_DECLARE_METATYPE(Config::Lang)

#endif // CONFIG_HH
