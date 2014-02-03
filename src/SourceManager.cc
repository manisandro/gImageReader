/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.cc
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#include "SourceManager.hh"
#include "MainWindow.hh"
#include "Utils.hh"

#include <pangomm.h>

SourceManager::SourceManager()
{
	m_notebook = Builder("notebook:sources");
	m_listView = Builder("treeview:input.images");
	m_addButton = Builder("tbbutton:sources.images.add");
	m_removeButton = Builder("tbbutton:sources.images.remove");
	m_deleteButton = Builder("tbbutton:sources.images.delete");
	m_clearButton = Builder("tbbutton:sources.images.clear");
	m_pasteItem = Builder("menuitem:sources.images.paste");

	m_listView->set_model(Gtk::ListStore::create(m_listViewCols));
	m_listView->append_column(_("Images"), m_listViewCols.filename);
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
	Builder("menuitem:sources.images.recent").as<Gtk::MenuItem>()->set_submenu(*recentChooser);

	m_addButton->set_menu(*Builder("menu:sources.images.add").as<Gtk::Menu>());

	m_clipboard = Gtk::Clipboard::get_for_display(Gdk::Display::get_default());

	Gtk::ToggleToolButton* toggleSourcesBtn = Builder("tbbutton:main.sources");
	CONNECTS(toggleSourcesBtn, toggled, [this](Gtk::ToggleToolButton* b){ m_notebook->set_visible(b->get_active()); });
	CONNECT(m_addButton, clicked, [this]{ addSourcesBrowse(); });
	CONNECT(m_addButton, show_menu, [this]{ m_pasteItem->set_sensitive(m_clipboard->wait_is_image_available()); });
	CONNECT(Builder("menuitem:sources.images.screenshot").as<Gtk::MenuItem>(), activate, [this]{ takeScreenshot(); });
	CONNECT(m_pasteItem, activate, [this]{ pasteClipboard(); });
	CONNECT(m_removeButton, clicked, [this]{ removeItem(false); });
	CONNECT(m_deleteButton, clicked, [this]{ removeItem(true); });
	CONNECT(m_clearButton, clicked, [this]{ clearItems(); });
	CONNECT(m_listView->get_selection(), changed, [this]{ selectionChanged(); });
	CONNECT(recentChooser, item_activated, [this, recentChooser]{ addSources({Gio::File::create_for_uri(recentChooser->get_current_uri())}); });

	toggleSourcesBtn->set_active(true);
}

SourceManager::~SourceManager()
{
	// Remove temporary files
	for(const Gtk::TreeModel::Row& row : m_listView->get_model()->children()){
		Source source = row.get_value(m_listViewCols.source);
		if(source.isTemp){
			source.file->remove();
		}
	}
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
			if(row.get_value(m_listViewCols.source).file->get_uri() == file->get_uri()){
				contains = true;
				break;
			}
		}
		if(contains){
			continue;
		}
		it = store->append();
		it->set_value(m_listViewCols.filename, file->get_basename());
		Source source = {file, file->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), false};
		it->set_value(m_listViewCols.source, source);
		CONNECT(source.monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::sourceChanged), it));
	}
	if(it){
		m_listView->get_selection()->select(it);
	}
	if(!failed.empty()){
		Utils::error_dialog(_("Unable to open files"), Glib::ustring::compose(_("The following files could not be opened:%1"), failed));
	}
}

std::string SourceManager::getSelectedSource() const
{
	std::string path;
	Gtk::TreeIter it = m_listView->get_selection()->get_selected();
	if(it){
		path = it->get_value(m_listViewCols.source).file->get_path();
	}
	return path;
}

void SourceManager::addSourcesBrowse()
{
	Gtk::FileChooserDialog dialog(*MAIN->getWindow(), _("Select sources..."));
	dialog.add_button(_("Cancel"), Gtk::RESPONSE_CANCEL);
	dialog.add_button(_("OK"), Gtk::RESPONSE_OK);
	dialog.set_select_multiple(true);
	dialog.set_local_only(false);
	Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
	filter->set_name(_("Images and PDFs"));
	filter->add_pixbuf_formats();
	filter->add_mime_type("application/pdf");
	dialog.add_filter(filter);

	std::string&& current = getSelectedSource();
	if(!current.empty()){
		dialog.set_current_folder(Glib::path_get_dirname(current));
	}else{
		dialog.set_current_folder(Glib::get_home_dir());
	}

	int response = dialog.run();
	dialog.hide();
	if(response == Gtk::RESPONSE_OK){
		addSources(dialog.get_files());
	}
}

