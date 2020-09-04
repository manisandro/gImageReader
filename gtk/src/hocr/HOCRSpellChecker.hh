/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRSpellChecker.hh
 * Copyright (C) 2020 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRSPELLCHECKER_HH
#define HOCRSPELLCHECKER_HH

#include <gtkspellmm.h>

class HOCRSpellChecker : public GtkSpell::Checker {
public:
	bool checkSpelling(const Glib::ustring& word, std::vector<Glib::ustring>* suggestions = nullptr, int limit = -1);

private:
	void generateCombinations(const std::vector<std::vector<Glib::ustring>>& lists, std::vector<std::vector<Glib::ustring>>& results, int depth, const std::vector<Glib::ustring>& c) const;
};

#endif // HOCRSPELLCHECKER_HH
