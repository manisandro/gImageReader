/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * XmlUtils.hh
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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

namespace xmlpp {
	class Document;
	class Element;
	class Node;
}

class XmlUtils {
public:
	static Glib::ustring elementText(const xmlpp::Element* element);
	static xmlpp::Element* firstChildElement(xmlpp::Node* node, const Glib::ustring& name = Glib::ustring());
	static xmlpp::Element* nextSiblingElement(xmlpp::Node* node, const Glib::ustring& name = Glib::ustring());
	static Glib::ustring documentXML(xmlpp::Document* doc);
	static Glib::ustring elementXML(const xmlpp::Element* element);
	static xmlpp::Element* takeChild(xmlpp::Element* parent, xmlpp::Element* child);
	static xmlpp::Element* createElement(const Glib::ustring& name);

private:
	static xmlpp::Element* dummyElement();
};

#endif // XMLUTILS_HH
