/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * common.hh
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#include <gtkmm.h>
#include <iostream>
#include <libintl.h>
#include <glibmm/i18n.h>

namespace sigc{

#ifndef SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
#define SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE \
template <typename T_functor>          \
struct functor_trait<T_functor, false> \
{                                      \
  typedef typename functor_trait<decltype(&T_functor::operator()), false>::result_type result_type; \
  typedef T_functor functor_type;      \
};
#endif

SIGC_FUNCTORS_DEDUCE_RESULT_TYPE_WITH_DECLTYPE
}

#define CONNECT(src, signal, ...) (src)->signal_##signal().connect(__VA_ARGS__)
#define CONNECTS(src, signal, ...) (src)->signal_##signal().connect(sigc::bind(__VA_ARGS__, src))
#define CONNECTP(src, property, ...) (src)->property_##property().signal_changed().connect(__VA_ARGS__)
#define CONNECTPS(src, property, ...) {auto sender = src; sender->property_##property().signal_changed().connect(sigc::bind(__VA_ARGS__, sender)); }

#ifdef SRCTREE_EXEC // Execute from source tree
#undef  PACKAGE_DATA_DIR
#define PACKAGE_DATA_DIR "data/"
#endif

#define APPLICATION_ID "org.gnome.gimagereader"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Builder {
public:
	static Glib::RefPtr<Gtk::Builder> builder;
	Builder(const Glib::ustring& name){
		builder->get_widget(name, m_widget);
	}
	template <class T>
	operator T*(){ return (T*)m_widget; }

	template <class T>
	T* as(){ return (T*)m_widget; }
private:
	Gtk::Widget* m_widget;
};

#endif
