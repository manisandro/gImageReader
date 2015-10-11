/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.hh
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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

#ifndef OUTPUTEDITORHOCR_HH
#define OUTPUTEDITORHOCR_HH

#include "OutputEditor.hh"
#include "Ui_OutputEditorHOCR.hh"

class OutputEditorHOCR : public OutputEditor {
	Q_OBJECT
public:
	OutputEditorHOCR();
	~OutputEditorHOCR();

	QWidget* getUI() override { return m_widget; }
	ReadSessionData* initRead() override{ return new ReadSessionData; }
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const QString& errorMsg, ReadSessionData* data) override;
	bool getModified() const override;

public slots:
	virtual bool clear() override;
	virtual bool save(const QString& filename = "") override;

private:
	QWidget* m_widget;
	UI_OutputEditorHOCR ui;

	void findReplace(bool backwards, bool replace);

private slots:
	void addText(const QString& text);
	void clearErrorState();
	void findNext();
	void findPrev();
	void replaceAll();
	void replaceNext();
	void setFont();
};

#endif // OUTPUTEDITORHOCR_HH
