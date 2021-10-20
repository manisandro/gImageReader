/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.hh
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

#ifndef OUTPUTEDITORTEXT_HH
#define OUTPUTEDITORTEXT_HH

#include <gtksourceviewmm.h>
#include <gtkspellmm.h>

#include "MainWindow.hh"
#include "OutputBuffer.hh"
#include "OutputEditor.hh"
#include "ui_OutputEditorText.hh"

class SearchReplaceFrame;

class OutputSession {
	public:
		std::string file;
};


class OutputEditorText : public OutputEditor {
public:
	class TextBatchProcessor : public BatchProcessor {
	public:
		TextBatchProcessor(bool prependPage) : m_prependPage(prependPage) {}
		std::string fileSuffix() const override { return std::string(".txt"); }
		void appendOutput(std::ostream& dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo, bool firstArea) const override;
	private:
		bool m_prependPage = false;
	};

	OutputEditorText();
	~OutputEditorText();

	Gtk::Box* getUI() const override {
		return ui.boxEditorText;
	}
	ReadSessionData* initRead(tesseract::TessBaseAPI& /*tess*/) override {
		return new TextReadSessionData;
	}
	bool clear(bool hide = true, Gtk::Widget* page = nullptr) override;
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const Glib::ustring& errorMsg, ReadSessionData* data) override;
	BatchProcessor* createBatchProcessor(const std::map<Glib::ustring, Glib::ustring>& options) const override;
	bool getModified(Gtk::Widget* page = nullptr) const override;
	bool hasSession(Gtk::Widget* page = nullptr);
	void onVisibilityChanged(bool visible) override;
	bool open(const std::string& file = "") override;
	bool save(const std::string& filename = "", Gtk::Widget* page = nullptr) override;
	void setLanguage(const Config::Lang& lang) override;
	Gtk::Notebook* getNotebook() const { return ui.notebook; };
	// get OutputBuffer at page; by default returns buffer at current page
	Glib::RefPtr<OutputBuffer> getBuffer(Gtk::Widget* page = nullptr) const;
	std::string getTabLabel(Gtk::Widget* page = nullptr);

private:
	struct TextReadSessionData : ReadSessionData {
		bool insertText = false;
	};

	enum class InsertMode { Append, Cursor, Replace };


	Ui::OutputEditorText ui;
	ClassData m_classdata;
	SearchReplaceFrame* m_searchFrame;

	InsertMode m_insertMode;
	GtkSpell::Checker m_spell;

	std::map<Gtk::Widget*, OutputSession> outputSession;

	void addText(const Glib::ustring& text, bool insert);
	void activateHighlightMode();
	void completeTextViewMenu(Gtk::Menu* menu);
	void filterBuffer();
	void findReplace(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase, bool backwards, bool replace);
	void replaceAll(const Glib::ustring& searchstr, const Glib::ustring& replacestr, bool matchCase);
	void applySubstitutions(const std::map<Glib::ustring, Glib::ustring>& substitutions, bool matchCase);
	void scrollCursorIntoView();
	void setFont(Gsv::View *view);
	void setInsertMode(InsertMode mode, const std::string& iconName);

	// get GtkSourceView at page; by default returns view at current page
	Gsv::View* getView(Gtk::Widget* page = nullptr) const;
	// get Gtk::Widget* page at tab position pageNum; by default returns page at current position
	Gtk::Widget* getPage(short int pageNum = -1) const;
	// creates Notebook tab widget: label + close button
	Gtk::Widget* tabWidget(std::string tabLabel, Gtk::Widget* page);
	// updates tab label, including buffer modified status; uses current label if no one is provided
	void setTabLabel(Gtk::Widget* page, std::string tabLabel = "");
	// creates new notebook tab, with file content (if provided) and returns pointer on the created page
	Gtk::Widget* addDocument(const std::string& file = "");
	void on_close_button_clicked(Gtk::Widget* page);
	void on_buffer_modified_changed(Gtk::Widget* page);
	void prepareCurView();
};

#endif // OUTPUTEDITORTEXT_HH