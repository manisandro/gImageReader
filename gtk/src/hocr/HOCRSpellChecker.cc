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

#include "HOCRSpellChecker.hh"
#include "Utils.hh"

bool HOCRSpellChecker::checkSpelling(const Glib::ustring& word, std::vector<Glib::ustring>* suggestions, int limit) {
	static const Glib::RefPtr<Glib::Regex> splitRe = Glib::Regex::create("[\u2013\u2014]+");
	std::vector<std::pair<Glib::ustring, int>> words = Utils::string_split_pos(word, splitRe);

	std::size_t perWordLimit = 0;
	// s = p^w => w = log_p(c) = log(c)/log(p) => p = 10^(log(c)/w)
	if(limit > 0) { perWordLimit = std::size_t(std::pow(10, std::log10(limit) / words.size())); }
	std::vector<std::vector<Glib::ustring>> wordSuggestions;
	bool valid = true;
	bool multipleWords = words.size() > 1;
	for(const std::pair<Glib::ustring, int>& wordRef : words) {
		bool wordValid = check_word(wordRef.first);
		valid &= wordValid;
		if(suggestions) {
			std::vector<Glib::ustring> ws = get_suggestions(wordRef.first);
			if(wordValid && multipleWords) { ws.insert(ws.begin(), wordRef.first); }
			if(limit == -1) {
				wordSuggestions.push_back(ws);
			} else if(limit > 0) {
				ws.resize(std::min(ws.size(), perWordLimit));
				wordSuggestions.push_back(ws);
			}
		}
	}
	if(suggestions) {
		suggestions->clear();
		std::vector<std::vector<Glib::ustring>> suggestionCombinations;
		generateCombinations(wordSuggestions, suggestionCombinations, 0, std::vector<Glib::ustring>());
		for(const std::vector<Glib::ustring>& combination : suggestionCombinations) {
			Glib::ustring s = "";
			auto originalWord = words.begin();
			int last = 0;
			int next;
			for(const Glib::ustring& suggestedWord : combination) {
				next = originalWord->second;
				s.append(word.substr(last, next - last));
				s.append(suggestedWord);
				last = next + originalWord->first.length();
				originalWord++;
			}
			s.append(word.substr(last));
			// Don't list the exact input word
			if(s != word) {
				suggestions->push_back(s);
			}
		}
	}
	return valid;
}

// each suggestion for each word => each word in each suggestion
void HOCRSpellChecker::generateCombinations(const std::vector<std::vector<Glib::ustring>>& lists, std::vector<std::vector<Glib::ustring>>& results, int depth, const std::vector<Glib::ustring>& c) const {
	if(depth == int(lists.size())) {
		results.push_back(c);
		return;
	}
	for(int i = 0, n = int(lists[depth].size()); i < n; ++i) {
		std::vector<Glib::ustring> newc = c;
		newc.insert(newc.end(), lists[depth][i]);
		generateCombinations(lists, results, depth + 1, newc);
	}
}
