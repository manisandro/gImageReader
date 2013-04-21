/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ConfigSettings.hh
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

#ifndef CONFIGSETTINGS_HH
#define CONFIGSETTINGS_HH

#include <gtkmm.h>
#include <tuple>
#include <vector>

#include "common.hh"

class AbstractSetting {
public:
	AbstractSetting(const Glib::ustring& key, Glib::RefPtr<Gio::Settings> settings)
		: m_settings(settings), m_key(key) { }
	virtual ~AbstractSetting() {}
	virtual bool isValid() const{ return true; }
	sigc::signal<void> signal_changed(){ return m_signal_changed; }
	virtual void reread() = 0;

protected:
	Glib::RefPtr<Gio::Settings> m_settings;
	Glib::ustring m_key;
	sigc::signal<void> m_signal_changed;

};

template <class T>
class VarSetting : public AbstractSetting {
public:
	VarSetting(const Glib::ustring& key, Glib::RefPtr<Gio::Settings> settings)
		: AbstractSetting(key, settings) { }

	const T& getValue() const{
		return m_value;
	}
	void setValue(const T& value){
		m_value = value;
		m_settings->set_value(m_key, Glib::Variant<T>::create(m_value));
	}
	void reread(){
		Glib::Variant<T> v;
		m_settings->get_value(m_key, v);
		m_value = v.get();
	}

private:
	T m_value;
};

class SwitchSetting : public AbstractSetting {
public:
	SwitchSetting(const Glib::ustring& key, Glib::RefPtr<Gio::Settings> settings)
		: AbstractSetting(key, settings) {}
	virtual void setValue(bool value) = 0;
	virtual bool getValue() const = 0;
};

template <class T>
class SwitchSettingT : public SwitchSetting {
public:
	SwitchSettingT(const Glib::ustring& key, Glib::RefPtr<Gio::Settings> settings, const Glib::ustring& builderpath)
		: SwitchSetting(key, settings), m_widget(Builder(builderpath))
	{
		m_connection_toggled = CONNECT(m_widget, toggled, [this]{ m_settings->set_value(m_key, Glib::Variant<bool>::create(m_widget->get_active())); });
	}
	void setValue(bool value){
		m_widget->set_active(value);
	}
	bool getValue() const{
		return m_widget->get_active();
	}
	void reread(){
		Glib::Variant<bool> v;
		m_settings->get_value(m_key, v);
		m_connection_toggled.block();
		m_widget->set_active(v.get());
		m_connection_toggled.unblock();
	}

private:
	T* m_widget;
	sigc::connection m_connection_toggled;
};

class ComboSetting : public AbstractSetting {
public:
	ComboSetting(const Glib::ustring& key, Glib::RefPtr<Gio::Settings> settings, const Glib::ustring& builderpath)
		: AbstractSetting(key, settings), m_widget(Builder(builderpath))
	{
		CONNECT(m_widget, changed, [this]{ m_settings->set_value(m_key, Glib::Variant<int>::create(m_widget->get_active_row_number())); });
	}

	void reread(){
		Glib::Variant<int> v;
		m_settings->get_value(m_key, v);
		m_connection_changed.block();
		m_widget->set_active(v.get());
		m_connection_changed.unblock();
	}

private:
	Gtk::ComboBox* m_widget;
	sigc::connection m_connection_changed;
};

class ListStoreSetting : public AbstractSetting {
public:
	ListStoreSetting(const Glib::ustring& key, Glib::RefPtr<Gio::Settings> settings, Glib::RefPtr<Gtk::ListStore> liststore)
		: AbstractSetting(key, settings), m_liststore(liststore)
	{
		m_connection_rowChanged = CONNECT(m_liststore, row_changed, [this](const Path&,const Iter&){ serialize(); });
		m_connection_rowDeleted = CONNECT(m_liststore, row_deleted, [this](const Path&){ serialize(); });
		m_connection_rowInserted = CONNECT(m_liststore, row_inserted, [this](const Path&,const Iter&){ serialize(); });
	}

	void reread();

private:
	Glib::RefPtr<Gtk::ListStore> m_liststore;
	typedef Gtk::TreeModel::Path Path;
	typedef Gtk::TreeModel::iterator Iter;
	sigc::connection m_connection_rowChanged;
	sigc::connection m_connection_rowDeleted;
	sigc::connection m_connection_rowInserted;

	void serialize();
};

#endif // CONFIGSETTINGS_HH
