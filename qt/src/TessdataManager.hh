/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * TessdataManager.hh
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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

#ifndef TESSDATAMANAGER_HH
#define TESSDATAMANAGER_HH

#include <QDialog>
#include <QMap>

class QListWidget;

class TessdataManager : public QDialog {
	Q_OBJECT
public:
	TessdataManager(QWidget* parent = 0);
	bool setup();

private:
	struct LangFile {
		QString name;
		QString url;
	};

	QListWidget* m_languageList;
	QMap<QString,QList<LangFile>> m_languageFiles;

	bool fetchLanguageList(QString& messages);

private slots:
	void applyChanges();
	void refresh();
};

#endif // TESSDATAMANAGER_HH
