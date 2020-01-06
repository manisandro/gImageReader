/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * XmlUtils.cc
 * Copyright (C) 2013-2020 Sandro Mani <manisandro@gmail.com>
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

#include "XmlUtils.hh"
#include "Utils.hh"

#include <glibmm/ustring.h>
#include <libxml++/libxml++.h>

Glib::ustring XmlUtils::elementText(const xmlpp::Element* element) {
	if(!element) {
		return Glib::ustring();
	}
	Glib::ustring text;
	for(const xmlpp::Node* node : element->get_children()) {
		if(dynamic_cast<const xmlpp::TextNode*>(node)) {
			text += static_cast<const xmlpp::TextNode*>(node)->get_content();
		} else if(dynamic_cast<const xmlpp::Element*>(node)) {
			text += elementText(static_cast<const xmlpp::Element*>(node));
		}
	}
	return Utils::string_trim(text);
}

const xmlpp::Element* XmlUtils::firstChildElement(const xmlpp::Node* node, const Glib::ustring& name) {
	if(!node) {
		return nullptr;
	}
	const xmlpp::Node* child = node->get_first_child(name);
	while(child && !dynamic_cast<const xmlpp::Element*>(child)) {
		child = child->get_next_sibling();
	}
	return child ? static_cast<const xmlpp::Element*>(child) : nullptr;
}

const xmlpp::Element* XmlUtils::nextSiblingElement(const xmlpp::Node* node, const Glib::ustring& name) {
	if(!node) {
		return nullptr;
	}
	const xmlpp::Node* child = node->get_next_sibling();
	if(name != "") {
		while(child && (child->get_name() != name || !dynamic_cast<const xmlpp::Element*>(child))) {
			child = child->get_next_sibling();
		}
	} else {
		while(child && !dynamic_cast<const xmlpp::Element*>(child)) {
			child = child->get_next_sibling();
		}
	}
	return child ? static_cast<const xmlpp::Element*>(child) : nullptr;
}

std::list<const xmlpp::Element*> XmlUtils::elementsByTagName(const xmlpp::Element* element, const Glib::ustring& name) {
	std::list<const xmlpp::Element*> elems;
	for(const xmlpp::Node* child : element->get_children()) {
		if(const xmlpp::Element* childElem = dynamic_cast<const xmlpp::Element*>(child)) {
			if(childElem->get_name() == name) {
				elems.push_back(childElem);
			}
			elems.splice(elems.end(), elementsByTagName(childElem, name));
		}
	}
	return elems;
}
