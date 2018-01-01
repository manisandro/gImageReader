/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SearchReplaceFrame.hh
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

#ifndef SEARCHREPLACEFRAME_HH
#define SEARCHREPLACEFRAME_HH

#include "common.hh"
#include "ui_SearchReplaceFrame.h"

#include <QFrame>

class SubstitutionsManager;

class SearchReplaceFrame : public QFrame {
	Q_OBJECT
public:
	explicit SearchReplaceFrame(QWidget* parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

public slots:
	void clear();
	void clearErrorState();
	void setErrorState();
	void hideSubstitutionsManager();

signals:
	void findReplace(const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace);
	void replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase);
	void applySubstitutions(const QMap<QString, QString>& substitutions, bool matchCase);

private:
	Ui::SearchReplaceFrame ui;
	SubstitutionsManager* m_substitutionsManager;

private slots:
	void findNext() { emit findReplace(ui.lineEditSearch->text(), ui.lineEditReplace->text(), ui.checkBoxMatchCase->isChecked(), false, false); }
	void findPrev() { emit findReplace(ui.lineEditSearch->text(), ui.lineEditReplace->text(), ui.checkBoxMatchCase->isChecked(), true, false); }
	void replaceNext() { emit findReplace(ui.lineEditSearch->text(), ui.lineEditReplace->text(), ui.checkBoxMatchCase->isChecked(), false, true); }
	void emitReplaceAll() { emit replaceAll(ui.lineEditSearch->text(), ui.lineEditReplace->text(), ui.checkBoxMatchCase->isChecked()); }
	void emitApplySubstitutions(const QMap<QString, QString>& substitutions) { emit applySubstitutions(substitutions, ui.checkBoxMatchCase->isChecked()); }
};

#endif // SEARCHREPLACEFRAME_HH
