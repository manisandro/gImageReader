/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputManager.cc
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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
#include "OutputBuffer.hh"
#include "OutputManager.hh"
#include "Recognizer.hh"
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
	m_filterKeepParagraphs = Builder("menuitem:output.stripcrlf.keepparagraphs");
	m_toggleSearchButton = Builder("tbbutton:output.findreplace");
	m_undoButton = Builder("tbbutton:output.undo");
	m_redoButton = Builder("tbbutton:output.redo");
	m_csCheckBox = Builder("checkbutton:output.matchcase");
	m_textBuffer = OutputBuffer::create();
	m_textView->set_source_buffer(m_textBuffer);
	Builder("tbbutton:output.stripcrlf").as<Gtk::MenuToolButton>()->set_menu(*Builder("menu:output.stripcrlf").as<Gtk::Menu>());
	Gtk::ToolButton* saveButton = Builder("tbbutton:output.save");

	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	m_undoButton->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	m_redoButton->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK|Gdk::SHIFT_MASK, Gtk::AccelFlags(0));
	m_toggleSearchButton->get_child()->add_accelerator("clicked", group, GDK_KEY_F, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	saveButton->add_accelerator("clicked", group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));

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
	CONNECT(m_toggleSearchButton, toggled, [this]{ toggleReplaceBox(); });
	CONNECT(m_undoButton, clicked, [this]{ m_textBuffer->undo(); scrollCursorIntoView(); });
	CONNECT(m_redoButton, clicked, [this]{ m_textBuffer->redo(); scrollCursorIntoView(); });
	CONNECT(saveButton, clicked, [this]{ saveBuffer(); });
	CONNECT(Builder("tbbutton:output.clear").as<Gtk::ToolButton>(), clicked, [this]{ clearBuffer(); });
	CONNECTP(m_textBuffer, can_undo, [this]{ m_undoButton->set_sensitive(m_textBuffer->can_undo()); });
	CONNECTP(m_textBuffer, can_redo, [this]{ m_redoButton->set_sensitive(m_textBuffer->can_redo()); });
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

	// If the insert or selection mark change save the bounds either if the view is focused or the selection is non-empty
	CONNECTP(m_textBuffer, cursor_position, [this]{ m_textBuffer->save_region_bounds(m_textView->is_focus()); });
	CONNECTP(m_textBuffer, has_selection, [this]{ m_textBuffer->save_region_bounds(m_textView->is_focus()); });

	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("keepdot", "menuitem:output.stripcrlf.keepdot"));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("keepquote", "menuitem:output.stripcrlf.keepquote"));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("joinhyphen", "menuitem:output.stripcrlf.joinhyphen"));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("joinspace", "menuitem:output.stripcrlf.joinspace"));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("keepparagraphs", "menuitem:output.stripcrlf.keepparagraphs"));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckButton>("searchmatchcase", "checkbutton:output.matchcase"));
	MAIN->getConfig()->addSetting(new VarSetting<Glib::ustring>("outputdir"));

	if(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue().empty()){
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Utils::get_documents_dir());
	}

	setFont();
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
#if GTKMM_CHECK_VERSION(3,12,0)
	m_insImage->set(Gdk::Pixbuf::create_from_resource(std::string("/org/gnome/gimagereader/") + iconName));
#else
	gtk_image_set_from_pixbuf(m_insImage->gobj(), gdk_pixbuf_new_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/%1", iconName).c_str(), 0));
#endif
}

void OutputManager::filterBuffer()
{
	Gtk::TextIter start, end;
	m_textBuffer->get_region_bounds(start, end);
	Glib::ustring txt = m_textBuffer->get_text(start, end);

	Utils::busyTask([this,&txt]{
		if(m_filterJoinHyphen->get_active()){
			txt = Glib::Regex::create("-\\s*\n\\s*")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags>(0));
		}
		Glib::ustring preChars, sucChars;
		if(m_filterKeepParagraphs->get_active()){
			preChars += "\\n"; // Keep if preceded by line break
		}
		if(m_filterKeepIfDot->get_active()){
			preChars += "\\."; // Keep if preceded by dot
		}
		if(m_filterKeepIfQuote->get_active()){
			preChars += "'\""; // Keep if preceded by dot
			sucChars += "'\""; // Keep if succeeded by dot
		}
		if(m_filterKeepParagraphs->get_active()){
			sucChars += "\\n"; // Keep if succeeded by line break
		}
		if(!preChars.empty()){
			preChars = "([^" + preChars + "])";
		}
		if(!sucChars.empty()){
			sucChars = "(?![" + sucChars + "])";
		}
		Glib::ustring expr = preChars + "\\n" + sucChars;
		txt = Glib::Regex::create(expr)->replace(txt, 0, preChars.empty() ? " " : "\\1 ", static_cast<Glib::RegexMatchFlags>(0));

		if(m_filterJoinSpace->get_active()){
			txt = Glib::Regex::create("[ \t]+")->replace(txt, 0, " ", static_cast<Glib::RegexMatchFlags>(0));
		}
		return true;
	}, _("Stripping line breaks..."));

	start = end = m_textBuffer->insert(m_textBuffer->erase(start, end), txt);
	start.backward_chars(txt.size());
	m_textBuffer->select_range(start, end);
}

