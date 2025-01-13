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

#include <QActionGroup>
#include <QDateTime>
#include <QMessageBox>
#include <QMouseEvent>
#include <QtSpell.hpp>

#include "ConfigSettings.hh"
#include "MainWindow.hh"
#include "RecognitionMenu.hh"


RecognitionMenu::RecognitionMenu(QWidget* parent)
	: QMenu(parent) {

	m_charListDialog = new QDialog(this);
	m_charListDialogUi.setupUi(m_charListDialog);

	connect(m_charListDialogUi.radioButtonBlacklist, &QRadioButton::toggled, m_charListDialogUi.lineEditBlacklist, &QLineEdit::setEnabled);
	connect(m_charListDialogUi.radioButtonWhitelist, &QRadioButton::toggled, m_charListDialogUi.lineEditWhitelist, &QLineEdit::setEnabled);

	ADD_SETTING(VarSetting<QString> ("language", "eng:en_EN"));
	ADD_SETTING(VarSetting<int> ("psm", 6));
	ADD_SETTING(LineEditSetting("ocrcharwhitelist", m_charListDialogUi.lineEditWhitelist));
	ADD_SETTING(LineEditSetting("ocrcharblacklist", m_charListDialogUi.lineEditBlacklist));
	ADD_SETTING(SwitchSetting("ocrblacklistenabled", m_charListDialogUi.radioButtonBlacklist, true));
	ADD_SETTING(SwitchSetting("ocrwhitelistenabled", m_charListDialogUi.radioButtonWhitelist, false));

	installEventFilter(this);
}

void RecognitionMenu::rebuild() {
	clear();
	delete m_langMenuRadioGroup;
	m_langMenuRadioGroup = new QActionGroup(this);
	delete m_langMenuCheckGroup;
	m_langMenuCheckGroup = new QActionGroup(this);
	m_langMenuCheckGroup->setExclusive(false);
	delete m_psmCheckGroup;
	m_psmCheckGroup = new QActionGroup(this);
	connect(m_psmCheckGroup, &QActionGroup::triggered, this, &RecognitionMenu::psmSelected);
	delete m_menuMultilanguage;
	m_menuMultilanguage = nullptr;
	m_curLang = Config::Lang();
	emit languageChanged(m_curLang);
	QAction* curitem = nullptr;
	QAction* activeitem = nullptr;
	bool haveOsd = false;

	QStringList parts = ConfigSettings::get<VarSetting<QString >> ("language")->getValue().split(":");
	Config::Lang curlang = {parts.empty() ? "eng" : parts[0], parts.size() < 2 ? "" : parts[1], parts.size() < 3 ? "" : parts[2]};

	QList<QString> dicts = QtSpell::Checker::getLanguageList();

	QStringList availLanguages = Config::getAvailableLanguages();

	if (availLanguages.empty()) {
		QMessageBox::warning(MAIN, _("No languages available"), _("No tesseract languages are available for use. Recognition will not work."));
	}

	// Add menu items for languages, with spelling submenu if available
	for (const QString& langprefix : availLanguages) {
		if (langprefix == "osd") {
			haveOsd = true;
			continue;
		}
		Config::Lang lang = {langprefix, QString(), QString() };
		if (!MAIN->getConfig()->searchLangSpec(lang)) {
			lang.name = lang.prefix;
		}
		QList<QString> spelldicts;
		if (!lang.code.isEmpty()) {
			for (const QString& dict : dicts) {
				if (dict.left(2) == lang.code.left(2)) {
					spelldicts.append(dict);
				}
			}
			std::sort(spelldicts.begin(), spelldicts.end());
		}
		if (!spelldicts.empty()) {
			QAction* item = new QAction(lang.name, this);
			QMenu* submenu = new QMenu();
			for (const QString& dict : spelldicts) {
				Config::Lang itemlang = {lang.prefix, dict, lang.name};
				curitem = new QAction(QtSpell::Checker::decodeLanguageCode(dict), m_langMenuRadioGroup);
				curitem->setCheckable(true);
				curitem->setData(QVariant::fromValue(itemlang));
				connect(curitem, &QAction::triggered, this, &RecognitionMenu::setLanguage);
				if (curlang.prefix == lang.prefix && (
				            curlang.code == dict ||
				            (!activeitem && (curlang.code == dict.left(2) || curlang.code.isEmpty())))) {
					curlang = itemlang;
					activeitem = curitem;
				}
				submenu->addAction(curitem);
			}
			item->setMenu(submenu);
			addAction(item);
		} else {
			curitem = new QAction(lang.name, m_langMenuRadioGroup);
			curitem->setCheckable(true);
			curitem->setData(QVariant::fromValue(lang));
			connect(curitem, &QAction::triggered, this, &RecognitionMenu::setLanguage);
			if (curlang.prefix == lang.prefix) {
				curlang = lang;
				activeitem = curitem;
			}
			addAction(curitem);
		}
	}

	// Add multilanguage menu
	bool isMultilingual = false;
	if (!availLanguages.isEmpty()) {
		addSeparator();
		m_multilingualAction = new QAction(_("Multilingual"), m_langMenuRadioGroup);
		m_multilingualAction->setCheckable(true);
		m_menuMultilanguage = new QMenu();
		isMultilingual = curlang.prefix.contains('+');
		QStringList sellangs = curlang.prefix.split('+', Qt::SkipEmptyParts);
		for (const QString& langprefix : availLanguages) {
			if (langprefix == "osd") {
				continue;
			}
			Config::Lang lang = {langprefix, "", ""};
			if (!MAIN->getConfig()->searchLangSpec(lang)) {
				lang.name = lang.prefix;
			}
			int index = sellangs.indexOf(lang.prefix);
			QAction* item = new QAction(lang.name, m_langMenuCheckGroup);
			item->setCheckable(true);
			item->setData(QVariant::fromValue(lang.prefix));
			item->setChecked(isMultilingual && index != -1);
			item->setProperty("last_click", QDateTime::fromSecsSinceEpoch(1 + index));    // Hack to preserve order
			connect(item, &QAction::triggered, item, [item] { item->setProperty("last_click", QDateTime::currentDateTime()); });
			connect(item, &QAction::triggered, this, &RecognitionMenu::setMultiLanguage);
			m_menuMultilanguage->addAction(item);
		}
		m_menuMultilanguage->installEventFilter(this);
		m_multilingualAction->setMenu(m_menuMultilanguage);
		addAction(m_multilingualAction);
	}
	if (isMultilingual) {
		activeitem = m_multilingualAction;
		setMultiLanguage();
	} else if (activeitem == nullptr) {
		activeitem = curitem;
	}
	if (activeitem) {
		activeitem->trigger();
	}

	// Add PSM items
	addSeparator();
	QMenu* psmMenu = new QMenu(this);
	int activePsm = ConfigSettings::get<VarSetting<int >> ("psm")->getValue();

	struct PsmEntry {
		QString label;
		tesseract::PageSegMode psmMode;
		bool requireOsd;
	};
	QVector<PsmEntry> psmModes = {
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
		QAction* item = psmMenu->addAction(entry.label);
		item->setData(entry.psmMode);
		item->setEnabled(!entry.requireOsd || haveOsd);
		item->setCheckable(true);
		item->setChecked(activePsm == entry.psmMode);
		m_psmCheckGroup->addAction(item);
	}

	QAction* psmAction = new QAction(_("Page segmentation mode"), this);
	psmAction->setMenu(psmMenu);
	addAction(psmAction);
	addAction(_("Character whitelist / blacklist..."), m_charListDialog, &QDialog::exec);


	// Add installer item
	addSeparator();
	addAction(_("Manage languages..."), MAIN, &MainWindow::manageLanguages);
}

