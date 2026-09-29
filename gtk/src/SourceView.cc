/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceView.cc
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

#include "SourceView.hh"

#include <gtksourceview/gtksource.h>

namespace {

// The get_type() functions are declared G_GNUC_CONST, so their result has to be
// stored to keep the compiler from eliding the calls, which would leave the
// GtkSourceView and GtkSourceBuffer types unregistered.
GType viewType() {
	static GType type = gtk_source_view_get_type();
	return type;
}

GType bufferType() {
	static GType type = gtk_source_buffer_get_type();
	return type;
}

// Applies the given language, enabling or disabling highlighting accordingly.
void applyLanguage(GtkSourceBuffer* buffer, const std::string& languageId) {
	GtkSourceLanguage* language = languageId.empty() ? nullptr :
	                              gtk_source_language_manager_get_language(gtk_source_language_manager_get_default(), languageId.c_str());
	gtk_source_buffer_set_highlight_syntax(buffer, language != nullptr);
	if (language) {
		gtk_source_buffer_set_language(buffer, language);
	}
}

std::string languageIdOf(GtkSourceBuffer* buffer) {
	GtkSourceLanguage* language = gtk_source_buffer_get_language(buffer);
	return language ? gtk_source_language_get_id(language) : std::string();
}

// Returns a new source buffer, transferring the ownership of the returned reference to the caller.
GtkTextBuffer* createSourceBuffer(const std::string& languageId) {
	bufferType();
	GtkSourceBuffer* buffer = gtk_source_buffer_new(nullptr);
	applyLanguage(buffer, languageId);
	return GTK_TEXT_BUFFER(buffer);
}

}

void Gsv::init() {
	viewType();
	bufferType();
}

std::vector<std::string> Gsv::languageIds() {
	bufferType();
	std::vector<std::string> ids;
	for (const gchar* const* id = gtk_source_language_manager_get_language_ids(gtk_source_language_manager_get_default()); *id; ++id) {
		ids.push_back(*id);
	}
	return ids;
}

Gtk::TextView* Gsv::createView(Buffer& buffer) {
	viewType();
	bufferType();
	return dynamic_cast<Gtk::TextView*>(Glib::wrap(gtk_source_view_new_with_buffer(GTK_SOURCE_BUFFER(buffer.gobj()))));
}

void Gsv::setDrawSpaces(Gtk::TextView& view, bool enable) {
	GtkSourceView* sourceView = GTK_SOURCE_VIEW(view.gobj());
#if GTK_SOURCE_CHECK_VERSION(3, 24, 0)
	GtkSourceSpaceDrawer* spaceDrawer = gtk_source_view_get_space_drawer(sourceView);
	gtk_source_space_drawer_set_types_for_locations(spaceDrawer, GTK_SOURCE_SPACE_LOCATION_ALL, GTK_SOURCE_SPACE_TYPE_ALL);
	gtk_source_space_drawer_set_enable_matrix(spaceDrawer, enable ? TRUE : FALSE);
#else
	gtk_source_view_set_draw_spaces(sourceView, enable ? GTK_SOURCE_DRAW_SPACES_ALL : GtkSourceDrawSpacesFlags());
#endif
}

Glib::RefPtr<Gsv::Buffer> Gsv::Buffer::create(const std::string& languageId) {
	return Glib::RefPtr<Buffer> (new Buffer(languageId));
}

Gsv::Buffer::Buffer(const std::string& languageId)
	: Gtk::TextBuffer(createSourceBuffer(languageId)) {
}

void Gsv::Buffer::undo() {
	gtk_source_buffer_undo(GTK_SOURCE_BUFFER(gobj()));
}

void Gsv::Buffer::redo() {
	gtk_source_buffer_redo(GTK_SOURCE_BUFFER(gobj()));
}

bool Gsv::Buffer::can_undo() {
	return gtk_source_buffer_can_undo(GTK_SOURCE_BUFFER(gobj()));
}

bool Gsv::Buffer::can_redo() {
	return gtk_source_buffer_can_redo(GTK_SOURCE_BUFFER(gobj()));
}

void Gsv::Buffer::begin_not_undoable_action() {
	gtk_source_buffer_begin_not_undoable_action(GTK_SOURCE_BUFFER(gobj()));
}

void Gsv::Buffer::end_not_undoable_action() {
	gtk_source_buffer_end_not_undoable_action(GTK_SOURCE_BUFFER(gobj()));
}

void Gsv::Buffer::set_language(const std::string& languageId) {
	applyLanguage(GTK_SOURCE_BUFFER(gobj()), languageId);
}

std::string Gsv::Buffer::get_language_id() {
	return languageIdOf(GTK_SOURCE_BUFFER(gobj()));
}

bool Gsv::Buffer::get_highlight_syntax() {
	return gtk_source_buffer_get_highlight_syntax(GTK_SOURCE_BUFFER(gobj()));
}

void Gsv::Buffer::set_highlight_matching_brackets(bool enable) {
	gtk_source_buffer_set_highlight_matching_brackets(GTK_SOURCE_BUFFER(gobj()), enable);
}
