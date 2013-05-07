/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * UndoableBuffer.hh
 * Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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

#ifndef UNDOABLEBUFFER_HH
#define UNDOABLEBUFFER_HH

#include "common.hh"

#include <memory>
#include <stack>

class UndoableBuffer : public Gtk::TextBuffer {
public:
	bool can_undo() const{ return !m_undoStack.empty(); }
	bool can_redo() const{ return !m_redoStack.empty(); }
	void undo();
	void redo();
	void clear_history(){ freeStack(m_undoStack); freeStack(m_redoStack); }
	void replace_selection(const Glib::ustring& text, bool allIfNoSelection = false);
	void replace_all(const Glib::ustring& text);
	sigc::signal<void> signal_history_changed() const{ return m_signal_histroyChanged; }

	static Glib::RefPtr<UndoableBuffer> create(){
		return Glib::RefPtr<UndoableBuffer>(new UndoableBuffer());
	}

private:
	UndoableBuffer();
	~UndoableBuffer(){ clear_history(); }
	struct Action;
	struct UndoableInsert;
	struct UndoableDelete;
	std::stack<Action*> m_undoStack;
	std::stack<Action*> m_redoStack;
	bool m_undoInProgress;
	sigc::signal<void> m_signal_histroyChanged;

	void onInsertText(const Gtk::TextIter& it, const Glib::ustring& text, int len);
	void onDeleteRange(const Gtk::TextIter& start, const Gtk::TextIter& end);
	void freeStack(std::stack<Action*>& stack);
};

#endif // UNDOABLEBUFFER_HH
