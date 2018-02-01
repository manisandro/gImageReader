/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SearchReplaceFrame.cc
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

#include "ConfigSettings.hh"
#include "SearchReplaceFrame.hh"
#include "SubstitutionsManager.hh"

SearchReplaceFrame::SearchReplaceFrame(QWidget* parent, Qt::WindowFlags f)
	: QFrame(parent, f) {
	ui.setupUi(this);

	m_substitutionsManager = new SubstitutionsManager(this);

	connect(ui.checkBoxMatchCase, SIGNAL(toggled(bool)), this, SLOT(clearErrorState()));
	connect(ui.lineEditSearch, SIGNAL(textChanged(QString)), this, SLOT(clearErrorState()));
	connect(ui.lineEditSearch, SIGNAL(returnPressed()), this, SLOT(findNext()));
	connect(ui.lineEditReplace, SIGNAL(returnPressed()), this, SLOT(replaceNext()));
	connect(ui.toolButtonFindNext, SIGNAL(clicked()), this, SLOT(findNext()));
	connect(ui.toolButtonFindPrev, SIGNAL(clicked()), this, SLOT(findPrev()));
	connect(ui.toolButtonReplace, SIGNAL(clicked()), this, SLOT(replaceNext()));
	connect(ui.toolButtonReplaceAll, SIGNAL(clicked()), this, SLOT(emitReplaceAll()));

	connect(ui.pushButtonSubstitutions, SIGNAL(clicked()), m_substitutionsManager, SLOT(show()));
	connect(ui.pushButtonSubstitutions, SIGNAL(clicked()), m_substitutionsManager, SLOT(raise()));
	connect(m_substitutionsManager, SIGNAL(applySubstitutions(QMap<QString, QString>)), this, SLOT(emitApplySubstitutions(QMap<QString, QString>)));

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
