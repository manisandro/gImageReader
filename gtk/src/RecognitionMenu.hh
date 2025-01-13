/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * RecognitionMenu.hh
 * Copyright (C) 2022-2025 Sandro Mani <manisandro@gmail.com>
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

#include <gtkmm.h>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "Config.hh"
#include "ui_CharacterListDialog.hh"


class RecognitionMenu : public Gtk::Menu {
public:
	RecognitionMenu();

	void rebuild();
	const Config::Lang& getRecognitionLanguage() const { return m_curLang; }
	tesseract::PageSegMode getPageSegmentationMode() const;
	Glib::ustring getCharacterWhitelist() const;
	Glib::ustring getCharacterBlacklist() const;

	sigc::signal<void, Config::Lang> signal_languageChanged() {
		return m_signal_languageChanged;
	}

private:
	class MultilingualMenuItem;

	ClassData m_classdata;
	Config::Lang m_curLang;
	Gtk::Menu* m_menuMultilanguage = nullptr;
	Gtk::RadioButtonGroup m_langMenuRadioGroup;
	Gtk::RadioButtonGroup m_psmRadioGroup;
	std::map<Gtk::CheckMenuItem*, std::pair<Glib::ustring, gint64 >> m_langMenuCheckGroup;
	MultilingualMenuItem* m_multilingualRadio = nullptr;
	Ui::CharacterListDialog m_charListDialogUi;

	sigc::signal<void, Config::Lang> m_signal_languageChanged;

	void setLanguage(const Gtk::RadioMenuItem* item, const Config::Lang& lang);
	void setMultiLanguage();
	bool onMultilingualMenuButtonEvent(GdkEventButton* ev);
	bool onMultilingualItemButtonEvent(GdkEventButton* ev, Gtk::CheckMenuItem* item);
	void manageCharaterLists();
};

#endif // RECOGNITIONMENU_HH