tesseract::PageSegMode RecognitionMenu::getPageSegmentationMode() const {
	return static_cast<tesseract::PageSegMode> (m_psmCheckGroup->checkedAction()->data().toInt());
}

QString RecognitionMenu::getCharacterWhitelist() const {
	return m_charListDialogUi.radioButtonWhitelist->isChecked() ? m_charListDialogUi.lineEditWhitelist->text() : QString();
}

QString RecognitionMenu::getCharacterBlacklist() const {
	return m_charListDialogUi.radioButtonBlacklist->isChecked() ? m_charListDialogUi.lineEditBlacklist->text() : QString();
}

void RecognitionMenu::psmSelected(QAction* action) {
	ConfigSettings::get<VarSetting<int >> ("psm")->setValue(action->data().toInt());
}

void RecognitionMenu::setLanguage() {
	QAction* item = qobject_cast<QAction*> (QObject::sender());
	if (item->isChecked()) {
		m_curLang = item->data().value<Config::Lang>();
		ConfigSettings::get<VarSetting<QString >> ("language")->setValue(m_curLang.prefix + ":" + m_curLang.code);
		emit languageChanged(m_curLang);
	}
}

void RecognitionMenu::setMultiLanguage() {
	m_multilingualAction->setChecked(true);
	QList<QPair<QString, QDateTime >> langs;
	for (QAction* action : m_langMenuCheckGroup->actions()) {
		if (action->isChecked()) {
			langs.append(qMakePair(action->data().toString(), action->property("last_click").toDateTime()));
		}
	}
	std::sort(langs.begin(), langs.end(), [](auto a, auto b) { return a.second < b.second; });
	QString lang;
	if (langs.isEmpty()) {
		lang = "eng+";
	} else {
		for (const auto& pair : langs) {
			lang += pair.first + "+";
		}
	}
	lang = lang.left(lang.length() - 1);
	m_curLang = {lang, "", "Multilingual"};
	ConfigSettings::get<VarSetting<QString >> ("language")->setValue(lang + ":");
	emit languageChanged(m_curLang);
}

bool RecognitionMenu::eventFilter(QObject* obj, QEvent* ev) {
	if (obj == this && ev->type() == QEvent::MouseButtonPress) {
		QMouseEvent* mouseEvent = static_cast<QMouseEvent*> (ev);
		QAction* actionAtPos = actionAt(mouseEvent->pos());
		if (actionAtPos && actionAtPos == m_multilingualAction) {
			m_multilingualAction->toggle();
			if (m_multilingualAction->isChecked()) {
				setMultiLanguage();
			}
			return true;
		}
	} else if (obj == m_menuMultilanguage && (ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::MouseButtonRelease)) {
		QMouseEvent* mouseEvent = static_cast<QMouseEvent*> (ev);
		QAction* action = m_menuMultilanguage->actionAt(mouseEvent->pos());
		if (action) {
			if (ev->type() == QEvent::MouseButtonRelease) {
				action->trigger();
			}
			return true;
		}
	}
	return QObject::eventFilter(obj, ev);
}
