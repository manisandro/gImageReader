/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SearchReplaceFrame.cc
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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
#include "Utils.hh"

SearchReplaceFrame::SearchReplaceFrame()
{
	ui.setupUi();

	m_substitutionsManager = new SubstitutionsManager();

	CONNECT(ui.checkbuttonMatchcase, toggled, [this]{ clearErrorState(); });
	CONNECT(ui.entrySearch, changed, [this]{ clearErrorState(); });
	CONNECT(ui.entrySearch, activate, [this]{ findNext(); });
	CONNECT(ui.entryReplace, activate, [this]{ replaceNext(); });
	CONNECT(ui.buttonSearchnext, clicked, [this] { findNext(); });
	CONNECT(ui.buttonSearchprev, clicked, [this] { findPrev(); });
	CONNECT(ui.buttonReplace, clicked, [this] { replaceNext(); });
	CONNECT(ui.buttonReplaceall, clicked, [this] { emitReplaceAll(); });
	CONNECT(ui.buttonSubstitutions, clicked, [this] { m_substitutionsManager->set_visible(true); });
	CONNECT(m_substitutionsManager, apply_substitutions, [this]( const std::map<Glib::ustring,Glib::ustring>& substitutions) { emitApplySubstitutions(substitutions); });

	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("searchmatchcase", ui.checkbuttonMatchcase));
}

SearchReplaceFrame::~SearchReplaceFrame()
{
	delete m_substitutionsManager;
}

void SearchReplaceFrame::clear()
{
	ui.entrySearch->set_text("");
	ui.entryReplace->set_text("");
}

void SearchReplaceFrame::clearErrorState()
{
	Utils::clear_error_state(ui.entrySearch);
}

void SearchReplaceFrame::setErrorState()
{
	Utils::set_error_state(ui.entrySearch);
}

void SearchReplaceFrame::hideSubstitutionsManager()
{
	m_substitutionsManager->set_visible(false);
}
