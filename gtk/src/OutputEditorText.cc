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

#include <tesseract/baseapi.h>

#include "FileDialogs.hh"
#include "OutputBuffer.hh"
#include "OutputEditorText.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "SubstitutionsManager.hh"
#include "Utils.hh"

#include <fstream>


OutputEditorText::OutputEditorText()
	: m_builder("/org/gnome/gimagereader/editor_text.ui") {
	m_paneWidget = m_builder("box:output");
	m_insButton = m_builder("menubutton:output.insert");
	m_insImage = m_builder("image:output.insert");
	m_replaceBox = m_builder("box:output.findreplace");
	m_outputBox = m_builder("box:output");
	m_textView = m_builder("textview:output");
	m_searchEntry = m_builder("entry:output.search");
	m_replaceEntry = m_builder("entry:output.replace");
	m_filterKeepIfEndMark = m_builder("menuitem:output.stripcrlf.keependmark");
	m_filterKeepIfQuote = m_builder("menuitem:output.stripcrlf.keepquote");
	m_filterJoinHyphen = m_builder("menuitem:output.stripcrlf.joinhyphen");
	m_filterJoinSpace = m_builder("menuitem:output.stripcrlf.joinspace");
	m_filterKeepParagraphs = m_builder("menuitem:output.stripcrlf.keepparagraphs");
	m_toggleSearchButton = m_builder("button:output.findreplace");
	m_undoButton = m_builder("button:output.undo");
	m_redoButton = m_builder("button:output.redo");
	m_csCheckBox = m_builder("checkbutton:output.matchcase");
	m_textBuffer = OutputBuffer::create();
	m_textView->set_source_buffer(m_textBuffer);
	Gtk::Button* saveButton = m_builder("button:output.save");

	Glib::RefPtr<Gtk::AccelGroup> group = MAIN->getWindow()->get_accel_group();
	m_undoButton->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	m_redoButton->add_accelerator("clicked", group, GDK_KEY_Z, Gdk::CONTROL_MASK|Gdk::SHIFT_MASK, Gtk::AccelFlags(0));
	m_toggleSearchButton->add_accelerator("clicked", group, GDK_KEY_F, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));
	saveButton->add_accelerator("clicked", group, GDK_KEY_S, Gdk::CONTROL_MASK, Gtk::AccelFlags(0));

	m_builder("image:output.insert").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/ins_append.png"));
	m_builder("image:output.stripcrlf").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/stripcrlf.png"));
	m_builder("image:output.insert.append").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/ins_append.png"));
	m_builder("image:output.insert.cursor").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/ins_cursor.png"));
	m_builder("image:output.insert.replace").as<Gtk::Image>()->set(Gdk::Pixbuf::create_from_resource("/org/gnome/gimagereader/ins_replace.png"));

	m_substitutionsManager = new SubstitutionsManager(m_builder, m_textBuffer);

	m_insertMode = InsertMode::Append;

	m_spell.attach(*m_textView);
	m_spell.property_decode_language_codes() = true;

	CONNECT(m_builder("menuitem:output.insert.append").as<Gtk::MenuItem>(), activate, [this] { setInsertMode(InsertMode::Append, "ins_append.png"); });
	CONNECT(m_builder("menuitem:output.insert.cursor").as<Gtk::MenuItem>(), activate, [this] { setInsertMode(InsertMode::Cursor, "ins_cursor.png"); });
	CONNECT(m_builder("menuitem:output.insert.replace").as<Gtk::MenuItem>(), activate, [this] { setInsertMode(InsertMode::Replace, "ins_replace.png"); });
	CONNECT(m_builder("button:output.stripcrlf").as<Gtk::Button>(), clicked, [this] { filterBuffer(); });
	CONNECT(m_toggleSearchButton, toggled, [this] { toggleReplaceBox(); });
	CONNECT(m_undoButton, clicked, [this] { m_textBuffer->undo(); scrollCursorIntoView(); });
	CONNECT(m_redoButton, clicked, [this] { m_textBuffer->redo(); scrollCursorIntoView(); });
	CONNECT(saveButton, clicked, [this] { save(); });
	CONNECT(m_builder("button:output.clear").as<Gtk::Button>(), clicked, [this] { clear(); });
	CONNECTP(m_textBuffer, can_undo, [this] { m_undoButton->set_sensitive(m_textBuffer->can_undo()); });
	CONNECTP(m_textBuffer, can_redo, [this] { m_redoButton->set_sensitive(m_textBuffer->can_redo()); });
	CONNECT(m_csCheckBox, toggled, [this] { Utils::clear_error_state(m_searchEntry); });
	CONNECT(m_searchEntry, changed, [this] { Utils::clear_error_state(m_searchEntry); });
	CONNECT(m_searchEntry, activate, [this] { findReplace(false, false); });
	CONNECT(m_replaceEntry, activate, [this] { findReplace(false, true); });
	CONNECT(m_builder("button:output.searchnext").as<Gtk::Button>(), clicked, [this] { findReplace(false, false); });
	CONNECT(m_builder("button:output.searchprev").as<Gtk::Button>(), clicked, [this] { findReplace(true, false); });
	CONNECT(m_builder("button:output.replace").as<Gtk::Button>(), clicked, [this] { findReplace(false, true); });
	CONNECT(m_builder("button:output.replaceall").as<Gtk::Button>(), clicked, [this] { replaceAll(); });
	m_connectionCustomFont = CONNECTP(MAIN->getWidget("fontbutton:config.settings.customoutputfont").as<Gtk::FontButton>(), font_name, [this] { setFont(); });
	m_connectionDefaultFont = CONNECT(MAIN->getWidget("checkbutton:config.settings.defaultoutputfont").as<Gtk::CheckButton>(), toggled, [this] { setFont(); });
	CONNECT(m_builder("button:output.substitutions").as<Gtk::Button>(), clicked, [this] { m_substitutionsManager->set_visible(true); });
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
	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckButton>("searchmatchcase", m_builder("checkbutton:output.matchcase")));

	if(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue().empty()) {
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Utils::get_documents_dir());
	}

	setFont();
}

