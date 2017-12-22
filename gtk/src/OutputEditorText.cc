/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.cc
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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

#include "FileDialogs.hh"
#include "OutputBuffer.hh"
#include "OutputEditorText.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "SearchReplaceFrame.hh"
#include "Utils.hh"

#include <fstream>


OutputEditorText::OutputEditorText()
	: m_builder("/org/gnome/gimagereader/editor_text.ui") {
	m_paneWidget = m_builder("box:output");
	m_insButton = m_builder("menubutton:output.insert");
	m_insImage = m_builder("image:output.insert");
	m_outputBox = m_builder("box:output");
	m_textView = m_builder("textview:output");
	m_filterKeepIfEndMark = m_builder("menuitem:output.stripcrlf.keependmark");
	m_filterKeepIfQuote = m_builder("menuitem:output.stripcrlf.keepquote");
	m_filterJoinHyphen = m_builder("menuitem:output.stripcrlf.joinhyphen");
	m_filterJoinSpace = m_builder("menuitem:output.stripcrlf.joinspace");
	m_filterKeepParagraphs = m_builder("menuitem:output.stripcrlf.keepparagraphs");
	m_searchFrame = new SearchReplaceFrame();
	m_builder("box:output.search").as<Gtk::Box>()->pack_start(*Gtk::manage(m_searchFrame->getWidget()));
	m_searchFrame->getWidget()->set_visible(false);
	m_textBuffer = OutputBuffer::create();
	m_textView->set_source_buffer(m_textBuffer);
	Gtk::Button* saveButton = m_builder("button:output.save");
	Gtk::ToggleButton* toggleSearchButton = m_builder("button:output.findreplace");
	Gtk::Button* undoButton = m_builder("button:output.undo");
	Gtk::Button* redoButton = m_builder("button:output.redo");

	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	undoButton->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	redoButton->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK|Gdk::SHIFT_MASK, Gtk::AccelFlags(0));
	toggleSearchButton->add_accelerator("clicked", group, GDK_KEY_F, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	saveButton->add_accelerator("clicked", group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));

#if GTKMM_CHECK_VERSION(3,12,0)
	m_builder("image:output.insert").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/ins_append.png"));
	m_builder("image:output.stripcrlf").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/stripcrlf.png"));
	m_builder("image:output.insert.append").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/ins_append.png"));
	m_builder("image:output.insert.cursor").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/ins_cursor.png"));
	m_builder("image:output.insert.replace").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/ins_replace.png"));
