/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRSpellChecker.cc
 * Copyright (C) 2022-2024 Sandro Mani <manisandro@gmail.com>
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
#include <QRegularExpression>
#include <QVector>

#include "HOCRSpellChecker.hh"


bool HOCRSpellChecker::checkSpelling(const QString& word, QStringList* suggestions, int limit) const {
	QList<QPair<QString, int>> words;
	QRegularExpression dashRe("[\\x2013\\x2014]+");
	QRegularExpressionMatch match;
	int pos = 0;
	while((match = dashRe.match(word, pos)).hasMatch()) {
		words.append(qMakePair(word.mid(pos, match.capturedStart() - pos), pos));
		pos = match.capturedEnd();
	}
	words.append(qMakePair(word.mid(pos), pos));

	int perWordLimit = 0;
	// s = p^w => w = log_p(c) = log(c)/log(p) => p = 10^(log(c)/w)
	if(limit > 0) { perWordLimit = int(std::pow(10, std::log10(limit) / words.size())); }
	QList<QList<QString>> wordSuggestions;
	bool valid = true;
	bool multipleWords = words.size() > 1;
	for(const auto& pair : words) {
		QString wordString = pair.first;
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
			auto originalWord = words.begin();
			int last = 0;
			int next;
			for(const QString& suggestedWord : combination) {
				next = originalWord->second;
				s.append(word.mid(last, next - last));
				s.append(suggestedWord);
				last = next + originalWord->first.length();
				originalWord++;
			}
			s.append(word.mid(last));
			suggestions->append(s);
		}
		// Don't list the exact input word
		suggestions->removeOne(word);
	}
	return valid;
}

// each suggestion for each word => each word in each suggestion
void HOCRSpellChecker::generateCombinations(const QList<QList<QString>>& lists, QList<QList<QString>>& results, int depth, const QList<QString>& c) const {
	if(depth == lists.size()) {
		results.append(c);
		return;
	}
	for(int i = 0; i < lists[depth].size(); ++i) {
		generateCombinations(lists, results, depth + 1, c + QList<QString>({lists[depth][i]}));
	}
}
