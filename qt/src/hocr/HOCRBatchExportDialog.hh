/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRBatchExportDialog.hh
 * Copyright (C) 2022 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRBATCHEXPORTDIALOG_HH
#define HOCRBATCHEXPORTDIALOG_HH

#include <QDialog>
#include <QTimer>

#include "common.hh"
#include "ui_HOCRBatchExportDialog.h"


class FileTreeModel;
class HOCRPdfExportWidget;

class HOCRBatchExportDialog : public QDialog {

	Q_OBJECT

public:
	HOCRBatchExportDialog(QWidget* parent = nullptr);

private:
	enum ExportMode { ExportPdf, ExportOdt, ExportTxt };
	Ui::BatchExportDialog ui;

	FileTreeModel* m_sourceTreeModel = nullptr;
	FileTreeModel* m_outputTreeModel = nullptr;
	QMap<QString, QStringList> m_outputMap;
	HOCRPdfExportWidget* m_pdfExportWidget = nullptr;
	QTimer m_previewTimer;

private slots:
	void apply();
	void setSourceFolder();
	void setExportFormat();
	void updateExportPreview();
	void updateOutputTree();
};

#endif // HOCRBATCHEXPORTDIALOG_HH
