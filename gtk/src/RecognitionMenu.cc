/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * RecognitionMenu.cc
 * Copyright (C) 2022-2025 Sandro Mani <manisandro@gmail.com>
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

#include <gtkspellmm.h>

#include "ConfigSettings.hh"
#include "MainWindow.hh"
#include "RecognitionMenu.hh"
#include "Utils.hh"


class RecognitionMenu::MultilingualMenuItem : public Gtk::RadioMenuItem {
public:
	MultilingualMenuItem(Group& groupx, const Glib::ustring& label)
		: Gtk::RadioMenuItem(groupx, label) {
		CONNECT(this, select, [this] {
			if (!m_selected) {
				m_ignoreNextActivate = true;
			}
			m_selected = true;
		});
		CONNECT(this, deselect, [this] {
			m_selected = false;
			m_ignoreNextActivate = false;
		});
		this->signal_button_press_event().connect([this](GdkEventButton* /*ev*/) {
			set_active(true);
			return true;
		}, false);
	}

protected:
	void on_activate() override {
		if (!m_ignoreNextActivate) {
			Gtk::RadioMenuItem::on_activate();
		}
		m_ignoreNextActivate = false;
	}

private:
	ClassData m_classdata;
	bool m_selected = false;
	bool m_ignoreNextActivate = false;
};


RecognitionMenu::RecognitionMenu() {

	m_charListDialogUi.setupUi();
	m_charListDialogUi.dialogCharacterLists->set_transient_for(*MAIN->getWindow());

	CONNECT(m_charListDialogUi.radioButtonBlacklist, toggled, [this] { m_charListDialogUi.entryBlacklist->set_sensitive(m_charListDialogUi.radioButtonBlacklist->get_active()); });
	CONNECT(m_charListDialogUi.radioButtonWhitelist, toggled, [this] { m_charListDialogUi.entryWhitelist->set_sensitive(m_charListDialogUi.radioButtonWhitelist->get_active()); });

	ADD_SETTING(VarSetting<Glib::ustring> ("language"));
	ADD_SETTING(VarSetting<int> ("psm"));
	ADD_SETTING(EntrySetting("ocrcharwhitelist", m_charListDialogUi.entryWhitelist));
	ADD_SETTING(EntrySetting("ocrcharblacklist", m_charListDialogUi.entryBlacklist));
	ADD_SETTING(SwitchSettingT<Gtk::RadioButton> ("ocrblacklistenabled", m_charListDialogUi.radioButtonBlacklist));
	ADD_SETTING(SwitchSettingT<Gtk::RadioButton> ("ocrwhitelistenabled", m_charListDialogUi.radioButtonWhitelist));
}