#else
	m_builder("image:output.insert").as<Gtk::Image>()->set(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/ins_append.png", 0)));
	m_builder("image:output.stripcrlf").as<Gtk::Image>()->set(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/stripcrlf.png", 0)));
	m_builder("image:output.insert.append").as<Gtk::Image>()->set(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/ins_append.png", 0)));
	m_builder("image:output.insert.cursor").as<Gtk::Image>()->set(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/ins_cursor.png", 0)));
	m_builder("image:output.insert.replace").as<Gtk::Image>()->set(Glib::wrap(gdk_pixbuf_new_from_resource("/org/gnome/gimagereader/ins_replace.png", 0)));
#endif

	m_insertMode = InsertMode::Append;

	m_spell.attach(*m_textView);
	m_spell.property_decode_language_codes() = true;

	CONNECT(m_builder("menuitem:output.insert.append").as<Gtk::MenuItem>(), activate, [this] { setInsertMode(InsertMode::Append, "ins_append.png"); });
	CONNECT(m_builder("menuitem:output.insert.cursor").as<Gtk::MenuItem>(), activate, [this] { setInsertMode(InsertMode::Cursor, "ins_cursor.png"); });
	CONNECT(m_builder("menuitem:output.insert.replace").as<Gtk::MenuItem>(), activate, [this] { setInsertMode(InsertMode::Replace, "ins_replace.png"); });
	CONNECT(m_builder("button:output.stripcrlf").as<Gtk::Button>(), clicked, [this] { filterBuffer(); });
	CONNECTS(toggleSearchButton, toggled, [this] (Gtk::ToggleButton* btn){ m_searchFrame->clear(); m_searchFrame->getWidget()->set_visible(btn->get_active()); });
	CONNECT(undoButton, clicked, [this] { m_textBuffer->undo(); scrollCursorIntoView(); });
	CONNECT(redoButton, clicked, [this] { m_textBuffer->redo(); scrollCursorIntoView(); });
	CONNECT(saveButton, clicked, [this] { save(); });
	CONNECT(m_builder("button:output.clear").as<Gtk::Button>(), clicked, [this] { clear(); });
	CONNECTP(m_textBuffer, can_undo, [this,undoButton] { undoButton->set_sensitive(m_textBuffer->can_undo()); });
	CONNECTP(m_textBuffer, can_redo, [this,redoButton] { redoButton->set_sensitive(m_textBuffer->can_redo()); });
	CONNECT(m_searchFrame, find_replace, sigc::mem_fun(this, &OutputEditorText::findReplace));
	CONNECT(m_searchFrame, replace_all, sigc::mem_fun(this, &OutputEditorText::replaceAll));
	CONNECT(m_searchFrame, apply_substitutions, sigc::mem_fun(this, &OutputEditorText::applySubstitutions));
	m_connectionCustomFont = CONNECTP(MAIN->getWidget("fontbutton:config.settings.customoutputfont").as<Gtk::FontButton>(), font_name, [this] { setFont(); });
	m_connectionDefaultFont = CONNECT(MAIN->getWidget("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>(), toggled, [this] { setFont(); });
	CONNECT(m_textView, populate_popup, [this](Gtk::Menu* menu) {
		completeTextViewMenu(menu);
	});
	CONNECTS(m_builder("menuitem:output.stripcrlf.drawwhitespace").as<Gtk::CheckMenuItem>(), toggled, [this](Gtk::CheckMenuItem* item) {
		m_textView->set_draw_spaces(item->get_active() ? (Gsv::DRAW_SPACES_NEWLINE|Gsv::DRAW_SPACES_TAB|Gsv::DRAW_SPACES_SPACE) : Gsv::DrawSpacesFlags(0));
	});

	// If the insert or selection mark change save the bounds either if the view is focused or the selection is non-empty
	CONNECTP(m_textBuffer, cursor_position, [this] { m_textBuffer->save_region_bounds(m_textView->is_focus()); });
	CONNECTP(m_textBuffer, has_selection, [this] { m_textBuffer->save_region_bounds(m_textView->is_focus()); });

	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("keepdot", m_builder("menuitem:output.stripcrlf.keependmark")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("keepquote", m_builder("menuitem:output.stripcrlf.keepquote")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("joinhyphen", m_builder("menuitem:output.stripcrlf.joinhyphen")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("joinspace", m_builder("menuitem:output.stripcrlf.joinspace")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("keepparagraphs", m_builder("menuitem:output.stripcrlf.keepparagraphs")));
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckMenuItem>("drawwhitespace", m_builder("menuitem:output.stripcrlf.drawwhitespace")));

	setFont();
}

OutputEditorText::~OutputEditorText() {
	delete m_searchFrame;
	m_connectionCustomFont.disconnect();
	m_connectionDefaultFont.disconnect();
	MAIN->getConfig()->removeSetting("keepdot");
	MAIN->getConfig()->removeSetting("keepquote");
	MAIN->getConfig()->removeSetting("joinhyphen");
	MAIN->getConfig()->removeSetting("joinspace");
	MAIN->getConfig()->removeSetting("keepparagraphs");
	MAIN->getConfig()->removeSetting("drawwhitespace");
}

void OutputEditorText::setFont() {
	if(MAIN->getWidget("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>()->get_active()) {
		m_builder("textview:output").as<Gtk::TextView>()->unset_font();
	} else {
		Gtk::FontButton* fontBtn = MAIN->getWidget("fontbutton:config.settings.customoutputfont");
		m_builder("textview:output").as<Gtk::TextView>()->override_font(Pango::FontDescription(fontBtn->get_font_name()));
	}
}

void OutputEditorText::scrollCursorIntoView() {
	m_textView->scroll_to(m_textView->get_buffer()->get_insert());
	m_textView->grab_focus();
}

void OutputEditorText::setInsertMode(InsertMode mode, const std::string& iconName) {
	m_insertMode = mode;
#if GTKMM_CHECK_VERSION(3,12,0)
	m_insImage->set(Gdk::Pixbuf::create_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/%1", iconName)));
#else
	m_insImage->set(Glib::wrap(gdk_pixbuf_new_from_resource(Glib::ustring::compose("/org/gnome/gimagereader/%1", iconName).c_str(), 0)));
#endif
}

void OutputEditorText::filterBuffer() {
	Gtk::TextIter start, end;
	m_textBuffer->get_region_bounds(start, end);
	Glib::ustring txt = m_textBuffer->get_text(start, end);

	Utils::busyTask([this,&txt] {
		// Always remove trailing whitespace
		txt = Glib::Regex::create("\\s+$")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags>(0));
		if(m_filterJoinHyphen->get_active()) {
			txt = Glib::Regex::create("[-\u2014]\\s*\\n\\s*")->replace(txt, 0, "", static_cast<Glib::RegexMatchFlags>(0));
		}
		Glib::ustring preChars, sucChars;
		if(m_filterKeepParagraphs->get_active()) {
			preChars += "\\n"; // Keep if preceded by line break
		}
		if(m_filterKeepIfEndMark->get_active()) {
			preChars += "\\.\\?!"; // Keep if preceded by end mark (.?!)
		}
		if(m_filterKeepIfQuote->get_active()) {
			preChars += "'\"\u00BB\u00AB"; // Keep if preceded by quote
			sucChars += "'\"\u00AB\u00BB"; // Keep if succeeded by quote
		}
		if(m_filterKeepParagraphs->get_active()) {
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

		if(m_filterJoinSpace->get_active()) {
			txt = Glib::Regex::create("[ \t]+")->replace(txt, 0, " ", static_cast<Glib::RegexMatchFlags>(0));
		}
		return true;
	}, _("Stripping line breaks..."));

	start = end = m_textBuffer->insert(m_textBuffer->erase(start, end), txt);
	start.backward_chars(txt.size());
	m_textBuffer->select_range(start, end);
}

void OutputEditorText::completeTextViewMenu(Gtk::Menu *menu) {
	Gtk::CheckMenuItem* item = Gtk::manage(new Gtk::CheckMenuItem(_("Check spelling")));
	item->set_active(bool(GtkSpell::Checker::get_from_text_view(*m_textView)));
	CONNECT(item, toggled, [this, item] {
		if(item->get_active()) {
			m_spell.attach(*m_textView);
		} else {
			m_spell.detach();
		}
	});
	menu->prepend(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	menu->prepend(*item);
	menu->show_all();
}

void OutputEditorText::findReplace(const Glib::ustring &searchstr, const Glib::ustring &replacestr, bool matchCase, bool backwards, bool replace) {
	m_searchFrame->clearErrorState();
	if(!m_textBuffer->findReplace(backwards, replace, matchCase, searchstr, replacestr, m_textView)) {
		m_searchFrame->setErrorState();
	}
}

void OutputEditorText::replaceAll(const Glib::ustring &searchstr, const Glib::ustring &replacestr, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	if(!m_textBuffer->replaceAll(searchstr, replacestr, matchCase)) {
		m_searchFrame->setErrorState();
	}
	MAIN->popState();
}

void OutputEditorText::applySubstitutions(const std::map<Glib::ustring,Glib::ustring>& substitutions, bool matchCase) {
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	Gtk::TextIter start, end;
	m_textBuffer->get_region_bounds(start, end);
	int startpos = start.get_offset();
	int endpos = end.get_offset();
	Gtk::TextSearchFlags flags = Gtk::TEXT_SEARCH_VISIBLE_ONLY|Gtk::TEXT_SEARCH_TEXT_ONLY;
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

void OutputEditorText::read(tesseract::TessBaseAPI &tess, ReadSessionData *data) {
	char* textbuf = tess.GetUTF8Text();
	Glib::ustring text = Glib::ustring(textbuf);
	if(!text.empty() && *--text.end() != '\n')
		text.append("\n");
	if(data->prependFile || data->prependPage) {
		std::vector<Glib::ustring> prepend;
		if(data->prependFile) {
			prepend.push_back(Glib::ustring::compose(_("File: %1"), data->file));
		}
		if(data->prependPage) {
			prepend.push_back(Glib::ustring::compose(_("Page: %1"), data->page));
		}
		text = Glib::ustring::compose("[%1]\n", Utils::string_join(prepend, "; ")) + text;
	}

	bool& insertText = static_cast<TextReadSessionData*>(data)->insertText;
	Utils::runInMainThreadBlocking([&] { addText(text, insertText); });
	delete[] textbuf;
	insertText = true;
}

void OutputEditorText::readError(const Glib::ustring &errorMsg, ReadSessionData *data) {
	bool& insertText = static_cast<TextReadSessionData*>(data)->insertText;
	Utils::runInMainThreadBlocking([&] { addText(Glib::ustring::compose(_("\n[Failed to recognize page %1]\n"), errorMsg), insertText); });
	insertText = true;
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

bool OutputEditorText::save(const std::string& filename) {
	std::string outname = filename;
	if(outname.empty()) {
		std::vector<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		std::string ext, base;
		std::string name = !sources.empty() ? sources.front()->displayname : _("output");
		Utils::get_filename_parts(name, base, ext);

		FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
		outname = FileDialogs::save_dialog(_("Save Output..."), base + ".txt", "outputdir", filter);
		if(outname.empty()) {
			return false;
		}
	}
	std::ofstream file(outname);
	if(!file.is_open()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	Glib::ustring txt = m_textBuffer->get_text(false);
	file.write(txt.data(), txt.bytes());
	m_textBuffer->set_modified(false);
	return true;
}

bool OutputEditorText::clear(bool hide) {
	if(!m_outputBox->get_visible()) {
		return true;
	}
	if(getModified()) {
		int response = Utils::question_dialog(_("Output not saved"), _("Save output before proceeding?"), Utils::Button::Save|Utils::Button::Discard|Utils::Button::Cancel);
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
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

bool OutputEditorText::getModified() const {
	return m_textBuffer->get_modified();
}

void OutputEditorText::onVisibilityChanged(bool /*visibile*/) {
	m_searchFrame->hideSubstitutionsManager();
}

void OutputEditorText::setLanguage(const Config::Lang& lang) {
	try {
		m_spell.set_language(lang.code.empty() ? Utils::getSpellingLanguage() : lang.code);
	} catch(const GtkSpell::Error& /*e*/) {}
}
