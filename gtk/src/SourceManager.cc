/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.cc
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

#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <pangomm.h>

SourceManager::SourceManager()
{
	m_notebook = MAIN->getWidget("notebook:sources");
	m_listView = MAIN->getWidget("treeview:input.images");
	m_addButton = MAIN->getWidget("button:sources.images.add");
	m_addButtonMenu = MAIN->getWidget("menubutton:sources.images.add");
	m_removeButton = MAIN->getWidget("button:sources.images.remove");
	m_deleteButton = MAIN->getWidget("button:sources.images.delete");
	m_clearButton = MAIN->getWidget("button:sources.images.clear");
	m_pasteItem = MAIN->getWidget("menuitem:sources.images.paste");

	m_listView->set_model(Gtk::ListStore::create(m_listViewCols));
	m_listView->append_column("", m_listViewCols.filename);
	Gtk::TreeViewColumn* col = m_listView->get_column(0);
	col->set_sizing(Gtk::TREE_VIEW_COLUMN_FIXED);
	Gtk::CellRendererText* cell = static_cast<Gtk::CellRendererText*>(col->get_cells().front());
	cell->property_ellipsize() = Pango::ELLIPSIZE_END;
	m_listView->set_fixed_height_mode();

	Glib::RefPtr<Gtk::RecentFilter> recentFilter = Gtk::RecentFilter::create();
	recentFilter->add_pixbuf_formats();
	recentFilter->add_mime_type("application/pdf");
	recentFilter->add_pattern("*.pdf");
	Gtk::RecentChooserMenu* recentChooser = Gtk::manage(new Gtk::RecentChooserMenu());
	recentChooser->set_filter(recentFilter);
	recentChooser->set_local_only(false);
	recentChooser->set_show_not_found(false);
	recentChooser->set_show_tips(true);
	recentChooser->set_sort_type(Gtk::RECENT_SORT_MRU);
	MAIN->getWidget("menuitem:sources.images.recent").as<Gtk::MenuItem>()->set_submenu(*recentChooser);

	m_clipboard = Gtk::Clipboard::get_for_display(Gdk::Display::get_default());

	Gtk::ToggleButton* toggleSourcesBtn = MAIN->getWidget("button:main.sources");
	CONNECTS(toggleSourcesBtn, toggled, [this](Gtk::ToggleButton* b){ m_notebook->set_visible(b->get_active()); });
	CONNECT(m_addButton, clicked, [this]{ openSources(); });
	CONNECT(m_addButtonMenu, clicked, [this]{ m_pasteItem->set_sensitive(m_clipboard->wait_is_image_available()); });
	CONNECT(m_pasteItem, activate, [this]{ pasteClipboard(); });
	CONNECT(MAIN->getWidget("menuitem:sources.images.screenshot").as<Gtk::MenuItem>(), activate, [this]{ takeScreenshot(); });
	CONNECT(m_removeButton, clicked, [this]{ removeSource(false); });
	CONNECT(m_deleteButton, clicked, [this]{ removeSource(true); });
	CONNECT(m_clearButton, clicked, [this]{ clearSources(); });
	m_connectionSelectionChanged = CONNECT(m_listView->get_selection(), changed, [this]{ selectionChanged(); });
	CONNECT(recentChooser, item_activated, [this, recentChooser]{ addSources({Gio::File::create_for_uri(recentChooser->get_current_uri())}); });

	// Handle drops on the scrolled window
	Gtk::ScrolledWindow* scollWin = MAIN->getWidget("scrollwin:sources.images");
	scollWin->drag_dest_set({Gtk::TargetEntry("text/uri-list")}, Gtk::DEST_DEFAULT_MOTION | Gtk::DEST_DEFAULT_DROP, Gdk::ACTION_COPY | Gdk::ACTION_MOVE);
	CONNECT(scollWin, drag_data_received, sigc::ptr_fun(Utils::handle_drag_drop));
}

SourceManager::~SourceManager()
{
	clearSources();
}

void SourceManager::addSources(const std::vector<Glib::RefPtr<Gio::File>>& files)
{
	Glib::ustring failed;
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	Gtk::TreeIter it = store->children().end();
	for(Glib::RefPtr<Gio::File> file : files){
		if(!file->query_exists()){
			failed += "\n\t" + file->get_path();
			continue;
		}
		bool contains = false;
		for(const Gtk::TreeModel::Row& row : store->children()){
			if(row.get_value(m_listViewCols.source)->file->get_uri() == file->get_uri()){
				contains = true;
				break;
			}
		}
		if(contains){
			continue;
		}
		it = store->append();
		it->set_value(m_listViewCols.filename, file->get_basename());
		Source* source = new Source(file, file->get_basename(), file->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), false);
		it->set_value(m_listViewCols.source, source);
		it->set_value(m_listViewCols.path, file->get_path());
		CONNECT(source->monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::fileChanged), it));

		Gtk::RecentManager::get_default()->add_item(file->get_uri());
	}
	if(it){
		m_connectionSelectionChanged.block(true);
		m_listView->get_selection()->unselect_all();
		m_connectionSelectionChanged.block(false);
		m_listView->get_selection()->select(it);
	}
	if(!failed.empty()){
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Unable to open files"), Glib::ustring::compose(_("The following files could not be opened:%1"), failed));
	}
}

std::vector<Source*> SourceManager::getSelectedSources() const
{
	std::vector<Source*> selectedSources;
	for(const Gtk::TreeModel::Path& path : m_listView->get_selection()->get_selected_rows()) {
		selectedSources.push_back(m_listView->get_model()->get_iter(path)->get_value(m_listViewCols.source));
	}
	return selectedSources;
}

