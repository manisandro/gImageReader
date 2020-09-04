/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.cc
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

#include "FileDialogs.hh"
#include "FileTreeModel.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <pangomm.h>
#include <poppler-document.h>


SourceManager::SourceManager(const Ui::MainWindow& _ui)
	: ui(_ui) {

	// Document tree view
	m_fileTreeModel = Glib::RefPtr<FileTreeModel>(new FileTreeModel());

	ui.treeviewSources->set_model(m_fileTreeModel);
	ui.treeviewSources->get_selection()->set_mode(Gtk::SELECTION_MULTIPLE);
	ui.treeviewSources->set_tooltip_column(FileTreeModel::COLUMN_TOOLTIP);
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
	ui.treeviewSources->append_column(*itemViewCol1);
	ui.treeviewSources->set_expander_column(*itemViewCol1);
	itemViewCol1->set_expand(true);
	itemViewCol1->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	// Second column: editable icon
	Gtk::TreeViewColumn* itemViewCol2 = Gtk::manage(new Gtk::TreeViewColumn(""));
	Gtk::CellRendererPixbuf& editiconRenderer = *Gtk::manage(new Gtk::CellRendererPixbuf);
	itemViewCol2->pack_start(editiconRenderer, true);
	itemViewCol2->add_attribute(editiconRenderer, "pixbuf", FileTreeModel::COLUMN_EDITICON);
	ui.treeviewSources->append_column(*itemViewCol2);
	itemViewCol2->set_fixed_width(32);
	itemViewCol2->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);

	ui.treeviewSources->set_fixed_height_mode();
	ui.treeviewSources->property_activate_on_single_click() = true;

	Glib::RefPtr<Gtk::RecentFilter> recentFilter = Gtk::RecentFilter::create();
	recentFilter->add_pixbuf_formats();
	recentFilter->add_mime_type("application/pdf");
	recentFilter->add_pattern("*.pdf");
	recentFilter->add_mime_type("image/vnd.djvu");
	recentFilter->add_pattern("*.djvu");
	Gtk::RecentChooserMenu* recentChooser = Gtk::manage(new Gtk::RecentChooserMenu());
	recentChooser->set_filter(recentFilter);
	recentChooser->set_local_only(false);
	recentChooser->set_show_not_found(false);
	recentChooser->set_show_tips(true);
	recentChooser->set_sort_type(Gtk::RECENT_SORT_MRU);
	ui.menuitemSourcesRecent->set_submenu(*recentChooser);

	m_clipboard = Gtk::Clipboard::get_for_display(Gdk::Display::get_default());

	CONNECT(ui.buttonSources, toggled, [this] { ui.notebookSources->set_visible(ui.buttonSources->get_active()); });
	CONNECT(ui.buttonSourcesAdd, clicked, [this] { openSources(); });
	CONNECT(ui.menubuttonSourcesAdd, clicked, [this] { ui.menuitemSourcesPaste->set_sensitive(m_clipboard->wait_is_image_available()); });
	CONNECT(ui.menuitemSourcesAddFolder, activate, [this] { addFolder(); });
	CONNECT(ui.menuitemSourcesPaste, activate, [this] { pasteClipboard(); });
	CONNECT(ui.menuitemSourcesScreenshot, activate, [this] { takeScreenshot(); });
	CONNECT(ui.buttonSourcesRemove, clicked, [this] { removeSource(false); });
	CONNECT(ui.buttonSourcesDelete, clicked, [this] { removeSource(true); });
	CONNECT(ui.buttonSourcesClear, clicked, [this] { clearSources(); });
	m_connectionSelectionChanged = CONNECT(ui.treeviewSources->get_selection(), changed, [this] { selectionChanged(); });
	CONNECT(ui.treeviewSources, row_activated, [this] (const Gtk::TreePath & path, Gtk::TreeViewColumn * col) { indexClicked(path, col); });
	CONNECT(recentChooser, item_activated, [this, recentChooser] { addSources({Gio::File::create_for_uri(recentChooser->get_current_uri())}); });
	CONNECT(m_fileTreeModel, row_inserted, [this] (const Gtk::TreePath&, const Gtk::TreeIter&) { ui.buttonSourcesClear->set_sensitive(!m_fileTreeModel->empty()); });
	CONNECT(m_fileTreeModel, row_deleted, [this] (const Gtk::TreePath&) { ui.buttonSourcesClear->set_sensitive(!m_fileTreeModel->empty()); });

	// Handle drops on the scrolled window
	ui.scrollwinSources->drag_dest_set({Gtk::TargetEntry("text/uri-list")}, Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
	CONNECT(ui.scrollwinSources, drag_data_received, sigc::ptr_fun(Utils::handle_drag_drop));
}

