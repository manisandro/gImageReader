/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * XmlUtils.hh
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#ifndef XMLUTILS_HH
#define XMLUTILS_HH

#include <glibmm/ustring.h>
#include <list>

namespace xmlpp {
class Document;
class Element;
class Node;
}

class XmlUtils {
public:
	static Glib::ustring elementText(const xmlpp::Element* element);
	static const xmlpp::Element* firstChildElement(const xmlpp::Node* node, const Glib::ustring& name = Glib::ustring());
	static const xmlpp::Element* nextSiblingElement(const xmlpp::Node* node, const Glib::ustring& name = Glib::ustring());
	static std::list<const xmlpp::Element*> elementsByTagName(const xmlpp::Element* element, const Glib::ustring& name);
};

#endif // XMLUTILS_HH
