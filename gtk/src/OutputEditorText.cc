/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.cc
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

#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#undef USE_STD_NAMESPACE

#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "OutputBuffer.hh"
#include "OutputEditorText.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "SearchReplaceFrame.hh"
#include "Utils.hh"

#include <fstream>
#include <gtkmm/scrolledwindow.h>

void OutputEditorText::TextBatchProcessor::appendOutput(std::ostream& dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo, bool firstArea) const {
	char* text = tess->GetUTF8Text();
	if(firstArea && m_prependPage) {
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
	recentFilter->add_application("gImageReader");
	Gtk::RecentChooserMenu* recentChooser = Gtk::manage(new Gtk::RecentChooserMenu());
	recentChooser->set_filter(recentFilter);
	recentChooser->set_local_only(true);
	recentChooser->set_show_not_found(false);
	recentChooser->set_show_tips(true);
	recentChooser->set_sort_type(Gtk::RECENT_SORT_MRU);
	ui.menuButtonOpenRecent->set_menu(*recentChooser);

	addDocument();
	prepareCurView();

	CONNECT(ui.notebook, switch_page, [this](Gtk::Widget* page, guint page_number) { prepareCurView(); });

	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	ui.buttonUndo->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonRedo->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK | Gdk::SHIFT_MASK, Gtk::AccelFlags(0));
	ui.buttonFindreplace->add_accelerator("clicked", group, GDK_KEY_F, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonSave->add_accelerator("clicked", group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonOpen->add_accelerator("clicked", group, GDK_KEY_O, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));

	m_insertMode = InsertMode::Append;

	CONNECT(recentChooser, item_activated, [this, recentChooser] { open(Glib::filename_from_uri(recentChooser->get_current_uri())); });
	CONNECT(ui.menuitemInsertAppend, activate, [this] { setInsertMode(InsertMode::Append, "ins_append.png"); });
	CONNECT(ui.menuitemInsertCursor, activate, [this] { setInsertMode(InsertMode::Cursor, "ins_cursor.png"); });
	CONNECT(ui.menuitemInsertReplace, activate, [this] { setInsertMode(InsertMode::Replace, "ins_replace.png"); });
	CONNECT(ui.buttonStripcrlf, clicked, [this] { filterBuffer(); });
	CONNECT(ui.buttonFindreplace, toggled, [this] { m_searchFrame->clear(); m_searchFrame->getWidget()->set_visible(ui.buttonFindreplace->get_active()); });
	CONNECT(ui.buttonUndo, clicked, [this] { getBuffer()->undo(); scrollCursorIntoView(); });
	CONNECT(ui.buttonRedo, clicked, [this] { getBuffer()->redo(); scrollCursorIntoView(); });
	CONNECT(ui.buttonSave, clicked, [this] { save(); });
	CONNECT(ui.buttonOpen, clicked, [this] { open(); });
	CONNECT(ui.buttonAddTab, clicked, [this] { ui.notebook->set_current_page(ui.notebook->page_num(*addDocument())); });
	CONNECT(ui.buttonClear, clicked, [this] { clear(); });


	CONNECT(m_searchFrame, find_replace, sigc::mem_fun(this, &OutputEditorText::findReplace));
	CONNECT(m_searchFrame, replace_all, sigc::mem_fun(this, &OutputEditorText::replaceAll));
	CONNECT(m_searchFrame, apply_substitutions, sigc::mem_fun(this, &OutputEditorText::applySubstitutions));
	CONNECT(ConfigSettings::get<FontSetting>("customoutputfont"), changed, [this] { setFont(getView()); });
	CONNECT(ConfigSettings::get<EntrySetting>("highlightmode"), changed, [this] { activateHighlightMode(); });
	CONNECT(ConfigSettings::get<SwitchSetting>("systemoutputfont"), changed, [this] { setFont(getView()); });

#if GTK_SOURCE_MAJOR_VERSION >= 4
	CONNECT(ui.menuitemStripcrlfDrawwhitespace, toggled, [this] {
		GtkSourceSpaceDrawer* space_drawer = gtk_source_view_get_space_drawer(getView()->gobj());
		gtk_source_space_drawer_set_types_for_locations (space_drawer, GTK_SOURCE_SPACE_LOCATION_ALL, GTK_SOURCE_SPACE_TYPE_ALL);
		gtk_source_space_drawer_set_enable_matrix (space_drawer, ui.menuitemStripcrlfDrawwhitespace->get_active() ? TRUE : FALSE);
	});
#else
	CONNECT(ui.menuitemStripcrlfDrawwhitespace, toggled, [this] {
		getView()->set_draw_spaces(ui.menuitemStripcrlfDrawwhitespace->get_active() ? (Gsv::DRAW_SPACES_NEWLINE | Gsv::DRAW_SPACES_TAB | Gsv::DRAW_SPACES_SPACE) : Gsv::DrawSpacesFlags(0));
	});
#endif

	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("keepdot", ui.menuitemStripcrlfKeependmark));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("keepquote", ui.menuitemStripcrlfKeepquote));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("joinhyphen", ui.menuitemStripcrlfJoinhyphen));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("joinspace", ui.menuitemStripcrlfJoinspace));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("keepparagraphs", ui.menuitemStripcrlfKeepparagraphs));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("drawwhitespace", ui.menuitemStripcrlfDrawwhitespace));

}

