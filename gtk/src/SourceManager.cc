/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.cc
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <pangomm.h>
#include <poppler-document.h>


SourceManager::SourceManager(const Ui::MainWindow& _ui)
	: ui(_ui) {
	ui.treeviewSources->set_model(Gtk::ListStore::create(m_listViewCols));
	ui.treeviewSources->append_column("", m_listViewCols.filename);
	Gtk::TreeViewColumn* col = ui.treeviewSources->get_column(0);
	col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	Gtk::CellRendererText* cell = static_cast<Gtk::CellRendererText*>(col->get_cells().front());
	cell->property_ellipsize() = Pango::ELLIPSIZE_END;
	ui.treeviewSources->set_fixed_height_mode();

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
	CONNECT(ui.menuitemSourcesPaste, activate, [this] { pasteClipboard(); });
	CONNECT(ui.menuitemSourcesScreenshot, activate, [this] { takeScreenshot(); });
	CONNECT(ui.buttonSourcesRemove, clicked, [this] { removeSource(false); });
	CONNECT(ui.buttonSourcesDelete, clicked, [this] { removeSource(true); });
	CONNECT(ui.buttonSourcesClear, clicked, [this] { clearSources(); });
	m_connectionSelectionChanged = CONNECT(ui.treeviewSources->get_selection(), changed, [this] { selectionChanged(); });
	CONNECT(recentChooser, item_activated, [this, recentChooser] { addSources({Gio::File::create_for_uri(recentChooser->get_current_uri())}); });

	// Handle drops on the scrolled window
	ui.scrollwinSources->drag_dest_set({Gtk::TargetEntry("text/uri-list")}, Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
	CONNECT(ui.scrollwinSources, drag_data_received, sigc::ptr_fun(Utils::handle_drag_drop));
}

SourceManager::~SourceManager() {
	clearSources();
}

int SourceManager::addSources(const std::vector<Glib::RefPtr<Gio::File>>& files, bool suppressTextWarning) {
	Glib::ustring failed;
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewSources->get_model());
	Gtk::TreeIter it = store->children().end();
	int added = 0;
	std::vector<Glib::ustring> filesWithText;
	for(Glib::RefPtr<Gio::File> file : files) {
		if(!file->query_exists()) {
			failed += "\n " + file->get_path();
			continue;
		}
		bool contains = false;
		for(const Gtk::TreeModel::Row& row : store->children()) {
			if(row.get_value(m_listViewCols.source)->file->get_uri() == file->get_uri()) {
				contains = true;
				it = row;
				break;
			}
		}
		if(contains) {
			++added;
			continue;
		}
		Source* source = new Source(file, file->get_basename(), file->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), false);
		std::string filename = file->get_path();
#ifdef G_OS_WIN32
		if(Glib::ustring(filename.substr(filename.length() - 4)).lowercase() == ".pdf" && !checkPdfSource(source, filesWithText)) {
#else
		if(Utils::get_content_type(filename) == "application/pdf" && !checkPdfSource(source, filesWithText)) {
#endif
			delete source;
			continue;
		}
		it = store->append();
		it->set_value(m_listViewCols.filename, file->get_basename());
		it->set_value(m_listViewCols.source, source);
		it->set_value(m_listViewCols.path, file->get_path());
		CONNECT(source->monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::fileChanged), it));

		Gtk::RecentManager::get_default()->add_item(file->get_uri());
		++added;
	}
	if(!suppressTextWarning && !filesWithText.empty()) {
		Utils::message_dialog(Gtk::MESSAGE_INFO, _("PDFs with text"), Glib::ustring::compose(_("These PDF files already contain text:\n%1"), Utils::string_join(filesWithText, "\n")));
	}
	m_connectionSelectionChanged.block(true);
	ui.treeviewSources->get_selection()->unselect_all();
	m_connectionSelectionChanged.block(false);
	if(it) {
		ui.treeviewSources->get_selection()->select(it);
	}
	if(!failed.empty()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Unable to open files"), Glib::ustring::compose(_("The following files could not be opened:%1"), failed));
	}
	return added;
}

