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
	m_textBuffer = OutputBuffer::create();
	ui.textview->set_source_buffer(m_textBuffer);

	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	ui.buttonUndo->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonRedo->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK | Gdk::SHIFT_MASK, Gtk::AccelFlags(0));
	ui.buttonFindreplace->add_accelerator("clicked", group, GDK_KEY_F, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonSave->add_accelerator("clicked", group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	ui.buttonOpen->add_accelerator("clicked", group, GDK_KEY_O, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));

	m_insertMode = InsertMode::Append;

	m_spell.attach(*ui.textview);
	m_spell.property_decode_language_codes() = true;

	CONNECT(ui.menuitemInsertAppend, activate, [this] { setInsertMode(InsertMode::Append, "ins_append.png"); });
	CONNECT(ui.menuitemInsertCursor, activate, [this] { setInsertMode(InsertMode::Cursor, "ins_cursor.png"); });
	CONNECT(ui.menuitemInsertReplace, activate, [this] { setInsertMode(InsertMode::Replace, "ins_replace.png"); });
	CONNECT(ui.buttonStripcrlf, clicked, [this] { filterBuffer(); });
	CONNECT(ui.buttonFindreplace, toggled, [this] { m_searchFrame->clear(); m_searchFrame->getWidget()->set_visible(ui.buttonFindreplace->get_active()); });
	CONNECT(ui.buttonUndo, clicked, [this] { m_textBuffer->undo(); scrollCursorIntoView(); });
	CONNECT(ui.buttonRedo, clicked, [this] { m_textBuffer->redo(); scrollCursorIntoView(); });
	CONNECT(ui.buttonSave, clicked, [this] { save(); });
	CONNECT(ui.buttonOpen, clicked, [this] { open(); });
	CONNECT(ui.buttonClear, clicked, [this] { clear(); });
	CONNECTP(m_textBuffer, can_undo, [this] { ui.buttonUndo->set_sensitive(m_textBuffer->can_undo()); });
	CONNECTP(m_textBuffer, can_redo, [this] { ui.buttonRedo->set_sensitive(m_textBuffer->can_redo()); });
	CONNECT(m_searchFrame, find_replace, sigc::mem_fun(this, &OutputEditorText::findReplace));
	CONNECT(m_searchFrame, replace_all, sigc::mem_fun(this, &OutputEditorText::replaceAll));
	CONNECT(m_searchFrame, apply_substitutions, sigc::mem_fun(this, &OutputEditorText::applySubstitutions));
	CONNECT(ConfigSettings::get<FontSetting>("customoutputfont"), changed, [this] { setFont(); });
	CONNECT(ConfigSettings::get<EntrySetting>("highlightmode"), changed, [this] { activateHighlightMode(); });
	CONNECT(ConfigSettings::get<SwitchSetting>("systemoutputfont"), changed, [this] { setFont(); });
	CONNECT(ui.textview, populate_popup, [this](Gtk::Menu * menu) {
		completeTextViewMenu(menu);
	});
#if GTK_SOURCE_MAJOR_VERSION >= 4
	CONNECT(ui.menuitemStripcrlfDrawwhitespace, toggled, [this] {
		GtkSourceSpaceDrawer* space_drawer = gtk_source_view_get_space_drawer(ui.textview->gobj());
		gtk_source_space_drawer_set_types_for_locations (space_drawer, GTK_SOURCE_SPACE_LOCATION_ALL, GTK_SOURCE_SPACE_TYPE_ALL);
		gtk_source_space_drawer_set_enable_matrix (space_drawer, ui.menuitemStripcrlfDrawwhitespace->get_active() ? TRUE : FALSE);
	});
#else
	CONNECT(ui.menuitemStripcrlfDrawwhitespace, toggled, [this] {
		ui.textview->set_draw_spaces(ui.menuitemStripcrlfDrawwhitespace->get_active() ? (Gsv::DRAW_SPACES_NEWLINE | Gsv::DRAW_SPACES_TAB | Gsv::DRAW_SPACES_SPACE) : Gsv::DrawSpacesFlags(0));
	});
#endif

	// If the insert or selection mark change save the bounds either if the view is focused or the selection is non-empty
	CONNECTP(m_textBuffer, cursor_position, [this] { m_textBuffer->save_region_bounds(ui.textview->is_focus()); });
	CONNECTP(m_textBuffer, has_selection, [this] { m_textBuffer->save_region_bounds(ui.textview->is_focus()); });

	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("keepdot", ui.menuitemStripcrlfKeependmark));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("keepquote", ui.menuitemStripcrlfKeepquote));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("joinhyphen", ui.menuitemStripcrlfJoinhyphen));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("joinspace", ui.menuitemStripcrlfJoinspace));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("keepparagraphs", ui.menuitemStripcrlfKeepparagraphs));
	ADD_SETTING(SwitchSettingT<Gtk::CheckMenuItem>("drawwhitespace", ui.menuitemStripcrlfDrawwhitespace));

	setFont();
}

