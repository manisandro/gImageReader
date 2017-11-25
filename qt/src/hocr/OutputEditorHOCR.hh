/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.hh
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

#ifndef OUTPUTEDITORHOCR_HH
#define OUTPUTEDITORHOCR_HH

#include <QtSpell.hpp>

#include "OutputEditor.hh"
#include "Ui_OutputEditorHOCR.hh"

class DisplayerToolHOCR;
class HOCRDocument;
class HOCRPage;
class QGraphicsPixmapItem;

class OutputEditorHOCR : public OutputEditor {
	Q_OBJECT
public:
	OutputEditorHOCR(DisplayerToolHOCR* tool);
	~OutputEditorHOCR();

	QWidget* getUI() override {
		return m_widget;
	}
	ReadSessionData* initRead(tesseract::TessBaseAPI &tess) override;
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const QString& errorMsg, ReadSessionData* data) override;
	void finalizeRead(ReadSessionData *data) override;
	bool getModified() const override{ return m_modified; }

public slots:
	bool clear(bool hide = true) override;
	void setLanguage(const Config::Lang &lang) override;
	void open();
	bool save(const QString& filename = "") override;
	bool savePDF();

private:
	class HTMLHighlighter;

	struct HOCRReadSessionData : ReadSessionData {
		QStringList errors;
	};

	static const int ParentAttrRole = Qt::UserRole + 1;

	DisplayerToolHOCR* m_tool;
	QWidget* m_widget;
	UI_OutputEditorHOCR ui;
	HTMLHighlighter* m_highlighter;
	bool m_modified = false;
	QtSpell::TextEditChecker m_spell;

	HOCRDocument* m_document;

	void expandCollapseChildren(const QModelIndex& index, bool expand) const;
	bool showPage(const HOCRPage* page);

private slots:
	void addGraphicRegion(const QRect& bbox);
	void addPage(const QString& hocrText, ReadSessionData data);
	void propertyCellChanged(int row, int col);
	void setFont();
	void setModified() { m_modified = true; }
	void showItemProperties(const QModelIndex& current);
	void showTreeWidgetContextMenu(const QPoint& point);
	void updateSourceText();
	void updateCurrentItemBBox(QRect bbox);
};


#endif // OUTPUTEDITORHOCR_HH