SourceManager::~SourceManager() {
	clearSources();
}

int SourceManager::addSources(const std::vector<Glib::RefPtr<Gio::File>>& files, bool suppressTextWarning) {
	std::vector<Glib::ustring> failed;
	int added = 0;
	PdfWithTextAction textAction = suppressTextWarning ? PdfWithTextAction::Add : PdfWithTextAction::Ask;

	std::vector<Gtk::TreeIter> selectIters;

	for(Glib::RefPtr<Gio::File> file : files) {
		std::string filename = file->get_path();
		if(!file->query_exists()) {
			failed.push_back(filename);
			continue;
		}
		Gtk::TreeIter index = m_fileTreeModel->findFile(filename);
		if(index) {
			selectIters.push_back(index);
			++added;
			continue;
		}
		Source* source = new Source(file, file->get_basename(), file->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), false);
#ifdef G_OS_WIN32
		if(Glib::ustring(filename.substr(filename.length() - 4)).lowercase() == ".pdf" && !checkPdfSource(source, textAction, failed)) {
#else
		if(Utils::get_content_type(filename) == "application/pdf" && !checkPdfSource(source, textAction, failed)) {
#endif
			delete source;
			continue;
		}
		index = m_fileTreeModel->insertFile(filename, source);
		std::string dir = Glib::path_get_dirname(filename);
		if(m_watchedDirectories.find(dir) == m_watchedDirectories.end()) {
			Glib::RefPtr<Gio::FileMonitor> dirmonitor = Gio::File::create_for_path(dir)->monitor_directory(Gio::FILE_MONITOR_SEND_MOVED);
			CONNECT(dirmonitor, changed, sigc::mem_fun(*this, &SourceManager::dirChanged));
			m_watchedDirectories.insert(std::make_pair(dir, std::make_pair(1, dirmonitor)));
		} else {
			m_watchedDirectories[dir].first += 1;
		}
		std::string base = Utils::split_filename(filename).first;
		if(Glib::file_test(base + ".txt", Glib::FILE_TEST_EXISTS) || Glib::file_test(base + ".html", Glib::FILE_TEST_EXISTS)) {
			m_fileTreeModel->setFileEditable(index, true);
		}
		selectIters.push_back(index);
		CONNECT(source->monitor, changed, sigc::mem_fun(*this, &SourceManager::fileChanged));

		Gtk::RecentManager::get_default()->add_item(file->get_uri());
		++added;
	}
	if(added > 0) {
		ui.treeviewSources->get_selection()->unselect_all();
	}
	m_connectionSelectionChanged.block(true);
	if(added > 0) {
		Glib::RefPtr<Gtk::TreeSelection> selection = ui.treeviewSources->get_selection();
		bool scrolled = false;
		for(const Gtk::TreeIter& it : selectIters) {
			Gtk::TreePath path = m_fileTreeModel->get_path(it);
			ui.treeviewSources->expand_to_path(path);
			if(!scrolled) {
				ui.treeviewSources->scroll_to_row(path);
				scrolled = true;
			}
			selection->select(it);
		}
	}
	m_connectionSelectionChanged.block(false);
	selectionChanged();
	if(!failed.empty()) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Unable to open files"), Glib::ustring::compose(_("The following files could not be opened:\n%1"), Utils::string_join(failed, "\n")));
	}
	return added;
}

