/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * common.hh
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

#ifndef COMMON_HH
#define COMMON_HH

#include <glibmm/i18n.h>
#include <gtkmm.h>
#include <libintl.h>

struct ClassDataItem {
	virtual ~ClassDataItem() = default;
};

class ClassData {
public:
	~ClassData() {
		std::for_each(m_connections.begin(), m_connections.end(), [](sigc::connection conn) {
			conn.disconnect();
		});
		for(ClassDataItem* item : m_items) {
			delete item;
		}
	}
	template<class T>
	sigc::connection addConn(const T& conn) {
		m_connections.push_back(conn);
		return m_connections.back();
	}
	void addItem(ClassDataItem* item) {
		m_items.push_back(item);
	}
private:
	std::vector<sigc::connection> m_connections;
	std::vector<ClassDataItem*> m_items;
};

class DataObject {
public:
	virtual ~DataObject() = default;
};

#define CONNECT(src, signal, ...) m_classdata.addConn((src)->signal_##signal().connect(__VA_ARGS__))
#define CONNECTP(src, property, ...) m_classdata.addConn((src)->property_##property().signal_changed().connect(__VA_ARGS__))
#define CONNECTX(src, signal, ...) (src)->signal_##signal().connect(__VA_ARGS__)

#define APPLICATION_ID "org.gnome.gimagereader"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern std::string pkgExePath;
extern std::string pkgDir;

#endif
