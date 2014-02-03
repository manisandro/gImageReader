/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Notifier.hh
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#ifndef NOTIFIER_HH
#define NOTIFIER_HH

#include "common.hh"

#include <functional>

namespace Notifier {

struct Action {
	Glib::ustring label;
	std::function<bool()> action;
};

typedef void* Handle;

void notify(const Glib::ustring& title, const Glib::ustring& message, const std::vector<Action>& actions, Handle* handle = nullptr);
void hide(Handle handle);

} // Notifier

#endif // NOTIFIER_HH