OutputEditorText::~OutputEditorText() {
	delete m_searchFrame;
}

void OutputEditorText::prepareCurView() {
	m_spell.detach();
	m_spell.attach(*getView());
	getView()->grab_focus();

	MAIN->getHeaderBar()->set_title(getTabLabel());

	std::map<Gtk::Widget*, OutputSession>::iterator it(outputSession.find(getPage()));
	if (it != outputSession.end())
		MAIN->getHeaderBar()->set_subtitle(Glib::path_get_dirname(it->second.file));
	else
		MAIN->getHeaderBar()->set_subtitle("");

	ui.buttonUndo->set_sensitive(getBuffer()->can_undo());
	ui.buttonRedo->set_sensitive(getBuffer()->can_redo());

	CONNECTP(getBuffer(), can_undo, [this] { ui.buttonUndo->set_sensitive(getBuffer()->can_undo()); });
	CONNECTP(getBuffer(), can_redo, [this] { ui.buttonRedo->set_sensitive(getBuffer()->can_redo()); });

	// If the insert or selection mark change save the bounds either if the view is focused or the selection is non-empty
	CONNECTP(getBuffer(), cursor_position, [this] { getBuffer()->save_region_bounds(getView()->is_focus()); });
	CONNECTP(getBuffer(), has_selection, [this] { getBuffer()->save_region_bounds(getView()->is_focus()); });
}

Gsv::View* OutputEditorText::getView(Gtk::Widget* page) const {
	if (!page)
		return dynamic_cast <Gsv::View *>((dynamic_cast <Gtk::ScrolledWindow *>(getPage()))->get_child());
	else
		return dynamic_cast <Gsv::View *>((dynamic_cast <Gtk::ScrolledWindow *>(ui.notebook->get_nth_page(ui.notebook->page_num(*page))))->get_child());
}

Glib::RefPtr<OutputBuffer> OutputEditorText::getBuffer(Gtk::Widget* page) const {
	return Glib::RefPtr<OutputBuffer>::cast_dynamic(getView(page)->get_source_buffer());
}

Gtk::Widget* OutputEditorText::getPage(short int pageNum) const {
	if (pageNum = -1)
		return ui.notebook->get_nth_page(ui.notebook->get_current_page());
	else
		return ui.notebook->get_nth_page(pageNum);
}

Gtk::Widget* OutputEditorText::getPageByFilename(std::string filename) const {
	for (auto it = outputSession.begin(); it != outputSession.end(); ++it)
		if (it->second.file == filename) {
			return it->first;
		}
	return nullptr;
}

std::string OutputEditorText::getTabLabel(Gtk::Widget* page) {
	std:: string label;
	Gtk::Widget* p = page;

	// if page == nullptr use current page
	if (!page)
		p = getPage();

	label = (dynamic_cast<Gtk::Label *>((dynamic_cast<Gtk::HBox *>(ui.notebook->get_tab_label(*p)))->get_children()[0])->get_text());

	// ignore buffer modification indicator
	if ((label[0] == ' ') || (label[0] == '*'))
		return label.substr(1, label.size() - 1);
	else
		return label;
}