bool SourceManager::checkPdfSource(Source* source, PdfWithTextAction& textAction, std::vector<Glib::ustring>& failed) const {
	GError* err = nullptr;
	std::string filename = source->file->get_path();
	PopplerDocument* document = poppler_document_new_from_file(Glib::filename_to_uri(filename).c_str(), 0, &err);

	// Unlock if necessary
	if(err && g_error_matches (err, POPPLER_ERROR, POPPLER_ERROR_ENCRYPTED)) {
		g_error_free(err);
		err = nullptr;
		ui.labelPdfpassword->set_text(Glib::ustring::compose(_("Enter password for file '%1':"), source->file->get_basename()));
		ui.entryPdfpassword->set_text("");
		while(true) {
			ui.entryPdfpassword->select_region(0, -1);
			ui.entryPdfpassword->grab_focus();
			int response = ui.dialogPdfpassword->run();
			ui.dialogPdfpassword->hide();
			if(response != Gtk::RESPONSE_OK) {
				return false;
			}
			Glib::ustring pass = ui.entryPdfpassword->get_text();
			document = poppler_document_new_from_file(Glib::filename_to_uri(filename).c_str(), pass.c_str(), &err);
			if(!err) {
				source->password = pass;
				break;
			} else if(!g_error_matches (err, POPPLER_ERROR, POPPLER_ERROR_ENCRYPTED)) {
				break;
			}
			g_error_free(err);
			err = nullptr;
		}
	}
	if(err) {
		g_error_free(err);
		err = nullptr;
		failed.push_back(filename);
	}

	// Check whether the PDF already contains text
	if(textAction != PdfWithTextAction::Add) {
		bool haveText = false;
		for (int i = 0, n = poppler_document_get_n_pages(document); i < n; ++i) {
			PopplerPage* poppage = poppler_document_get_page(document, i);
			bool hasText = poppler_page_get_text(poppage) != nullptr;
			g_object_unref(poppage);
			if(hasText) {
				haveText = true;
				break;
			}
		}
		if(haveText && textAction == PdfWithTextAction::Skip) {
			return false;
		}
		int response = Utils::messageBox(Gtk::MESSAGE_QUESTION, _("PDF with text"), Glib::ustring::compose(_("The PDF file already contains text:\n%1\nOpen it regardless?"), filename), "", Utils::Button::Yes | Utils::Button::YesAll | Utils::Button::No | Utils::Button::NoAll);
		if(response == Utils::Button::No) {
			return false;
		} else if(response == Utils::Button::NoAll) {
			textAction = PdfWithTextAction::Skip;
			return false;
		} else if(response == Utils::Button::YesAll) {
			textAction = PdfWithTextAction::Add;
		}
	}

	// Extract document metadata
	source->author = poppler_document_get_author(document) ? poppler_document_get_author(document) : "";
	source->creator = poppler_document_get_creator(document) ? poppler_document_get_creator(document) : "";
	source->keywords = poppler_document_get_keywords(document) ? poppler_document_get_keywords(document) : "";
	source->producer = poppler_document_get_producer(document) ? poppler_document_get_producer(document) : "";
	source->title = poppler_document_get_title(document) ? poppler_document_get_title(document) : "";
	source->subject = poppler_document_get_subject(document) ? poppler_document_get_subject(document) : "";
	poppler_document_get_pdf_version(document, &source->pdfVersionMajor, &source->pdfVersionMinor);

	return true;
}

std::vector<Source*> SourceManager::getSelectedSources() const {
	std::vector<Source*> selectedSources;
	for(const Gtk::TreeModel::Path& path : ui.treeviewSources->get_selection()->get_selected_rows()) {
		Source* source = m_fileTreeModel->fileData<Source*>(m_fileTreeModel->get_iter(path));
		if(source) {
			selectedSources.push_back(source);
		}
	}
	return selectedSources;
}

