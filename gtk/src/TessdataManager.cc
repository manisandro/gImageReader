/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * TessdataManager.hh
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

#include "Config.hh"
#include "MainWindow.hh"
#include "Recognizer.hh"
#include "TessdataManager.hh"
#include "Utils.hh"

#include <fstream>
#include <json-glib/json-glib.h>
#ifdef G_OS_UNIX
#include <gdk/gdkx.h>
#endif

void TessdataManager::exec()
{
	static TessdataManager instance;
	instance.run();
}

TessdataManager::TessdataManager()
{
	m_dialog = MAIN->getWidget("dialog:tessdatamanager");
	m_languageList = MAIN->getWidget("treeview:tessdatamanager");
	m_languageListStore = Gtk::ListStore::create(m_viewCols);
	m_languageList->set_model(m_languageListStore);
	m_languageList->append_column_editable("selected", m_viewCols.selected);
	m_languageList->append_column("label", m_viewCols.label);
}

void TessdataManager::run()
{
#ifdef G_OS_UNIX
	if(MAIN->getConfig()->useSystemDataLocations())
	{
		Glib::ustring service_owner;
		try{
			m_dbusProxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BUS_TYPE_SESSION, "org.freedesktop.PackageKit",
															"/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify");
			service_owner = m_dbusProxy->get_name_owner();
		}catch(...){
		}
		if(service_owner.empty()) {
			Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), _("PackageKit is required for managing system-wide tesseract language packs, but it was not found. Please use the system package management software to manage the tesseract language packs, or switch to use the user tessdata path in the configuration dialog."));
			return;
		}
	}
#endif
	MAIN->pushState(MainWindow::State::Busy, _("Fetching available languages"));
	Glib::ustring messages;
	bool success = fetchLanguageList(messages);
	MAIN->popState();
	if(!success)
	{
		Utils::message_dialog(Gtk::MESSAGE_ERROR,_("Error"), Glib::ustring::compose(_("Failed to fetch list of available languages: %1"), messages));
		return;
	}
	while(true) {
		int response = m_dialog->run();
		if(response == Gtk::RESPONSE_APPLY) {
			applyChanges();
		} else if(response == 1) {
			refresh();
		} else {
			break;
		}
	}
	m_dialog->hide();
}

bool TessdataManager::fetchLanguageList(Glib::ustring& messages)
{
	m_languageListStore->clear();

	Glib::RefPtr<Glib::ByteArray> data = Utils::download("https://api.github.com/repos/tesseract-ocr/tessdata/contents", messages);

	if(!data) {
		messages = Glib::ustring::compose(_("Failed to fetch list of available languages: %1"), messages);
		return false;
	}

	JsonParser* parser = json_parser_new();
	GError* parserError = nullptr;
	json_parser_load_from_data(parser, reinterpret_cast<gchar*>(data->get_data()), data->size(), &parserError);

	if(parserError) {
		messages = Glib::ustring::compose(_("Parsing error: %1"), parserError->message);
		g_object_unref(parser);
		return false;
	}
	JsonNode* root = json_parser_get_root(parser);
	JsonArray* array = json_node_get_array(root);
	GList* elementArray = json_array_get_elements(array);
	std::vector<std::pair<Glib::ustring,Glib::ustring>> extraFiles;
	while(elementArray != 0) {
		JsonNode* value = static_cast<JsonNode*>(elementArray->data);
		JsonObject* treeObj = json_node_get_object(value);
		Glib::ustring name = json_object_get_string_member(treeObj, "name");
		Glib::ustring url = json_object_get_string_member(treeObj, "download_url");
		if(name.length() > 12 && name.compare(name.length() - 12, name.npos, "traineddata")) {
			Glib::ustring key = name.substr(0, name.find("."));
			auto it = m_languageFiles.find(key);
			if(it == m_languageFiles.end()) {
				it = m_languageFiles.insert(std::make_pair(key, std::vector<LangFile>())).first;
			}
			it->second.push_back({name, url});
		} else {
			// Delay decision to determine whether file is a supplementary language file
			extraFiles.push_back(std::make_pair(name, url));
		}
		elementArray = elementArray->next;
	}
	g_list_free(elementArray);
	for(const std::pair<Glib::ustring,Glib::ustring>& extraFile : extraFiles) {
		Glib::ustring lang = extraFile.first.substr(0, extraFile.first.find("."));
		auto it = m_languageFiles.find(lang);
		if(it != m_languageFiles.end()) {
			it->second.push_back({extraFile.first,extraFile.second});
		}
	}

	std::vector<Glib::ustring> availableLanguages = MAIN->getRecognizer()->getAvailableLanguages();
	std::vector<Glib::ustring> languages;
	for(auto it : m_languageFiles) {
		languages.push_back(it.first);
	}
	std::sort(languages.begin(), languages.end());

	for(const Glib::ustring& prefix : languages) {
		Config::Lang lang;
		lang.prefix = prefix;
		Glib::ustring label;
		if(MAIN->getConfig()->searchLangSpec(lang)) {
			label = Glib::ustring::compose("%1 (%2)", lang.name, lang.prefix);
		} else {
			label = lang.prefix;
		}
		Gtk::TreeIter it = m_languageListStore->append();
		bool installed = std::find(availableLanguages.begin(), availableLanguages.end(), prefix) != availableLanguages.end();
		(*it)[m_viewCols.selected] = installed;
		(*it)[m_viewCols.label] = label;
		(*it)[m_viewCols.prefix] = prefix;
	}
	return true;
}

