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

#include <QtSpell.hpp>

class DisplayerToolHOCR;
class QDomDocument;
class QDomElement;

class OutputEditorHOCR : public OutputEditor {
	Q_OBJECT
public:
	OutputEditorHOCR(DisplayerToolHOCR* tool);
	~OutputEditorHOCR();

	QWidget* getUI() override { return m_widget; }
	ReadSessionData* initRead() override{ return new ReadSessionData; }
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const QString& errorMsg, ReadSessionData* data) override;
	bool getModified() const override;

public slots:
	bool clear() override;
	bool save(const QString& filename = "") override;
	void savePDF(bool overlay = false);
	void savePDFOverlay(){ savePDF(true); }

private:
	class HTMLHighlighter;

	static const int IdRole = Qt::UserRole + 1;
	static const int TextRole = Qt::UserRole + 2;
	static const int BBoxRole = Qt::UserRole + 3;
	static const int ClassRole = Qt::UserRole + 4;
	static const int FontSizeRole = Qt::UserRole + 5;

	static const QRegExp s_bboxRx;
	static const QRegExp s_pageTitleRx;
	static const QRegExp s_idRx;
	static const QRegExp s_fontSizeRx;

	int m_idCounter = 0;
	DisplayerToolHOCR* m_tool;
	QWidget* m_widget;
	UI_OutputEditorHOCR ui;
	HTMLHighlighter* m_highlighter;
	QtSpell::TextEditChecker m_spell;

	void findReplace(bool backwards, bool replace);
	bool addChildItems(QDomElement element, QTreeWidgetItem* parentItem);
	QDomElement getHOCRElementForItem(QTreeWidgetItem* item, QDomDocument& doc) const;
	QDomElement elementById(QDomElement element, const QString& id) const;
	void expandChildren(QTreeWidgetItem* item) const;
	void printChildren(QPainter& painter, QTreeWidgetItem* item, bool overlayMode) const;
	bool setCurrentSource(const QDomElement& pageElement, int* pageDpi = 0) const;
	void updateItemText(QTreeWidgetItem* item);

private slots:
	void addPage(const QString& hocrText, ReadSessionData data);
	void setFont();
	void showItemProperties(QTreeWidgetItem* item);
	void itemChanged(QTreeWidgetItem* item, int col);
	void showTreeWidgetContextMenu(const QPoint& point);
};

#endif // OUTPUTEDITORHOCR_HH
