/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * RecognitionMenu.hh
 * Copyright (C) 2022-2024 Sandro Mani <manisandro@gmail.com>
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

#ifndef RECOGNITIONMENU_HH
#define RECOGNITIONMENU_HH

#include <QMenu>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "Config.hh"
#include "ui_CharacterListDialog.h"

class QActionGroup;

class RecognitionMenu : public QMenu {
	Q_OBJECT
public:
	RecognitionMenu(QWidget* parent = nullptr);

	void rebuild();
	const Config::Lang& getRecognitionLanguage() const { return m_curLang; }
	tesseract::PageSegMode getPageSegmentationMode() const;
	QString getCharacterWhitelist() const;
	QString getCharacterBlacklist() const;

signals:
	void languageChanged(Config::Lang lang);

private slots:
	void psmSelected(QAction* action);
	void setLanguage();
	void setMultiLanguage();

private:
	Config::Lang m_curLang;

	QMenu* m_menuMultilanguage = nullptr;
	QActionGroup* m_langMenuRadioGroup = nullptr;
	QActionGroup* m_langMenuCheckGroup = nullptr;
	QActionGroup* m_psmCheckGroup = nullptr;
	QAction* m_multilingualAction = nullptr;
	QDialog* m_charListDialog = nullptr;
	Ui::CharacterListDialog m_charListDialogUi;

	bool eventFilter(QObject* obj, QEvent* ev) override;
};

#endif // RECOGNITIONMENU_HH
