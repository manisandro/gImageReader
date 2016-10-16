/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.hh
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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

#ifndef RECOGNIZER_HH
#define RECOGNIZER_HH

#include "common.hh"
#include "Config.hh"

#include <cairomm/cairomm.h>

namespace tesseract {
class TessBaseAPI;
}

class Recognizer {
public:
	enum class OutputDestination { Buffer, Clipboard };

	Recognizer();
	std::vector<Glib::ustring> getAvailableLanguages() const;
	const Config::Lang& getSelectedLanguage() const {
		return m_curLang;
	}

	void setRecognizeMode(const Glib::ustring& mode);
	void updateLanguagesMenu();
	bool recognizeImage(const Cairo::RefPtr<Cairo::ImageSurface>& img, OutputDestination dest);
	sigc::signal<void,Config::Lang> signal_languageChanged() const {
		return m_signal_languageChanged;
	}

private:
	struct ProgressMonitor;
	class MultilingualMenuItem;

	enum class PageArea { EntirePage, Autodetect };
	enum class TaskState { Waiting, Succeeded, Failed };

	Gtk::Menu* m_menuLanguages;
	Gtk::Menu* m_menuPages;
	Gtk::Dialog* m_pagesDialog;
	Gtk::Entry* m_pagesEntry;
	Gtk::Label* m_langLabel;
	Gtk::Label* m_modeLabel;
	Gtk::Button* m_recognizeBtn;
	Gtk::Label* m_pageAreaLabel;
	Gtk::ComboBoxText* m_pageAreaCombo;
	sigc::signal<void,Config::Lang> m_signal_languageChanged;
	Gtk::RadioButtonGroup m_langMenuRadioGroup;
	std::vector<std::pair<Gtk::CheckMenuItem*,Glib::ustring>> m_langMenuCheckGroup;
	MultilingualMenuItem* m_multilingualRadio = nullptr;
	Gtk::CheckMenuItem* m_osdItem = nullptr;
	Config::Lang m_curLang;

	bool initTesseract(tesseract::TessBaseAPI& tess, const char* language = nullptr) const;
	void recognizeButtonClicked();
	void recognizeCurrentPage();
	void recognizeMultiplePages();
	void recognize(const std::vector<int>& pages, bool autodetectLayout = false);
	std::vector<int> selectPages(bool& autodetectLayout);
	void setLanguage(const Gtk::RadioMenuItem *item, const Config::Lang& lang);
	void setMultiLanguage();
	bool setPage(int page, bool autodetectLayout);
	bool onMultilingualMenuButtonEvent(GdkEventButton* ev);
	bool onMultilingualItemButtonEvent(GdkEventButton* ev, Gtk::CheckMenuItem* item);
	void manageInstalledLanguages();
};

#endif // RECOGNIZER_HH
