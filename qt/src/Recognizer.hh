/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.hh
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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
#include "OutputEditor.hh"
#include "ui_PageRangeDialog.h"
#include "ui_BatchModeDialog.h"

class QActionGroup;
class UI_MainWindow;
namespace Utils { class TesseractHandle; }

class Recognizer : public QObject {
	Q_OBJECT
public:
	enum class OutputDestination { Buffer, Clipboard };

	Recognizer(const UI_MainWindow& _ui);

public slots:
	void recognizeImage(const QImage& image, OutputDestination dest);
	void setRecognizeMode(const QString& mode);

private:
	class ProgressMonitor;
	enum class PageSelection { Prompt, Current, Multiple, Batch };
	enum class PageArea { EntirePage, Autodetect };
	struct PageData {
		bool success;
		QList<QImage> ocrAreas;
		OutputEditor::PageInfo pageInfo;
	};
	enum BatchExistingBehaviour { BatchOverwriteOutput, BatchSkipSource };

	const UI_MainWindow& ui;
	QMenu* m_menuPages = nullptr;
	QAction* m_actionBatchMode = nullptr;
	QDialog* m_pagesDialog;
	Ui::PageRangeDialog m_pagesDialogUi;
	QDialog* m_batchDialog;
	Ui::BatchModeDialog m_batchDialogUi;
	QString m_modeLabel;
	QString m_langLabel;

	QList<int> selectPages(bool& autodetectLayout);
	std::unique_ptr<Utils::TesseractHandle> setupTesseract();
	void recognize(const QList<int>& pages, bool autodetectLayout = false);
	void showRecognitionErrorsDialog(const QStringList& errors);

private slots:
	void recognitionLanguageChanged(const Config::Lang& lang);
	void clearLineEditPageRangeStyle();
	void recognizeButtonClicked();
	void recognizeCurrentPage();
	void recognizeMultiplePages();
	void recognizeBatch();
	PageData setPage(int page, bool autodetectLayout);
};

#endif // RECOGNIZER_HPP
