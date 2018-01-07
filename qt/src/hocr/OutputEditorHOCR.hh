/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.hh
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
	void onVisibilityChanged(bool visible) override;
	void open();
	bool save(const QString& filename = "") override;
	bool exportToODT();
	bool exportToPDF();
	bool exportToText();

private:
	class HTMLHighlighter;

	struct HOCRReadSessionData : ReadSessionData {
		QStringList errors;
	};

	DisplayerToolHOCR* m_tool;
	QWidget* m_widget;
	UI_OutputEditorHOCR ui;
	HTMLHighlighter* m_highlighter;
	bool m_modified = false;
	QString m_filebasename;
	QtSpell::TextEditChecker m_spell;

	HOCRDocument* m_document;

	QWidget* createAttrWidget(const QModelIndex& itemIndex, const QString& attrName, const QString& attrValue, const QString& attrItemClass = QString(), bool multiple = false);
	void expandCollapseChildren(const QModelIndex& index, bool expand) const;
	void expandCollapseItemClass(bool expand);
	void navigateNextPrev(bool next);
	bool findReplaceInItem(const QModelIndex& index, const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace, bool& currentSelectionMatchesSearch);
	bool showPage(const HOCRPage* page);

private slots:
	void addGraphicRegion(const QRect& bbox);
	void addPage(const QString& hocrText, ReadSessionData data);
	void expandItemClass(){ expandCollapseItemClass(true); }
	void collapseItemClass(){ expandCollapseItemClass(false); }
	void navigateTargetChanged();
	void navigateNext(){ navigateNextPrev(true); }
	void navigatePrev(){ navigateNextPrev(false); }
	void pickItem(const QPoint& point);
	void setFont();
	void setModified();
	void showItemProperties(const QModelIndex& index);
	void showTreeWidgetContextMenu(const QPoint& point);
	void toggleWConfColumn(bool active);
	void updateSourceText();
	void updateCurrentItemBBox(QRect bbox);
	void findReplace(const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace);
	void replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase);
	void applySubstitutions(const QMap<QString, QString>& substitutions, bool matchCase);
};

class HOCRAttributeEditor : public QLineEdit
{
	Q_OBJECT
public:
	HOCRAttributeEditor(const QString& value, HOCRDocument* doc, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass);

protected:
	void focusOutEvent(QFocusEvent *ev);

private:
	HOCRDocument* m_doc;
	QModelIndex m_itemIndex;
	QString m_attrName;
	QString m_origValue;
	QString m_attrItemClass;

private slots:
	void updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value);
	void validateChanges();
};

class HOCRAttributeCheckbox : public QCheckBox
{
	Q_OBJECT
public:
	HOCRAttributeCheckbox(Qt::CheckState value, HOCRDocument* doc, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass);

private:
	HOCRDocument* m_doc;
	QModelIndex m_itemIndex;
	QString m_attrName;
	QString m_attrItemClass;

private slots:
	void updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value);
	void valueChanged();
};

#endif // OUTPUTEDITORHOCR_HH