bool SourceManager::checkPdfSource(Source* source, std::vector<Glib::ustring>& filesWithText) const {
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
			}
			g_error_free(err);
			err = nullptr;
		}
	}

	// Check whether the PDF already contains text
	for (int i = 0, n = poppler_document_get_n_pages(document); i < n; ++i) {
		PopplerPage* poppage = poppler_document_get_page(document, i);
		bool hasText = poppler_page_get_text(poppage) != nullptr;
		g_object_unref(poppage);
		if(hasText) {
			filesWithText.push_back(filename);
			break;
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
		selectedSources.push_back(ui.treeviewSources->get_model()->get_iter(path)->get_value(m_listViewCols.source));
	}
	return selectedSources;
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
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Clipboard Error"),  _("Failed to read the clipboard."));
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
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Screenshot Error"),  _("Failed to take screenshot."));
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
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Cannot Write File"),  Glib::ustring::compose(_("Could not write to %1."), filename));
		MAIN->popState();
		return;
	}
	MAIN->popState();
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewSources->get_model());
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(filename);
	Source* source = new Source(file, displayname, file->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), true);
	Gtk::TreeIter it = store->append();
	it->set_value(m_listViewCols.filename, displayname);
	it->set_value(m_listViewCols.path, filename);
	it->set_value(m_listViewCols.source, source);
	CONNECT(source->monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::fileChanged), it));
	m_connectionSelectionChanged.block(true);
	ui.treeviewSources->get_selection()->unselect_all();
	m_connectionSelectionChanged.block(false);
	ui.treeviewSources->get_selection()->select(it);
}

void SourceManager::removeSource(bool deleteFile) {
	std::string paths;
	for(const Gtk::TreeModel::Path& path : ui.treeviewSources->get_selection()->get_selected_rows()) {
		paths += std::string("\n") + ui.treeviewSources->get_model()->get_iter(path)->get_value(m_listViewCols.source)->file->get_path();
	}
	if(paths.empty()) {
		return;
	}
	if(deleteFile && Utils::Button::Yes != Utils::question_dialog(_("Delete File?"), Glib::ustring::compose(_("The following files will be deleted:%1"), paths), Utils::Button::Yes | Utils::Button::No)) {
		return;
	}
	// Avoid multiple sourceChanged emissions when removing items
	m_connectionSelectionChanged.block(true);
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewSources->get_model());
	Gtk::TreeIter it;
	std::vector<Gtk::TreeModel::Path> selected = ui.treeviewSources->get_selection()->get_selected_rows();
	while(!selected.empty()) {
		it = ui.treeviewSources->get_model()->get_iter(selected.back());
		selected.pop_back();
		Source* source = it->get_value(m_listViewCols.source);
		if(deleteFile || source->isTemp) {
			source->file->remove();
		}
		delete source;
		it = store->erase(it);
	}
	if(!it && store->children().size() > 0) {
		it = --store->children().end();
	}
	if(it) {
		ui.treeviewSources->get_selection()->select(it);
	}
	m_connectionSelectionChanged.block(false);
	selectionChanged();
}

void SourceManager::clearSources() {
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewSources->get_model());
	for(const Gtk::TreeModel::Row& row : store->children()) {
		Source* source = row.get_value(m_listViewCols.source);
		if(source->isTemp) {
			source->file->remove();
		}
		delete source;
	}
	m_connectionSelectionChanged.block(true);
	store->clear();
	m_connectionSelectionChanged.block(false);
	selectionChanged();
}

void SourceManager::selectionChanged() {
	bool enabled = !ui.treeviewSources->get_selection()->get_selected_rows().empty();
	ui.buttonSourcesRemove->set_sensitive(enabled);
	ui.buttonSourcesDelete->set_sensitive(enabled);
	ui.buttonSourcesClear->set_sensitive(enabled);
	m_signal_sourceChanged.emit();
}

void SourceManager::fileChanged(const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& otherFile, Gio::FileMonitorEvent event, Gtk::TreeIter it) {
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.treeviewSources->get_model());
	Source* source = it->get_value(m_listViewCols.source);
	if(event == Gio::FILE_MONITOR_EVENT_MOVED) {
		source->file = otherFile;
		source->displayname = otherFile->get_basename();
		source->monitor = otherFile->monitor_file(Gio::FILE_MONITOR_SEND_MOVED);
		it->set_value(m_listViewCols.filename, otherFile->get_basename());
		it->set_value(m_listViewCols.path, otherFile->get_path());
		CONNECT(source->monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::fileChanged), it));
		if(ui.treeviewSources->get_selection()->is_selected(it)) {
			m_signal_sourceChanged.emit();
		}
	} else if(event == Gio::FILE_MONITOR_EVENT_DELETED) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Missing File"), Glib::ustring::compose(_("The following file has been deleted or moved:\n%1"), file->get_path()));
		delete source;
		it = store->erase(it);
		if(!it) {
			it = store->children().begin();
		}
		if(it) {
			ui.treeviewSources->get_selection()->select(it);
		}
	}
}