void RecognitionMenu::rebuild() {
	foreach ([this](Gtk::Widget& w) {
	remove(w);
	});
	m_langMenuRadioGroup = Gtk::RadioButtonGroup();
	m_langMenuCheckGroup = std::map<Gtk::CheckMenuItem*, std::pair<Glib::ustring, gint64 >> ();
	m_psmRadioGroup = Gtk::RadioButtonGroup();
	m_curLang = Config::Lang();
	m_signal_languageChanged.emit(m_curLang);
	Gtk::RadioMenuItem* curitem = nullptr;
	Gtk::RadioMenuItem* activeitem = nullptr;
	bool haveOsd = false;

	std::vector<Glib::ustring> parts = Utils::string_split(ConfigSettings::get<VarSetting<Glib::ustring >> ("language")->getValue(), ':');
	Config::Lang curlang = {parts.empty() ? "eng" : parts[0], parts.size() < 2 ? "" : parts[1]};

	std::vector<Glib::ustring> dicts = GtkSpell::Checker::get_language_list();

	std::vector<Glib::ustring> availLanguages = Config::getAvailableLanguages();

	if (availLanguages.empty()) {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("No languages available"), _("No tesseract languages are available for use. Recognition will not work."));
	}

	// Add menu items for languages, with spelling submenu if available
	for (const Glib::ustring& langprefix : availLanguages) {
		if (langprefix == "osd") {
			haveOsd = true;
			continue;
		}
		Config::Lang lang = {langprefix};
		if (!MAIN->getConfig()->searchLangSpec(lang)) {
			lang.name = lang.prefix;
		}
		std::vector<Glib::ustring> spelldicts;
		if (!lang.code.empty()) {
			for (const Glib::ustring& dict : dicts) {
				if (dict.substr(0, 2) == lang.code.substr(0, 2)) {
					spelldicts.push_back(dict);
				}
			}
			std::sort(spelldicts.begin(), spelldicts.end());
		}
		if (!spelldicts.empty()) {
			Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(lang.name));
			Gtk::Menu* submenu = Gtk::manage(new Gtk::Menu);
			for (const Glib::ustring& dict : spelldicts) {
				Config::Lang itemlang = {lang.prefix, dict, lang.name};
				curitem = Gtk::manage(new Gtk::RadioMenuItem(m_langMenuRadioGroup, GtkSpell::Checker::decode_language_code(dict)));
				CONNECT(curitem, toggled, [this, curitem, itemlang] { setLanguage(curitem, itemlang); });
				if (curlang.prefix == lang.prefix && (
				            curlang.code == dict ||
				            (!activeitem && (curlang.code == dict.substr(0, 2) || curlang.code.empty())))) {
					curlang = itemlang;
					activeitem = curitem;
				}
				submenu->append(*curitem);
			}
			item->set_submenu(*submenu);
			append(*item);
		} else {
			curitem = Gtk::manage(new Gtk::RadioMenuItem(m_langMenuRadioGroup, lang.name));
			CONNECT(curitem, toggled, [this, curitem, lang] { setLanguage(curitem, lang); });
			if (curlang.prefix == lang.prefix) {
				curlang = lang;
				activeitem = curitem;
			}
			append(*curitem);
		}
	}
	// Add multilanguage menu
	bool isMultilingual = false;
	if (!availLanguages.empty()) {
		append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
		m_multilingualRadio = Gtk::manage(new MultilingualMenuItem(m_langMenuRadioGroup, _("Multilingual")));
		Gtk::Menu* submenu = Gtk::manage(new Gtk::Menu);
		isMultilingual = curlang.prefix.find('+') != curlang.prefix.npos;
		std::vector<Glib::ustring> sellangs = Utils::string_split(curlang.prefix, '+', false);
		for (const Glib::ustring& langprefix : availLanguages) {
			if (langprefix == "osd") {
				continue;
			}
			Config::Lang lang = {langprefix};
			if (!MAIN->getConfig()->searchLangSpec(lang)) {
				lang.name = lang.prefix;
			}
			Gtk::CheckMenuItem* item = Gtk::manage(new Gtk::CheckMenuItem(lang.name));
			auto it = std::find(sellangs.begin(), sellangs.end(), lang.prefix);
			item->set_active(isMultilingual && it != sellangs.end());
			CONNECT(item, button_press_event, [this, item](GdkEventButton * ev) {
				return onMultilingualItemButtonEvent(ev, item);
			}, false);
			CONNECT(item, button_release_event, [this, item](GdkEventButton * ev) {
				return onMultilingualItemButtonEvent(ev, item);
			}, false);
			submenu->append(*item);
			m_langMenuCheckGroup.insert(std::make_pair(item, std::make_pair(lang.prefix, it != sellangs.end() ? std::distance(sellangs.begin(), it) : -1)));
		}
		m_multilingualRadio->set_submenu(*submenu);
		CONNECT(m_multilingualRadio, toggled, [this] { if (m_multilingualRadio->get_active()) setMultiLanguage(); });
		CONNECT(submenu, button_press_event, [this](GdkEventButton * ev) {
			return onMultilingualMenuButtonEvent(ev);
		}, false);
		CONNECT(submenu, button_release_event, [this](GdkEventButton * ev) {
			return onMultilingualMenuButtonEvent(ev);
		}, false);
		append(*m_multilingualRadio);
	}
	if (isMultilingual) {
		activeitem = m_multilingualRadio;
		setMultiLanguage();
	} else if (activeitem == nullptr) {
		activeitem = curitem;
	}

	// Add PSM items
	append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	Gtk::Menu* psmMenu = Gtk::manage(new Gtk::Menu);
	int activePsm = ConfigSettings::get<VarSetting<int >> ("psm")->getValue();

	struct PsmEntry {
		Glib::ustring label;
		tesseract::PageSegMode psmMode;
		bool requireOsd;
	};
	std::vector<PsmEntry> psmModes = {
		PsmEntry{_("Automatic page segmentation"), tesseract::PSM_AUTO, false},
		PsmEntry{_("Page segmentation with orientation and script detection"), tesseract::PSM_AUTO_OSD, true},
		PsmEntry{_("Assume single column of text"), tesseract::PSM_SINGLE_COLUMN, false},
		PsmEntry{_("Assume single block of vertically aligned text"), tesseract::PSM_SINGLE_BLOCK_VERT_TEXT, false},
		PsmEntry{_("Assume a single uniform block of text"), tesseract::PSM_SINGLE_BLOCK, false},
		PsmEntry{_("Assume a line of text"), tesseract::PSM_SINGLE_LINE, false},
		PsmEntry{_("Assume a single word"), tesseract::PSM_SINGLE_WORD, false},
		PsmEntry{_("Assume a single word in a circle"), tesseract::PSM_CIRCLE_WORD, false},
		PsmEntry{_("Sparse text in no particular order"), tesseract::PSM_SPARSE_TEXT, false},
		PsmEntry{_("Sparse text with orientation and script detection"), tesseract::PSM_SPARSE_TEXT_OSD, true}
	};
	for (const auto& entry : psmModes) {
		Gtk::RadioMenuItem* item = Gtk::manage(new Gtk::RadioMenuItem(m_psmRadioGroup, entry.label));
		item->set_sensitive(!entry.requireOsd || haveOsd);
		item->set_active(entry.psmMode == activePsm);
		CONNECT(item, toggled, [entry, item] {
			if (item->get_active()) {
				ConfigSettings::get<VarSetting<int >> ("psm")->setValue(entry.psmMode);
			}
		});
		psmMenu->append(*item);
	}

	Gtk::MenuItem* psmItem = Gtk::manage(new Gtk::MenuItem(_("Page segmentation mode")));
	psmItem->set_submenu(*psmMenu);
	append(*psmItem);

	Gtk::MenuItem* charlistItem = Gtk::manage(new Gtk::MenuItem(_("Character whitelist / blacklist...")));
	CONNECT(charlistItem, activate, [this] { manageCharaterLists(); });
	append(*charlistItem);

	// Add installer item
	append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	Gtk::MenuItem* manageItem = Gtk::manage(new Gtk::MenuItem(_("Manage languages...")));
	CONNECT(manageItem, activate, [this] { MAIN->manageLanguages(); });
	append(*manageItem);

	show_all();
	if (activeitem) {
		activeitem->set_active(true);
		activeitem->toggled(); // Ensure signal is emitted
	}
}

