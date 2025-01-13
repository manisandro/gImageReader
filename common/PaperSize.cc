/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * PaperSize.cc
 * Copyright (C) 2017-2018 Alexander Zaitsev <zamazan4ik@tut.by>
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

#include "PaperSize.hh"

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>
#include <utility>


template<typename T>
bool PaperSize::Size<T>::operator< (const Size<T>& val) {
	return std::tie(width, height) < std::tie(val.width, val.height);
}


PaperSize::Size<double> PaperSize::getSize(PaperSize::Unit unit, const std::string& format, bool landscape) {
	auto result = std::find_if(paperSizes.begin(), paperSizes.end(),
	[&format](const std::pair<std::string, Size<int >> & val) { return val.first == format; });
	if (result == paperSizes.end()) {
		return {0.0, 0.0};
	}

	// Translate size into cm from mm
	Size<double> formatResult = {static_cast<double> (result->second.width) / 10.0,
	                             static_cast<double> (result->second.height) / 10.0
	                            };

	if (landscape) {
		std::swap(formatResult.width, formatResult.height);
	}
	if (unit == PaperSize::inch) {
		return {formatResult.width / CMtoInch, formatResult.height / CMtoInch};
	} else { /*if(unit == PaperSize::cm)*/
		return formatResult;
	}
}

const double PaperSize::CMtoInch = 2.54;

