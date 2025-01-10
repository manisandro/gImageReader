/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.cc
 * Copyright (C) 2013-2025 Sandro Mani <manisandro@gmail.com>
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

#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "OutputBuffer.hh"
#include "OutputEditorText.hh"
#include "Recognizer.hh"
#include "SearchReplaceFrame.hh"
#include "Utils.hh"

#include <fstream>
#include <gtkmm/scrolledwindow.h>

void OutputEditorText::TextBatchProcessor::appendOutput(std::ostream& dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo, bool firstArea) const {
	char* text = tess->GetUTF8Text();
	if (firstArea && m_prependPage) {
		dev << Glib::ustring::compose(_("Page: %1\n"), pageInfo.page).raw();
	}
	dev << text;
	dev << "\n";
	delete[] text;
}

OutputEditorText::OutputEditorText() {
	ui.setupUi();

	m_searchFrame = new SearchReplaceFrame();
	ui.boxSearch->pack_start(*Gtk::manage(m_searchFrame->getWidget()));
	m_searchFrame->getWidget()->set_visible(false);
	m_spell.property_decode_language_codes() = true;


	Glib::RefPtr<Gtk::RecentFilter> recentFilter = Gtk::RecentFilter::create();
	recentFilter->add_mime_type("text/plain");
	recentFilter->add_pattern("*.txt");
	Gtk::RecentChooserMenu* recentChooser = Gtk::manage(new Gtk::RecentChooserMenu());
	recentChooser->set_filter(recentFilter);
	recentChooser->set_local_only(false);
	recentChooser->set_show_not_found(false);
	recentChooser->set_show_tips(true);
	recentChooser->set_sort_type(Gtk::RECENT_SORT_MRU);
	ui.menuButtonOpenRecent->set_menu(*recentChooser);

	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	ui.buttonUndo->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonRedo->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK | Gdk::SHIFT_MASK, Gtk::AccelFlags(0));
	ui.buttonFindreplace->add_accelerator("clicked", group, GDK_KEY_F, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonSave->add_accelerator("clicked", group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonOpen->add_accelerator("clicked", group, GDK_KEY_O, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));

	m_insertMode = InsertMode::Append;

	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem> ("keepdot", ui.menuitemStripcrlfKeependmark));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem> ("keepquote", ui.menuitemStripcrlfKeepquote));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem> ("joinhyphen", ui.menuitemStripcrlfJoinhyphen));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem> ("joinspace", ui.menuitemStripcrlfJoinspace));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem> ("keepparagraphs", ui.menuitemStripcrlfKeepparagraphs));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem> ("drawwhitespace", ui.menuitemStripcrlfDrawwhitespace));
	ADD_SETTING(VarSetting<std::string> ("highlightmode"));

	CONNECT(recentChooser, item_activated, [this, recentChooser] { open(Glib::filename_from_uri(recentChooser->get_current_uri())); });
	CONNECT(ui.menuitemInsertAppend, activate, [this] { setInsertMode(InsertMode::Append, "ins_append.png"); });
	CONNECT(ui.menuitemInsertCursor, activate, [this] { setInsertMode(InsertMode::Cursor, "ins_cursor.png"); });
	CONNECT(ui.menuitemInsertReplace, activate, [this] { setInsertMode(InsertMode::Replace, "ins_replace.png"); });
	CONNECT(ui.buttonStripcrlf, clicked, [this] { filterBuffer(); });
	CONNECT(ui.buttonFindreplace, toggled, [this] { m_searchFrame->clear(); m_searchFrame->getWidget()->set_visible(ui.buttonFindreplace->get_active()); });
	CONNECT(ui.buttonUndo, clicked, [this] { textBuffer()->undo(); scrollCursorIntoView(); });
	CONNECT(ui.buttonRedo, clicked, [this] { textBuffer()->redo(); scrollCursorIntoView(); });
	CONNECT(ui.buttonSave, clicked, [this] { save(); });
	CONNECT(ui.buttonOpen, clicked, [this] { open(); });
	CONNECT(ui.buttonAddTab, clicked, [this] { addTab(); });
	CONNECT(ui.buttonClear, clicked, [this] { clear(); });
	CONNECT(ui.notebook, switch_page, [this](Gtk::Widget*, int) { tabChanged(); });
	CONNECT(m_searchFrame, find_replace, sigc::mem_fun(this, &OutputEditorText::findReplace));
	CONNECT(m_searchFrame, replace_all, sigc::mem_fun(this, &OutputEditorText::replaceAll));
	CONNECT(m_searchFrame, apply_substitutions, sigc::mem_fun(this, &OutputEditorText::applySubstitutions));
	CONNECT(ConfigSettings::get<FontSetting> ("customoutputfont"), changed, [this] { setFont(); });
	CONNECT(ConfigSettings::get<SwitchSetting> ("systemoutputfont"), changed, [this] { setFont(); });
	CONNECT(ui.menuitemStripcrlfDrawwhitespace, toggled, [this] { setDrawWhitspace(ui.menuitemStripcrlfDrawwhitespace->get_active()); });

	ui.notebook->set_current_page(addTab());
}

