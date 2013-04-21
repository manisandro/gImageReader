/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * UndoableBuffer.cc
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

#include "UndoableBuffer.hh"

struct UndoableBuffer::Action {
	virtual ~Action(){}
};

// Something that has been inserted into the text buffer
struct UndoableBuffer::UndoableInsert : public UndoableBuffer::Action {
	int offset;
	Glib::ustring text;
	bool mergeable;
	bool isChained;

	UndoableInsert(const Gtk::TextIter& _iter, const Glib::ustring& _text, bool _isChained){
		offset = _iter.get_offset();
		text = _text;
		isChained = _isChained;
		mergeable = !isChained && (text.empty() || (text != "\r" && text != "\n" && text != " "));
	}

	static bool canBeMerged(const UndoableInsert& prev, const UndoableInsert& cur){
		if( (!cur.mergeable || !prev.mergeable) ||
			(cur.offset != (prev.offset + int(prev.text.length()))) ||
			((cur.text == " " || cur.text == "\t") && !(prev.text == " " || prev.text == "\t")) ||
			((prev.text == " " || prev.text == "\t") && !(cur.text == " " || cur.text == "\t")))
		{
			return false;
		}
		return true;
	}
};

// Something that has been deleted from the textbuffer
struct UndoableBuffer::UndoableDelete : public UndoableBuffer::Action {
	Glib::ustring text;
	int start, end;
	bool deleteKeyUsed;
	bool mergeable;
	bool isChained;

	UndoableDelete(Gtk::TextBuffer& _buffer, const Gtk::TextIter& _startIter, const Gtk::TextIter& _endIter, bool _isChained){
		text = _buffer.get_text(_startIter, _endIter, false);
		start = _startIter.get_offset();
		end = _endIter.get_offset();
		// Need to find out if backspace or delete key has been used so we don't mess up during redo
		Gtk::TextIter insertIter = _buffer.get_iter_at_mark(_buffer.get_insert());
		deleteKeyUsed = (insertIter.get_offset() <= start);
		isChained = _isChained;
		mergeable = !isChained && (text.empty() || (text != "\r" && text != "\n" && text != " "));
	}

	static bool canBeMerged(const UndoableDelete& prev, const UndoableDelete& cur){
		if( (!cur.mergeable || !prev.mergeable) ||
			(prev.deleteKeyUsed != cur.deleteKeyUsed) ||
			(prev.start != cur.start and prev.start != cur.end) ||
			(!(cur.text == " " || cur.text == "\t") && (prev.text == " " || prev.text == "\t")) ||
			(!(prev.text == " " || prev.text == "\t") && (cur.text == " " || cur.text == "\t")))
		{
			return false;
		}
		return true;
	}
};

UndoableBuffer::UndoableBuffer()
	: Gtk::TextBuffer()
{
	m_notUndoableAction = false;
	m_undoInProgress = false;
	m_isChain = false;
	CONNECT(this, insert, [this](const Gtk::TextIter& it, const Glib::ustring& text, int len){ onInsertText( it, text, len); });
	CONNECT(this, erase, [this](const Gtk::TextIter& start, const Gtk::TextIter& end){ onDeleteRange(start, end); });
}

void UndoableBuffer::onInsertText(const Gtk::TextIter &it, const Glib::ustring &text, int len)
{
	if(!m_undoInProgress){
		clearStack(m_redoStack);
	}
	if(m_notUndoableAction){
		m_signal_histroyChanged.emit();
		return;
	}
	UndoableInsert* undoAction = new UndoableInsert(it, text, m_isChain);
	Action* prev;
	if(!m_undoStack.empty()){
		prev = m_undoStack.top();
		m_undoStack.pop();
	}else{
		m_undoStack.push(undoAction);
		m_signal_histroyChanged.emit();
		return;
	}
	UndoableInsert* prevInsert = dynamic_cast<UndoableInsert*>(prev);
	if(!prevInsert){
		m_undoStack.push(prev);
		m_undoStack.push(undoAction);
		m_signal_histroyChanged.emit();
		return;
	}
	if(UndoableInsert::canBeMerged(*prevInsert, *undoAction)){
		prevInsert->text += undoAction->text;
		m_undoStack.push(prevInsert);
	}else{
		m_undoStack.push(prevInsert);
		m_undoStack.push(undoAction);
	}
	m_signal_histroyChanged.emit();
}