void TessdataManager::applyChanges()
{
	MAIN->pushState(MainWindow::State::Busy, _("Applying changes..."));
	m_dialog->set_sensitive(false);
	m_dialog->get_window()->set_cursor(Gdk::Cursor::create(Gdk::WATCH));
	Glib::ustring errorMsg;
	std::vector<Glib::ustring> availableLanguages = MAIN->getRecognizer()->getAvailableLanguages();
	std::string tessDataPath = MAIN->getConfig()->tessdataLocation();
#ifdef G_OS_WIN32
	bool isWindows = true;
#else
	bool isWindows = false;
#endif
	if(!isWindows && MAIN->getConfig()->useSystemDataLocations()) {
		// Place this in a ifdef since DBus stuff cannot be compiled on Windows
#ifdef G_OS_UNIX
		std::vector<Glib::ustring> installFiles;
		std::vector<Glib::ustring> removeFiles;
		for(const Gtk::TreeModel::Row& row : m_languageListStore->children()) {
			Glib::ustring prefix = row.get_value(m_viewCols.prefix);
			bool selected = row.get_value(m_viewCols.selected);
			bool installed = std::find(availableLanguages.begin(), availableLanguages.end(), prefix) != availableLanguages.end();
			if(selected && !installed) {
				installFiles.push_back(Glib::build_filename(tessDataPath, prefix + ".traineddata"));
			} else if(!selected && installed) {
				removeFiles.push_back(Glib::build_filename(tessDataPath, prefix + ".traineddata"));
			}
		}
		std::uint32_t xid = gdk_x11_window_get_xid(m_dialog->get_window()->gobj());

		if(!installFiles.empty()) {
			std::vector<Glib::VariantBase> params = { Glib::Variant<std::uint32_t>::create(xid),
													  Glib::Variant<std::vector<Glib::ustring>>::create(installFiles),
													  Glib::Variant<Glib::ustring>::create("always") };
			try {
				m_dbusProxy->call_sync("InstallProvideFiles", Glib::VariantContainerBase::create_tuple(params), 3600000);
			} catch(const Glib::Error& e) {
				errorMsg = e.what();
			}
		}
		if(errorMsg.empty() && !removeFiles.empty()) {
			std::vector<Glib::VariantBase> params = { Glib::Variant<std::uint32_t>::create(xid),
													  Glib::Variant<std::vector<Glib::ustring>>::create(removeFiles),
													  Glib::Variant<Glib::ustring>::create("always") };
			try {
				m_dbusProxy->call_sync("RemovePackageByFiles", Glib::VariantContainerBase::create_tuple(params), 3600000);
			} catch(const Glib::Error& e) {
				errorMsg = e.what();
			}
		}
#endif
	} else {
		Glib::ustring errors;
		Glib::RefPtr<Gio::File> tessDataDir = Gio::File::create_for_path(tessDataPath);
		bool dirExists = false;
		try {
			dirExists = tessDataDir->make_directory_with_parents();
		} catch(...) {
			dirExists = tessDataDir->query_exists();
		}
		if(!dirExists) {
			errors.append(Glib::ustring(_("Failed to create directory for tessdata files.")) + "\n");
		} else {
			for(const Gtk::TreeModel::Row& row : m_languageListStore->children()) {
				Glib::ustring prefix = row.get_value(m_viewCols.prefix);
				bool selected = row.get_value(m_viewCols.selected);
				auto it = m_languageFiles.find(prefix);
				bool installed = std::find(availableLanguages.begin(), availableLanguages.end(), prefix) != availableLanguages.end();
				if(selected && !installed) {
					for(const LangFile& langFile : it->second) {
						MAIN->pushState(MainWindow::State::Busy, Glib::ustring::compose(_("Downloading %1..."), langFile.name));
						Glib::ustring messages;
						Glib::RefPtr<Glib::ByteArray> data = Utils::download(langFile.url, messages);
						std::ofstream file(Glib::build_filename(tessDataPath, langFile.name));
						if(!data || !file.is_open()) {
							errors.append(langFile.name + "\n");
						} else {
							file.write(reinterpret_cast<char*>(data->get_data()), data->size());
						}
						MAIN->popState();
					}
				} else if(!selected && installed) {
					for(const std::string& file : Glib::Dir(tessDataPath)) {
						if(file.size() > 4 && file.compare(0, 4, prefix + ".") == 0) {
							Gio::File::create_for_path(Glib::build_filename(tessDataPath, file))->remove();
						}
					}
				}
			}
			if(!errors.empty()) {
				errorMsg = Glib::ustring::compose(_("The following files could not be downloaded or removed:\n%1\n\nCheck the connectivity and directory permissions."), errors);
			}
		}
	}
	m_dialog->get_window()->set_cursor(Glib::RefPtr<Gdk::Cursor>());
	m_dialog->set_sensitive(true);
	MAIN->popState();
	refresh();
	if(!errorMsg.empty()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), errorMsg, m_dialog);
	}
}

void TessdataManager::refresh()
{
	MAIN->getRecognizer()->updateLanguagesMenu();
	std::vector<Glib::ustring> availableLanguages = MAIN->getRecognizer()->getAvailableLanguages();
	for(const Gtk::TreeModel::Row& row : m_languageListStore->children()) {
		Glib::ustring prefix = row.get_value(m_viewCols.prefix);
		bool installed = std::find(availableLanguages.begin(), availableLanguages.end(), prefix) != availableLanguages.end();
		row.set_value(m_viewCols.selected, installed);
	}
}