void SourceManager::pasteClipboard()
{
	Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_clipboard->wait_for_image();
	if(!pixbuf){
		return;
	}
	++m_pasteCount;
	std::string displayname = Glib::ustring::compose(_("Pasted %1"), m_pasteCount);
	MAIN->pushState(MainWindow::State::Busy, _("Pasting clipboard..."));
	m_thread = Glib::Threads::Thread::create([this,pixbuf,displayname]{ savePixbuf(pixbuf, displayname); });
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
		return;
	}
	++m_screenshotCount;
	std::string displayname = Glib::ustring::compose(_("Screenshot %1"), m_screenshotCount);
	MAIN->pushState(MainWindow::State::Busy, _("Taking screenshot..."));
	m_thread = Glib::Threads::Thread::create([this,pixbuf,displayname]{ savePixbuf(pixbuf, displayname); });
}

void SourceManager::savePixbuf(const Glib::RefPtr<Gdk::Pixbuf> &pixbuf, const std::string &displayname)
{
	std::string path;
	int fd = Glib::file_open_tmp(path, PACKAGE_NAME);
	close(fd);
	pixbuf->save(path, "png");
	Glib::signal_idle().connect_once([this,path,displayname]{ savePixbufDone(path, displayname); });
}

void SourceManager::savePixbufDone(const std::string &path, const std::string &displayname)
{
	m_thread->join();
	MAIN->popState();
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(path);
	Source source = {file, file->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), true};
	Gtk::TreeIter it = store->append();
	it->set_value(m_listViewCols.filename, displayname);
	it->set_value(m_listViewCols.source, source);
	CONNECT(source.monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::sourceChanged), it));
	m_listView->get_selection()->select(it);
}

void SourceManager::removeItem(bool deleteFile)
{
	Gtk::TreeIter it = m_listView->get_selection()->get_selected();
	if(!it){
		return;
	}
	if(deleteFile){
		std::string path = it->get_value(m_listViewCols.source).file->get_path();
		if(1 != Utils::question_dialog(_("Delete the file?"), Glib::ustring::compose(_("The following file will be deleted:\n%1"), path))){
			return;
		}
		Gio::File::create_for_path(path)->remove();
	}
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	it = store->erase(it);
	if(!it){
		it = store->children().begin();
	}
	if(it){
		m_listView->get_selection()->select(it);
	}
	m_signal_sourceChanged.emit(getSelectedSource());
}

void SourceManager::clearItems()
{
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	store->clear();
	m_signal_sourceChanged.emit(getSelectedSource());
}

void SourceManager::selectionChanged()
{
	Gtk::TreeIter it = m_listView->get_selection()->get_selected();
	bool valid = it;
	m_removeButton->set_sensitive(valid);
	m_deleteButton->set_sensitive(valid);
	m_clearButton->set_sensitive(valid);
	m_signal_sourceChanged.emit(getSelectedSource());
}

void SourceManager::sourceChanged(const Glib::RefPtr<Gio::File>& file, const Glib::RefPtr<Gio::File>& otherFile, Gio::FileMonitorEvent event, Gtk::TreeIter it)
{
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(m_listView->get_model());
	if(event == Gio::FILE_MONITOR_EVENT_MOVED){
		Source source = {otherFile, otherFile->monitor_file(Gio::FILE_MONITOR_SEND_MOVED), false};
		it->set_value(m_listViewCols.filename, otherFile->get_basename());
		it->set_value(m_listViewCols.source, source);
		CONNECT(source.monitor, changed, sigc::bind(sigc::mem_fun(*this, &SourceManager::sourceChanged), it));
		if(it == m_listView->get_selection()->get_selected()){
			m_signal_sourceChanged.emit(getSelectedSource());
		}
	}else if(event == Gio::FILE_MONITOR_EVENT_DELETED){
		Utils::error_dialog(_("Deleted file"), Glib::ustring::compose(_("The following file has been deleted:\n%1"), file->get_path()));
		it = store->erase(it);
		if(m_listView->get_selection()->get_selected()){
			return;
		}
		if(!it){
			it = store->children().begin();
		}
		if(it){
			m_listView->get_selection()->select(it);
		}
		m_signal_sourceChanged.emit(getSelectedSource());
	}
}
