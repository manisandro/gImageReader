/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.hh
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#include <QTimer>

#include "OutputEditor.hh"
#include "Ui_OutputEditorHOCR.hh"

class DisplayerToolHOCR;
class HOCRDocument;
class HOCRItem;
class HOCRPage;
class HOCRProofReadWidget;
class QGraphicsPixmapItem;

class OutputEditorHOCR : public OutputEditor {
	Q_OBJECT
public:
	class HOCRBatchProcessor : public BatchProcessor {
	public:
		QString fileSuffix() const override { return QString(".html"); }
		void writeHeader(QIODevice* dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo) const override;
		void writeFooter(QIODevice* dev) const override;
		void appendOutput(QIODevice* dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfos, bool firstArea) const override;
	};

	enum class InsertMode { Replace, Append, InsertBefore };

	OutputEditorHOCR(DisplayerToolHOCR* tool);
	~OutputEditorHOCR();

	QWidget* getUI() override {
		return m_widget;
	}
	ReadSessionData* initRead(tesseract::TessBaseAPI& tess) override;
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const QString& errorMsg, ReadSessionData* data) override;
	void finalizeRead(ReadSessionData* data) override;
	BatchProcessor* createBatchProcessor(const QMap<QString, QVariant>& /*options*/) const override { return new HOCRBatchProcessor; }
	bool containsSource(const QString& source, int sourcePage) const override;
	QString crashSave(const QString& filename) const override;

	HOCRDocument* getDocument() const { return m_document; }
	DisplayerToolHOCR* getTool() const { return m_tool; }
	bool open(InsertMode mode, QStringList files = QStringList());
	bool selectPage(int nr);

public slots:
	bool open(const QString& filename) override { return open(InsertMode::Replace, {filename}); }
	bool clear(bool hide = true) override;
	void setLanguage(const Config::Lang& lang) override;
	void onVisibilityChanged(bool visible) override;
	bool save(const QString& filename = "");
	bool exportToODT();
	bool exportToPDF();
	bool exportToText();
	void removeItem();

private:
	class HTMLHighlighter;

	struct HOCRReadSessionData : ReadSessionData {
		int insertIndex;
		QStringList errors;
	};
	friend struct QMetaTypeId<HOCRReadSessionData>;

	DisplayerToolHOCR* m_tool;
	QWidget* m_widget;
	QGraphicsPixmapItem* m_preview = nullptr;
	HOCRProofReadWidget* m_proofReadWidget = nullptr;
	QTimer m_previewTimer;
	UI_OutputEditorHOCR ui;
	HTMLHighlighter* m_highlighter;
	bool m_modified = false;
	bool m_blockSourceChanged = false;
	QString m_filebasename;
	InsertMode m_insertMode = InsertMode::Append;

	HOCRDocument* m_document;

	QWidget* createAttrWidget(const QModelIndex& itemIndex, const QString& attrName, const QString& attrValue, const QString& attrItemClass = QString(), bool multiple = false);
	void expandCollapseChildren(const QModelIndex& index, bool expand) const;
	void expandCollapseItemClass(bool expand);
	void navigateNextPrev(bool next);
	bool findReplaceInItem(const QModelIndex& index, const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace, bool& currentSelectionMatchesSearch);
	bool showPage(const HOCRPage* page);
	int currentPage();
	void drawPreview(QPainter& painter, const HOCRItem* item);

private slots:
	void bboxDrawn(const QRect& bbox, int action);
	void addPage(const QString& hocrText, HOCRReadSessionData data);
	void expandItemClass() {
		expandCollapseItemClass(true);
	}
	void collapseItemClass() {
		expandCollapseItemClass(false);
	}
	void navigateTargetChanged();
	void navigateNext() {
		navigateNextPrev(true);
	}
	void navigatePrev() {
		navigateNextPrev(false);
	}
	void pickItem(const QPoint& point);
	void setFont();
	void setInsertMode(QAction* action);
	void setModified();
	void showItemProperties(const QModelIndex& index, const QModelIndex& prev = QModelIndex());
	void showTreeWidgetContextMenu(const QPoint& point);
	void toggleWConfColumn(bool active);
	void itemAttributeChanged(const QModelIndex& itemIndex, const QString& name, const QString& value);
	void updateSourceText();
	void updateCurrentItemBBox(QRect bbox);
	void findReplace(const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace);
	void replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase);
	void applySubstitutions(const QMap<QString, QString>& substitutions, bool matchCase);
	void sourceChanged();
	void previewToggled(bool active);
	void updatePreview();
};

class HOCRAttributeEditor : public QLineEdit {
	Q_OBJECT
public:
	HOCRAttributeEditor(const QString& value, HOCRDocument* doc, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass);

protected:
	void focusOutEvent(QFocusEvent* ev);

private:
	HOCRDocument* m_doc;
	QModelIndex m_itemIndex;
	QString m_attrName;
	QString m_origValue;
	QString m_attrItemClass;

private slots:
	void updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value);
	void validateChanges(bool force = false);
};

class HOCRAttributeCheckbox : public QCheckBox {
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

class HOCRAttributeLangCombo : public QComboBox {
	Q_OBJECT
public:
	HOCRAttributeLangCombo(const QString& value, bool multiple, HOCRDocument* doc, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass);

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
