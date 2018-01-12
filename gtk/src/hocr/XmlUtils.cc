/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * XmlUtils.cc
 * Copyright (C) (\d+)-2018 Sandro Mani <manisandro@gmail.com>
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
	if(!element)
		return Glib::ustring();
	Glib::ustring text;
	for(xmlpp::Node* node : element->get_children()) {
		if(dynamic_cast<xmlpp::TextNode*>(node)) {
			text += static_cast<xmlpp::TextNode*>(node)->get_content();
		} else if(dynamic_cast<xmlpp::Element*>(node)) {
			text += elementText(static_cast<xmlpp::Element*>(node));
		}
	}
	return Utils::string_trim(text);
}

xmlpp::Element* XmlUtils::firstChildElement(xmlpp::Node* node, const Glib::ustring& name) {
	if(!node)
		return nullptr;
	xmlpp::Node* child = node->get_first_child(name);
	while(child && !dynamic_cast<xmlpp::Element*>(child)) {
		child = child->get_next_sibling();
	}
	return child ? static_cast<xmlpp::Element*>(child) : nullptr;
}

xmlpp::Element* XmlUtils::nextSiblingElement(xmlpp::Node* node, const Glib::ustring& name) {
	if(!node)
		return nullptr;
	xmlpp::Node* child = node->get_next_sibling();
	if(name != "") {
		while(child && (child->get_name() != name || !dynamic_cast<xmlpp::Element*>(child))) {
			child = child->get_next_sibling();
		}
	} else {
		while(child && !dynamic_cast<xmlpp::Element*>(child)) {
			child = child->get_next_sibling();
		}
	}
	return child ? static_cast<xmlpp::Element*>(child) : nullptr;
}

std::list<xmlpp::Element*> XmlUtils::elementsByTagName(const xmlpp::Element* element, const Glib::ustring& name) {
	std::list<xmlpp::Element*> elems;
	for(xmlpp::Node* child : element->get_children()) {
		if(xmlpp::Element* childElem = dynamic_cast<xmlpp::Element*>(child)) {
			if(childElem->get_name() == name) {
				elems.push_back(childElem);
			}
			elems.splice(elems.end(), elementsByTagName(childElem, name));
		}
	}
	return elems;
}

Glib::ustring XmlUtils::documentXML(xmlpp::Document* doc) {
	Glib::ustring xml = doc->write_to_string();
	// Strip entity declaration
	if(xml.substr(0, 5) == "<?xml") {
		std::size_t pos = xml.find("?>\n");
		xml = xml.substr(pos + 3);
	}
	return xml;
}

Glib::ustring XmlUtils::elementXML(const xmlpp::Element* element) {
	xmlpp::Document doc;
	doc.create_root_node_by_import(element);
	return documentXML(&doc);
}

xmlpp::Element* XmlUtils::takeChild(xmlpp::Element* parent, xmlpp::Element* child) {
	xmlpp::Element* clone = static_cast<xmlpp::Element*>(dummyElement()->import_node(child));
	parent->remove_child(child);
	return clone;
}

xmlpp::Element* XmlUtils::createElement(const Glib::ustring& name) {
	return dummyElement()->add_child(name);
}

xmlpp::Element* XmlUtils::dummyElement() {
	static xmlpp::Document doc;
	static xmlpp::Element* root = doc.create_root_node("root");
	return root;
}
