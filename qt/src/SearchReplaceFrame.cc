/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SearchReplaceFrame.cc
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#include "ConfigSettings.hh"
#include "SearchReplaceFrame.hh"
#include "SubstitutionsManager.hh"

SearchReplaceFrame::SearchReplaceFrame(QWidget* parent, Qt::WindowFlags f)
	: QFrame(parent, f) {
	ui.setupUi(this);

	m_substitutionsManager = new SubstitutionsManager(this);

	connect(ui.checkBoxMatchCase, &QCheckBox::toggled, this, &SearchReplaceFrame::clearErrorState);
	connect(ui.lineEditSearch, &QLineEdit::textChanged, this, &SearchReplaceFrame::clearErrorState);
	connect(ui.lineEditSearch, &QLineEdit::returnPressed, this, &SearchReplaceFrame::findNext);
	connect(ui.lineEditReplace, &QLineEdit::returnPressed, this, &SearchReplaceFrame::replaceNext);
	connect(ui.toolButtonFindNext, &QToolButton::clicked, this, &SearchReplaceFrame::findNext);
	connect(ui.toolButtonFindPrev, &QToolButton::clicked, this, &SearchReplaceFrame::findPrev);
	connect(ui.toolButtonReplace, &QToolButton::clicked, this, &SearchReplaceFrame::replaceNext);
	connect(ui.toolButtonReplaceAll, &QToolButton::clicked, this, &SearchReplaceFrame::emitReplaceAll);

	connect(ui.pushButtonSubstitutions, &QPushButton::clicked, m_substitutionsManager, &SubstitutionsManager::show);
	connect(ui.pushButtonSubstitutions, &QPushButton::clicked, m_substitutionsManager, &SubstitutionsManager::raise);
	connect(m_substitutionsManager, &SubstitutionsManager::applySubstitutions, this, &SearchReplaceFrame::emitApplySubstitutions);

	ADD_SETTING(SwitchSetting("searchmatchcase", ui.checkBoxMatchCase));
}

void SearchReplaceFrame::clear() {
	ui.lineEditSearch->clear();
	ui.lineEditReplace->clear();
}

void SearchReplaceFrame::clearErrorState() {
	ui.lineEditSearch->setStyleSheet("");
}

void SearchReplaceFrame::setErrorState() {
	ui.lineEditSearch->setStyleSheet("background: #FF7777; color: #FFFFFF;");
}

void SearchReplaceFrame::hideSubstitutionsManager() {
	m_substitutionsManager->hide();
}
