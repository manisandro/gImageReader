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

#include "Config.hh"
#include "MainWindow.hh"
#include "SearchReplaceFrame.hh"
#include "SubstitutionsManager.hh"
#include "Utils.hh"

SearchReplaceFrame::SearchReplaceFrame()
	: m_builder("/org/gnome/gimagereader/searchreplace.ui")
{
	m_widget = m_builder("box:searchreplace");
	m_lineEditSearch = m_builder("entry:search");
	m_lineEditReplace = m_builder("entry:replace");
	m_checkBoxMatchCase = m_builder("checkbutton:matchcase");
	m_substitutionsManager = new SubstitutionsManager(m_builder);

	CONNECT(m_checkBoxMatchCase, toggled, [this]{ clearErrorState(); });
	CONNECT(m_lineEditSearch, changed, [this]{ clearErrorState(); });
	CONNECT(m_lineEditSearch, activate, [this]{ findNext(); });
	CONNECT(m_lineEditReplace, activate, [this]{ replaceNext(); });
	CONNECT(m_builder("button:searchnext").as<Gtk::Button>(), clicked, [this] { findNext(); });
	CONNECT(m_builder("button:searchprev").as<Gtk::Button>(), clicked, [this] { findPrev(); });
	CONNECT(m_builder("button:replace").as<Gtk::Button>(), clicked, [this] { replaceNext(); });
	CONNECT(m_builder("button:replaceall").as<Gtk::Button>(), clicked, [this] { emitReplaceAll(); });

	CONNECT(m_builder("button:substitutions").as<Gtk::Button>(), clicked, [this] { m_substitutionsManager->set_visible(true); });
	CONNECT(m_substitutionsManager, apply_substitutions, [this]( const std::map<Glib::ustring,Glib::ustring>& substitutions) { emitApplySubstitutions(substitutions); });

	MAIN->getConfig()->addSetting(new SwitchSettingT<Gtk::CheckButton>("searchmatchcase", m_checkBoxMatchCase));
}

SearchReplaceFrame::~SearchReplaceFrame()
{
	delete m_substitutionsManager;
	MAIN->getConfig()->removeSetting("searchmatchcase");
}

void SearchReplaceFrame::clear()
{
	m_lineEditSearch->set_text("");
	m_lineEditReplace->set_text("");
}

void SearchReplaceFrame::clearErrorState()
{
	Utils::clear_error_state(m_lineEditSearch);
}

void SearchReplaceFrame::setErrorState()
{
	Utils::set_error_state(m_lineEditSearch);
}

void SearchReplaceFrame::hideSubstitutionsManager()
{
	m_substitutionsManager->set_visible(false);
}
