/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRBatchExportDialog.cc
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

#include "HOCRBatchExportDialog.hh"
#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "HOCRDocument.hh"
#include "HOCRPdfExporter.hh"
#include "HOCRPdfExportWidget.hh"
#include "HOCROdtExporter.hh"
#include "HOCRTextExporter.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Utils.hh"


HOCRBatchExportDialog::HOCRBatchExportDialog(Gtk::Window* parent) {

	if (!parent) {
		parent = MAIN->getWindow();
	}
	ui.setupUi();
	ui.BatchExportDialog->set_transient_for(*parent);

	Glib::RefPtr<Gtk::ListStore> formatComboModel = Gtk::ListStore::create(m_formatComboCols);
	ui.comboBoxFormat->set_model(formatComboModel);
	Gtk::TreeModel::Row row = *(formatComboModel->append());
	row[m_formatComboCols.format] = ExportPdf;
	row[m_formatComboCols.label] = _("PDF");
	row = *(formatComboModel->append());
	row[m_formatComboCols.format] = ExportOdt;
	row[m_formatComboCols.label] = _("ODT");
	row = *(formatComboModel->append());
	row[m_formatComboCols.format] = ExportTxt;
	row[m_formatComboCols.label] = _("Plain text");
	ui.comboBoxFormat->pack_start(m_formatComboCols.label);
	ui.comboBoxFormat->set_active(-1);

	m_sourceTreeModel = Glib::RefPtr<FileTreeModel>(new FileTreeModel());
	m_outputTreeModel = Glib::RefPtr<FileTreeModel>(new FileTreeModel());

	ui.treeViewInput->set_model(m_sourceTreeModel);
	setupFileTreeView(ui.treeViewInput);

	ui.treeViewOutput->set_model(m_outputTreeModel);
	setupFileTreeView(ui.treeViewOutput);

	ui.progressBar->hide();
	ui.tabWidget->get_nth_page(1)->set_visible(false);

	MAIN->setOutputMode(MainWindow::OutputModeHOCR);

	CONNECT(ui.buttonSourceFolder, clicked, [this] { setSourceFolder(); });
	CONNECT(ui.comboBoxFormat, changed, [this] { setExportFormat(); });
	CONNECT(ui.spinBoxExportLevel, value_changed, [this] { updateOutputTree(); });

	ADD_SETTING(ComboSetting("batchexportformat", ui.comboBoxFormat));
}

HOCRBatchExportDialog::~HOCRBatchExportDialog() {
	delete m_pdfExportWidget;
}

void HOCRBatchExportDialog::run() {
	while(ui.BatchExportDialog->run() == Gtk::RESPONSE_APPLY) {
		apply();
	}
}

void HOCRBatchExportDialog::setupFileTreeView(Gtk::TreeView* treeView) {
	treeView->set_tooltip_column(FileTreeModel::COLUMN_TOOLTIP);
	// First column: [icon][text]
	Gtk::TreeViewColumn* itemViewCol1 = Gtk::manage(new Gtk::TreeViewColumn(""));
	Gtk::CellRendererPixbuf& iconRenderer = *Gtk::manage(new Gtk::CellRendererPixbuf);
	Gtk::CellRendererText& textRenderer = *Gtk::manage(new Gtk::CellRendererText);
	textRenderer.property_ellipsize() = Pango::ELLIPSIZE_END;
	itemViewCol1->pack_start(iconRenderer, false);
	itemViewCol1->pack_start(textRenderer, true);
	itemViewCol1->add_attribute(iconRenderer, "pixbuf", FileTreeModel::COLUMN_ICON);
	itemViewCol1->add_attribute(textRenderer, "text", FileTreeModel::COLUMN_TEXT);
	itemViewCol1->add_attribute(textRenderer, "style", FileTreeModel::COLUMN_TEXTSTYLE);
	treeView->append_column(*itemViewCol1);
	treeView->set_expander_column(*itemViewCol1);
	itemViewCol1->set_expand(true);
	itemViewCol1->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
}

void HOCRBatchExportDialog::setSourceFolder() {
	std::string initialFolder = Utils::get_documents_dir();
	std::string dir = FileDialogs::open_folder_dialog(_("Select folder..."), initialFolder, "outputdir");
	if(dir.empty()) {
		return;
	}
	ui.lineEditSourceFolder->set_text(dir);
	m_sourceTreeModel->clear();

	std::vector<std::string> htmlFiles;
	scanSourceDir(dir, htmlFiles);
	for(const std::string& filename : htmlFiles) {
		m_sourceTreeModel->insertFile(filename, nullptr);
	}

	ui.treeViewInput->expand_all();
	updateOutputTree();
}

void HOCRBatchExportDialog::scanSourceDir(const std::string& dir, std::vector<std::string>& files) {
	Glib::RefPtr<Gio::FileEnumerator> it = Gio::File::create_for_path(dir)->enumerate_children();
	while(Glib::RefPtr<Gio::FileInfo> file = it->next_file()) {
		if(file->get_file_type() == Gio::FILE_TYPE_DIRECTORY) {
			scanSourceDir(Glib::build_filename(dir, file->get_name()), files);
		} else if(Utils::string_endswith(file->get_name(), ".html")) {
			files.push_back(Glib::build_filename(dir, file->get_name()));
		}
	}
}