bool RecognitionMenu::onMultilingualMenuButtonEvent(GdkEventButton* ev) {
	Gtk::Allocation alloc = m_multilingualRadio->get_allocation();
	int radio_x_root, radio_y_root;
	m_multilingualRadio->get_window()->get_root_coords(alloc.get_x(), alloc.get_y(), radio_x_root, radio_y_root);

	if (ev->x_root >= radio_x_root && ev->x_root <= radio_x_root + alloc.get_width() &&
	        ev->y_root >= radio_y_root && ev->y_root <= radio_y_root + alloc.get_height()) {
		if (ev->type == GDK_BUTTON_PRESS) {
			m_multilingualRadio->set_active(true);
		}
		return true;
	}
	return false;
}

bool RecognitionMenu::onMultilingualItemButtonEvent(GdkEventButton* ev, Gtk::CheckMenuItem* item) {
	Gtk::Allocation alloc = item->get_allocation();
	int item_x_root, item_y_root;
	item->get_window()->get_root_coords(alloc.get_x(), alloc.get_y(), item_x_root, item_y_root);

	if (ev->x_root >= item_x_root && ev->x_root <= item_x_root + alloc.get_width() &&
	        ev->y_root >= item_y_root && ev->y_root <= item_y_root + alloc.get_height()) {
		if (ev->type == GDK_BUTTON_RELEASE) {
			m_langMenuCheckGroup[item].second = Glib::DateTime::create_now_utc().to_unix();
			item->set_active(!item->get_active());
			setMultiLanguage();
		}
		return true;
	}
	return false;
}

tesseract::PageSegMode RecognitionMenu::getPageSegmentationMode() const {
	return static_cast<tesseract::PageSegMode> (ConfigSettings::get<VarSetting<int >> ("psm")->getValue());
}

Glib::ustring RecognitionMenu::getCharacterWhitelist() const {
	return m_charListDialogUi.radioButtonWhitelist->get_active() ? m_charListDialogUi.entryWhitelist->get_text() : Glib::ustring();
}

Glib::ustring RecognitionMenu::getCharacterBlacklist() const {
	return m_charListDialogUi.radioButtonBlacklist->get_active() ? m_charListDialogUi.entryBlacklist->get_text() : Glib::ustring();
}

void RecognitionMenu::setLanguage(const Gtk::RadioMenuItem* item, const Config::Lang& lang) {
	if (item->get_active()) {
		m_curLang = lang;
		ConfigSettings::get<VarSetting<Glib::ustring >> ("language")->setValue(lang.prefix + ":" + lang.code);
		m_signal_languageChanged.emit(m_curLang);
	}
}

void RecognitionMenu::setMultiLanguage() {
	m_multilingualRadio->set_active(true);
	std::vector<std::pair<Glib::ustring, gint64 >> langs;
	for (const auto& pair : m_langMenuCheckGroup) {
		if (pair.first->get_active()) {
			langs.push_back(std::make_pair(pair.second.first, pair.second.second));
		}
	}
	std::sort(langs.begin(), langs.end(), [](auto a, auto b) { return a.second < b.second; });
	Glib::ustring lang;
	if (langs.empty()) {
		lang = "eng+";
	}
	else {
		for (const auto& pair : langs) {
			lang += pair.first + "+";
		}
	}
	lang = lang.substr(0, lang.length() - 1);
	m_curLang = {lang, "", "Multilingual"};
	ConfigSettings::get<VarSetting<Glib::ustring >> ("language")->setValue(lang + ":");
	m_signal_languageChanged.emit(m_curLang);
}

void RecognitionMenu::manageCharaterLists() {
	m_charListDialogUi.dialogCharacterLists->run();
	m_charListDialogUi.dialogCharacterLists->hide();
}
