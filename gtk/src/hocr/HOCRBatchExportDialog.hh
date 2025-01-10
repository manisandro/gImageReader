/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRBatchExportDialog.hh
 * Copyright (C) 2020-2024 Sandro Mani <manisandro@gmail.com>
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

#include "common.hh"
#include "FileTreeModel.hh"
#include "ui_HOCRBatchExportDialog.hh"


class HOCRPdfExportWidget;

class HOCRBatchExportDialog {

public:
	HOCRBatchExportDialog(Gtk::Window* parent = nullptr);
	~HOCRBatchExportDialog();
	void run();

private:
	enum ExportMode { ExportPdf, ExportOdt, ExportTxt };
	Ui::HOCRBatchExportDialog ui;

	ClassData m_classdata;
	Glib::RefPtr<FileTreeModel> m_sourceTreeModel;
	Glib::RefPtr<FileTreeModel> m_outputTreeModel;
	std::map<std::string, std::vector<Glib::RefPtr<Gio::File >>> m_outputMap;
	HOCRPdfExportWidget* m_pdfExportWidget = nullptr;
	sigc::connection m_connectionPreviewTimeout;

	struct FormatComboColums : public Gtk::TreeModel::ColumnRecord {
		Gtk::TreeModelColumn<ExportMode> format;
		Gtk::TreeModelColumn<Glib::ustring> label;
		FormatComboColums() {
			add(format);
			add(label);
		}
	} m_formatComboCols;

	void setupFileTreeView(Gtk::TreeView* treeView);
	void apply();
	void setSourceFolder();
	void scanSourceDir(const std::string& dir, std::vector<std::string>& files);
	void setExportFormat();
	bool updateExportPreview();
	void updateOutputTree();
};

#endif // HOCRBATCHEXPORTDIALOG_HH
