/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Notifier.cc
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

#include "Notifier.hh"
#include "Utils.hh"

static Glib::Quark handleQuark("handle");

void Notifier::notify(const Glib::ustring &title, const Glib::ustring &message, const std::vector<Action> &actions, Notifier::Handle* handle)
{
	Gtk::Frame* frame = Gtk::manage(new Gtk::Frame);
	frame->set_data(handleQuark, handle);
	Gtk::Box* box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL, 5));
	Gtk::Label* titlelabel = Gtk::manage(new Gtk::Label);
	Gtk::Label* msglabel = Gtk::manage(new Gtk::Label("", 0.0, 0.5));
	Gtk::Button* closebtn = Gtk::manage(new Gtk::Button);
	frame->set_shadow_type(Gtk::SHADOW_OUT);
	frame->override_background_color(Gdk::RGBA("#FFD000"), Gtk::STATE_FLAG_NORMAL);
	titlelabel->set_markup(Glib::ustring::compose("<b>%1:</b>", title));
	msglabel->set_markup(message);
	closebtn->set_image(*Gtk::manage(Utils::image_from_icon_name("gtk-close", Gtk::ICON_SIZE_MENU)));
	frame->add(*box);
	CONNECT(closebtn, clicked, [frame]{ Notifier::hide(frame); });
	box->pack_start(*titlelabel, false, true);
	box->pack_start(*msglabel, true, true);
	box->pack_end(*closebtn, false, true);
	for(const Action& action : actions){
		Gtk::Button* btn = Gtk::manage(new Gtk::Button(action.label));
		btn->set_relief(Gtk::RELIEF_NONE);
		CONNECT(btn, clicked, [frame,action]{ if(action.action()){ Notifier::hide(frame); }});
		box->pack_start(*btn, false, true);
		btn->get_child()->override_color(Gdk::RGBA("#0000FF"), Gtk::STATE_FLAG_NORMAL);
	}
	frame->show_all();
	Builder("box:main").as<Gtk::Box>()->pack_end(*frame, false, true);
	if(handle != nullptr){
		*handle = frame;
	}
}

void Notifier::hide(Handle handle)
{
	if(handle){
		Gtk::Frame* frame = static_cast<Gtk::Frame*>(handle);
		void* data = frame->get_data(handleQuark);
		if(data != nullptr){
			*static_cast<Gtk::Frame**>(data) = nullptr;
		}
		Builder("box:main").as<Gtk::Box>()->remove(*frame);
	}
}