const std::vector<std::pair<std::string, PaperSize::Size<int >>> PaperSize::paperSizes = {
	// A type
	{"4A0", Size<int> (1682, 2378) },
	{"2A0", Size<int> (1189, 1682) },
	{"A0", Size<int> (841, 1189) },
	{"A1", Size<int> (594, 841) },
	{"A2", Size<int> (420, 594) },
	{"A3", Size<int> (297, 420) },
	{"A4", Size<int> (210, 297) },
	{"A5", Size<int> (148, 210) },
	{"A6", Size<int> (105, 148) },
	{"A7", Size<int> (74, 105) },
	{"A8", Size<int> (52, 74) },
	{"A9", Size<int> (37, 52) },
	{"A10", Size<int> (26, 37) },
	{"A11", Size<int> (18, 26) },
	{"A12", Size<int> (13, 18) },
	{"A13", Size<int> (9, 13) },

	// B type
	{"B0", Size<int> (1000, 1414) },
	{"B1", Size<int> (707, 1000) },
	{"B2", Size<int> (500, 707) },
	{"B3", Size<int> (353, 500) },
	{"B4", Size<int> (250, 353) },
	{"B5", Size<int> (176, 250) },
	{"B6", Size<int> (125, 176) },
	{"B7", Size<int> (88, 125) },
	{"B8", Size<int> (62, 88) },
	{"B9", Size<int> (44, 62) },
	{"B10", Size<int> (31, 44) },
	{"B11", Size<int> (22, 31) },
	{"B12", Size<int> (15, 22) },
	{"B13", Size<int> (11, 15) },

	// C type
	{"C0", Size<int> (917, 1297) },
	{"C1", Size<int> (648, 917) },
	{"C2", Size<int> (458, 648) },
	{"C3", Size<int> (324, 458) },
	{"C4", Size<int> (229, 324) },
	{"C5", Size<int> (162, 229) },
	{"C6", Size<int> (114, 162) },
	{"C7", Size<int> (81, 114) },
	{"C8", Size<int> (57, 81) },
	{"C9", Size<int> (40, 57) },
	{"C10", Size<int> (28, 40) },

	// D type. (German extension)
	{"D0", Size<int> (771, 1090) },
	{"D1", Size<int> (545, 771) },
	{"D2", Size<int> (385, 545) },
	{"D3", Size<int> (272, 385) },
	{"D4", Size<int> (192, 272) },
	{"D5", Size<int> (136, 192) },
	{"D6", Size<int> (96, 136) },
	{"D7", Size<int> (68, 96) },
	{"D8", Size<int> (48, 68) },

	{"D0 (China)", Size<int> (764, 1064) },
	{"D1 (China)", Size<int> (532, 760) },
	{"D2 (China)", Size<int> (380, 528) },
	{"D3 (China)", Size<int> (264, 376) },
	{"D4 (China)", Size<int> (188, 260) },
	{"D5 (China)", Size<int> (130, 184) },
	{"D6 (China)", Size<int> (92, 126) },

	{"RD0 (China)", Size<int> (787, 1092) },
	{"RD1 (China)", Size<int> (546, 787) },
	{"RD2 (China)", Size<int> (393, 546) },
	{"RD3 (China)", Size<int> (273, 393) },
	{"RD4 (China)", Size<int> (196, 273) },
	{"RD5 (China)", Size<int> (136, 196) },
	{"RD6 (China)", Size<int> (98, 136) },

	{"CAN P1", Size<int> (560, 860) },
	{"CAN P2", Size<int> (430, 560) },
	{"CAN P3", Size<int> (280, 430) },
	{"CAN P4", Size<int> (215, 280) },
	{"CAN P5", Size<int> (140, 215) },
	{"CAN P6", Size<int> (107, 140) },

	{"ANSI A", Size<int> (216, 279) },
	{"ANSI B", Size<int> (279, 432) },
	{"ANSI C", Size<int> (432, 559) },
	{"ANSI D", Size<int> (559, 864) },
	{"ANSI E", Size<int> (864, 1118) },

	{"Arch A/1", Size<int> (229, 305) },
	{"Arch B/2", Size<int> (305, 457) },
	{"Arch C/3", Size<int> (457, 610) },
	{"Arch D/4", Size<int> (610, 914) },
	{"Arch E/6", Size<int> (914, 1219) },
	{"Arch E1/5", Size<int> (762, 1067) },
	{"Arch E2", Size<int> (660, 965) },
	{"Arch E3", Size<int> (686, 991) },

	{"B0 (JIS)", Size<int> (1030, 1456) },
	{"B1 (JIS)", Size<int> (728, 1030) },
	{"B2 (JIS)", Size<int> (515, 728) },
	{"B3 (JIS)", Size<int> (364, 515) },
	{"B4 (JIS)", Size<int> (257, 364) },
	{"B5 (JIS)", Size<int> (182, 257) },
	{"B6 (JIS)", Size<int> (128, 182) },
	{"B7 (JIS)", Size<int> (91, 128) },
	{"B8 (JIS)", Size<int> (64, 91) },
	{"B9 (JIS)", Size<int> (45, 64) },
	{"B10 (JIS)", Size<int> (32, 45) },
	{"B11 (JIS)", Size<int> (22, 32) },
	{"B12 (JIS)", Size<int> (16, 22) },

	{"F0", Size<int> (841, 1321) },
	{"F1", Size<int> (660, 841) },
	{"F2", Size<int> (420, 660) },
	{"F3", Size<int> (330, 420) },
	{"F4", Size<int> (210, 330) },
	{"F5", Size<int> (165, 210) },
	{"F6", Size<int> (105, 165) },
	{"F7", Size<int> (82, 105) },
	{"F8", Size<int> (52, 82) },
	{"F9", Size<int> (41, 52) },
	{"F10", Size<int> (26, 41) },

	// North American paper sizes
	{"Letter", Size<int> (216, 279) },
	{"Legal", Size<int> (216, 356) },
	{"Tabloid", Size<int> (279, 432) },
	{"Ledger", Size<int> (432, 279) },
	{"Junior Legal", Size<int> (127, 203) },
	{"Half Letter/Memo", Size<int> (140, 216) },
	{"Government Letter", Size<int> (203, 267) },
	{"Government Legal", Size<int> (216, 330) },

	// Traditional British paper sizes
	{"Foolscap", Size<int> (203, 330) },
	{"Quarto", Size<int> (203, 254) },
	{"Imperial", Size<int> (178, 229) },
	{"Kings", Size<int> (165, 203) },
	{"Dukes", Size<int> (140, 178) },

	// Raw sizes
	{"RA0", Size<int> (860, 1220) },
	{"RA1", Size<int> (610, 860) },
	{"RA2", Size<int> (430, 610) },
	{"RA3", Size<int> (305, 430) },
	{"RA4", Size<int> (215, 305) },
	{"SRA0", Size<int> (900, 1280) },
	{"SRA1", Size<int> (640, 900) },
	{"SRA2", Size<int> (450, 640) },
	{"SRA3", Size<int> (320, 450) },
	{"SRA4", Size<int> (225, 320) }
};
