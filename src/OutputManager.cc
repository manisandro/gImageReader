/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputManager.cc
 * Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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

#include "MainWindow.hh"
#include "Notifier.hh"
#include "OutputManager.hh"
#include "SourceManager.hh"
#include "Utils.hh"

#include <fstream>

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
	m_searchIter = m_textBuffer->end();

	Builder("tbbutton:output.stripcrlf").as<Gtk::MenuToolButton>()->set_menu(*Builder("menu:output.stripcrlf").as<Gtk::Menu>());
	Builder("tbbutton:output.insert").as<Gtk::ToggleToolButton>()->set_homogeneous(false);

	CONNECT(m_insButton, toggled, [this]{ showInsertMenu(); });
	CONNECT(m_insMenu, deactivate, [this]{ m_insButton->set_active(false); });
	CONNECTS(Builder("menuitem:output.insert.append").as<Gtk::ImageMenuItem>(), activate, [this](Gtk::ImageMenuItem* i){ setInsertMode(InsertMode::Append, i); });
	CONNECTS(Builder("menuitem:output.insert.cursor").as<Gtk::ImageMenuItem>(), activate, [this](Gtk::ImageMenuItem* i){ setInsertMode(InsertMode::Cursor, i); });
	CONNECTS(Builder("menuitem:output.insert.replace").as<Gtk::ImageMenuItem>(), activate, [this](Gtk::ImageMenuItem* i){ setInsertMode(InsertMode::Replace, i); });
	CONNECT(Builder("tbbutton:output.stripcrlf").as<Gtk::ToolButton>(), clicked, [this]{ filterBuffer(); });
	CONNECTS(Builder("tbbutton:output.findreplace").as<Gtk::ToggleToolButton>(), toggled, [this](Gtk::ToggleToolButton* b){ toggleReplaceBox(b); });
	CONNECT(m_textView, set_anchor, [this]{ setSearchIter(); });
	CONNECT(m_textBuffer, history_changed, [this]{ historyChanged(); });
	CONNECT(m_searchEntry, changed, [this]{ Utils::clear_error_state(m_searchEntry); });
	CONNECT(m_searchEntry, activate, [this]{ findInBuffer(); });
	CONNECT(Builder("button:output.search").as<Gtk::Button>(), clicked, [this]{ findInBuffer(); });
	CONNECT(m_replaceEntry, activate, [this]{ findInBuffer(true); });
	CONNECT(Builder("button:output.replace").as<Gtk::Button>(), clicked, [this]{ findInBuffer(true); });
	CONNECT(m_undoButton, clicked, [this]{ m_textBuffer->undo(); });
	CONNECT(m_redoButton, clicked, [this]{ m_textBuffer->redo(); });
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

void OutputManager::setInsertMode(InsertMode mode, Gtk::ImageMenuItem* item)
{
	m_insertMode = mode;
	m_insImage->set(((Gtk::Image*)item->get_image())->get_pixbuf());
}

void OutputManager::filterBuffer()
{
	m_textView->set_editable(false);
	Gtk::TextIter start, end;
	if(!m_textBuffer->get_selection_bounds(start, end)){
		start = m_textBuffer->begin();
		end = m_textBuffer->end();
	}
	Glib::ustring txt = m_textBuffer->get_text(start, end);

	if(m_filterJoinHyphen->get_active()){
		txt = Glib::Regex::create("-\\s*\n\\s*")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags>(0));
	}
	Glib::ustring expr;
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
	txt = Glib::Regex::create(expr)->replace(txt, 0, "\\1", static_cast<Glib::RegexMatchFlags>(0));

	if(m_filterJoinSpace->get_active()){
		txt = Glib::Regex::create("\\s+")->replace(txt, 0, " ", static_cast<Glib::RegexMatchFlags>(0));
	}
	m_textBuffer->replace_selection(txt, true);
	start = end = m_textBuffer->get_iter_at_mark(m_textBuffer->get_insert());
	start.backward_chars(txt.size());
	m_textBuffer->select_range(start, end);
	m_textView->set_editable(true);
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

void OutputManager::setSearchIter()
{
	m_searchIter = m_textBuffer->get_iter_at_mark(m_textBuffer->get_insert());
}

