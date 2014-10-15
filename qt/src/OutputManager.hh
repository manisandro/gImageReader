/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputManager.hh
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#ifndef OUTPUTMANAGER_HH
#define OUTPUTMANAGER_HH

#include <QtSpell.hpp>

#include "Config.hh"
#include "MainWindow.hh"

class QDBusInterface;
template <class T> class QDBusReply;
class QDBusError;
class SubstitutionsManager;
class UI_MainWindow;

class OutputManager : public QObject {
	Q_OBJECT
public:
	OutputManager(const UI_MainWindow& _ui);
	void addText(const QString& text, bool insert = false);

public slots:
	bool clearBuffer();
	void setLanguage(const Config::Lang &lang, bool force = false);

private:
	enum class InsertMode { Append, Cursor, Replace };

#ifdef Q_OS_LINUX
	QDBusInterface* m_dbusIface = nullptr;
#endif
	const UI_MainWindow& ui;
	InsertMode m_insertMode;
	QtSpell::TextEditChecker m_spell;
	MainWindow::Notification m_notifierHandle = nullptr;
	SubstitutionsManager* m_substitutionsManager;

	void findReplace(bool backwards, bool replace);
#ifdef Q_OS_LINUX
	void dictionaryAutoinstall();
#endif

private slots:
	void clearErrorState();
	void filterBuffer();
	void findNext();
	void findPrev();
	void replaceAll();
	void replaceNext();
	bool saveBuffer(const QString& filename = "");
	void setFont();
	void setInsertMode(QAction* action);
#ifdef Q_OS_LINUX
	void dictionaryAutoinstallDone();
	void dictionaryAutoinstallError(const QDBusError& error);
#endif
};

#endif // OUTPUTMANAGER_HH
