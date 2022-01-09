/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRProofReadWidget.hh
 * Copyright (C) 2022 Sandro Mani <manisandro@gmail.com>
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

#ifndef HOCRPROOFREADWIDGET_HH
#define HOCRPROOFREADWIDGET_HH

#include <gtkmm.h>
#include "common.hh"

class HOCRItem;


class HOCRProofReadWidget : public Gtk::Window {
public:
	HOCRProofReadWidget(Gtk::TreeView* treeView);
	~HOCRProofReadWidget();
	void clear();
	Gtk::TreeView* documentTree() const { return m_treeView; }
	void setConfidenceLabel(int wconf);
	Glib::ustring confidenceStyle(int wconf) const;
	void adjustFontSize(int diff);

private:
	class LineEdit;
	ClassData m_classdata;

	Gtk::Box* m_mainLayout = nullptr;
	Gtk::TreeView* m_treeView = nullptr;
	Gtk::Box* m_linesLayout = nullptr;
	const HOCRItem* m_currentLine = nullptr;
	Gtk::Box* m_controlsWidget = nullptr;
	Gtk::Label* m_confidenceLabel = nullptr;
	std::map<const HOCRItem*, Gtk::Box*> m_currentLines;
	Gtk::Window* m_settingsMenu = nullptr;
	Gtk::SpinButton* m_spinLinesBefore = nullptr;
	Gtk::SpinButton* m_spinLinesAfter = nullptr;
	Gtk::Button* m_settingsButton = nullptr;
	int m_fontSizeDiff = 0;

	// TODO: Disable auto tab handling
//	bool focusNextPrevChild(bool) override { return false; }

	void showSettingsMenu();
	void repositionWidget();
	void updateWidget(bool force = false);
	void showShortcutsDialog();
};

#endif // HOCRPROOFREADWIDGET_HH
