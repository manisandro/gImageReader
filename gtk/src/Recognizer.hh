/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.hh
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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

#include "Config.hh"

#include <cairomm/cairomm.h>

namespace tesseract { class TessBaseAPI; }
namespace Ui { class MainWindow; }

class Recognizer {
public:
	enum class OutputDestination { Buffer, Clipboard };

	Recognizer(const Ui::MainWindow& _ui);
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
	struct PageData {
		bool success;
		std::string filename;
		int page;
		double angle;
		int resolution;
		std::vector<Cairo::RefPtr<Cairo::ImageSurface>> ocrAreas;
	};

	const Ui::MainWindow& ui;
	ConnectionsStore m_connections;
	Gtk::RadioButtonGroup m_langMenuRadioGroup;
	Gtk::RadioButtonGroup m_psmRadioGroup;
	int m_currentPsmMode;
	std::vector<std::pair<Gtk::CheckMenuItem*,Glib::ustring>> m_langMenuCheckGroup;
	MultilingualMenuItem* m_multilingualRadio = nullptr;
	Config::Lang m_curLang;

	sigc::signal<void,Config::Lang> m_signal_languageChanged;

	bool initTesseract(tesseract::TessBaseAPI& tess, const char* language = nullptr) const;
	void recognizeButtonClicked();
	void recognizeCurrentPage();
	void recognizeMultiplePages();
	void recognize(const std::vector<int>& pages, bool autodetectLayout = false);
	std::vector<int> selectPages(bool& autodetectLayout);
	void setLanguage(const Gtk::RadioMenuItem *item, const Config::Lang& lang);
	void setMultiLanguage();
	PageData setPage(int page, bool autodetectLayout);
	bool onMultilingualMenuButtonEvent(GdkEventButton* ev);
	bool onMultilingualItemButtonEvent(GdkEventButton* ev, Gtk::CheckMenuItem* item);
	void manageInstalledLanguages();
};

#endif // RECOGNIZER_HH
