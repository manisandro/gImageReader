/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceView.hh
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

#ifndef SOURCEVIEW_HH
#define SOURCEVIEW_HH

#include "common.hh"

/* Thin wrapper around the parts of the GtkSourceView C API used by gImageReader.
 * GtkSourceView has no C++ binding, but as GtkSourceView and GtkSourceBuffer derive
 * from GtkTextView and GtkTextBuffer, source views and buffers can be used like
 * plain gtkmm ones, with the syntax highlighting and undo features accessed
 * through the C API.
 */
namespace Gsv {

// Registers the GtkSourceView and GtkSourceBuffer types.
// Must be called before any source view or buffer is created, in particular
// before parsing ui files containing a GtkSourceView.
void init();

// Returns the ids of all languages known to the default language manager.
std::vector<std::string> languageIds();

class Buffer : public Gtk::TextBuffer {
public:
	// Returns a new source buffer with syntax highlighting enabled for the given
	// language, or disabled if the language is unknown or empty.
	static Glib::RefPtr<Buffer> create(const std::string& languageId = std::string());

	void undo();
	void redo();
	bool can_undo();
	bool can_redo();
	void begin_not_undoable_action();
	void end_not_undoable_action();
	Glib::PropertyProxy<bool> property_can_undo() { return Glib::PropertyProxy<bool>(this, "can-undo"); }
	Glib::PropertyProxy<bool> property_can_redo() { return Glib::PropertyProxy<bool>(this, "can-redo"); }

	// Enables syntax highlighting for the given language, or disables
	// highlighting if the language is unknown or empty.
	void set_language(const std::string& languageId);
	// Returns the id of the language currently set on the buffer, empty if none is set.
	std::string get_language_id();
	bool get_highlight_syntax();
	void set_highlight_matching_brackets(bool enable);

protected:
	explicit Buffer(const std::string& languageId = std::string());
};

// Returns a new source view showing the given buffer.
// The returned reference is owned by the caller.
Gtk::TextView* createView(Buffer& buffer);

// Enables or disables the drawing of whitespace characters on the given source view.
void setDrawSpaces(Gtk::TextView& view, bool enable);

}

#endif // SOURCEVIEW_HH