void SourceManager::addFolder() {
	std::string path = FileDialogs::open_folder_dialog(_("Select folder..."), Utils::get_documents_dir(), "sourcedir");
	if(path.empty()) {
		return;
	}
	std::set<std::string> filters;
	for(const Gdk::PixbufFormat& format : Gdk::Pixbuf::get_formats()) {
		for(const Glib::ustring& extension : format.get_extensions()) {
			filters.insert(extension);
		}
	}
	filters.insert("pdf");
	filters.insert("djvu");

	std::vector<Glib::RefPtr<Gio::File>> filenames;
	Utils::list_dir(path, filters, filenames);
	if(!filenames.empty()) {
		addSources(filenames);
	}
}

void SourceManager::openSources() {
	std::vector<Source*> curSrc = getSelectedSources();
	std::string initialFolder = !curSrc.empty() ? Glib::path_get_dirname(curSrc.front()->file->get_path()) : "";
	FileDialogs::FileFilter filter = FileDialogs::FileFilter::pixbuf_formats();
	filter.name = _("Images and PDFs");
	filter.mime_types.push_back("application/pdf");
	filter.patterns.push_back("*.pdf");
	filter.mime_types.push_back("image/vnd.djvu");
	filter.patterns.push_back("*.djvu");
	addSources(FileDialogs::open_dialog(_("Select Files"), initialFolder, "sourcedir", filter, true));
}

void SourceManager::pasteClipboard() {
	Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_clipboard->wait_for_image();
	if(!pixbuf) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Clipboard Error"),  _("Failed to read the clipboard."));
		return;
	}
	++m_pasteCount;
	std::string displayname = Glib::ustring::compose(_("Pasted %1"), m_pasteCount);
	savePixbuf(pixbuf, displayname);
}

void SourceManager::takeScreenshot() {
	Glib::RefPtr<Gdk::Window> root = Gdk::Window::get_default_root_window();
	int x, y, w, h;
	root->get_origin(x, y);
	w = root->get_width();
	h = root->get_height();
	Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create(root, x, y, w, h);
	if(!pixbuf) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Screenshot Error"),  _("Failed to take screenshot."));
		return;
	}
	++m_screenshotCount;
	std::string displayname = Glib::ustring::compose(_("Screenshot %1"), m_screenshotCount);
	savePixbuf(pixbuf, displayname);
}

void SourceManager::savePixbuf(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf, const std::string& displayname) {
	MAIN->pushState(MainWindow::State::Busy, _("Saving image..."));
	std::string filename;
	try {
		int fd = Glib::file_open_tmp(filename, PACKAGE_NAME);
		close(fd);
		pixbuf->save(filename, "png");
	} catch(Glib::FileError&) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Cannot Write File"),  Glib::ustring::compose(_("Could not write to %1."), filename));
		MAIN->popState();
		return;
	}
	MAIN->popState();
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(filename);
	Source* source = new Source(file, displayname, file->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), true);
	Gtk::TreeIter it = m_fileTreeModel->insertFile(filename, source, displayname);
	CONNECT(source->monitor, changed, sigc::mem_fun(*this, &SourceManager::fileChanged));
	std::string dir = Glib::path_get_dirname(filename);
	if(m_watchedDirectories.find(dir) == m_watchedDirectories.end()) {
		Glib::RefPtr<Gio::FileMonitor> dirmonitor = Gio::File::create_for_path(dir)->monitor_directory(Gio::FILE_MONITOR_SEND_MOVED);
		CONNECT(dirmonitor, changed, sigc::mem_fun(*this, &SourceManager::dirChanged));
		m_watchedDirectories.insert(std::make_pair(dir, std::make_pair(1, dirmonitor)));
	} else {
		m_watchedDirectories[dir].first += 1;
	}
	m_connectionSelectionChanged.block(true);
	ui.treeviewSources->get_selection()->unselect_all();
	m_connectionSelectionChanged.block(false);
	Gtk::TreePath path = m_fileTreeModel->get_path(it);
	ui.treeviewSources->expand_to_path(path);
	ui.treeviewSources->get_selection()->select(it);
	ui.treeviewSources->scroll_to_row(path);
}