OutputEditorText::~OutputEditorText() {
	delete m_searchFrame;
}

int OutputEditorText::addTab(const Glib::ustring& title) {
	int page = ui.notebook->get_n_pages();

	Glib::RefPtr<OutputBuffer> textBuffer = OutputBuffer::create();

	Gsv::View* textView = Gtk::make_managed<Gsv::View> (textBuffer);
	if (ConfigSettings::get<SwitchSetting> ("systemoutputfont")->getValue()) {
		textView->unset_font();
	} else {
		Glib::ustring fontName = ConfigSettings::get<FontSetting> ("customoutputfont")->getValue();
		textView->override_font(Pango::FontDescription(fontName));
	}
	textView->set_wrap_mode(Gtk::WRAP_WORD);

	Gtk::ScrolledWindow* scrollWin = Gtk::make_managed<Gtk::ScrolledWindow>();
	scrollWin->add(*textView);
	scrollWin->show_all();

	Gtk::Button* button = Gtk::make_managed<Gtk::Button>();
	button->set_image_from_icon_name("window-close-symbolic", (Gtk::BuiltinIconSize) GTK_ICON_SIZE_MENU);
	button->set_tooltip_text(_("Close document"));
	button->set_relief((Gtk::ReliefStyle) GTK_RELIEF_NONE);
	CONNECT(button, clicked, [this, scrollWin] { closeTab(scrollWin); });

	Gtk::Label* tabLabel = Gtk::make_managed<Gtk::Label> (title.empty() ? Glib::ustring::compose(_("Untitled %1"), ++m_tabCounter) :  title);

	Gtk::HBox* hbox = Gtk::make_managed<Gtk::HBox> (false, 0);
	hbox->pack_start(*tabLabel);
	hbox->pack_end(*button, false, false, 0);
	hbox->show_all();

	ui.notebook->append_page(*scrollWin, *hbox);

	CONNECT(textView, populate_popup, [this](Gtk::Menu * menu) {
		completeTextViewMenu(menu);
	});
	CONNECT(textBuffer, modified_changed, [tabLabel, textBuffer] {
		Glib::ustring tabText = tabLabel->get_text();
		if (textBuffer->get_modified()) {
			tabLabel->set_text(Utils::string_rstrip(tabText, "*") + "*");
		} else {
			tabLabel->set_text(Utils::string_rstrip(tabText, "*"));
		}
	});
	// If the insert or selection mark change save the bounds either if the view is focused or the selection is non-empty
	CONNECTP(textBuffer, cursor_position, [textBuffer, textView] { textBuffer->save_region_bounds(textView->is_focus()); });
	CONNECTP(textBuffer, has_selection, [textBuffer, textView] { textBuffer->save_region_bounds(textView->is_focus()); });
	CONNECTP(textBuffer, can_undo, [this, textBuffer] { ui.buttonUndo->set_sensitive(textBuffer->can_undo()); });
	CONNECTP(textBuffer, can_redo, [this, textBuffer] { ui.buttonRedo->set_sensitive(textBuffer->can_redo()); });

	ui.notebook->set_current_page(page);
	return page;
}