void UndoableBuffer::onDeleteRange(const Gtk::TextIter &start, const Gtk::TextIter &end)
{
	if(!m_undoInProgress){
		clearStack(m_redoStack);
	}
	if(m_notUndoableAction){
		m_signal_histroyChanged.emit();
		return;
	}
	UndoableDelete* undoAction = new UndoableDelete(*this, start, end, m_isChain);
	Action* prev;
	if(!m_undoStack.empty()){
		prev = m_undoStack.top();
		m_undoStack.pop();
	}else{
		m_undoStack.push(undoAction);
		m_signal_histroyChanged.emit();
		return;
	}
	UndoableDelete* prevDelete = dynamic_cast<UndoableDelete*>(prev);
	if(!prevDelete){
		m_undoStack.push(prev);
		m_undoStack.push(undoAction);
		m_signal_histroyChanged.emit();
		return;
	}
	if(UndoableDelete::canBeMerged(*prevDelete, *undoAction)){
		if(prevDelete->start == undoAction->start){ // Delete key used
			prevDelete->text += undoAction->text;
			prevDelete->end += (undoAction->end - undoAction->start);
		}else{ // Backspace used
			prevDelete->text = undoAction->text + prevDelete->text;
			prevDelete->start = undoAction->start;
		}
		m_undoStack.push(prevDelete);
	}else{
		m_undoStack.push(prevDelete);
		m_undoStack.push(undoAction);
	}
	m_signal_histroyChanged.emit();
}

void UndoableBuffer::undo()
{
	if(m_undoStack.empty()){
		return;
	}
	begin_not_undoable_action();
	m_undoInProgress = true;
	Action* undoAction = m_undoStack.top();
	m_undoStack.pop();
	m_redoStack.push(undoAction);
	UndoableInsert* insertAction = dynamic_cast<UndoableInsert*>(undoAction);
	if(insertAction){
		Gtk::TextIter start = get_iter_at_offset(insertAction->offset);
		Gtk::TextIter end = get_iter_at_offset(insertAction->offset + insertAction->text.size());
		erase(start, end);
		place_cursor(start);
		if(insertAction->isChained){ // TODO: was deactivated in python?!
			undo();
			return;
		}
	}else{
		UndoableDelete* deleteAction = dynamic_cast<UndoableDelete*>(undoAction);
		g_assert(deleteAction);
		Gtk::TextIter start = get_iter_at_offset(deleteAction->start);
		insert(start, deleteAction->text);
		if(deleteAction->deleteKeyUsed){
			place_cursor(start);
		}else{
			place_cursor(get_iter_at_offset(deleteAction->end));
		}
	}
	end_not_undoable_action();
	m_signal_histroyChanged.emit();
	m_undoInProgress = false;
}

void UndoableBuffer::redo()
{
	if(m_redoStack.empty()){
		return;
	}
	begin_not_undoable_action();
	m_undoInProgress = true;
	Action* redoAction = m_redoStack.top();
	m_redoStack.pop();
	m_undoStack.push(redoAction);
	UndoableInsert* insertAction = dynamic_cast<UndoableInsert*>(redoAction);
	if(insertAction){
		Gtk::TextIter start = get_iter_at_offset(insertAction->offset);
		insert(start, insertAction->text);
		place_cursor(get_iter_at_offset(insertAction->offset + insertAction->text.length()));
	}else{
		UndoableDelete* deleteAction = dynamic_cast<UndoableDelete*>(redoAction);
		g_assert(deleteAction);
		Gtk::TextIter start = get_iter_at_offset(deleteAction->start);
		Gtk::TextIter end = get_iter_at_offset(deleteAction->end);
		erase(start, end);
		place_cursor(start);
		if(deleteAction->isChained){ // TODO: was deactivated in python?!
			redo();
			return;
		}
		end_not_undoable_action();
		m_signal_histroyChanged.emit();
		m_undoInProgress = false;
	}
}

void UndoableBuffer::clear_history()
{
	clearStack(m_undoStack);
	clearStack(m_redoStack);
}

void UndoableBuffer::replace_selection(const Glib::ustring &text, bool allIfNoSelection)
{
	Gtk::TextIter startIt = begin(), endIt = end();
	if(get_has_selection()){
		get_selection_bounds(startIt, endIt);
	}else if(!allIfNoSelection){
		insert_at_cursor(text);
		return;
	}
	begin_chain();
	erase(startIt, endIt);
	insert_at_cursor(text);
	end_chain();
}

void UndoableBuffer::replace_all(const Glib::ustring &text)
{
	Gtk::TextIter startIt = begin(), endIt = end();
	begin_chain();
	erase(startIt, endIt);
	insert_at_cursor(text);
	end_chain();
}

void UndoableBuffer::clearStack(std::stack<Action *> &stack)
{
	while(!stack.empty()){
		delete stack.top();
		stack.pop();
	}
}
