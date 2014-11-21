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
#include "OutputManager.hh"
#include "SourceManager.hh"
#include "SubstitutionsManager.hh"
#include "Utils.hh"

#include <fstream>
#include <cstring>

#ifdef G_OS_UNIX
#include <gdk/gdkx.h>
#endif

OutputManager::OutputManager()
{
	m_togglePaneButton = Builder("tbbutton:main.outputpane");
	m_insButton = Builder("tbbutton:output.insert");
	m_insMenu = Builder("menu:output.insert");
	m_insImage = Builder("image:output.insert");
	m_replaceBox = Builder("box:output.findreplace");
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
	m_csCheckBox = Builder("checkbutton:output.matchcase");
	m_textBuffer = UndoableBuffer::create();
	m_textView->set_buffer(m_textBuffer);
	Builder("tbbutton:output.stripcrlf").as<Gtk::MenuToolButton>()->set_menu(*Builder("menu:output.stripcrlf").as<Gtk::Menu>());

	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	m_undoButton->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	m_redoButton->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK|Gdk::SHIFT_MASK, Gtk::AccelFlags(0));

	m_substitutionsManager = new SubstitutionsManager(m_textBuffer, m_csCheckBox);

	m_insertMode = InsertMode::Append;

	m_spell.property_decode_language_codes() = true;

	CONNECTS(Builder("combo:config.settings.paneorient").as<Gtk::ComboBoxText>(), changed, [this](Gtk::ComboBoxText* combo){
		Builder("paned:output").as<Gtk::Paned>()->set_orientation(static_cast<Gtk::Orientation>(!combo->get_active_row_number())); });
	CONNECT(m_togglePaneButton, toggled, [this]{ m_outputBox->set_visible(m_togglePaneButton->get_active());});
	CONNECT(m_togglePaneButton, toggled, [this]{ m_substitutionsManager->set_visible(false);});
	CONNECT(m_insButton, toggled, [this]{ showInsertMenu(); });
	CONNECT(m_insMenu, deactivate, [this]{ m_insButton->set_active(false); });
	CONNECT(Builder("menuitem:output.insert.append").as<Gtk::MenuItem>(), activate, [this]{ setInsertMode(InsertMode::Append, "ins_append.png"); });
	CONNECT(Builder("menuitem:output.insert.cursor").as<Gtk::MenuItem>(), activate, [this]{ setInsertMode(InsertMode::Cursor, "ins_cursor.png"); });
	CONNECT(Builder("menuitem:output.insert.replace").as<Gtk::MenuItem>(), activate, [this]{ setInsertMode(InsertMode::Replace, "ins_replace.png"); });
	CONNECT(Builder("tbbutton:output.stripcrlf").as<Gtk::ToolButton>(), clicked, [this]{ filterBuffer(); });
	CONNECTS(Builder("tbbutton:output.findreplace").as<Gtk::ToggleToolButton>(), toggled, [this](Gtk::ToggleToolButton* b){ toggleReplaceBox(b); });
	CONNECT(m_undoButton, clicked, [this]{ m_textBuffer->undo(); m_textView->grab_focus(); });
	CONNECT(m_redoButton, clicked, [this]{ m_textBuffer->redo(); m_textView->grab_focus(); });
	CONNECT(Builder("tbbutton:output.save").as<Gtk::ToolButton>(), clicked, [this]{ saveBuffer(); });
	CONNECT(Builder("tbbutton:output.clear").as<Gtk::ToolButton>(), clicked, [this]{ clearBuffer(); });
	CONNECT(m_textBuffer, history_changed, [this]{ m_undoButton->set_sensitive(m_textBuffer->can_undo()); });
	CONNECT(m_textBuffer, history_changed, [this]{ m_redoButton->set_sensitive(m_textBuffer->can_redo()); });
	CONNECT(m_csCheckBox, toggled, [this]{ Utils::clear_error_state(m_searchEntry); });
	CONNECT(m_searchEntry, changed, [this]{ Utils::clear_error_state(m_searchEntry); });
	CONNECT(m_searchEntry, activate, [this]{ findReplace(false, false); });
	CONNECT(m_replaceEntry, activate, [this]{ findReplace(false, true); });
	CONNECT(Builder("button:output.searchnext").as<Gtk::Button>(), clicked, [this]{ findReplace(false, false); });
	CONNECT(Builder("button:output.searchprev").as<Gtk::Button>(), clicked, [this]{ findReplace(true, false); });
	CONNECT(Builder("button:output.replace").as<Gtk::Button>(), clicked, [this]{ findReplace(false, true); });
	CONNECT(Builder("button:output.replaceall").as<Gtk::Button>(), clicked, [this]{ replaceAll(); });
	CONNECTP(Builder("fontbutton:config.settings.customoutputfont").as<Gtk::FontButton>(), font_name, [this]{ setFont(); });
	CONNECT(Builder("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>(), toggled, [this]{ setFont(); });
	CONNECT(Builder("button:output.substitutions").as<Gtk::ToolButton>(), clicked, [this]{ m_substitutionsManager->set_visible(true); });
	CONNECT(m_textView, populate_popup, [this](Gtk::Menu* menu){ completeTextViewMenu(menu); });

	// If the contents changes, always save the selections bounds (since the previous ones are invalid)
	CONNECT(m_textBuffer, changed, [this]{ m_textBuffer->save_selection_bounds(true); });
	// If the insert or selection mark change save the bounds either if the view is focused or the selection is non-empty
	CONNECTP(m_textBuffer, cursor_position, [this]{ m_textBuffer->save_selection_bounds(m_textView->is_focus()); });
	CONNECTP(m_textBuffer, has_selection, [this]{ m_textBuffer->save_selection_bounds(m_textView->is_focus()); });

	MAIN->getConfig()->addSetting("outputorient", new ComboSetting("combo:config.settings.paneorient"));
	MAIN->getConfig()->addSetting("systemoutputfont", new SwitchSettingT<Gtk::CheckButton>("checkbutton:config.settings.defaultoutputfont"));
	MAIN->getConfig()->addSetting("customoutputfont", new FontSetting("fontbutton:config.settings.customoutputfont"));
	MAIN->getConfig()->addSetting("keepdot", new SwitchSettingT<Gtk::CheckMenuItem>("menuitem:output.stripcrlf.keepdot"));
	MAIN->getConfig()->addSetting("keepquote", new SwitchSettingT<Gtk::CheckMenuItem>("menuitem:output.stripcrlf.keepquote"));
	MAIN->getConfig()->addSetting("joinhyphen", new SwitchSettingT<Gtk::CheckMenuItem>("menuitem:output.stripcrlf.joinhyphen"));
	MAIN->getConfig()->addSetting("joinspace", new SwitchSettingT<Gtk::CheckMenuItem>("menuitem:output.stripcrlf.joinspace"));
	MAIN->getConfig()->addSetting("searchmatchcase", new SwitchSettingT<Gtk::CheckButton>("checkbutton:output.matchcase"));
}

OutputManager::~OutputManager(){
	delete m_substitutionsManager;
}

void OutputManager::setFont()
{
	if(Builder("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>()->get_active()){
		Builder("textview:output").as<Gtk::TextView>()->unset_font();
	}else{
		Gtk::FontButton* fontBtn = Builder("fontbutton:config.settings.customoutputfont");
		Builder("textview:output").as<Gtk::TextView>()->override_font(Pango::FontDescription(fontBtn->get_font_name()));
	}
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
	m_insImage->set(Gdk::Pixbuf::create_from_resource(std::string("/org/gnome/gimagereader/") + iconName));
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

	start = end = m_textBuffer->replace_range(txt, start, end);
	start.backward_chars(txt.size());
	m_textBuffer->select_range(start, end);
}

void OutputManager::toggleReplaceBox(Gtk::ToggleToolButton* button)
{
	m_searchEntry->set_text("");
	m_replaceEntry->set_text("");
	m_replaceBox->set_visible(button->get_active());
}

void OutputManager::replaceAll()
{
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	Gtk::TextIter start, end;
	if(!m_textBuffer->get_selection_bounds(start, end)){
		start = m_textBuffer->begin();
		end = m_textBuffer->end();
	}
	int startpos = start.get_offset();
	int endpos = end.get_offset();
	Gtk::TextSearchFlags flags = Gtk::TEXT_SEARCH_VISIBLE_ONLY|Gtk::TEXT_SEARCH_TEXT_ONLY;
	if(!m_csCheckBox->get_active()){
		flags |= Gtk::TEXT_SEARCH_CASE_INSENSITIVE;
	}
	Glib::ustring search = m_searchEntry->get_text();
	Glib::ustring replace = m_replaceEntry->get_text();
	int diff = replace.length() - search.length();
	int count = 0;
	Gtk::TextIter it = m_textBuffer->get_iter_at_offset(startpos);
	while(true){
		Gtk::TextIter matchStart, matchEnd;
		if(!it.forward_search(search, flags, matchStart, matchEnd) || matchEnd.get_offset() > endpos){
			break;
		}
		it = m_textBuffer->replace_range(replace, matchStart, matchEnd);
		endpos += diff;
		++count;
		while(Gtk::Main::events_pending()){
			Gtk::Main::iteration();
		}
	}
	if(count == 0){
		Utils::set_error_state(m_searchEntry);
	}
	MAIN->popState();
}

void OutputManager::findReplace(bool backwards, bool replace)
{
	Glib::ustring findStr = m_searchEntry->get_text();
	if(findStr.empty()){
		return;
	}
	Gtk::TextSearchFlags flags = Gtk::TEXT_SEARCH_VISIBLE_ONLY|Gtk::TEXT_SEARCH_TEXT_ONLY;
	auto comparator = m_csCheckBox->get_active() ?
				[](const Glib::ustring& s1, const Glib::ustring& s2){ return s1 == s2; } :
				[](const Glib::ustring& s1, const Glib::ustring& s2){ return s1.lowercase() == s2.lowercase(); };
	if(!m_csCheckBox->get_active()){
		flags |= Gtk::TEXT_SEARCH_CASE_INSENSITIVE;
	}

	Gtk::TextIter start, end;
	m_textBuffer->get_selection_bounds(start, end);
	if(comparator(m_textBuffer->get_text(start, end, false), findStr)){
		if(replace){
			Glib::ustring replaceStr = m_replaceEntry->get_text();
			end = m_textBuffer->replace_range(replaceStr, start, end);
			start = end;
			start.backward_chars(replaceStr.length());
			m_textBuffer->select_range(start, end);
			m_textView->scroll_to(end);
			return;
		}
		if(backwards){
			end.backward_char();
		}else{
			start.forward_char();
		}
	}
	Gtk::TextIter matchStart, matchEnd;
	if(backwards){
		if(!end.backward_search(findStr, flags, matchStart, matchEnd) &&
		   !m_textBuffer->end().backward_search(findStr, flags, matchStart, matchEnd))
		{
			Utils::set_error_state(m_searchEntry);
			return;
		}
	}else{
		if(!start.forward_search(findStr, flags, matchStart, matchEnd) &&
		   !m_textBuffer->begin().forward_search(findStr, flags, matchStart, matchEnd))
		{
			Utils::set_error_state(m_searchEntry);
			return;
		}
	}
	m_textBuffer->select_range(matchStart, matchEnd);
	m_textView->scroll_to(matchStart);
}

void OutputManager::completeTextViewMenu(Gtk::Menu *menu)
{
	Gtk::CheckMenuItem* item = Gtk::manage(new Gtk::CheckMenuItem(_("Check spelling")));
	item->set_active(GtkSpell::Checker::get_from_text_view(*m_textView));
	CONNECT(item, toggled, [this, item]{
		if(item->get_active()) {
			setLanguage(MAIN->getConfig()->getSelectedLanguage(), true);
		} else {
			setLanguage(Config::Lang(), false);
		}
	});
	menu->prepend(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	menu->prepend(*item);
	menu->show_all();
}

void OutputManager::addText(const Glib::ustring& text, bool insert)
{
	if(insert){
		m_textBuffer->insert_at_cursor(text);
	}else{
		if(m_insertMode == InsertMode::Append){
			m_textBuffer->insert(m_textBuffer->end(), text);
		}else if(m_insertMode == InsertMode::Cursor){
			Gtk::TextIter start, end;
			m_textBuffer->get_selection_bounds(start, end);
			m_textBuffer->replace_range(text, start, end);
		}else if(m_insertMode == InsertMode::Replace){
			m_textBuffer->replace_all(text);
		}
	}
	m_outputBox->show();
	m_togglePaneButton->set_active(true);
}

bool OutputManager::saveBuffer(const std::string& filename)
{
	std::string outname = filename;
	if(outname.empty()){
		Source* source = MAIN->getSourceManager()->getSelectedSource();
		std::string ext, base;
		std::string name = source ? source->displayname : _("output");
		Utils::get_filename_parts(name, base, ext);
		outname = Glib::build_filename(Utils::get_documents_dir(), base + ".txt");

		FileDialogs::FileFilter filter = {_("Text Files"), "text/plain", "*.txt"};
		outname = FileDialogs::save_dialog(_("Save Output..."), outname, filter);
		if(outname.empty()) {
			return false;
		}
	}
	std::ofstream file(outname);
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
	m_textBuffer->set_modified(false);
	m_togglePaneButton->set_active(false);
	return true;
}

bool OutputManager::getBufferModified() const
{
	m_textBuffer->get_modified();
}

void OutputManager::setLanguage(const Config::Lang& lang, bool force)
{
	MAIN->hideNotification(m_notifierHandle);
	m_notifierHandle = nullptr;
	m_spell.detach();
	std::string code = lang.code;
	if(force && code.empty()) {
		code = _("en_US");
	}
	if(!code.empty()){
		try{
			m_spell.set_language(code);
			m_spell.attach(*m_textView);
		}catch(const GtkSpell::Error& e){
			if(MAIN->getConfig()->getSetting<SwitchSetting>("dictinstall")->getValue()){
				MainWindow::NotificationAction actionDontShowAgain = {_("Don't show again"), []{ MAIN->getConfig()->getSetting<SwitchSetting>("dictinstall")->setValue(false); return true; }};
				MainWindow::NotificationAction actionInstall = {_("Help"), []{ MAIN->showHelp("#InstallSpelling"); return false; }};
				// Try initiating a DBUS connection for PackageKit
				Glib::RefPtr<Gio::DBus::Proxy> proxy;
	#ifdef G_OS_UNIX
				try{
					proxy = Gio::DBus::Proxy::create_for_bus_sync(Gio::DBus::BUS_TYPE_SESSION, "org.freedesktop.PackageKit",
																  "/org/freedesktop/PackageKit", "org.freedesktop.PackageKit.Modify");
					actionInstall = MainWindow::NotificationAction{_("Install"), [this,proxy,lang]{ dictionaryAutoinstall(proxy, lang.code); return true; }};
				}catch(const Glib::Error&){
					g_warning("Could not find PackageKit on DBus, dictionary autoinstallation will not work");
				}
	#endif
				MAIN->addNotification(_("Spelling dictionary missing"), Glib::ustring::compose(_("The spellcheck dictionary for %1 is not installed"), lang.name), {actionInstall, actionDontShowAgain}, &m_notifierHandle);
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
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), Glib::ustring::compose(_("Failed to install spelling dictionary: %1"), e.what()));
	}
	MAIN->getConfig()->updateLanguagesMenu();
	MAIN->popState();
}
#endif