void OutputEditorText::tabChanged() {
	if (m_spellHaveLang) {
		m_spell.detach();
		m_spell.attach(*textView());
	}
	OutputBuffer* buffer = textBuffer();
	ui.buttonUndo->set_sensitive(buffer->can_undo());
	ui.buttonRedo->set_sensitive(buffer->can_redo());
	setDrawWhitspace(ui.menuitemStripcrlfDrawwhitespace->get_active());
	Gsv::View* view = textView();
	view->grab_focus();
}

void OutputEditorText::closeTab(Gtk::Widget* pageWidget) {
	int page = -1;
	for (int i = 0, n = ui.notebook->get_n_pages(); i < n; ++i) {
		if (ui.notebook->get_nth_page(i) == pageWidget) {
			page = i;
			break;
		}
	}
	if (page == -1) {
		return;
	}
	if (textBuffer(page)->get_modified()) {
		int response = Utils::messageBox(Gtk::MESSAGE_QUESTION, _("Document not saved"), _("Save document before proceeding?"), "", Utils::Button::Save | Utils::Button::Discard | Utils::Button::Cancel);
		if (response == Utils::Button::Cancel) {
			return;
		}
		if (response == Utils::Button::Save && !save(page, textBuffer(page)->getFilename())) {
			return;
		}
	}
	ui.notebook->remove_page(page);
	if (ui.notebook->get_n_pages() == 0) {
		m_tabCounter = 0;
		addTab();
	}
}

Glib::ustring OutputEditorText::tabName(int page) const {
	Gtk::Label* label = static_cast<Gtk::Label*> (static_cast<Gtk::HBox*> (ui.notebook->get_tab_label(*ui.notebook->get_nth_page(page)))->get_children() [0]);
	return Utils::string_rstrip(label->get_text(), "*");
}

void OutputEditorText::setTabName(int page, const Glib::ustring& title) {
	Gtk::Label* label = static_cast<Gtk::Label*> (static_cast<Gtk::HBox*> (ui.notebook->get_tab_label(*ui.notebook->get_nth_page(page)))->get_children() [0]);
	label->set_text(title);
}

Gsv::View* OutputEditorText::textView(int page) const {
	page = page == -1 ? ui.notebook->get_current_page() : page;
	return static_cast<Gsv::View*> ((static_cast <Gtk::ScrolledWindow*> (ui.notebook->get_nth_page(page)))->get_child());
}

OutputBuffer* OutputEditorText::textBuffer(int page) const {
	page = page == -1 ? ui.notebook->get_current_page() : page;
	return static_cast<OutputBuffer*> (textView(page)->get_buffer().get());
}

void OutputEditorText::setFont() {
	if (ConfigSettings::get<SwitchSetting> ("systemoutputfont")->getValue()) {
		for (int i = 0, n = ui.notebook->get_n_pages(); i < n; ++i) {
			textView(i)->unset_font();
		}
	} else {
		Glib::ustring fontName = ConfigSettings::get<FontSetting> ("customoutputfont")->getValue();
		const Pango::FontDescription& desc = Pango::FontDescription(fontName);
		for (int i = 0, n = ui.notebook->get_n_pages(); i < n; ++i) {
			textView(i)->override_font(desc);
		}
	}
}

void OutputEditorText::scrollCursorIntoView() {
	Gsv::View* view = textView();
	view->scroll_to(view->get_buffer()->get_insert());
	view->grab_focus();
}