void OutputManager::toggleReplaceBox()
{
	m_searchEntry->set_text("");
	m_replaceEntry->set_text("");
	m_replaceBox->set_visible(m_toggleSearchButton->get_active());
}

void OutputManager::replaceAll()
{
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	Gtk::TextIter start, end;
	m_textBuffer->get_region_bounds(start, end);
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
		it = m_textBuffer->insert(m_textBuffer->erase(matchStart, matchEnd), replace);
		endpos += diff;
		++count;
		while(Gtk::Main::events_pending()){
			Gtk::Main::iteration();
		}
	}
	if(count == 0){
		Utils::set_error_state(m_searchEntry);
	}
	m_textBuffer->select_range(m_textBuffer->get_iter_at_offset(startpos), m_textBuffer->get_iter_at_offset(endpos));
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

	Gtk::TextIter rstart, rend;
	m_textBuffer->get_region_bounds(rstart, rend);

	Gtk::TextIter start, end;
	m_textBuffer->get_selection_bounds(start, end);
	if(comparator(m_textBuffer->get_text(start, end, false), findStr)){
		if(replace){
			Glib::ustring replaceStr = m_replaceEntry->get_text();
			start = end = m_textBuffer->insert(m_textBuffer->erase(start, end), replaceStr);
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
		if(!end.backward_search(findStr, flags, matchStart, matchEnd, rstart) &&
		   !rend.backward_search(findStr, flags, matchStart, matchEnd, rstart))
		{
			Utils::set_error_state(m_searchEntry);
			return;
		}
	}else{
		if(!start.forward_search(findStr, flags, matchStart, matchEnd, rend) &&
		   !rstart.forward_search(findStr, flags, matchStart, matchEnd, rend))
		{
			Utils::set_error_state(m_searchEntry);
			return;
		}
	}
	// FIXME: backward_search appears to be buggy?
	matchEnd = matchStart; matchEnd.forward_chars(findStr.length());
	m_textBuffer->select_range(matchStart, matchEnd);
	m_textView->scroll_to(matchStart);
}

void OutputManager::completeTextViewMenu(Gtk::Menu *menu)
{
	Gtk::CheckMenuItem* item = Gtk::manage(new Gtk::CheckMenuItem(_("Check spelling")));
	item->set_active(GtkSpell::Checker::get_from_text_view(*m_textView));
	CONNECT(item, toggled, [this, item]{
		if(item->get_active()) {
			setLanguage(MAIN->getRecognizer()->getSelectedLanguage(), true);
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
			m_textBuffer->get_region_bounds(start, end);
			m_textBuffer->insert(m_textBuffer->erase(start, end), text);
		}else if(m_insertMode == InsertMode::Replace){
			m_textBuffer->insert(m_textBuffer->erase(m_textBuffer->begin(), m_textBuffer->end()), text);
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
		outname = Glib::build_filename(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue(), base + ".txt");

		FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
		outname = FileDialogs::save_dialog(_("Save Output..."), outname, filter);
		if(outname.empty()) {
			return false;
		}
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Glib::path_get_dirname(outname));
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
		int response = Utils::question_dialog(_("Output not saved"), _("Save output before proceeding?"), Utils::Button::Save|Utils::Button::Discard|Utils::Button::Cancel);
		if(response == Utils::Button::Save){
			if(!saveBuffer()){
				return false;
			}
		}else if(response != Utils::Button::Discard){
			return false;
		}
	}
	m_textBuffer->begin_not_undoable_action();
	m_textBuffer->set_text("");
	m_textBuffer->end_not_undoable_action();
	m_textBuffer->set_modified(false);
	m_togglePaneButton->set_active(false);
	return true;
}

bool OutputManager::getBufferModified() const
{
	return m_textBuffer->get_modified();
}

void OutputManager::scrollCursorIntoView()
{
	m_textView->scroll_to(m_textView->get_buffer()->get_insert());
	m_textView->grab_focus();
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
		}catch(const GtkSpell::Error& /*e*/){
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
	MAIN->getRecognizer()->updateLanguagesMenu();
	MAIN->popState();
}
#endif
