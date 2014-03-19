/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputManager.cc
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

#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "Notifier.hh"
#include "OutputManager.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <fstream>
#include <cstring>

#ifdef G_OS_UNIX
#include <gdk/gdkx.h>
#endif

OutputManager::OutputManager()
{
	m_insButton = Builder("tbbutton:output.insert");
	m_insMenu = Builder("menu:output.insert");
	m_insImage = Builder("image:output.insert");
	m_replaceBox = Builder("evbox:findreplace");
	m_outputBox = Builder("box:output");
	m_textView = Builder("textview:output");
	m_searchEntry = Builder("entry:output.search");
	m_replaceEntry = Builder("entry:output.replace");
	m_filterKeepIfDot = Builder("menuitem:output.stripcrlf.keepdot");
	m_filterKeepIfQuote = Builder("menuitem:output.stripcrlf.keepquote");
	m_filterJoinHyphen = Builder("menuitem:output.stripcrlf.joinhyphen");
	m_filterJoinSpace = Builder("menuitem:output.stripcrlf.joinspace");
	m_undoButton = Builder("tbbutton:output.undo");
	m_redoButton = Builder("tbbutton:output.redo");

	m_searchEntry->set_placeholder_text(_("Search for"));
	m_replaceEntry->set_placeholder_text(_("Replace with"));

	m_textBuffer = UndoableBuffer::create();
	m_textView->set_buffer(m_textBuffer);
	m_insertMode = InsertMode::Append;
	m_insertIter = m_textBuffer->end();
	m_selectIter = m_textBuffer->end();

	m_spell.property_decode_language_codes() = true;

	Builder("tbbutton:output.stripcrlf").as<Gtk::MenuToolButton>()->set_menu(*Builder("menu:output.stripcrlf").as<Gtk::Menu>());

	CONNECT(m_insButton, toggled, [this]{ showInsertMenu(); });
	CONNECT(m_insMenu, deactivate, [this]{ m_insButton->set_active(false); });
	CONNECT(Builder("menuitem:output.insert.append").as<Gtk::MenuItem>(), activate, [this]{ setInsertMode(InsertMode::Append, PACKAGE_DATA_DIR "icons/ins_append.png"); });
	CONNECT(Builder("menuitem:output.insert.cursor").as<Gtk::MenuItem>(), activate, [this]{ setInsertMode(InsertMode::Cursor, PACKAGE_DATA_DIR "icons/ins_cursor.png"); });
	CONNECT(Builder("menuitem:output.insert.replace").as<Gtk::MenuItem>(), activate, [this]{ setInsertMode(InsertMode::Replace, PACKAGE_DATA_DIR "icons/ins_replace.png"); });
	CONNECT(Builder("tbbutton:output.stripcrlf").as<Gtk::ToolButton>(), clicked, [this]{ filterBuffer(); });
	CONNECTS(Builder("tbbutton:output.findreplace").as<Gtk::ToggleToolButton>(), toggled, [this](Gtk::ToggleToolButton* b){ toggleReplaceBox(b); });
	CONNECT(m_textBuffer, changed, [this]{ saveIters(); });
	CONNECT(m_textBuffer, mark_set, [this](const Gtk::TextIter&,const Glib::RefPtr<Gtk::TextMark>&){ if(m_textView->is_focus()) { saveIters(); }});
	CONNECT(m_textBuffer, history_changed, [this]{ historyChanged(); });
	CONNECT(m_searchEntry, changed, [this]{ Utils::clear_error_state(m_searchEntry); });
	CONNECT(m_searchEntry, activate, [this]{ findInBuffer(); });
	CONNECT(Builder("button:output.search").as<Gtk::Button>(), clicked, [this]{ findInBuffer(); });
	CONNECT(m_replaceEntry, activate, [this]{ findInBuffer(true); });
	CONNECT(Builder("button:output.replace").as<Gtk::Button>(), clicked, [this]{ findInBuffer(true); });
	CONNECT(m_undoButton, clicked, [this]{ m_textBuffer->undo(); m_textView->grab_focus(); });
	CONNECT(m_redoButton, clicked, [this]{ m_textBuffer->redo(); m_textView->grab_focus(); });
	CONNECT(Builder("tbbutton:output.save").as<Gtk::ToolButton>(), clicked, [this]{ saveBuffer(); });
	CONNECT(Builder("tbbutton:output.clear").as<Gtk::ToolButton>(), clicked, [this]{ clearBuffer(); });
}

void OutputManager::showInsertMenu()
{
	if(m_insButton->get_active()){
		auto positioner = sigc::bind(sigc::ptr_fun(Utils::popup_positioner), m_insButton, m_insMenu, false, true);
		m_insMenu->popup(positioner, 0, gtk_get_current_event_time());
	}
}