void SourceManager::openSources()
{
	std::vector<Source*> curSrc = getSelectedSources();
	std::string initialFolder = !curSrc.empty() ? Glib::path_get_dirname(curSrc.front()->file->get_path()) : "";
	FileDialogs::FileFilter filter = FileDialogs::FileFilter::pixbuf_formats();
	filter.name = _("Images and PDFs");
	filter.mime_types.push_back("application/pdf");
	filter.patterns.push_back("*.pdf");
	addSources(FileDialogs::open_dialog(_("Select Files"), initialFolder, filter, true));
}

void SourceManager::pasteClipboard()
{
	Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_clipboard->wait_for_image();
	if(!pixbuf){
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Clipboard Error"),  _("Failed to read the clipboard."));
		return;
	}
	++m_pasteCount;
	std::string displayname = Glib::ustring::compose(_("Pasted %1"), m_pasteCount);
	savePixbuf(pixbuf, displayname);
}

void SourceManager::takeScreenshot()
{
	Glib::RefPtr<Gdk::Window> root = Gdk::Window::get_default_root_window();
	int x, y, w, h;
	root->get_origin(x, y);
	w = root->get_width();
	h = root->get_height();
	Glib::RefPtr<Gdk::Pixbuf> pixbuf = Gdk::Pixbuf::create(root, x, y, w, h);
	if(!pixbuf){
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Screenshot Error"),  _("Failed to take screenshot."));
		return;
	}
	++m_screenshotCount;
	std::string displayname = Glib::ustring::compose(_("Screenshot %1"), m_screenshotCount);
	savePixbuf(pixbuf, displayname);
}

void SourceManager::savePixbuf(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf, const std::string &displayname)
{
	MAIN->pushState(MainWindow::State::Busy, _("Saving image..."));
	std::string filename;
	try{
		int fd = Glib::file_open_tmp(filename, PACKAGE_NAME);
		close(fd);
		pixbuf->save(filename, "png");
	}catch(Glib::FileError&){
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Cannot Write File"),  Glib::ustring::compose(_("Could not write to %1."), filename));
		MAIN->popState();
		return;
	}
	MAIN->popState();
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(filename);
	Source* source = new Source(file, displayname, file->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), true);
	Gtk::TreeIter it = store->append();
	it->set_value(m_listViewCols.filename, displayname);
	it->set_value(m_listViewCols.path, filename);
	it->set_value(m_listViewCols.source, source);
	CONNECT(source->monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::fileChanged), it));
	m_connectionSelectionChanged.block(true);
	m_listView->get_selection()->unselect_all();
	m_connectionSelectionChanged.block(false);
	m_listView->get_selection()->select(it);
}

void SourceManager::removeSource(bool deleteFile)
{
	std::string paths;
	for(const Gtk::TreeModel::Path& path : m_listView->get_selection()->get_selected_rows()) {
		paths += std::string("\n") + m_listView->get_model()->get_iter(path)->get_value(m_listViewCols.source)->file->get_path();
	}
	if(paths.empty()) {
		return;
	}
	if(deleteFile && Utils::Button::Yes != Utils::question_dialog(_("Delete File?"), Glib::ustring::compose(_("The following files will be deleted:%1"), paths), Utils::Button::Yes|Utils::Button::No)){
		return;
	}
	// Avoid multiple sourceChanged emissions when removing items
	m_connectionSelectionChanged.block(true);
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	Gtk::TreeIter it;
	for(const Gtk::TreeModel::Path& path : m_listView->get_selection()->get_selected_rows()) {
		it = m_listView->get_model()->get_iter(path);
		Source* source = it->get_value(m_listViewCols.source);
		if(deleteFile || source->isTemp){
			source->file->remove();
		}
		delete source;
		it = store->erase(it);
	}
	if(!it && store->children().size() > 0){
		it = --store->children().end();
	}
	if(it){
		m_listView->get_selection()->select(it);
	}
	m_connectionSelectionChanged.block(false);
	m_signal_sourceChanged.emit();
}

void SourceManager::clearSources()
{
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	for(const Gtk::TreeModel::Row& row : store->children()){
		Source* source = row.get_value(m_listViewCols.source);
		if(source->isTemp){
			source->file->remove();
		}
		delete source;
	}
	m_connectionSelectionChanged.block(true);
	store->clear();
	m_connectionSelectionChanged.block(false);
	m_signal_sourceChanged.emit();
}

void SourceManager::selectionChanged()
{
	bool enabled = !m_listView->get_selection()->get_selected_rows().empty();
	m_removeButton->set_sensitive(enabled);
	m_deleteButton->set_sensitive(enabled);
	m_clearButton->set_sensitive(enabled);
	m_signal_sourceChanged.emit();
}

void SourceManager::fileChanged(const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& otherFile, Gio::FileMonitorEvent event, Gtk::TreeIter it)
{
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	Source* source = it->get_value(m_listViewCols.source);
	if(event == Gio::FILE_MONITOR_EVENT_MOVED){
		source->file = otherFile;
		source->monitor = otherFile->monitor_file(Gio::FILE_MONITOR_SEND_MOVED);
		it->set_value(m_listViewCols.filename, otherFile->get_basename());
		it->set_value(m_listViewCols.path, otherFile->get_path());
		CONNECT(source->monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::fileChanged), it));
		if(it == m_listView->get_selection()->get_selected()){
			m_signal_sourceChanged.emit();
		}
	}else if(event == Gio::FILE_MONITOR_EVENT_DELETED){
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Missing File"), Glib::ustring::compose(_("The following file has been deleted or moved:\n%1"), file->get_path()));
		delete source;
		it = store->erase(it);
		if(!it){
			it = store->children().begin();
		}
		if(it){
			m_listView->get_selection()->select(it);
		}
	}
}
