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

#ifndef RECOGNIZER_HPP
#define RECOGNIZER_HPP

#include <QToolButton>
#include <memory>

#include "Config.hh"
#include "ui_PageRangeDialog.h"

class QActionGroup;
class UI_MainWindow;

class Recognizer : public QObject {
	Q_OBJECT
public:
	enum class OutputDestination { Buffer, Clipboard };

	Recognizer(const UI_MainWindow& _ui);

public slots:
	bool recognizeImage(const QImage& image, OutputDestination dest);
	void setRecognizeMode(const QString& mode);

private:
	class ProgressMonitor;
	enum class PageSelection { Prompt, Current, Multiple };
	enum class PageArea { EntirePage, Autodetect };
	struct PageData {
		bool success;
		QString filename;
		int page;
		double angle;
		int resolution;
		QList<QImage> ocrAreas;
	};

	const UI_MainWindow& ui;
	QMenu* m_menuPages = nullptr;
	QDialog* m_pagesDialog;
	Ui::PageRangeDialog m_pagesDialogUi;
	QString m_modeLabel;
	QString m_langLabel;

	QList<int> selectPages(bool& autodetectLayout);
	void recognize(const QList<int>& pages, bool autodetectLayout = false);

private slots:
	void recognitionLanguageChanged(const Config::Lang& lang);
	void clearLineEditPageRangeStyle();
	void recognizeButtonClicked();
	void recognizeCurrentPage();
	void recognizeMultiplePages();
	PageData setPage(int page, bool autodetectLayout);
};

#endif // RECOGNIZER_HPP
