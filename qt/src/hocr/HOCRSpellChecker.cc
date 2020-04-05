/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRSpellChecker.cc
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

#include <cmath>

#include "HOCRSpellChecker.hh"


bool HOCRSpellChecker::checkSpelling(const QString& word, QStringList* suggestions, int limit) const {
	QVector<QStringRef> words = word.splitRef(QRegExp("[\\x2013\\x2014]+"));
	int perWordLimit = 0;
	// s = p^w => w = log_p(c) = log(c)/log(p) => p = 10^(log(c)/w)
	if(limit > 0) { perWordLimit = int(std::pow(10, std::log10(limit) / words.size())); }
	QList<QList<QString>> wordSuggestions;
	bool valid = true;
	bool multipleWords = words.size() > 1;
	for(const QStringRef& word : words) {
		QString wordString = word.toString();
		bool wordValid = checkWord(wordString);
		valid &= wordValid;
		if(suggestions) {
			QList<QString> ws = getSpellingSuggestions(wordString);
			if(wordValid && multipleWords) { ws.prepend(wordString); }
			if(limit == -1) {
				wordSuggestions.append(ws);
			} else if(limit > 0) {
				wordSuggestions.append(ws.mid(0, perWordLimit));
			}
		}
	}
	if(suggestions) {
		suggestions->clear();
		QList<QList<QString>> suggestionCombinations;
		generateCombinations(wordSuggestions, suggestionCombinations, 0, QList<QString>());
		for(const QList<QString>& combination : suggestionCombinations) {
			QString s = "";
			const QStringRef* originalWord = words.begin();
			int last = 0;
			int next;
			for(const QString& suggestedWord : combination) {
				next = originalWord->position();
				s.append(word.midRef(last, next - last));
				s.append(suggestedWord);
				last = next + originalWord->length();
				originalWord++;
			}
			s.append(word.midRef(last));
			suggestions->append(s);
		}
		// Don't list the exact input word
		suggestions->removeOne(word);
	}
	return valid;
}

// each suggestion for each word => each word in each suggestion
void HOCRSpellChecker::generateCombinations(const QList<QList<QString>>& lists, QList<QList<QString>>& results, int depth, const QList<QString> c) const {
	if(depth == lists.size()) {
		results.append(c);
		return;
	}
	for(int i = 0; i < lists[depth].size(); ++i) {
		generateCombinations(lists, results, depth + 1, c + QList<QString>({lists[depth][i]}));
	}
}
