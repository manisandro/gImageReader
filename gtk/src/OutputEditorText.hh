/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.hh
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

#ifndef OUTPUTEDITORTEXT_HH
#define OUTPUTEDITORTEXT_HH

#include <gtksourceviewmm.h>
#include <gtkspellmm.h>

#include "Config.hh"
#include "MainWindow.hh"
#include "OutputBuffer.hh"
#include "OutputEditor.hh"

class SubstitutionsManager;

class OutputEditorText : public OutputEditor {
public:
	OutputEditorText();
	~OutputEditorText();

	Gtk::Box* getUI() override { return m_paneWidget; }
	ReadSessionData* initRead() override{ return new TextReadSessionData; }
	bool clear() override;
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const Glib::ustring& errorMsg, ReadSessionData* data) override;
	bool getModified() const override;
	void onVisibilityChanged(bool visible) override;
	bool save(const std::string& filename = "") override;
	void setLanguage(const Config::Lang &lang) override;

private:
	struct TextReadSessionData : ReadSessionData {
		bool insertText = false;
	};

	enum class InsertMode { Append, Cursor, Replace };

	Builder m_builder;
	Gtk::Box* m_paneWidget;
	Gtk::MenuButton* m_insButton;
	Gtk::Image* m_insImage;
	Gtk::EventBox* m_replaceBox;
	Gtk::Box* m_outputBox;
	Gsv::View* m_textView;
	Gtk::Entry* m_searchEntry;
	Gtk::Entry* m_replaceEntry;
	Gtk::CheckMenuItem* m_filterKeepIfEndMark;
	Gtk::CheckMenuItem* m_filterKeepIfQuote;
	Gtk::CheckMenuItem* m_filterJoinHyphen;
	Gtk::CheckMenuItem* m_filterJoinSpace;
	Gtk::CheckMenuItem* m_filterKeepParagraphs;
	Gtk::ToggleButton* m_toggleSearchButton;
	Gtk::Button* m_undoButton;
	Gtk::Button* m_redoButton;
	Gtk::CheckButton* m_csCheckBox;

	Glib::RefPtr<OutputBuffer> m_textBuffer;

	InsertMode m_insertMode;
	GtkSpell::Checker m_spell;
	SubstitutionsManager* m_substitutionsManager;

	void addText(const Glib::ustring& text, bool insert);
	void completeTextViewMenu(Gtk::Menu *menu);
	void filterBuffer();
	void findNext();
	void findPrev();
	void findReplace(bool backwards, bool replace);
	void replaceAll();
	void replaceNext();
	void scrollCursorIntoView();
	void setFont();
	void setInsertMode(InsertMode mode, const std::string& iconName);
	void toggleReplaceBox();
};

#endif // OUTPUTEDITORTEXT_HH