void SourceManager::removeSource(bool deleteFile) {
	std::vector<Glib::ustring> paths;
	std::vector<Gtk::TreePath> treePaths;
	for(const Gtk::TreeModel::Path& path : ui.treeviewSources->get_selection()->get_selected_rows()) {
		Source* source = m_fileTreeModel->fileData<Source*>(m_fileTreeModel->get_iter(path));
		if(source) {
			paths.push_back(source->file->get_path());
			treePaths.push_back(path);
		}
	}
	if(paths.empty()) {
		return;
	}
	if(deleteFile && Utils::Button::Yes != Utils::messageBox(Gtk::MESSAGE_QUESTION, _("Delete File?"), _("Delete the following files?"), Utils::string_join(paths, "\n"), Utils::Button::Yes | Utils::Button::No)) {
		return;
	}

	// Avoid multiple sourceChanged emissions when removing items
	m_connectionSelectionChanged.block(true);
	// Selection returns ordered tree paths - remove them in reverse order so that the previous ones stay valid
	for(Gtk::TreePath treePath : Utils::reverse(treePaths)) {
		Gtk::TreeIter index = m_fileTreeModel->get_iter(treePath);
		Source* source = m_fileTreeModel->fileData<Source*>(index);
		if(source && deleteFile) {
			source->file->remove();
			std::string dir = Glib::path_get_dirname(source->file->get_path());
			auto it = m_watchedDirectories.find(dir);
			if(it != m_watchedDirectories.end()) {
				if(it->second.first == 1) {
					m_watchedDirectories.erase(it);
				} else {
					it->second.first -= 1;
				}
			}
		}
		m_fileTreeModel->removeIndex(index);
	}
	m_connectionSelectionChanged.block(false);
	selectionChanged();
}

void SourceManager::clearSources() {
	m_fileTreeModel->clear();
	m_watchedDirectories.clear();
	selectionChanged();
}

void SourceManager::indexClicked(const Gtk::TreePath& path, Gtk::TreeViewColumn* col) {
	Gtk::TreeIter index = m_fileTreeModel->get_iter(path);
	Source* source = m_fileTreeModel->fileData<Source*>(index);
	if(col == ui.treeviewSources->get_column(1) && source && m_fileTreeModel->isFileEditable(index)) {
		std::string base = Utils::split_filename(source->file->get_path()).first;
		bool hasTxt = Glib::file_test(base + ".txt", Glib::FILE_TEST_EXISTS);
		bool hasHtml = Glib::file_test(base + ".html", Glib::FILE_TEST_EXISTS);
		if(hasTxt && hasHtml) {
			Utils::Button::Type response = Utils::messageBox(Gtk::MESSAGE_QUESTION, _("Open output"), _("Both a text and a hOCR output were found. Which one do you want to open?"), "", Utils::Button::Text | Utils::Button::HOCR | Utils::Button::Cancel);
			if(response == Utils::Button::Text) {
				MAIN->openOutput(base + ".txt");
			} else if(response == Utils::Button::HOCR) {
				MAIN->openOutput(base + ".html");
			}
		} else if(hasTxt) {
			MAIN->openOutput(base + ".txt");
		} else if(hasHtml) {
			MAIN->openOutput(base + ".html");
		}
	}
}

void SourceManager::selectRecursive(const Gtk::TreeIter& iter, std::vector<Gtk::TreeIter>& selection) const {
	for(const Gtk::TreeIter& child : iter->children()) {
		selection.push_back(child);
		selectRecursive(child, selection);
	}
}

void SourceManager::selectionChanged() {
	if(m_inSelectionChanged) {
		return;
	}
	m_inSelectionChanged = true;

	// Merge selection of all children of selected items
	std::vector<Gtk::TreeIter> selection;
	for(const Gtk::TreePath& path : ui.treeviewSources->get_selection()->get_selected_rows()) {
		selectRecursive(m_fileTreeModel->get_iter(path), selection);
	}
	for(const Gtk::TreeIter& iter : selection) {
		ui.treeviewSources->expand_to_path(m_fileTreeModel->get_path(iter));
		ui.treeviewSources->get_selection()->select(iter);
	}

	bool enabled = !ui.treeviewSources->get_selection()->get_selected_rows().empty();
	ui.buttonSourcesRemove->set_sensitive(enabled);
	ui.buttonSourcesDelete->set_sensitive(enabled);
	m_signal_sourceChanged.emit();
	m_inSelectionChanged = false;
}