void OutputManager::setInsertMode(InsertMode mode, const std::string& iconName)
{
	m_insertMode = mode;
	m_insImage->set(Gdk::Pixbuf::create_from_file(iconName));
}

void OutputManager::filterBuffer()
{
	Gtk::TextIter start, end;
	if(!m_textBuffer->get_selection_bounds(start, end)){
		start = m_textBuffer->begin();
		end = m_textBuffer->end();
	}
	Glib::ustring txt = m_textBuffer->get_text(start, end);

	Utils::busyTask([this,&txt]{
		if(m_filterJoinHyphen->get_active()){
			txt = Glib::Regex::create("-\\s*\n\\s*")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags>(0));
		}
		Glib::ustring expr;
		expr += "\\n"; // Keep if preceeded by line break
		if(m_filterKeepIfDot->get_active()){
			expr += "\\.";
		}
		if(m_filterKeepIfQuote->get_active()){
			expr += "'\"";
		}
		if(!expr.empty()){
			expr = "([^" + expr + "])";
		}else{
			expr = "(.)";
		}
		expr += "\n";
		if(m_filterKeepIfQuote->get_active()){
			expr += "(?!['\"])";
		}
		txt = Glib::Regex::create(expr)->replace(txt, 0, "\\1 ", static_cast<Glib::RegexMatchFlags>(0));

		if(m_filterJoinSpace->get_active()){
			txt = Glib::Regex::create("[ \t]+")->replace(txt, 0, " ", static_cast<Glib::RegexMatchFlags>(0));
		}
		return true;
	}, _("Stripping line breaks..."));

	m_textBuffer->replace_range(txt, start, end);
	start = end = m_textBuffer->get_iter_at_mark(m_textBuffer->get_insert());
	start.backward_chars(txt.size());
	m_textBuffer->select_range(start, end);
}

void OutputManager::toggleReplaceBox(Gtk::ToggleToolButton* button)
{
	if(button->get_active()){
		m_searchEntry->set_text("");
		m_replaceEntry->set_text("");
	}
	m_replaceBox->set_visible(button->get_active());
}

void OutputManager::historyChanged()
{
	m_undoButton->set_sensitive(m_textBuffer->can_undo());
	m_redoButton->set_sensitive(m_textBuffer->can_redo());
}

void OutputManager::saveIters()
{
	m_insertIter = m_textBuffer->get_iter_at_mark(m_textBuffer->get_insert());
	m_selectIter = m_textBuffer->get_iter_at_mark(m_textBuffer->get_selection_bound());
}

void OutputManager::findInBuffer(bool replace)
{
	Utils::clear_error_state(m_searchEntry);
	Glib::ustring find = m_searchEntry->get_text();
	if(find.empty()){
		return;
	}
	if(m_insertIter.is_end()){
		m_insertIter = m_textBuffer->begin();
	}
	Gtk::TextIter start, end;
	if(m_textBuffer->get_selection_bounds(start, end) && m_textBuffer->get_text(start, end, false) == find){
		if(replace){
			m_textBuffer->erase(start, end);
			m_textBuffer->insert_at_cursor(m_replaceEntry->get_text());
			m_insertIter = m_textBuffer->get_iter_at_mark(m_textBuffer->get_insert());
		}else{
			m_insertIter.forward_char();
		}
	}
	bool found = m_insertIter.forward_search(find, Gtk::TEXT_SEARCH_VISIBLE_ONLY, m_insertIter, end);
	if(!found && m_insertIter != m_textBuffer->begin()){
		m_insertIter = m_textBuffer->begin();
		found = m_insertIter.forward_search(find, Gtk::TEXT_SEARCH_VISIBLE_ONLY, m_insertIter, end);
	}
	if(!found){
		Utils::set_error_state(m_searchEntry);
	}else{
		m_textBuffer->select_range(m_insertIter, end);
		m_textView->scroll_to(end);
	}
}

void OutputManager::addText(const Glib::ustring& text, bool insert)
{
	if(insert){
		m_textBuffer->insert_at_cursor(text);
	}else{
		if(m_insertMode == InsertMode::Append){
			m_textBuffer->insert(m_textBuffer->end(), text);
		}else if(m_insertMode == InsertMode::Cursor){
			m_textBuffer->replace_range(text, m_insertIter, m_selectIter);
		}else if(m_insertMode == InsertMode::Replace){
			m_textBuffer->replace_all(text);
		}
	}
	m_outputBox->show();
}