void OutputEditorText::setTabLabel(Gtk::Widget* page, std::string tabLabel) {

	std::string label(tabLabel);
	Gtk::Widget* p = page;

	// if page == nullptr use current page
	if (!page)
		p = getPage();

	// reuse current label if nothing was provided
	if (tabLabel == "")
		label = getTabLabel(p);

	// prepend space to be used as place holder for buffer modification indicator `*` if not done yet
	if ((label[0] != '*') && (label[0] != ' '))
		label = " " + label;

	if (getBuffer(p)->get_modified()) {
		label[0] = '*';
	} else {
		label[0] = ' ';
	}

	MAIN->getHeaderBar()->set_title(label);
	std::map<Gtk::Widget*, OutputSession>::iterator it;
	it = outputSession.find(p);
	if (it != outputSession.end())
		MAIN->getHeaderBar()->set_subtitle(Glib::path_get_dirname(it->second.file));
	else
		MAIN->getHeaderBar()->set_subtitle("");

	dynamic_cast<Gtk::Label *>((dynamic_cast<Gtk::HBox *>(ui.notebook->get_tab_label(*p)))->get_children()[0])->set_text(label);
	dynamic_cast<Gtk::Label *>((dynamic_cast<Gtk::HBox *>(ui.notebook->get_tab_label(*p)))->get_children()[0])->show();
}

void OutputEditorText::setFont(Gsv::View *view) {
	if(ConfigSettings::get<SwitchSetting>("systemoutputfont")->getValue()) {
		view->unset_font();
	} else {
		Glib::ustring fontName = ConfigSettings::get<FontSetting>("customoutputfont")->getValue();
		view->override_font(Pango::FontDescription(fontName));
	}
}

void OutputEditorText::scrollCursorIntoView() {
	getView()->scroll_to(getView()->get_buffer()->get_insert());
	getView()->grab_focus();
}

void OutputEditorText::setInsertMode(InsertMode mode, const std::string& iconName) {
	m_insertMode = mode;
	ui.imageInsert->set(Gdk::Pixbuf::create_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/%1", iconName)));
}

void OutputEditorText::filterBuffer() {
	Gtk::TextIter start, end;
	getBuffer()->get_region_bounds(start, end);
	Glib::ustring txt = getBuffer()->get_text(start, end);

	Utils::busyTask([this, &txt] {
		// Always remove trailing whitespace
		txt = Glib::Regex::create("\\s+$")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags>(0));
		if(ui.menuitemStripcrlfJoinhyphen->get_active()) {
			txt = Glib::Regex::create("[-\u2014]\\s*\\n\\s*")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags>(0));
		}
		Glib::ustring preChars, sucChars;
		if(ui.menuitemStripcrlfKeepparagraphs->get_active()) {
			preChars += "\\n"; // Keep if preceded by line break
		}
		if(ui.menuitemStripcrlfKeependmark->get_active()) {
			preChars += "\\.\\?!"; // Keep if preceded by end mark (.?!)
		}
		if(ui.menuitemStripcrlfKeepquote->get_active()) {
			preChars += "'\"\u00BB\u00AB"; // Keep if preceded by quote
			sucChars += "'\"\u00AB\u00BB"; // Keep if succeeded by quote
		}
		if(ui.menuitemStripcrlfKeepparagraphs->get_active()) {
			sucChars += "\\n"; // Keep if succeeded by line break
		}
		if(!preChars.empty()) {
			preChars = "([^" + preChars + "])";
		}
		if(!sucChars.empty()) {
			sucChars = "(?![" + sucChars + "])";
		}
		Glib::ustring expr = preChars + "\\n" + sucChars;
		txt = Glib::Regex::create(expr)->replace(txt, 0, preChars.empty() ? " " : "\\1 ", static_cast<Glib::RegexMatchFlags>(0));

		if(ui.menuitemStripcrlfJoinspace->get_active()) {
			txt = Glib::Regex::create("[ \t]+")->replace(txt, 0, " ", static_cast<Glib::RegexMatchFlags>(0));
		}
		return true;
	}, _("Stripping line breaks..."));

	start = end = getBuffer()->insert(getBuffer()->erase(start, end), txt);
	start.backward_chars(txt.size());
	getBuffer()->select_range(start, end);
}