OutputEditorText::~OutputEditorText() {
	delete m_substitutionsManager;
	m_connectionCustomFont.disconnect();
	m_connectionDefaultFont.disconnect();
	MAIN->getConfig()->removeSetting("keepdot");
	MAIN->getConfig()->removeSetting("keepquote");
	MAIN->getConfig()->removeSetting("joinhyphen");
	MAIN->getConfig()->removeSetting("joinspace");
	MAIN->getConfig()->removeSetting("keepparagraphs");
	MAIN->getConfig()->removeSetting("drawwhitespace");
	MAIN->getConfig()->removeSetting("searchmatchcase");
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

void OutputEditorText::toggleReplaceBox() {
	m_searchEntry->set_text("");
	m_replaceEntry->set_text("");
	m_replaceBox->set_visible(m_toggleSearchButton->get_active());
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

void OutputEditorText::findNext() {
	findReplace(false, false);
}

void OutputEditorText::findPrev() {
	findReplace(true, false);
}

void OutputEditorText::replaceNext() {
	findReplace(false, true);
}

void OutputEditorText::replaceAll() {
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	Glib::ustring searchstr = m_searchEntry->get_text();
	Glib::ustring replacestr = m_replaceEntry->get_text();
	if(!m_textBuffer->replaceAll(searchstr, replacestr, m_csCheckBox->get_active())) {
		Utils::set_error_state(m_searchEntry);
	}
	MAIN->popState();
}

void OutputEditorText::findReplace(bool backwards, bool replace) {
	Utils::clear_error_state(m_searchEntry);
	Glib::ustring searchstr = m_searchEntry->get_text();
	Glib::ustring replacestr = m_replaceEntry->get_text();
	if(!m_textBuffer->findReplace(backwards, replace, m_csCheckBox->get_active(), searchstr, replacestr, m_textView)) {
		Utils::set_error_state(m_searchEntry);
	}
}

void OutputEditorText::read(tesseract::TessBaseAPI &tess, ReadSessionData *data) {
	char* text = tess.GetUTF8Text();
	bool& insertText = static_cast<TextReadSessionData*>(data)->insertText;
	Utils::runInMainThreadBlocking([&] { addText(text, insertText); });
	delete[] text;
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
		outname = Glib::build_filename(MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->getValue(), base + ".txt");

		FileDialogs::FileFilter filter = {_("Text Files"), {"text/plain"}, {"*.txt"}};
		outname = FileDialogs::save_dialog(_("Save Output..."), outname, filter);
		if(outname.empty()) {
			return false;
		}
		MAIN->getConfig()->getSetting<VarSetting<Glib::ustring>>("outputdir")->setValue(Glib::path_get_dirname(outname));
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
	m_substitutionsManager->set_visible(false);
}

void OutputEditorText::setLanguage(const Config::Lang& lang) {
	try {
		m_spell.set_language(lang.code.empty() ? Utils::getSpellingLanguage() : lang.code);
	} catch(const GtkSpell::Error& /*e*/) {}
}