OutputEditorText::~OutputEditorText() {
	delete m_searchFrame;
}

void OutputEditorText::setFont() {
	if(ConfigSettings::get<SwitchSetting>("systemoutputfont")->getValue()) {
		ui.textview->unset_font();
	} else {
		Glib::ustring fontName = ConfigSettings::get<FontSetting>("customoutputfont")->getValue();
		ui.textview->override_font(Pango::FontDescription(fontName));
	}
}

void OutputEditorText::scrollCursorIntoView() {
	ui.textview->scroll_to(ui.textview->get_buffer()->get_insert());
	ui.textview->grab_focus();
}

void OutputEditorText::setInsertMode(InsertMode mode, const std::string& iconName) {
	m_insertMode = mode;
	ui.imageInsert->set(Gdk::Pixbuf::create_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/%1", iconName)));
}

void OutputEditorText::filterBuffer() {
	Gtk::TextIter start, end;
	m_textBuffer->get_region_bounds(start, end);
	Glib::ustring txt = m_textBuffer->get_text(start, end);

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

	start = end = m_textBuffer->insert(m_textBuffer->erase(start, end), txt);
	start.backward_chars(txt.size());
	m_textBuffer->select_range(start, end);
}

void OutputEditorText::completeTextViewMenu(Gtk::Menu* menu) {
	Gtk::CheckMenuItem* item = Gtk::manage(new Gtk::CheckMenuItem(_("Check spelling")));
	item->set_active(bool(GtkSpell::Checker::get_from_text_view(*ui.textview)));
	CONNECT(item, toggled, [this, item] {
		if(item->get_active()) {
			m_spell.attach(*ui.textview);
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
	if(!m_textBuffer->findReplace(backwards, replace, matchCase, searchstr, replacestr, ui.textview)) {
		m_searchFrame->setErrorState();
	}
}

void OutputEditorText::replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	if(!m_textBuffer->replaceAll(searchstr, replacestr, matchCase)) {
		m_searchFrame->setErrorState();
	}
	MAIN->popState();
}

void OutputEditorText::applySubstitutions(const std::map<Glib::ustring, Glib::ustring>& substitutions, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	Gtk::TextIter start, end;
	m_textBuffer->get_region_bounds(start, end);
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
		Gtk::TextIter it = m_textBuffer->get_iter_at_offset(startpos);
		while(true) {
			Gtk::TextIter matchStart, matchEnd;
			if(!it.forward_search(search, flags, matchStart, matchEnd) || matchEnd.get_offset() > endpos) {
				break;
			}
			it = m_textBuffer->insert(m_textBuffer->erase(matchStart, matchEnd), replace);
			endpos += diff;
		}
		while(Gtk::Main::events_pending()) {
			Gtk::Main::iteration();
		}
	}
	m_textBuffer->select_range(m_textBuffer->get_iter_at_offset(startpos), m_textBuffer->get_iter_at_offset(endpos));
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
		m_textBuffer->insert_at_cursor(text);
	} else {
		if(m_insertMode == InsertMode::Append) {
			m_textBuffer->place_cursor(m_textBuffer->insert(m_textBuffer->end(), text));
		} else if(m_insertMode == InsertMode::Cursor) {
			Gtk::TextIter start, end;
			m_textBuffer->get_region_bounds(start, end);
			m_textBuffer->place_cursor(m_textBuffer->insert(m_textBuffer->erase(start, end), text));
		} else if(m_insertMode == InsertMode::Replace) {
			m_textBuffer->place_cursor(m_textBuffer->insert(m_textBuffer->erase(m_textBuffer->begin(), m_textBuffer->end()), text));
		}
	}
	MAIN->setOutputPaneVisible(true);
}

void OutputEditorText::activateHighlightMode() {
	Glib::RefPtr<Gsv::LanguageManager> language_manager = Gsv::LanguageManager::get_default();
	Glib::RefPtr<Gsv::Language> language = language_manager->get_language(MAIN->getConfig()->highlightMode());
	m_textBuffer->set_language(language);
}

bool OutputEditorText::open(const std::string& file) {
	if(!clear(false)) {
		return false;
	}
	try {
		if (file.empty()) {
			FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
			std::vector<Glib::RefPtr<Gio::File> > files = FileDialogs::open_dialog(_("Select Files"), "", "outputdir", filter, true);
			// TODO: for now open only the first file. Once multi-tab UI is implemented - open each file in a separate tab.
			m_textBuffer->set_text(Glib::file_get_contents(files[0]->get_path()));
		} else
			m_textBuffer->set_text(Glib::file_get_contents(file));
		MAIN->setOutputPaneVisible(true);
		return true;
	} catch(const Glib::Error&) {
		Glib::ustring errorMsg = Glib::ustring::compose(_("The following files could not be opened:\n%1"), file);
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Unable to open files"), errorMsg);
		return false;
	}
}

bool OutputEditorText::save(const std::string& filename) {
	std::string outname = filename;
	if(outname.empty()) {
		std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		std::string suggestion = !sources.empty() ? Utils::split_filename(sources.front()->displayname).first : _("output");

		FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
		outname = FileDialogs::save_dialog(_("Save Output..."), suggestion + ".txt", "outputdir", filter);
		if(outname.empty()) {
			return false;
		}
	}
	std::ofstream file(outname);
	if(!file.is_open()) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	Glib::ustring txt = m_textBuffer->get_text(false);
	file.write(txt.data(), txt.bytes());
	m_textBuffer->set_modified(false);
	return true;
}

bool OutputEditorText::clear(bool hide) {
	if(!ui.boxEditorText->get_visible()) {
		return true;
	}
	if(getModified()) {
		int response = Utils::messageBox(Gtk::MESSAGE_QUESTION, _("Output not saved"), _("Save output before proceeding?"), "", Utils::Button::Save | Utils::Button::Discard | Utils::Button::Cancel);
		if(response == Utils::Button::Save) {
			if(!save()) {
				return false;
			}
		} else if(response != Utils::Button::Discard) {
			return false;
		}
	}
	m_textBuffer->begin_not_undoable_action();
	m_textBuffer->set_text("");
	m_textBuffer->end_not_undoable_action();
	m_textBuffer->set_modified(false);
	if(hide) {
		MAIN->setOutputPaneVisible(false);
	}
	return true;
}

bool OutputEditorText::getModified() const {
	return m_textBuffer->get_modified();
}

void OutputEditorText::onVisibilityChanged(bool /*visible*/) {
	m_searchFrame->hideSubstitutionsManager();
}

void OutputEditorText::setLanguage(const Config::Lang& lang) {
	try {
		m_spell.set_language(lang.code.empty() ? Utils::getSpellingLanguage() : lang.code);
	} catch(const GtkSpell::Error& /*e*/) {}
}