bool OutputManager::saveBuffer(std::string filename)
{
	if(filename.empty()){
		std::string base, ext;
		const std::string& currentSource = Glib::path_get_basename(MAIN->getSourceManager()->getSelectedSource());
		Utils::get_filename_parts(currentSource, base, ext);
		std::string outputDir = Glib::get_user_special_dir(G_USER_DIRECTORY_DOCUMENTS);
		if(!Glib::file_test(outputDir, Glib::FILE_TEST_IS_DIR)){
			outputDir = Glib::get_home_dir();
		}
		std::string outputName = Glib::build_filename(outputDir, base + ".txt");

		FileDialogs::FileFilter filter = {_("Text Files"), "text/plain", "*.txt"};
		filename = FileDialogs::save_dialog(_("Save Output..."), outputName, filter);

		if(filename.empty()) {
			return false;
		}
	}
	std::ofstream file(filename);
	if(!file.is_open()){
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	Glib::ustring txt = m_textBuffer->get_text(false);
	file.write(txt.data(), std::strlen(txt.data()));
	m_textBuffer->set_modified(false);
	return true;
}

bool OutputManager::clearBuffer()
{
	if(!m_outputBox->get_visible()){
		return true;
	}
	if(m_textBuffer->get_modified()){
		int response = Utils::question_dialog(_("Output not saved"), _("Save output before proceeding?"));
		if((response == 1 && !saveBuffer()) || response == 2){
			return false;
		}
	}
	m_textBuffer->set_text("");
	m_textBuffer->clear_history();
	m_outputBox->hide();
	return true;
}

void OutputManager::setLanguage(const Config::Lang& lang)
{
	Notifier::hide(m_notifierHandle);
	m_spell.detach();
	if(!lang.code.empty()){
		try{
			m_spell.set_language(lang.code);
			m_spell.attach(*m_textView);
		}catch(const GtkSpell::Error& e){
			if(MAIN->getConfig()->getSetting<SwitchSetting>("dictinstall")->getValue()){
				Notifier::Action actionDontShowAgain = {_("Don't show again"), []{ MAIN->getConfig()->getSetting<SwitchSetting>("dictinstall")->setValue(false); return true; }};
				Notifier::Action actionInstall = Notifier::Action{_("Help"), []{ MAIN->showHelp("#InstallSpelling"); return false; }};
				// Try initiating a DBUS connection for PackageKit
				Glib::RefPtr<Gio::DBus::Proxy> proxy;
	#ifdef G_OS_UNIX
				try{
					proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BUS_TYPE_SESSION, "org.freedesktop.PackageKit",
																  "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify");
					actionInstall = Notifier::Action{_("Install"), [this,proxy,lang]{ dictionaryAutoinstall(proxy, lang.code); return true; }};
				}catch(const Glib::Error&){
					g_warning("Could not find PackageKit on DBus, dictionary autoinstallation will not work");
				}
	#endif
				Notifier::notify(_("Spelling dictionary missing"), Glib::ustring::compose(_("The spellcheck dictionary for %1 is not installed"), lang.name), {actionInstall, actionDontShowAgain}, &m_notifierHandle);
			}
		}
	}
}

#ifdef G_OS_UNIX
void OutputManager::dictionaryAutoinstall(Glib::RefPtr<Gio::DBus::Proxy> proxy, const Glib::ustring &lang)
{
	MAIN->pushState(MainWindow::State::Busy, Glib::ustring::compose(_("Installing spelling dictionary for '%1'"), lang));
	std::uint32_t xid = gdk_x11_window_get_xid(MAIN->getWindow()->get_window()->gobj());
	std::vector<Glib::ustring> files = {"/usr/share/myspell/" + lang + ".dic", "/usr/share/hunspell/" + lang + ".dic"};
	std::vector<Glib::VariantBase> params = { Glib::Variant<std::uint32_t>::create(xid),
											  Glib::Variant<std::vector<Glib::ustring>>::create(files),
											  Glib::Variant<Glib::ustring>::create("always") };
	proxy->call("InstallProvideFiles", [proxy,this](Glib::RefPtr<Gio::AsyncResult> r){ dictionaryAutoinstallDone(proxy, r); },
				Glib::VariantContainerBase::create_tuple(params), 3600000);
}

void OutputManager::dictionaryAutoinstallDone(Glib::RefPtr<Gio::DBus::Proxy> proxy, Glib::RefPtr<Gio::AsyncResult>& result)
{
	try {
		proxy->call_finish(result);
	} catch (const Glib::Error& e) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to install spelling dictionary"), e.what());
	}
	MAIN->getConfig()->updateLanguagesMenu();
	MAIN->popState();
}
#endif
