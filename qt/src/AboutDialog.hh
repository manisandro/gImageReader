/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * AboutDialog.hh
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

#ifndef ABOUTDIALOG_HH
#define ABOUTDIALOG_HH

#include "common.hh"
#include "ui_AboutDialog.h"

class AboutDialog : public QDialog
{
public:
	explicit AboutDialog(QWidget *parent = 0)
		: QDialog(parent)
	{
		ui.setupUi(this);
		ui.labelAbout->setText(ui.labelAbout->text().arg(PACKAGE_VERSION));
	}

private:
	Ui::AboutDialog ui;
};

#endif // ABOUTDIALOG_HH
