/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ConfigSettings.hh
 * Copyright (C) 2013-2018 Sandro Mani <manisandro@gmail.com>
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
#include "FontComboBox.hh"

class AbstractSetting;

#define ADD_SETTING(...) m_classdata.addItem(new __VA_ARGS__)

class ConfigSettings {
public:
	template<class T>
	static T* get(const Glib::ustring& key) {
		auto it = s_settings.find(key);
		return it == s_settings.end() ? nullptr : static_cast<T*>(it->second);
	}

private:
	friend class AbstractSetting;
	static std::map<Glib::ustring, AbstractSetting*> s_settings;

	static void add(AbstractSetting* setting);
	static void remove(const Glib::ustring& key);
};


class AbstractSetting : public ClassDataItem {
public:
	AbstractSetting(const Glib::ustring& key)
		: m_key(key) {
		ConfigSettings::add(this);
	}
	~AbstractSetting() {
		ConfigSettings::remove(m_key);
	}
	const Glib::ustring& key() const {
		return m_key;
	}
	virtual void serialize() {}
	sigc::signal<void> signal_changed() {
		return m_signal_changed;
	}

protected:
	ClassData m_classdata;
	Glib::ustring m_key;
	sigc::signal<void> m_signal_changed;

	static Glib::RefPtr<Gio::Settings> get_default_settings();
};

template <class T>
class VarSetting : public AbstractSetting {
public:
	using AbstractSetting::AbstractSetting;
	T getValue() const {
		Glib::Variant<T> v;
		get_default_settings()->get_value(m_key, v);
		return v.get();
	}
	void setValue(const T& value) {
		get_default_settings()->set_value(m_key, Glib::Variant<T>::create(value));
		m_signal_changed.emit();
	}
};

class FontSetting : public AbstractSetting {
public:
	FontSetting(const Glib::ustring& key, Gtk::FontButton* widget)
		: AbstractSetting(key), m_widget(widget) {
		m_widget->set_font_name(get_default_settings()->get_string(m_key));
		CONNECTP(m_widget, font_name, [this] { serialize(); });
	}
	void serialize() override {
		get_default_settings()->set_string(m_key, m_widget->get_font_name());
		m_signal_changed.emit();
	}
	Glib::ustring getValue() const {
		return m_widget->get_font_name();
	}

private:
	Gtk::FontButton* m_widget;
};

class SwitchSetting : public AbstractSetting {
public:
	using AbstractSetting::AbstractSetting;
	virtual void setValue(bool value) = 0;
	virtual bool getValue() const = 0;
};

template<class T>
class SwitchSettingT : public SwitchSetting {
public:
	SwitchSettingT(const Glib::ustring& key, T* widget)
		: SwitchSetting(key), m_widget(widget) {
		m_widget->set_active(get_default_settings()->get_boolean(m_key));
		CONNECT(m_widget, toggled, [this] { serialize(); });
	}
	void serialize() override {
		get_default_settings()->set_boolean(m_key, m_widget->get_active());
		m_signal_changed.emit();
	}
	void setValue(bool value) override {
		m_widget->set_active(value);
	}
	bool getValue() const override {
		return m_widget->get_active();
	}

private:
	T* m_widget;
};

class ComboSetting : public AbstractSetting {
public:
	ComboSetting(const Glib::ustring& key, Gtk::ComboBox* widget)
		: AbstractSetting(key), m_widget(widget) {
		int idx = get_default_settings()->get_int(m_key);
		int nrows = m_widget->get_model()->children().size();
		m_widget->set_active(std::min(std::max(0, idx), nrows - 1));
		CONNECT(m_widget, changed, [this] { serialize(); });
	}
	void serialize() override {
		get_default_settings()->set_int(m_key, m_widget->get_active_row_number());
		m_signal_changed.emit();
	}
	int getValue() const {
		return m_widget->get_active_row_number();
	}

private:
	Gtk::ComboBox* m_widget;
};

class FontComboSetting : public AbstractSetting {
public:
	FontComboSetting(const Glib::ustring& key, FontComboBox* widget)
		: AbstractSetting(key), m_widget(widget) {
		Glib::ustring font = get_default_settings()->get_string(m_key);
		m_widget->set_active_font(font);
		CONNECT(m_widget, changed, [this] { serialize(); });
	}
	void serialize() override {
		get_default_settings()->set_string(m_key, m_widget->get_active_font());
		m_signal_changed.emit();
	}
	Glib::ustring getValue() const {
		return m_widget->get_active_font();
	}

private:
	FontComboBox* m_widget;
};

class SpinSetting : public AbstractSetting {
public:
	SpinSetting(const Glib::ustring& key, Gtk::SpinButton* widget)
		: AbstractSetting(key), m_widget(widget) {
		int value = get_default_settings()->get_int(m_key);
		m_widget->set_value(value);
		CONNECT(m_widget, value_changed, [this] { serialize(); });
	}
	void serialize() override {
		get_default_settings()->set_int(m_key, m_widget->get_value());
		m_signal_changed.emit();
	}

private:
	Gtk::SpinButton* m_widget;
};

class ListStoreSetting : public AbstractSetting {
public:
	ListStoreSetting(const Glib::ustring& key, Glib::RefPtr<Gtk::ListStore> liststore);
	void serialize() override;

private:
	Glib::RefPtr<Gtk::ListStore> m_liststore;
};

class EntrySetting : public AbstractSetting {
public:
	EntrySetting(const Glib::ustring& key, Gtk::Entry* entry)
		: AbstractSetting(key), m_entry(entry) {
		entry->set_text(get_default_settings()->get_string(m_key));
		CONNECT(m_entry, changed, [this] { serialize(); });
	}
	void serialize() override {
		get_default_settings()->set_string(m_key, m_entry->get_text());
		m_signal_changed.emit();
	}

private:
	Gtk::Entry* m_entry;
};


#endif // CONFIGSETTINGS_HH
