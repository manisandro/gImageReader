/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Notifier.cc
 * Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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

Notifier::Notifier()
{
	m_notifyEvBox = Builder("evbox:notifier");
	m_notifyBox = Builder("box:notifier");
	m_notifyTitle = Builder("label:notifier.title");
	m_notifyMessage = Builder("label:notifier.message");
	m_notifyEvBox->override_background_color(Gdk::RGBA("#FFE090"), Gtk::STATE_FLAG_NORMAL);
	CONNECT(Builder("button:notifier.close").as<Gtk::Button>(), clicked, [this]{ hide(); });
}

void Notifier::notify(const Glib::ustring &title, const Glib::ustring &message, const std::vector<Action> &actions)
{
	hide();
	m_notifyTitle->set_markup(Glib::ustring::compose("<b>%1:</b>", title));
	m_notifyMessage->set_text(message);
	for(const Action& action : actions){
		Gtk::Button* btn = new Gtk::Button(action.label);
		btn->set_relief(Gtk::RELIEF_NONE);
		CONNECT(btn, clicked, action.action);
		m_notifyBox->pack_start(*btn, Gtk::PACK_SHRINK);
		m_buttons.push_back(btn);
		btn->get_child()->override_color(Gdk::RGBA("#0000FF"), Gtk::STATE_FLAG_NORMAL);
	}
	m_notifyEvBox->show_all();
}

void Notifier::hide()
{
	m_notifyEvBox->hide();
	for(Gtk::Button* btn : m_buttons){
		m_notifyBox->remove(*btn);
		delete btn;
	}
	m_buttons.clear();
}