void HOCRBatchExportDialog::setExportFormat() {
	updateOutputTree();
	ExportMode mode = (*ui.comboBoxFormat->get_active())[m_formatComboCols.format];
	if(mode == ExportPdf) {
		if(!m_pdfExportWidget) {
			OutputEditorHOCR* editor = static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor());
			m_pdfExportWidget = new HOCRPdfExportWidget(editor->getTool());
			ui.boxExportOptions->pack_start(*m_pdfExportWidget->getWidget(), true, true);
		}
		ui.tabWidget->get_nth_page(1)->show_all();
	} else {
		if (m_pdfExportWidget) {
			ui.boxExportOptions->remove(*m_pdfExportWidget->getWidget());
			delete m_pdfExportWidget;
			m_pdfExportWidget = nullptr;
		}
		ui.tabWidget->get_nth_page(1)->set_visible(false);
	}
}

void HOCRBatchExportDialog::updateOutputTree() {
	m_connectionPreviewTimeout.disconnect();
	m_outputTreeModel->clear();

	m_outputMap.clear();

	int exportLevel = ui.spinBoxExportLevel->get_value_as_int();

	std::string dir = ui.lineEditSourceFolder->get_text();
	if(dir.empty()) {
		return;
	}

	std::string exportSuffix;
	ExportMode mode = (*ui.comboBoxFormat->get_active())[m_formatComboCols.format];
	switch(mode) {
	case ExportPdf:
		exportSuffix = ".pdf";
		break;
	case ExportOdt:
		exportSuffix = ".pdf";
		break;
	case ExportTxt:
		exportSuffix = ".txt";
		break;
	}

	std::vector<std::string> htmlFiles;
	scanSourceDir(dir, htmlFiles);
	int deepestlevel = 0;
	int toplevel = std::count(dir.begin(), dir.end(), '/');
	for(const std::string& filename : htmlFiles) {
		deepestlevel = std::max(int(std::count(filename.begin(), filename.end(), '/')) - toplevel, deepestlevel);
	}
	int groupAboveDepth = std::max(0, deepestlevel - exportLevel);
	for(const std::string& filename : htmlFiles) {
		int level = std::count(filename.begin(), filename.end(), '/') - toplevel;
		std::string output = filename.substr(0, filename.rfind(".")) + exportSuffix;
		for (int i = level; i >= groupAboveDepth; --i) {
			output = Glib::path_get_dirname(output) + exportSuffix;
		}
		auto it = m_outputMap.find(output);
		if (it == m_outputMap.end()) {
			it = m_outputMap.insert(std::make_pair(output, std::vector<Glib::RefPtr<Gio::File>>())).first;
		}
		it->second.push_back(Gio::File::create_for_path(filename));
	}

	for(auto it = m_outputMap.begin(), itEnd = m_outputMap.end(); it != itEnd; ++it) {
		m_outputTreeModel->insertFile(it->first, nullptr);
	}
	ui.treeViewOutput->expand_all();

	if(m_pdfExportWidget) {
		m_connectionPreviewTimeout = Glib::signal_timeout().connect([this] { return updateExportPreview(); }, 250);
	}
}

void HOCRBatchExportDialog::apply() {
	m_connectionPreviewTimeout.disconnect();

	ui.progressBar->show();

	HOCRExporter* exporter = nullptr;
	HOCRPdfExporter::PDFSettings settings;

	ExportMode mode = (*ui.comboBoxFormat->get_active())[m_formatComboCols.format];
	switch(mode) {
	case ExportPdf:
		exporter = new HOCRPdfExporter();
		settings = m_pdfExportWidget->getPdfSettings();
		break;
	case ExportOdt:
		exporter = new HOCROdtExporter();
		break;
	case ExportTxt:
		exporter = new HOCRTextExporter();
		break;
	}

	OutputEditorHOCR* editor = static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor());

	int count = 0;
	for(auto it = m_outputMap.begin(), itEnd = m_outputMap.end(); it != itEnd; ++it) {
		editor->open(OutputEditorHOCR::InsertMode::Replace, it->second);
		exporter->run(editor->getDocument(), it->first, &settings);
		ui.progressBar->set_fraction(double(++count) / m_outputMap.size());
	}

	delete exporter;

	ui.progressBar->hide();
}

bool HOCRBatchExportDialog::updateExportPreview() {
	if (m_outputMap.empty()) {
		return false;
	}
	OutputEditorHOCR* editor = static_cast<OutputEditorHOCR*>(MAIN->getOutputEditor());
	editor->open(OutputEditorHOCR::InsertMode::Replace, m_outputMap.begin()->second);
	HOCRDocument* document = editor->getDocument();
	editor->selectPage(0);
	m_pdfExportWidget->setPreviewPage(document->pageCount() > 0 ? document : nullptr, document->pageCount() > 0 ? document->page(0) : nullptr);
	return false;
}