void OutputEditorText::setInsertMode(InsertMode mode, const std::string& iconName) {
	m_insertMode = mode;
	ui.imageInsert->set(Gdk::Pixbuf::create_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/%1", iconName)));
}

void OutputEditorText::setDrawWhitspace(bool enable) {
#if GTK_SOURCE_MAJOR_VERSION >= 4
	GtkSourceSpaceDrawer* space_drawer = gtk_source_view_get_space_drawer(textView()->gobj());
	gtk_source_space_drawer_set_types_for_locations(space_drawer, GTK_SOURCE_SPACE_LOCATION_ALL, GTK_SOURCE_SPACE_TYPE_ALL);
	gtk_source_space_drawer_set_enable_matrix(space_drawer, enable ? TRUE : FALSE);
#else
	textView()->set_draw_spaces(enable ? (Gsv::DRAW_SPACES_NEWLINE | Gsv::DRAW_SPACES_TAB | Gsv::DRAW_SPACES_SPACE) : Gsv::DrawSpacesFlags(0));
#endif
}

void OutputEditorText::filterBuffer() {
	OutputBuffer* buffer = textBuffer();

	Gtk::TextIter start, end;
	buffer->get_region_bounds(start, end);
	Glib::ustring txt = buffer->get_text(start, end);

	Utils::busyTask([this, &txt] {
		// Always remove trailing whitespace
		txt = Glib::Regex::create("\\s+$")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags> (0));
		if (ui.menuitemStripcrlfJoinhyphen->get_active()) {
			txt = Glib::Regex::create("[-\u2014]\\s*\\n\\s*")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags> (0));
		}
		Glib::ustring preChars, sucChars;
		if (ui.menuitemStripcrlfKeepparagraphs->get_active()) {
			preChars += "\\n"; // Keep if preceded by line break
		}
		if (ui.menuitemStripcrlfKeependmark->get_active()) {
			preChars += "\\.\\?!"; // Keep if preceded by end mark (.?!)
		}
		if (ui.menuitemStripcrlfKeepquote->get_active()) {
			preChars += "'\"\u00BB\u00AB"; // Keep if preceded by quote
			sucChars += "'\"\u00AB\u00BB"; // Keep if succeeded by quote
		}
		if (ui.menuitemStripcrlfKeepparagraphs->get_active()) {
			sucChars += "\\n"; // Keep if succeeded by line break
		}
		if (!preChars.empty()) {
			preChars = "([^" + preChars + "])";
		}
		if (!sucChars.empty()) {
			sucChars = "(?![" + sucChars + "])";
		}
		Glib::ustring expr = preChars + "\\n" + sucChars;
		txt = Glib::Regex::create(expr)->replace(txt, 0, preChars.empty() ? " " : "\\1 ", static_cast<Glib::RegexMatchFlags> (0));

		if (ui.menuitemStripcrlfJoinspace->get_active()) {
			txt = Glib::Regex::create("[ \t]+")->replace(txt, 0, " ", static_cast<Glib::RegexMatchFlags> (0));
		}
		return true;
	}, _("Stripping line breaks..."));

	start = end = buffer->insert(buffer->erase(start, end), txt);
	start.backward_chars(txt.size());
	buffer->select_range(start, end);
}