void OutputEditorText::completeTextViewMenu(Gtk::Menu* menu) {
	Gtk::CheckMenuItem* item = Gtk::manage(new Gtk::CheckMenuItem(_("Check spelling")));
	item->set_active(bool(GtkSpell::Checker::get_from_text_view(*getView())));
	CONNECT(item, toggled, [this, item] {
		if(item->get_active()) {
			m_spell.attach(*getView());
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
	if(!getBuffer()->findReplace(backwards, replace, matchCase, searchstr, replacestr, getView())) {
		m_searchFrame->setErrorState();
	}
}

void OutputEditorText::replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	unsigned int count(getBuffer()->replaceAll(searchstr, replacestr, matchCase));
	if(!count) {
		m_searchFrame->setErrorState();
	}
	MAIN->popState();
	MAIN->pushState(MainWindow::State::Idle, Glib::ustring::compose(_("Amount of occurrences replaced: %1"), count));
}

void OutputEditorText::applySubstitutions(const std::map<Glib::ustring, Glib::ustring>& substitutions, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	Gtk::TextIter start, end;
	getBuffer()->get_region_bounds(start, end);
	int startpos = start.get_offset();
	int endpos = end.get_offset();
	Gtk::TextSearchFlags flags = Gtk::TEXT_SEARCH_VISIBLE_ONLY | Gtk::TEXT_SEARCH_TEXT_ONLY;
	if(!matchCase) {
		flags |= Gtk::TEXT_SEARCH_CASE_INSENSITIVE;
	}
	for(auto sit = substitutions.begin(), sitEnd = substitutions.end(); sit != sitEnd; ++sit) {
		Glib::ustring search = sit->first;
		Glib::ustring replace = sit->second;
		int diff = replace.length() - search.length();
		Gtk::TextIter it = getBuffer()->get_iter_at_offset(startpos);
		while(true) {
			Gtk::TextIter matchStart, matchEnd;
			if(!it.forward_search(search, flags, matchStart, matchEnd) || matchEnd.get_offset() > endpos) {
				break;
			}
			it = getBuffer()->insert(getBuffer()->erase(matchStart, matchEnd), replace);
			endpos += diff;
		}
		while(Gtk::Main::events_pending()) {
			Gtk::Main::iteration();
		}
	}
	getBuffer()->select_range(getBuffer()->get_iter_at_offset(startpos), getBuffer()->get_iter_at_offset(endpos));
	MAIN->popState();
}

void OutputEditorText::read(tesseract::TessBaseAPI& tess, ReadSessionData* data) {
	char* textbuf = tess.GetUTF8Text();
	Glib::ustring text = Glib::ustring(textbuf);
	if(!text.empty() && *--text.end() != '\n') {
		text.append("\n");
	}
	if(data->prependFile || data->prependPage) {
		std::vector<Glib::ustring> prepend;
		if(data->prependFile) {
			prepend.push_back(Glib::ustring::compose(_("File: %1"), data->pageInfo.filename));
		}
		if(data->prependPage) {
			prepend.push_back(Glib::ustring::compose(_("Page: %1"), data->pageInfo.page));
		}
		text = Glib::ustring::compose("[%1]\n", Utils::string_join(prepend, "; ")) + text;
	}

	bool& insertText = static_cast<TextReadSessionData*>(data)->insertText;
	Utils::runInMainThreadBlocking([&] { addText(text, insertText); });
	delete[] textbuf;
	insertText = true;
}

void OutputEditorText::readError(const Glib::ustring& errorMsg, ReadSessionData* data) {
	bool& insertText = static_cast<TextReadSessionData*>(data)->insertText;
	Utils::runInMainThreadBlocking([&] { addText(Glib::ustring::compose(_("\n[Failed to recognize page %1]\n"), errorMsg), insertText); });
	insertText = true;
}

OutputEditorText::BatchProcessor* OutputEditorText::createBatchProcessor(const std::map<Glib::ustring, Glib::ustring>& options) const {
	return new TextBatchProcessor(Utils::get_default(options, Glib::ustring("prependPage"), Glib::ustring("0")) == "1");
}

void OutputEditorText::addText(const Glib::ustring& text, bool insert) {
	if(insert) {
		getBuffer()->insert_at_cursor(text);
	} else {
		if(m_insertMode == InsertMode::Append) {
			getBuffer()->place_cursor(getBuffer()->insert(getBuffer()->end(), text));
		} else if(m_insertMode == InsertMode::Cursor) {
			Gtk::TextIter start, end;
			getBuffer()->get_region_bounds(start, end);
			getBuffer()->place_cursor(getBuffer()->insert(getBuffer()->erase(start, end), text));
		} else if(m_insertMode == InsertMode::Replace) {
			getBuffer()->place_cursor(getBuffer()->insert(getBuffer()->erase(getBuffer()->begin(), getBuffer()->end()), text));
		}
	}
	MAIN->setOutputPaneVisible(true);
}

void OutputEditorText::activateHighlightMode() {
	Glib::RefPtr<Gsv::LanguageManager> language_manager = Gsv::LanguageManager::get_default();
	Glib::RefPtr<Gsv::Language> language = language_manager->get_language(MAIN->getConfig()->highlightMode());
	getBuffer()->set_language(language);
}

bool OutputEditorText::open(const std::string& file) {
	try {
		std::string inputname(file);
		std::vector<Glib::RefPtr<Gio::File> > files;
		if (file.empty()) {
			FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
			files = FileDialogs::open_dialog(_("Select Files"), "", "outputdir", filter, true);
			if (!files.empty()) {
				inputname = files[0]->get_path();
			}
		}

		if (getPageByFilename(inputname)) {
			ui.notebook->set_current_page(ui.notebook->page_num(*getPageByFilename(inputname)));
			return true;
		}

		if (hasSession() || (getModified())) {
			ui.notebook->set_current_page(ui.notebook->page_num(*addDocument(inputname)));
		}

		getBuffer()->begin_not_undoable_action();
		getBuffer()->set_text(Glib::file_get_contents(inputname));
		getBuffer()->end_not_undoable_action();
		getBuffer()->set_modified(false);
		outputSession[getPage()].file = inputname;
		setTabLabel(getPage(), Glib::path_get_basename(inputname));

		if (files.size() > 1) {
			for (int i = 1; i < files.size(); ++i)
				addDocument(files[i]->get_path());
		}

		MAIN->setOutputPaneVisible(true);
		return true;
	} catch(const Glib::Error&) {
		Glib::ustring errorMsg = Glib::ustring::compose(_("The following files could not be opened:\n%1"), file);
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Unable to open files"), errorMsg);
		return false;
	}
}

void OutputEditorText::on_close_button_clicked(Gtk::Widget* page) {
	if (clear(false, page))
		ui.notebook->remove_page(*page);
	if (ui.notebook->get_n_pages() < 2) ui.notebook->set_show_tabs(false);
}

void OutputEditorText::on_buffer_modified_changed(Gtk::Widget* page) {
	setTabLabel(page);
}

Gtk::Widget* OutputEditorText::tabWidget(std::string tabLabel, Gtk::Widget* page) {
	Gtk::Button* button = Gtk::make_managed<Gtk::Button>();
	button->set_image_from_icon_name("window-close-symbolic", (Gtk::BuiltinIconSize) GTK_ICON_SIZE_MENU);
	button->set_tooltip_text (_("Close document"));
	button->set_relief((Gtk::ReliefStyle) GTK_RELIEF_NONE);
	button->set_focus_on_click(false);

	button->signal_clicked().connect( sigc::bind<Gtk::Widget*>( sigc::mem_fun(*this, &OutputEditorText::on_close_button_clicked), page) );

	Gtk::Label* label = Gtk::make_managed<Gtk::Label>(tabLabel);
	label->set_markup(tabLabel);
	label->set_use_underline(false);

	Gtk::HBox* hbox = Gtk::make_managed<Gtk::HBox>(false, 0);
	hbox->set_spacing(0);
	hbox->pack_start(*label);
	hbox->pack_end(*button, false, false, 0);
	hbox->show_all();

	return hbox;
}

Gtk::Widget* OutputEditorText::addDocument(const std::string& file) {
	// if the file is already opened somewhere - return that page
	if (getPageByFilename(file))
		return getPageByFilename(file);

	Glib::RefPtr<OutputBuffer> textBuffer;
	textBuffer = OutputBuffer::create();

	Gsv::View* textView = Gtk::make_managed<Gsv::View>(textBuffer);
	setFont(textView);

	Gtk::ScrolledWindow* scrWindow = Gtk::make_managed<Gtk::ScrolledWindow>();
	scrWindow->add(*textView);

	CONNECT(textView, populate_popup, [this](Gtk::Menu * menu) {
		completeTextViewMenu(menu);
	});

	textView->signal_focus_in_event().connect( sigc::bind<Gtk::Widget*>( sigc::mem_fun(*this, &OutputEditorText::view_focused_in), scrWindow) );

	std::string tabLabel;
	if (file == "")
		tabLabel = Glib::ustring::compose(_("New document %1"), std::to_string(ui.notebook->get_n_pages() + 1));
	else {
		try {
			textBuffer->begin_not_undoable_action();
			textBuffer->set_text(Glib::file_get_contents(file));
			textBuffer->end_not_undoable_action();
			textBuffer->set_modified(false);
		} catch(const Glib::Error&) {
			Glib::ustring errorMsg = Glib::ustring::compose(_("The following files could not be opened:\n%1"), file);
			Utils::messageBox(Gtk::MESSAGE_ERROR, _("Unable to open files"), errorMsg);
			return nullptr;
		}
		outputSession[scrWindow].file = file;
		tabLabel = Glib::path_get_basename(file);
	}

	ui.notebook->append_page(*scrWindow, *tabWidget(tabLabel, scrWindow));

	textBuffer->signal_modified_changed().connect( sigc::bind<Gtk::Widget*>( sigc::mem_fun(*this, &OutputEditorText::on_buffer_modified_changed), scrWindow) );

	if (ui.notebook->get_n_pages() > 1)
		ui.notebook->set_show_tabs(true);
	ui.notebook->show_all();

	return scrWindow;
}

bool OutputEditorText::save(const std::string& filename, Gtk::Widget* page) {
	std::string outname = filename;

	if(outname.empty()) {
		std::map<Gtk::Widget*, OutputSession>::iterator it(outputSession.find(page == nullptr ? getPage() : page));
		if (it != outputSession.end()) {
			outname = it->second.file;
		} else {
			std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
			std::string suggestion;
			if(!sources.empty()) {
				suggestion = Utils::split_filename(sources.front()->file->get_path()).first;
			} else {
				suggestion = getTabLabel(page);
			}

			FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
			outname = FileDialogs::save_dialog(_("Save Output..."), suggestion, "outputdir", filter);
			if(outname.empty()) {
				return false;
			}
		}
	}
	std::ofstream file(outname);
	if(!file.is_open()) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	Glib::ustring txt = getBuffer(page)->get_text(false);
	file.write(txt.data(), txt.bytes());
	getBuffer(page)->set_modified(false);
	outputSession[page == nullptr ? getPage() : page].file = outname;
	setTabLabel(page, Glib::path_get_basename(outname));
	return true;
}

bool OutputEditorText::clear(bool hide, Gtk::Widget* page) {
	if(!ui.boxEditorText->get_visible()) {
		return true;
	}
	if(getModified(page)) {
		int response = Utils::messageBox(Gtk::MESSAGE_QUESTION, _("Output not saved"), _("Save output before proceeding?"), "", Utils::Button::Save | Utils::Button::Discard | Utils::Button::Cancel);
		if(response == Utils::Button::Save) {
			if(!save("", page)) {
				return false;
			}
		} else if(response != Utils::Button::Discard) {
			return false;
		}
	}
	if (!hasSession(page)) getBuffer(page)->begin_not_undoable_action();
	getBuffer(page)->set_text("");
	if (!hasSession(page)) {
		getBuffer(page)->end_not_undoable_action();
		getBuffer(page)->set_modified(false);
	}
	getView(page)->grab_focus();
	return true;
}

bool OutputEditorText::getModified(Gtk::Widget* page) const {
	return getBuffer(page)->get_modified();
}

void OutputEditorText::onVisibilityChanged(bool /*visible*/) {
	m_searchFrame->hideSubstitutionsManager();
}

void OutputEditorText::setLanguage(const Config::Lang& lang) {
	try {
		m_spell.set_language(lang.code.empty() ? Utils::getSpellingLanguage(lang.prefix) : lang.code);
	} catch(const GtkSpell::Error& /*e*/) {}
}

bool OutputEditorText::hasSession(Gtk::Widget* page) {
	std::map<Gtk::Widget*, OutputSession>::iterator it;
	it = outputSession.find(page == nullptr ? getPage() : page);
	return it != outputSession.end();
}

bool OutputEditorText::view_focused_in(GdkEventFocus* focus_event, Gtk::Widget* page) {

/* Todo: start using GtkSourceFile in OutputSession.file

	GtkSourceFile *file;

//  obtain *file from session

	gtk_source_file_check_file_on_disk (file);

	if (gtk_source_file_is_externally_modified (file))
	{
//		display_externally_modified_notification (page);
	}

*/
	return true;
}
