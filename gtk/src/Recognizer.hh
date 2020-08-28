/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.hh
 * Copyright (C) 2013-2020 Sandro Mani <manisandro@gmail.com>
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

namespace Ui {
class MainWindow;
}

class Recognizer {
public:
	enum class OutputDestination { Buffer, Clipboard };

	Recognizer(const Ui::MainWindow& _ui);

	void setRecognizeMode(const Glib::ustring& mode);
	bool recognizeImage(const Cairo::RefPtr<Cairo::ImageSurface>& img, OutputDestination dest);

private:
	class ProgressMonitor;

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
	ClassData m_classdata;

	void recognitionLanguageChanged(const Config::Lang& lang);
	void recognizeButtonClicked();
	void recognizeCurrentPage();
	void recognizeMultiplePages();
	void recognize(const std::vector<int>& pages, bool autodetectLayout = false);
	std::vector<int> selectPages(bool& autodetectLayout);
	PageData setPage(int page, bool autodetectLayout);
};

#endif // RECOGNIZER_HH