void SourceManager::fileChanged(const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& otherFile, Gio::FileMonitorEvent event) {
	Gtk::TreeIter it = m_fileTreeModel->findFile(file->get_path());
	if(!it) {
		return;
	}
	if(event == Gio::FILE_MONITOR_EVENT_MOVED) {
		bool wasSelected = ui.treeviewSources->get_selection()->is_selected(it);
		if(wasSelected) {
			ui.treeviewSources->get_selection()->unselect(it);
		}
		m_fileTreeModel->removeIndex(it);
		std::string dir = Glib::path_get_dirname(file->get_path());
		auto watchIt = m_watchedDirectories.find(dir);
		if(watchIt != m_watchedDirectories.end()) {
			if(watchIt->second.first == 1) {
				m_watchedDirectories.erase(watchIt);
			} else {
				watchIt->second.first -= 1;
			}
		}
		Source* newSource = new Source(otherFile, otherFile->get_basename(), otherFile->monitor_file(Gio::FILE_MONITOR_SEND_MOVED));
		CONNECT(newSource->monitor, changed, sigc::mem_fun(*this, &SourceManager::fileChanged));
		it = m_fileTreeModel->insertFile(otherFile->get_path(), newSource);
		if(wasSelected) {
			ui.treeviewSources->get_selection()->select(it);
		}
		dir = Glib::path_get_dirname(otherFile->get_path());
		if(m_watchedDirectories.find(dir) == m_watchedDirectories.end()) {
			Glib::RefPtr<Gio::FileMonitor> dirmonitor = Gio::File::create_for_path(dir)->monitor_directory(Gio::FILE_MONITOR_SEND_MOVED);
			CONNECT(dirmonitor, changed, sigc::mem_fun(*this, &SourceManager::dirChanged));
			m_watchedDirectories.insert(std::make_pair(dir, std::make_pair(1, dirmonitor)));
		} else {
			m_watchedDirectories[dir].first += 1;
		}
		std::string base = Utils::split_filename(otherFile->get_path()).first;
		m_fileTreeModel->setFileEditable(it, Glib::file_test(base + ".txt", Glib::FILE_TEST_EXISTS) || Glib::file_test(base + ".html", Glib::FILE_TEST_EXISTS));
	} else if(event == Gio::FILE_MONITOR_EVENT_DELETED) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Missing File"), Glib::ustring::compose(_("The following file has been deleted or moved:\n%1"), file->get_path()));
		m_fileTreeModel->removeIndex(it);

		std::string dir = Glib::path_get_dirname(file->get_path());
		auto watchIt = m_watchedDirectories.find(dir);
		if(watchIt != m_watchedDirectories.end()) {
			if(watchIt->second.first == 1) {
				m_watchedDirectories.erase(watchIt);
			} else {
				watchIt->second.first -= 1;
			}
		}
	}
}

void SourceManager::dirChanged(const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& /*otherFile*/, Gio::FileMonitorEvent event) {
	if(event == Gio::FILE_MONITOR_EVENT_CREATED || event == Gio::FILE_MONITOR_EVENT_DELETED) {
		std::string dir = Glib::path_get_dirname(file->get_path());
		// Update editable status of files in directory
		Gtk::TreeIter index = m_fileTreeModel->findFile(dir, false);
		if(index) {
			for(const Gtk::TreeIter& child : index->children()) {
				Source* source = m_fileTreeModel->fileData<Source*>(child);
				if(source) {
					std::string base = Utils::split_filename(source->file->get_path()).first;
					m_fileTreeModel->setFileEditable(child, Glib::file_test(base + ".txt", Glib::FILE_TEST_EXISTS) || Glib::file_test(base + ".html", Glib::FILE_TEST_EXISTS));
				}
			}
		}
	}
}