void OutputEditorText::completeTextViewMenu(Gtk::Menu* menu) {
	OutputBuffer* buffer = textBuffer();

	Gtk::Menu* highlightmenu = Gtk::manage(new Gtk::Menu);
	Gtk::RadioMenuItem* nolangitem = Gtk::manage(new Gtk::RadioMenuItem(_("No highlight")));
	CONNECT(nolangitem, toggled, [nolangitem, buffer] {
		if (nolangitem->get_active()) {
			buffer->setHightlightLanguage("");
		}
	});
	nolangitem->set_active(!buffer->get_highlight_syntax());
	highlightmenu->append(*nolangitem);
	highlightmenu->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	Glib::RefPtr<Gsv::LanguageManager> language_manager = Gsv::LanguageManager::get_default();
	for (const std::string& lang_id : language_manager->get_language_ids()) {
		Gtk::RadioMenuItem* langitem = Gtk::manage(new Gtk::RadioMenuItem(lang_id));
		CONNECT(langitem, toggled, [buffer, langitem, lang_id] {
			if (langitem->get_active()) {
				buffer->setHightlightLanguage(lang_id);
			}
		});
		langitem->set_active(buffer->get_highlight_syntax() && buffer->get_language() && (buffer->get_language()->get_id() == lang_id));
		highlightmenu->append(*langitem);
	}
	Gtk::MenuItem* highlightitem = Gtk::manage(new Gtk::MenuItem(_("Highlight mode")));
	highlightitem->set_submenu(*highlightmenu);
	menu->prepend(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	menu->prepend(*highlightitem);

	Gtk::CheckMenuItem* item = Gtk::manage(new Gtk::CheckMenuItem(_("Check spelling")));
	item->set_active(bool (GtkSpell::Checker::get_from_text_view(*textView())));
	CONNECT(item, toggled, [this, item] {
		if (item->get_active()) {
			m_spell.attach(*textView());
		} else {
			m_spell.detach();
		}
	});
	menu->prepend(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	menu->prepend(*item);

	menu->show_all();
}

void OutputEditorText::findReplace(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase, bool backwards, bool replace) {
	m_searchFrame->clearErrorState();
	if (!textBuffer()->findReplace(backwards, replace, matchCase, searchstr, replacestr, textView())) {
		m_searchFrame->setErrorState();
	}
}

void OutputEditorText::replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	int count(textBuffer()->replaceAll(searchstr, replacestr, matchCase));
	if (!count) {
		m_searchFrame->setErrorState();
	}
	MAIN->popState();
	Gtk::Popover* popover = Gtk::manage(new Gtk::Popover);
	popover->add(*Gtk::manage(new Gtk::Label(Glib::ustring::compose(_("%1 occurrences replaced"), count))));
	popover->set_relative_to(*m_searchFrame->replaceAllButton());
	popover->set_modal(false);
	popover->show_all();
	Glib::signal_timeout().connect_once([popover] { popover->popdown(); delete popover; }, 4000);
}

void OutputEditorText::applySubstitutions(const std::map<Glib::ustring, Glib::ustring>& substitutions, bool matchCase) {
	OutputBuffer* buffer = textBuffer();
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	Gtk::TextIter start, end;
	buffer->get_region_bounds(start, end);
	int startpos = start.get_offset();
	int endpos = end.get_offset();
	Gtk::TextSearchFlags flags = Gtk::TEXT_SEARCH_VISIBLE_ONLY | Gtk::TEXT_SEARCH_TEXT_ONLY;
	if (!matchCase) {
		flags |= Gtk::TEXT_SEARCH_CASE_INSENSITIVE;
	}
	for (auto sit = substitutions.begin(), sitEnd = substitutions.end(); sit != sitEnd; ++sit) {
		Glib::ustring search = sit->first;
		Glib::ustring replace = sit->second;
		int diff = replace.length() - search.length();
		Gtk::TextIter it = buffer->get_iter_at_offset(startpos);
		while (true) {
			Gtk::TextIter matchStart, matchEnd;
			if (!it.forward_search(search, flags, matchStart, matchEnd) || matchEnd.get_offset() > endpos) {
				break;
			}
			it = buffer->insert(buffer->erase(matchStart, matchEnd), replace);
			endpos += diff;
		}
		while (Gtk::Main::events_pending()) {
			Gtk::Main::iteration();
		}
	}
	buffer->select_range(buffer->get_iter_at_offset(startpos), buffer->get_iter_at_offset(endpos));
	MAIN->popState();
}

void OutputEditorText::read(tesseract::TessBaseAPI& tess, ReadSessionData* data) {
	char* textbuf = tess.GetUTF8Text();
	Glib::ustring text = Glib::ustring(textbuf);
	if (!text.empty() && *--text.end() != '\n') {
		text.append("\n");
	}
	if (data->prependFile || data->prependPage) {
		std::vector<Glib::ustring> prepend;
		if (data->prependFile) {
			prepend.push_back(Glib::ustring::compose(_("File: %1"), data->pageInfo.filename));
		}
		if (data->prependPage) {
			prepend.push_back(Glib::ustring::compose(_("Page: %1"), data->pageInfo.page));
		}
		text = Glib::ustring::compose("[%1]\n", Utils::string_join(prepend, "; ")) + text;
	}

	bool& insertText = static_cast<TextReadSessionData*> (data)->insertText;
	Utils::runInMainThreadBlocking([&] { addText(text, insertText); });
	delete[] textbuf;
	insertText = true;
}

void OutputEditorText::readError(const Glib::ustring& errorMsg, ReadSessionData* data) {
	bool& insertText = static_cast<TextReadSessionData*> (data)->insertText;
	Utils::runInMainThreadBlocking([&] { addText(Glib::ustring::compose(_("\n[Failed to recognize page %1]\n"), errorMsg), insertText); });
	insertText = true;
}

OutputEditorText::BatchProcessor* OutputEditorText::createBatchProcessor(const std::map<Glib::ustring, Glib::ustring>& options) const {
	return new TextBatchProcessor(Utils::get_default(options, Glib::ustring("prependPage"), Glib::ustring("0")) == "1");
}

void OutputEditorText::addText(const Glib::ustring& text, bool insert) {
	OutputBuffer* buffer = textBuffer();
	if (insert) {
		buffer->insert_at_cursor(text);
	} else {
		if (m_insertMode == InsertMode::Append) {
			buffer->place_cursor(buffer->insert(buffer->end(), text));
		} else if (m_insertMode == InsertMode::Cursor) {
			Gtk::TextIter start, end;
			buffer->get_region_bounds(start, end);
			buffer->place_cursor(buffer->insert(buffer->erase(start, end), text));
		} else if (m_insertMode == InsertMode::Replace) {
			buffer->place_cursor(buffer->insert(buffer->erase(buffer->begin(), buffer->end()), text));
		}
	}
	MAIN->setOutputPaneVisible(true);
}

bool OutputEditorText::open(const std::string& filename) {
	std::vector<Glib::RefPtr<Gio::File >> files;
	if (filename.empty()) {
		FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
		files = FileDialogs::open_dialog(_("Select Files"), "", "outputdir", filter, true);
		if (files.empty()) {
			return false;
		}
	}
	else {
		try {
			files.push_back(Gio::File::create_for_path(filename));
		} catch (const Glib::Error&) {
			Glib::ustring errorMsg = Glib::ustring::compose(_("The following files could not be opened:\n%1"), filename);
			Utils::messageBox(Gtk::MESSAGE_ERROR, _("Unable to open files"), errorMsg);
			return false;
		}
	}

	int currentPage = -1;
	std::vector<Glib::ustring> failed;
	for (const auto& file : files) {
		Glib::ustring contents;
		try {
			contents = Glib::file_get_contents(file->get_path());
		} catch (const Glib::Error&) {
			failed.push_back(file->get_path());
			continue;
		}
		Gtk::RecentManager::get_default()->add_item(file->get_uri());
		// Look if document already opened, if so, switch to that tab
		bool alreadyOpen = false;
		for (int i = 0, n = ui.notebook->get_n_pages(); i < n; ++i) {
			if (textBuffer(i)->getFilename() == file->get_path()) {
				currentPage = i;
				alreadyOpen = true;
				break;
			}
		}
		if (!alreadyOpen) {
			OutputBuffer* buffer = textBuffer();
			int page = ui.notebook->get_current_page();
			// Only add new tab if current tab is not empty
			if (buffer->get_modified() == true || !buffer->getFilename().empty()) {
				page = addTab(Glib::path_get_basename(file->get_path()));
				buffer = textBuffer(page);
			} else {
				setTabName(page, Glib::path_get_basename(file->get_path()));
			}
			buffer->begin_not_undoable_action();
			buffer->set_text(contents);
			buffer->end_not_undoable_action();
			buffer->set_modified(false);
			buffer->setFilename(file->get_path());
			currentPage = page;
		}
	}

	if (currentPage >= 0) {
		ui.notebook->set_current_page(currentPage);
	}
	if (!failed.empty()) {
		Glib::ustring filenames = Utils::string_join(failed, "\n");
		Glib::ustring errorMsg = Glib::ustring::compose(_("The following files could not be opened:\n%1"), filenames);
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Unable to open files"), errorMsg);
	}

	MAIN->setOutputPaneVisible(true);
	return failed.empty();
}

bool OutputEditorText::save(int page, const std::string& filename) {
	std::string outname = filename;
	page = page == -1 ? ui.notebook->get_current_page() : page;

	if (outname.empty()) {
		Glib::ustring suggestion = textBuffer(page)->getFilename();
		suggestion = suggestion.empty() ? tabName(page) + ".txt" : suggestion;
		FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
		outname = FileDialogs::save_dialog(_("Save Output..."), suggestion, "outputdir", filter);
		if (outname.empty()) {
			return false;
		}
	}
	std::ofstream file(outname);
	if (!file.is_open()) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	OutputBuffer* buffer = textBuffer(page);
	Glib::ustring txt = buffer->get_text(false);
	file.write(txt.data(), txt.bytes());
	buffer->set_modified(false);
	buffer->setFilename(outname);
	Gtk::RecentManager::get_default()->add_item(Gio::File::create_for_path(outname)->get_uri());
	setTabName(page, Glib::path_get_basename(outname));
	return true;
}

std::string OutputEditorText::crashSave(const std::string& filename) const {
	std::ofstream file(filename + ".txt");
	if (file.is_open()) {
		for (int i = 0, n = ui.notebook->get_n_pages(); i < n; ++i) {
			Glib::ustring txt = textBuffer(i)->get_text(false);
			file.write(txt.data(), txt.bytes());
			file.write("\n\n", 2);
		}
		return filename + ".txt";
	}
	return "";
}

bool OutputEditorText::clear(bool hide) {
	if (!ui.boxEditorText->get_visible()) {
		return true;
	}
	std::map<int, bool> changed;
	for (int i = 0, n = ui.notebook->get_n_pages(); i < n; ++i) {
		if (textBuffer(i)->get_modified()) {
			changed.insert(std::make_pair(i, true));
		}
	}

	if (!changed.empty()) {
		Gtk::ListBox listDocuments;
		for (auto it = changed.begin(), itEnd = changed.end(); it != itEnd; ++it) {
			Gtk::CheckButton* checkButton = Gtk::make_managed<Gtk::CheckButton> (tabName(it->first));
			checkButton->set_active(true);
			CONNECT(checkButton, toggled, [it, &changed, checkButton] { changed[it->first] = checkButton->get_active(); });
			listDocuments.append(*checkButton);
		}
		int response = Utils::messageBox(Gtk::MESSAGE_QUESTION, _("Unsaved documents"),
		                                 _("The following documents have unsaved changes. Which documents do you want to save before proceeding?"),
		                                 "",
		                                 Utils::Button::Save | Utils::Button::DiscardAll | Utils::Button::Cancel,
		                                 nullptr, &listDocuments);
		if (response == Utils::Button::Cancel) {
			return false;
		}
		if (response == Utils::Button::Save) {
			for (auto it = changed.begin(), itEnd = changed.end(); it != itEnd; ++it) {
				if (it->second && !save(it->first, textBuffer(it->first)->getFilename())) {
					return false;
				}
			}
		}
	}
	for (int i = ui.notebook->get_n_pages(); i >= 0; --i) {
		ui.notebook->remove_page(i);
	}
	m_tabCounter = 0;
	addTab(); // Add one empty tab
	if (hide) {
		MAIN->setOutputPaneVisible(false);
	}
	return true;
}

void OutputEditorText::onVisibilityChanged(bool /*visible*/) {
	m_searchFrame->hideSubstitutionsManager();
}

void OutputEditorText::setLanguage(const Config::Lang& lang) {
	try {
		m_spell.set_language(lang.code.empty() ? Utils::getSpellingLanguage(lang.prefix) : lang.code);
		m_spell.detach();
		m_spell.attach(*textView());
		m_spellHaveLang = true;
	} catch (const GtkSpell::Error& /*e*/) {
		m_spellHaveLang = false;
	}
}