void OutputManager::findInBuffer(bool replace)
{
	Utils::clear_error_state(m_searchEntry);
	Glib::ustring find = m_searchEntry->get_text();
	if(find.empty()){
		return;
	}
	if(m_searchIter.is_end()){
		m_searchIter = m_textBuffer->begin();
	}
	Gtk::TextIter start, end;
	if(m_textBuffer->get_selection_bounds(start, end) && m_textBuffer->get_text(start, end, false) == find){
		if(replace){
			m_textBuffer->erase(start, end);
			m_textBuffer->insert_at_cursor(m_replaceEntry->get_text());
		}else{
			m_searchIter.forward_char();
		}
	}
	bool found = m_searchIter.forward_search(find, Gtk::TEXT_SEARCH_VISIBLE_ONLY, m_searchIter, end);
	if(!found && m_searchIter != m_textBuffer->begin()){
		m_searchIter = m_textBuffer->begin();
		found = m_searchIter.forward_search(find, Gtk::TEXT_SEARCH_VISIBLE_ONLY, m_searchIter, end);
	}
	if(!found){
		Utils::set_error_state(m_searchEntry);
	}else{
		m_textBuffer->select_range(m_searchIter, end);
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
			m_textBuffer->replace_selection(text);
		}else if(m_insertMode == InsertMode::Replace){
			m_textBuffer->replace_all(text);
		}
	}
	m_outputBox->show();
}

bool OutputManager::saveBuffer()
{
	std::string base, ext;
	const std::string& currentSource = MAIN->getSourceManager()->getSelectedSource();
	Utils::get_filename_parts(currentSource, base, ext);
	std::string output = base + ".txt";
	Gtk::FileChooserDialog savedialog(*MAIN->getWindow(), _("Save Output..."), Gtk::FILE_CHOOSER_ACTION_SAVE);
	savedialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	savedialog.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);
	savedialog.set_local_only(false);
	savedialog.set_do_overwrite_confirmation(true);
	Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
	filter->set_name(_("Text Files"));
	filter->add_pattern(("*.txt"));
	savedialog.add_filter(filter);
	savedialog.set_current_folder(Glib::path_get_dirname(output));
	savedialog.set_current_name(Glib::path_get_basename(output));
	int response = savedialog.run();
	savedialog.hide();
	if(response != Gtk::RESPONSE_OK){
		return false;
	}
	std::ofstream file(savedialog.get_filename());
	if(!file.is_open()){
		Utils::error_dialog(_("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	Glib::ustring txt = m_textBuffer->get_text(false);
	file.write(txt.c_str(), txt.size());
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
	MAIN->getNotifier()->hide();
	m_spell.detach();
	if(!lang.code.empty()){
		try{
			m_spell.set_language(lang.code);
			m_spell.attach(*m_textView);
		}catch(const GtkSpell::Error& e){
			if(MAIN->getConfig()->getSetting<SwitchSetting>("dictinstall")->getValue()){
				// Try initiating a DBUS connection for PackageKit
				Glib::RefPtr<Gio::DBus::Proxy> proxy;
	#ifdef G_OS_UNIX
				try{
					proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BUS_TYPE_SESSION, "org.freedesktop.PackageKit",
																  "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify");
				}catch(const Glib::Error&){
					g_warning("Could not find PackageKit on DBus, dictionary autoinstallation will not work");
				}
	#endif
				Notifier::Action actionDontShowAgain = {_("Don't show again"), []{ MAIN->getConfig()->getSetting<SwitchSetting>("dictinstall")->setValue(false); MAIN->getNotifier()->hide(); }};
				Notifier::Action actionInstall = proxy ? Notifier::Action{_("Install"), [this,proxy,lang]{ dictionaryAutoinstall(proxy, lang.code); }} : Notifier::Action{_("Help"), []{ MAIN->showHelp(); MAIN->getNotifier()->hide(); }};
				MAIN->getNotifier()->notify(_("Spelling dictionary missing"), Glib::ustring::compose(_("The spellcheck dictionary for %1 is not installed"), lang.name), {actionInstall, actionDontShowAgain});
			}
		}
	}
}

#ifdef G_OS_UNIX
void OutputManager::dictionaryAutoinstall(Glib::RefPtr<Gio::DBus::Proxy> proxy, const Glib::ustring &lang)
{
	MAIN->pushState(MainWindow::State::Busy, Glib::ustring::compose(_("Installing spelling dictionary for '%1'"), lang));
	unsigned long xid = gdk_x11_window_get_xid(MAIN->getWindow()->get_window()->gobj());
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
		MAIN->getConfig()->updateLanguagesMenu();
	} catch (const Glib::Error& e) {
//		Utils::error_dialog(_("Failed to install spelling dictionary"), e.what());
	}
	MAIN->popState();
}
#endif
