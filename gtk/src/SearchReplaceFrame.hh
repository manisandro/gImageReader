/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SearchReplaceFrame.hh
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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

#ifndef SEARCHREPLACEFRAME_HH
#define SEARCHREPLACEFRAME_HH

#include "common.hh"
#include "ui_SearchReplaceFrame.hh"

class SubstitutionsManager;

class SearchReplaceFrame {
public:
	explicit SearchReplaceFrame();
	~SearchReplaceFrame();

	Gtk::Box* getWidget() const {
		return ui.boxSearchreplace;
	}
	void clear();
	void clearErrorState();
	void setErrorState();
	void hideSubstitutionsManager();

	sigc::signal<void, const Glib::ustring&, const Glib::ustring&, bool, bool, bool> signal_find_replace() {
		return m_signal_find_replace;
	}
	sigc::signal<void, const Glib::ustring&, const Glib::ustring&, bool> signal_replace_all() {
		return m_signal_replace_all;
	}
	sigc::signal<void, const std::map<Glib::ustring, Glib::ustring>&, bool> signal_apply_substitutions() {
		return m_signal_apply_substitutions;
	}

private:
	sigc::signal<void, const Glib::ustring&, const Glib::ustring&, bool, bool, bool> m_signal_find_replace;
	sigc::signal<void, const Glib::ustring&, const Glib::ustring&, bool> m_signal_replace_all;
	sigc::signal<void, const std::map<Glib::ustring, Glib::ustring>&, bool> m_signal_apply_substitutions;

	Ui::SearchReplaceFrame ui;
	ClassData m_classdata;
	SubstitutionsManager* m_substitutionsManager;

private:
	void findNext() {
		m_signal_find_replace.emit(ui.entrySearch->get_text(), ui.entryReplace->get_text(), ui.checkbuttonMatchcase->get_active(), false, false);
	}
	void findPrev() {
		m_signal_find_replace.emit(ui.entrySearch->get_text(), ui.entryReplace->get_text(), ui.checkbuttonMatchcase->get_active(), true, false);
	}
	void replaceNext() {
		m_signal_find_replace.emit(ui.entrySearch->get_text(), ui.entryReplace->get_text(), ui.checkbuttonMatchcase->get_active(), false, true);
	}
	void emitReplaceAll() {
		m_signal_replace_all.emit(ui.entrySearch->get_text(), ui.entryReplace->get_text(), ui.checkbuttonMatchcase->get_active());
	}
	void emitApplySubstitutions(const std::map<Glib::ustring, Glib::ustring>& substitutions) {
		m_signal_apply_substitutions.emit(substitutions, ui.checkbuttonMatchcase->get_active());
	}
};

#endif // SEARCHREPLACEFRAME_HH
