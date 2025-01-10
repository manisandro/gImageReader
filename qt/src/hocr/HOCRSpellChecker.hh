/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRSpellChecker.hh
 * Copyright (C) 2022-2025 Sandro Mani <manisandro@gmail.com>
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

#include <QtSpell.hpp>

class HOCRSpellChecker : public QtSpell::Checker {
public:

	using QtSpell::Checker::Checker;

	bool checkSpelling(const QString& word, QStringList* suggestions = nullptr, int limit = -1) const;

	// Unused interface, no-op
	void checkSpelling(int /*start*/ = 0, int /*end*/ = -1) override { }
	QString getWord(int /*pos*/, int* /*start*/ = 0, int* /*end*/ = 0) const override { return QString(); }
	void insertWord(int /*start*/, int /*end*/, const QString& /*word*/) override { }
	bool isAttached() const override { return true; }

private:
	void generateCombinations(const QList<QList<QString >> & lists, QList<QList<QString >>& results, int depth, const QList<QString>& c) const;
};

#endif // HOCRSPELLCHECKER_HH
