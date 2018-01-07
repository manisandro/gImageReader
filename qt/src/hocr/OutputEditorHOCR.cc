/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
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

#include <QApplication>
#include <QDomDocument>
#include <QFileInfo>
#include <QFontComboBox>
#include <QImage>
#include <QStyledItemDelegate>
#include <QMessageBox>
#include <QPointer>
#include <QSyntaxHighlighter>
#include <cmath>
#include <cstring>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#undef USE_STD_NAMESPACE

#include "ConfigSettings.hh"
#include "DisplayerToolHOCR.hh"
#include "FileDialogs.hh"
#include "HOCRDocument.hh"
#include "HOCROdtExporter.hh"
#include "HOCRPdfExporter.hh"
#include "HOCRTextExporter.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"


class OutputEditorHOCR::HTMLHighlighter : public QSyntaxHighlighter {
public:
	HTMLHighlighter(QTextDocument *document) : QSyntaxHighlighter(document) {
		mFormatMap[NormalState].setForeground(QColor(Qt::black));
		mFormatMap[InTag].setForeground(QColor(75, 75, 255));
		mFormatMap[InAttrKey].setForeground(QColor(75, 200, 75));
		mFormatMap[InAttrValue].setForeground(QColor(255, 75, 75));
		mFormatMap[InAttrValueDblQuote].setForeground(QColor(255, 75, 75));

		mStateMap[NormalState].append({QRegExp("<"), InTag, false});
		mStateMap[InTag].append({QRegExp(">"), NormalState, true});
		mStateMap[InTag].append({QRegExp("\\w+="), InAttrKey, false});
		mStateMap[InAttrKey].append({QRegExp("'"), InAttrValue, false});
		mStateMap[InAttrKey].append({QRegExp("\""), InAttrValueDblQuote, false});
		mStateMap[InAttrKey].append({QRegExp("\\s"), NormalState, false});
		mStateMap[InAttrValue].append({QRegExp("'[^']*'"), InTag, true});
		mStateMap[InAttrValueDblQuote].append({QRegExp("\"[^\"]*\""), InTag, true});
	}

private:
	enum State { NormalState = -1, InComment, InTag, InAttrKey, InAttrValue, InAttrValueDblQuote };
	struct Rule {
		QRegExp pattern;
		State nextState;
		bool addMatched; // add matched length to pos
	};

	QMap<State,QTextCharFormat> mFormatMap;
	QMap<State,QList<Rule>> mStateMap;

	void highlightBlock(const QString &text) override {
		int pos = 0;
		int len = text.length();
		State state = static_cast<State>(previousBlockState());
		while(pos < len) {
			State minState = state;
			int minPos = -1;
			for(const Rule& rule : mStateMap.value(state)) {
				int matchPos = rule.pattern.indexIn(text, pos);
				if(matchPos != -1 && (minPos < 0 || matchPos < minPos)) {
					minPos = matchPos + (rule.addMatched ? rule.pattern.matchedLength() : 0);
					minState = rule.nextState;
				}
			}
			if(minPos == -1) {
				setFormat(pos, len - pos, mFormatMap[state]);
				pos = len;
			} else {
				setFormat(pos, minPos - pos, mFormatMap[state]);
				pos = minPos;
				state = minState;
			}
		}
		setCurrentBlockState(state);
	}
};

///////////////////////////////////////////////////////////////////////////////

HOCRAttributeEditor::HOCRAttributeEditor(const QString& value, HOCRDocument* doc, const QModelIndex& itemIndex, const QString& attrName, const QString& attrItemClass)
	: QLineEdit(value), m_doc(doc), m_itemIndex(itemIndex), m_attrName(attrName), m_origValue(value), m_attrItemClass(attrItemClass)
{
	setFrame(false);
	connect(m_doc, SIGNAL(itemAttributeChanged(QModelIndex,QString, QString)), this, SLOT(updateValue(QModelIndex,QString, QString)));
	connect(this, SIGNAL(textChanged(QString)), this, SLOT(validateChanges()));
}

void HOCRAttributeEditor::focusOutEvent(QFocusEvent *ev){
	QLineEdit::focusOutEvent(ev);
	validateChanges();
}

void HOCRAttributeEditor::updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value) {
	if(itemIndex == m_itemIndex && name == m_attrName) {
		blockSignals(true);
		setText(value);
		blockSignals(false);
	}
}

void HOCRAttributeEditor::validateChanges() {
	if(!hasFocus()) {
		int pos;
		QString newValue = text();
		if(newValue == m_origValue) {
			return;
		}
		if(validator() && validator()->validate(newValue, pos) != QValidator::Acceptable) {
			setText(m_origValue);
		} else {
			m_doc->editItemAttribute(m_itemIndex, m_attrName, newValue, m_attrItemClass);
			m_origValue = newValue;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

HOCRAttributeCheckbox::HOCRAttributeCheckbox(Qt::CheckState value, HOCRDocument *doc, const QModelIndex &itemIndex, const QString &attrName, const QString &attrItemClass)
	: m_doc(doc), m_itemIndex(itemIndex), m_attrName(attrName), m_attrItemClass(attrItemClass)
{
	setCheckState(value);
	connect(m_doc, SIGNAL(itemAttributeChanged(QModelIndex,QString, QString)), this, SLOT(updateValue(QModelIndex,QString, QString)));
	connect(this, SIGNAL(stateChanged(int)), this, SLOT(valueChanged()));
}

void HOCRAttributeCheckbox::updateValue(const QModelIndex& itemIndex, const QString& name, const QString& value) {
	if(itemIndex == m_itemIndex && name == m_attrName) {
		blockSignals(true);
		setChecked(value == "1");
		blockSignals(false);
	}
}

void HOCRAttributeCheckbox::valueChanged()
{
	m_doc->editItemAttribute(m_itemIndex, m_attrName, isChecked() ? "1" : "0", m_attrItemClass);
}

///////////////////////////////////////////////////////////////////////////////

class HOCRTextDelegate : public QStyledItemDelegate {
public:
	using QStyledItemDelegate::QStyledItemDelegate;

	QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &/* option */, const QModelIndex &/* index */) const
	{
		return new QLineEdit(parent);
	}
	void setEditorData(QWidget *editor, const QModelIndex &index) const
	{
		m_currentIndex = index;
		m_currentEditor = static_cast<QLineEdit*>(editor);
		static_cast<QLineEdit*>(editor)->setText(index.model()->data(index, Qt::EditRole).toString());
	}
	void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const
	{
		model->setData(index, static_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
	}
	const QModelIndex& getCurrentIndex() const{ return m_currentIndex; }
	QLineEdit* getCurrentEditor() const{ return m_currentEditor; }

private:
	mutable QModelIndex m_currentIndex;
	mutable QPointer<QLineEdit> m_currentEditor;
};

///////////////////////////////////////////////////////////////////////////////

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool) {
	static int reg = qRegisterMetaType<QList<QRect>>("QList<QRect>");
	Q_UNUSED(reg);

	m_tool = tool;
	m_widget = new QWidget;
	ui.setupUi(m_widget);
	m_highlighter = new HTMLHighlighter(ui.plainTextEditOutput->document());

	ui.actionOutputReplace->setShortcut(Qt::CTRL + Qt::Key_F);
	ui.actionOutputSaveHOCR->setShortcut(Qt::CTRL + Qt::Key_S);
	ui.actionNavigateNext->setShortcut(Qt::Key_F3);
	ui.actionNavigatePrev->setShortcut(Qt::SHIFT + Qt::Key_F3);

	m_document = new HOCRDocument(&m_spell, ui.treeViewHOCR);
	ui.treeViewHOCR->setModel(m_document);
	ui.treeViewHOCR->setContextMenuPolicy(Qt::CustomContextMenu);
	ui.treeViewHOCR->header()->setStretchLastSection(false);
	ui.treeViewHOCR->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui.treeViewHOCR->setColumnWidth(1, 32);
	ui.treeViewHOCR->setColumnHidden(1, true);
	ui.treeViewHOCR->setItemDelegateForColumn(0, new HOCRTextDelegate(ui.treeViewHOCR));

	ui.comboBoxNavigate->addItem(_("Page"), "ocr_page");
	ui.comboBoxNavigate->addItem(_("Block"), "ocr_carea");
	ui.comboBoxNavigate->addItem(_("Paragraph"), "ocr_par");
	ui.comboBoxNavigate->addItem(_("Line"), "ocr_line");
	ui.comboBoxNavigate->addItem(_("Word"), "ocrx_word");
	ui.comboBoxNavigate->addItem(_("Misspelled word"), "ocrx_word_bad");

	connect(ui.actionOutputOpen, SIGNAL(triggered()), this, SLOT(open()));
	connect(ui.actionOutputSaveHOCR, SIGNAL(triggered()), this, SLOT(save()));
	connect(ui.actionOutputExportODT, SIGNAL(triggered()), this, SLOT(exportToODT()));
	connect(ui.actionOutputExportPDF, SIGNAL(triggered()), this, SLOT(exportToPDF()));
	connect(ui.actionOutputExportText, SIGNAL(triggered()), this, SLOT(exportToText()));
	connect(ui.actionOutputClear, SIGNAL(triggered()), this, SLOT(clear()));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.searchFrame, SLOT(setVisible(bool)));
	connect(ui.actionOutputReplace, SIGNAL(toggled(bool)), ui.searchFrame, SLOT(clear()));
	connect(ui.actionToggleWConf, SIGNAL(toggled(bool)), this, SLOT(toggleWConfColumn(bool)));
	connect(ui.searchFrame, SIGNAL(findReplace(QString,QString,bool,bool,bool)), this, SLOT(findReplace(QString,QString,bool,bool,bool)));
	connect(ui.searchFrame, SIGNAL(replaceAll(QString,QString,bool)), this, SLOT(replaceAll(QString,QString,bool)));
	connect(ui.searchFrame, SIGNAL(applySubstitutions(QMap<QString,QString>,bool)), this, SLOT(applySubstitutions(QMap<QString,QString>,bool)));
	connect(ConfigSettings::get<FontSetting>("customoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(ConfigSettings::get<SwitchSetting>("systemoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(ui.treeViewHOCR->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(showItemProperties(QModelIndex)));
	connect(ui.treeViewHOCR, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showTreeWidgetContextMenu(QPoint)));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(updateSourceText()));
	connect(m_tool, SIGNAL(bboxChanged(QRect)), this, SLOT(updateCurrentItemBBox(QRect)));
	connect(m_tool, SIGNAL(bboxDrawn(QRect)), this, SLOT(addGraphicRegion(QRect)));
	connect(m_tool, SIGNAL(positionPicked(QPoint)), this, SLOT(pickItem(QPoint)));
	connect(m_document, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(setModified()));
	connect(m_document, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(setModified()));
	connect(m_document, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(setModified()));
	connect(m_document, SIGNAL(itemAttributeChanged(QModelIndex,QString,QString)), this, SLOT(setModified()));
	connect(m_document, SIGNAL(itemAttributeChanged(QModelIndex,QString,QString)), this, SLOT(updateSourceText()));
	connect(ui.comboBoxNavigate, SIGNAL(currentIndexChanged(int)), this, SLOT(navigateTargetChanged()));
	connect(ui.actionNavigateNext, SIGNAL(triggered(bool)), this, SLOT(navigateNext()));
	connect(ui.actionNavigatePrev, SIGNAL(triggered(bool)), this, SLOT(navigatePrev()));
	connect(ui.actionExpandAll, SIGNAL(triggered(bool)), this, SLOT(expandItemClass()));
	connect(ui.actionCollapseAll, SIGNAL(triggered(bool)), this, SLOT(collapseItemClass()));

	setFont();
}

OutputEditorHOCR::~OutputEditorHOCR() {
	delete m_widget;
}

void OutputEditorHOCR::setFont() {
	if(ConfigSettings::get<SwitchSetting>("systemoutputfont")->getValue()) {
		ui.plainTextEditOutput->setFont(QFont());
	} else {
		ui.plainTextEditOutput->setFont(ConfigSettings::get<FontSetting>("customoutputfont")->getValue());
	}
}

void OutputEditorHOCR::setModified(){
	ui.actionOutputSaveHOCR->setEnabled(m_document->pageCount() > 0);
	ui.toolButtonOutputExport->setEnabled(m_document->pageCount() > 0);
	ui.toolBarNavigate->setEnabled(m_document->pageCount() > 0);
	m_modified = true;
}

OutputEditorHOCR::ReadSessionData* OutputEditorHOCR::initRead(tesseract::TessBaseAPI &tess) {
	tess.SetPageSegMode(tesseract::PSM_AUTO_ONLY);
	return new HOCRReadSessionData;
}

void OutputEditorHOCR::read(tesseract::TessBaseAPI &tess, ReadSessionData *data) {
	tess.SetVariable("hocr_font_info", "true");
	char* text = tess.GetHOCRText(data->page);
	QMetaObject::invokeMethod(this, "addPage", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(text)), Q_ARG(ReadSessionData, *data));
	delete[] text;
}

void OutputEditorHOCR::readError(const QString& errorMsg, ReadSessionData *data) {
	static_cast<HOCRReadSessionData*>(data)->errors.append(QString("%1[%2]: %3").arg(data->file).arg(data->page).arg(errorMsg));
}

void OutputEditorHOCR::finalizeRead(ReadSessionData *data) {
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	if(!hdata->errors.isEmpty()) {
		QString message = QString(_("The following pages could not be processed:\n%1").arg(hdata->errors.join("\n")));
		QMessageBox::warning(MAIN, _("Recognition errors"), message);
	}
	OutputEditor::finalizeRead(data);
}

void OutputEditorHOCR::addPage(const QString& hocrText, ReadSessionData data) {
	QDomDocument doc;
	doc.setContent(hocrText);

	QDomElement pageDiv = doc.firstChildElement("div");
	QMap<QString, QString> attrs = HOCRItem::deserializeAttrGroup(pageDiv.attribute("title"));
	attrs["image"] = QString("'%1'").arg(data.file);
	attrs["ppageno"] = QString::number(data.page);
	attrs["rot"] = QString::number(data.angle);
	attrs["res"] = QString::number(data.resolution);
	pageDiv.setAttribute("title", HOCRItem::serializeAttrGroup(attrs));

	QModelIndex index = m_document->addPage(pageDiv, true);

	expandCollapseChildren(index, true);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
}

void OutputEditorHOCR::navigateTargetChanged()
{
	QString target = ui.comboBoxNavigate->itemData(ui.comboBoxNavigate->currentIndex()).toString();
	bool allowExpandCollapse = !target.startsWith("ocrx_word");
	ui.actionExpandAll->setEnabled(allowExpandCollapse);
	ui.actionCollapseAll->setEnabled(allowExpandCollapse);
}

void OutputEditorHOCR::expandCollapseItemClass(bool expand)
{
	QString target = ui.comboBoxNavigate->itemData(ui.comboBoxNavigate->currentIndex()).toString();
	QModelIndex start = m_document->index(0, 0);
	QModelIndex next = start;
	do {
		const HOCRItem* item = m_document->itemAtIndex(next);
		if(item && item->itemClass() == target) {
			if(expand) {
				ui.treeViewHOCR->setExpanded(next, expand);
				for(QModelIndex parent = next.parent(); parent.isValid(); parent = parent.parent()) {
					ui.treeViewHOCR->setExpanded(parent, true);
				}
				for(QModelIndex child = next.child(0, 0); child.isValid(); child = child.sibling(child.row() + 1, 0)) {
					expandCollapseChildren(child, false);
				}
			} else {
				expandCollapseChildren(next, false);
			}
		}
		next = m_document->nextIndex(next);
	} while(next != start);
	ui.treeViewHOCR->scrollTo(ui.treeViewHOCR->currentIndex());
}

void OutputEditorHOCR::navigateNextPrev(bool next)
{
	QString target = ui.comboBoxNavigate->itemData(ui.comboBoxNavigate->currentIndex()).toString();
	bool misspelled = false;
	if(target == "ocrx_word_bad") {
		target = "ocrx_word";
		misspelled = true;
	}
	QModelIndex start = ui.treeViewHOCR->currentIndex();
	if(!start.isValid()) {
		start = m_document->index(0, 0);
	}
	QModelIndex curr = next? m_document->nextIndex(start) : m_document->prevIndex(start);
	while(curr != start) {
		const HOCRItem* item = m_document->itemAtIndex(curr);
		if(item && item->itemClass() == target && (!misspelled || m_document->indexIsMisspelledWord(curr))) {
			break;
		}
		curr = next? m_document->nextIndex(curr) : m_document->prevIndex(curr);
	};
	ui.treeViewHOCR->setCurrentIndex(curr);
}

void OutputEditorHOCR::expandCollapseChildren(const QModelIndex& index, bool expand) const {
	int nChildren = m_document->rowCount(index);
	if(nChildren > 0) {
		ui.treeViewHOCR->setExpanded(index, expand);
		for(int i = 0; i < nChildren; ++i) {
			expandCollapseChildren(index.child(i, 0), expand);
		}
	}
}

bool OutputEditorHOCR::showPage(const HOCRPage *page)
{
	return page && MAIN->getSourceManager()->addSource(page->sourceFile()) && MAIN->getDisplayer()->setup(&page->pageNr(), &page->resolution(), &page->angle());
}

void OutputEditorHOCR::showItemProperties(const QModelIndex& index) {
	if(ui.treeViewHOCR->currentIndex() != index) {
		ui.treeViewHOCR->blockSignals(true);
		ui.treeViewHOCR->setCurrentIndex(index);
		ui.treeViewHOCR->blockSignals(false);
	}
	ui.tableWidgetProperties->setRowCount(0);
	ui.plainTextEditOutput->setPlainText("");

	const HOCRItem* currentItem = m_document->itemAtIndex(index);
	if(!currentItem) {
		m_tool->clearSelection();
		return;
	}
	const HOCRPage* page = currentItem->page();

	int row = -1;
	QMap<QString, QString> attrs = currentItem->getAllAttributes();
	for(auto it = attrs.begin(), itEnd = attrs.end(); it != itEnd; ++it) {
		QString attrName = it.key();
		if(attrName == "class" || attrName == "id"){
			continue;
		}
		QStringList parts = attrName.split(":");
		ui.tableWidgetProperties->insertRow(++row);
		QTableWidgetItem* attrNameItem = new QTableWidgetItem(parts.last());
		attrNameItem->setFlags(attrNameItem->flags() & ~Qt::ItemIsEditable);
		ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
		ui.tableWidgetProperties->setCellWidget(row, 1, createAttrWidget(index, attrName, it.value()));
	}

	// ocr_class:attr_key:attr_values
	QMap<QString, QMap<QString, QSet<QString>>> occurences;
	currentItem->getPropagatableAttributes(occurences);
	for(auto it = occurences.begin(), itEnd = occurences.end(); it != itEnd; ++it) {
		ui.tableWidgetProperties->insertRow(++row);
		QTableWidgetItem* sectionItem = new QTableWidgetItem(it.key());
		sectionItem->setFlags(sectionItem->flags() & ~(Qt::ItemIsEditable|Qt::ItemIsSelectable));
		sectionItem->setBackgroundColor(Qt::lightGray);
		QFont sectionFont = sectionItem->font();
		sectionFont.setBold(true);
		sectionItem->setFont(sectionFont);
		ui.tableWidgetProperties->setItem(row, 0, sectionItem);
		ui.tableWidgetProperties->setSpan(row, 0, 1, 2);
		for(auto attrIt = it.value().begin(), attrItEnd = it.value().end(); attrIt != attrItEnd; ++attrIt) {
			const QString& attrName = attrIt.key();
			const QSet<QString>& attrValues = attrIt.value();
			int attrValueCount = attrValues.size();
			ui.tableWidgetProperties->insertRow(++row);
			QStringList parts = attrName.split(":");
			QTableWidgetItem* attrNameItem = new QTableWidgetItem(parts.last());
			attrNameItem->setFlags(attrNameItem->flags() & ~Qt::ItemIsEditable);
			ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
			ui.tableWidgetProperties->setCellWidget(row, 1, createAttrWidget(index, attrName, attrValueCount == 1 ? *(attrValues.begin()) : "", it.key(), attrValueCount > 1));
		}
	}

	ui.plainTextEditOutput->setPlainText(currentItem->toHtml());

	if(showPage(page)) {
		// Minimum bounding box
		QRect minBBox;
		if(currentItem->itemClass() == "ocr_page") {
			minBBox = currentItem->bbox();
		} else {
			for(const HOCRItem* child : currentItem->children()) {
				minBBox = minBBox.united(child->bbox());
			}
		}
		m_tool->setSelection(currentItem->bbox(), minBBox);
	}
}

QWidget* OutputEditorHOCR::createAttrWidget(const QModelIndex& itemIndex, const QString& attrName, const QString& attrValue, const QString& attrItemClass, bool multiple)
{
	static QMap<QString, QString> attrLineEdits = {
		{"title:bbox", "\\d+\\s+\\d+\\s+\\d+\\s+\\d+"},
		{"lang", "[a-z]{2}(?:_[A-Z]{2})?"},
		{"title:x_fsize", "\\d+"},
		{"title:baseline", "[-+]?\\d+\\.?\\d*\\s[-+]?\\d+\\.?\\d*"}
	};
	auto it = attrLineEdits.find(attrName);
	if(it != attrLineEdits.end()) {
		QLineEdit* lineEdit = new HOCRAttributeEditor(attrValue, m_document, itemIndex, attrName, attrItemClass);
		lineEdit->setValidator(new QRegExpValidator(QRegExp(it.value())));
		if(multiple) {
			lineEdit->setPlaceholderText(_("Multiple values"));
		}
		return lineEdit;
	} else if(attrName == "title:x_font") {
		QFontComboBox* combo = new QFontComboBox();
		combo->setCurrentIndex(-1);
		QLineEdit* edit = new HOCRAttributeEditor(attrValue, m_document, itemIndex, attrName, attrItemClass);
		edit->blockSignals(true); // Because the combobox alters the text as soon as setLineEdit is called...
		combo->setLineEdit(edit);
		edit->setText(attrValue);
		edit->blockSignals(false);
		if(multiple) {
			combo->lineEdit()->setPlaceholderText(_("Multiple values"));
		}
		return combo;
	} else if(attrName == "bold" || attrName == "italic") {
		Qt::CheckState value = multiple ? Qt::PartiallyChecked : attrValue == "1" ? Qt::Checked : Qt::Unchecked;
		return new HOCRAttributeCheckbox(value, m_document, itemIndex, attrName, attrItemClass);
	} else {
		QLineEdit* lineEdit = new QLineEdit(attrValue);
		lineEdit->setFrame(false);
		lineEdit->setReadOnly(true);
		return lineEdit;
	}
}

void OutputEditorHOCR::updateCurrentItemBBox(QRect bbox) {
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
	m_document->editItemAttribute(current, "title:bbox", bboxstr);
}

void OutputEditorHOCR::updateSourceText() {
	if(ui.tabWidget->currentWidget() == ui.plainTextEditOutput) {
		QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
		const HOCRItem* currentItem = m_document->itemAtIndex(current);
		if(currentItem) {
			ui.plainTextEditOutput->setPlainText(currentItem->toHtml());
		}
	}
}

void OutputEditorHOCR::addGraphicRegion(const QRect& bbox) {
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	QDomElement graphicElement = m_document->getDomDocument().createElement("div");
	graphicElement.setAttribute("class", "ocr_graphic");
	graphicElement.setAttribute("title", QString("bbox %1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom()));
	QModelIndex index = m_document->addItem(current, graphicElement);
	if(index.isValid()) {
		ui.treeViewHOCR->selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
	}
}

void OutputEditorHOCR::showTreeWidgetContextMenu(const QPoint &point) {
	QModelIndexList indices = ui.treeViewHOCR->selectionModel()->selectedRows();
	int nIndices = indices.size();
	if(nIndices > 1) {
		// Check if item merging is allowed (no pages, all items of same class and with same parent and consecutive)
		const HOCRItem* firstItem = m_document->itemAtIndex(indices.first());
		bool ok = firstItem;
		QSet<QString> classes;
		if(firstItem) {
			classes.insert(firstItem->itemClass());
		}
		QVector<int> rows = {indices.first().row()};
		for(int i = 1; i < nIndices && ok; ++i) {
			const HOCRItem* item = m_document->itemAtIndex(indices[i]);
			if(!item) {
				ok = false;
				break;
			}
			ok &= item->parent() == firstItem->parent();
			classes.insert(item->itemClass());
			rows.append(indices[i].row());
		}
		qSort(rows);
		ok &= classes.size() == 1 && *classes.begin() != "ocr_page";
		ok &= (rows.last() - rows.first()) == nIndices - 1;
		if(ok) {
			QMenu menu;
			QAction* mergeAction = menu.addAction(_("Merge"));
			if(menu.exec(ui.treeViewHOCR->mapToGlobal(point)) == mergeAction) {
				ui.treeViewHOCR->selectionModel()->blockSignals(true);
				QModelIndex newIndex = m_document->mergeItems(indices.first().parent(), rows.first(), rows.last());
				if(newIndex.isValid()) {
					ui.treeViewHOCR->selectionModel()->setCurrentIndex(newIndex, QItemSelectionModel::ClearAndSelect);
					showItemProperties(newIndex);
				}
				ui.treeViewHOCR->selectionModel()->blockSignals(false);
			}
		}
		return;
	}
	QModelIndex index = ui.treeViewHOCR->indexAt(point);
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(!item) {
		return;
	}

	QMenu menu;
	QAction* actionAddGraphic = nullptr;
	QAction* addWordAction = nullptr;
	QAction* ignoreWordAction = nullptr;
	QList<QAction*> setTextActions;
	QAction* actionRemoveItem = nullptr;
	QAction* actionExpand = nullptr;
	QAction* actionCollapse = nullptr;
	if(item->itemClass() == "ocr_page") {
		actionAddGraphic = menu.addAction(_("Add graphic region"));
	} else if(item->itemClass() == "ocrx_word") {
		QString prefix, suffix, trimmedWord = HOCRItem::trimmedWord(item->text(), &prefix, &suffix);
		QString spellLang = item->lang();
		bool haveLanguage = true;
		if(m_spell.getLanguage() != spellLang) {
			haveLanguage = m_spell.setLanguage(spellLang);
		}
		if(!trimmedWord.isEmpty()) {
			for(const QString& suggestion : m_spell.getSpellingSuggestions(trimmedWord)) {
				setTextActions.append(menu.addAction(prefix + suggestion + suffix));
			}
			if(setTextActions.isEmpty()) {
				menu.addAction(_("No suggestions"))->setEnabled(false);
			}
			if(!m_spell.checkWord(trimmedWord)) {
				menu.addSeparator();
				addWordAction = menu.addAction(_("Add to dictionary"));
				addWordAction->setData(trimmedWord);
				ignoreWordAction = menu.addAction(_("Ignore word"));
				ignoreWordAction->setData(trimmedWord);
			}
		}
	}
	if(!menu.actions().isEmpty()) {
		menu.addSeparator();
	}
	actionRemoveItem = menu.addAction(_("Remove"));
	if(m_document->rowCount(index) > 0) {
		actionExpand = menu.addAction(_("Expand all"));
		actionCollapse = menu.addAction(_("Collapse all"));
	}

	QAction* clickedAction = menu.exec(ui.treeViewHOCR->mapToGlobal(point));
	if(!clickedAction) {
		return;
	}
	if(clickedAction == actionAddGraphic) {
		m_tool->setAction(DisplayerToolHOCR::ACTION_DRAW_RECT);
	} else if(clickedAction == addWordAction) {
		m_spell.addWordToDictionary(addWordAction->data().toString());
		m_document->recheckSpelling();
	} else if(clickedAction == ignoreWordAction) {
		m_spell.ignoreWord(ignoreWordAction->data().toString());
		m_document->recheckSpelling();
	} else if(setTextActions.contains(clickedAction)) {
		m_document->setData(index, clickedAction->text(), Qt::EditRole);
	} else if(clickedAction == actionRemoveItem) {
		m_document->removeItem(ui.treeViewHOCR->selectionModel()->currentIndex());
	} else if(clickedAction == actionExpand) {
		expandCollapseChildren(index, true);
	} else if(clickedAction == actionCollapse) {
		expandCollapseChildren(index, false);
	}
}

void OutputEditorHOCR::pickItem(const QPoint& point)
{
	int pageNr;
	QString filename = MAIN->getDisplayer()->getCurrentImage(pageNr);
	QModelIndex pageIndex = m_document->searchPage(filename, pageNr);
	const HOCRItem* currentItem = m_document->itemAtIndex(pageIndex);
	if(!currentItem) {
		return;
	}
	const HOCRPage* page = currentItem->page();
	// Transform point in coordinate space used when page was OCRed
	double alpha = (page->angle() - MAIN->getDisplayer()->getCurrentAngle()) / 180. * M_PI;
	double scale = double(page->resolution()) / double(MAIN->getDisplayer()->getCurrentResolution());
	QPoint newPoint( scale * (point.x() * std::cos(alpha) - point.y() * std::sin(alpha)) + 0.5 * page->bbox().width(),
					 scale * (point.x() * std::sin(alpha) + point.y() * std::cos(alpha)) + 0.5 * page->bbox().height());
	showItemProperties(m_document->searchAtCanvasPos(pageIndex, newPoint));
	ui.treeViewHOCR->setFocus();
}

void OutputEditorHOCR::toggleWConfColumn(bool active)
{
	ui.treeViewHOCR->setColumnHidden(1, !active);
}

void OutputEditorHOCR::open() {
	if(!clear(false)) {
		return;
	}
	QStringList files = FileDialogs::openDialog(_("Open hOCR File"), "", "outputdir", QString("%1 (*.html)").arg(_("hOCR HTML Files")), false);
	if(files.isEmpty()) {
		return;
	}
	QString filename = files.front();
	QFile file(filename);
	if(!file.open(QIODevice::ReadOnly)) {
		QMessageBox::critical(MAIN, _("Failed to open file"), _("The file could not be opened: %1.").arg(filename));
		return;
	}
	QDomDocument doc;
	doc.setContent(&file);
	QDomElement div = doc.firstChildElement("html").firstChildElement("body").firstChildElement("div");
	if(div.isNull() || div.attribute("class") != "ocr_page") {
		QMessageBox::critical(MAIN, _("Invalid hOCR file"), _("The file does not appear to contain valid hOCR HTML: %1").arg(filename));
		return;
	}
	while(!div.isNull()) {
		// Need to query next before adding page since the element is reparented
		QDomElement nextDiv = div.nextSiblingElement("div");
		m_document->addPage(div, false);
		div = nextDiv;
	}
	m_document->convertSourcePaths(QFileInfo(filename).absolutePath(), true);
	m_modified = false;
	m_filebasename = QFileInfo(filename).completeBaseName();
}

bool OutputEditorHOCR::save(const QString& filename) {
	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	QString outname = filename;
	if(outname.isEmpty()) {
		QString suggestion = m_filebasename;
		if(suggestion.isEmpty()) {
			QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
			suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
		}
		outname = FileDialogs::saveDialog(_("Save hOCR Output..."), suggestion + ".html", "outputdir", QString("%1 (*.html)").arg(_("hOCR HTML Files")));
		if(outname.isEmpty()) {
			return false;
		}
	}
	QFile file(outname);
	if(!file.open(QIODevice::WriteOnly)) {
		QMessageBox::critical(MAIN, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	tesseract::TessBaseAPI tess;
	QString header = QString(
						 "<!DOCTYPE html>\n"
						 "<html>\n"
						 " <head>\n"
						 "  <title>%1</title>\n"
						 "  <meta charset=\"utf-8\" /> \n"
						 "  <meta name='ocr-system' content='tesseract %2' />\n"
						 "  <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
						 " </head>\n").arg(QFileInfo(outname).fileName()).arg(tess.Version());
	file.write(header.toUtf8());
	m_document->convertSourcePaths(QFileInfo(outname).absolutePath(), false);
	file.write(m_document->toHTML().toUtf8());
	m_document->convertSourcePaths(QFileInfo(outname).absolutePath(), true);
	file.write("</html>\n");
	m_modified = false;
	m_filebasename = QFileInfo(outname).completeBaseName();
	return true;
}

bool OutputEditorHOCR::exportToODT()
{
	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	return HOCROdtExporter(m_tool).run(m_document, m_filebasename);
}

bool OutputEditorHOCR::exportToPDF()
{
	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* item = m_document->itemAtIndex(current);
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	if(showPage(page)) {
		return HOCRPdfExporter(m_document, page, m_tool).run(m_filebasename);
	}
	return false;
}

bool OutputEditorHOCR::exportToText()
{
	ui.treeViewHOCR->setFocus(); // Ensure any item editor loses focus and commits its changes
	return HOCRTextExporter().run(m_document, m_filebasename);
}

bool OutputEditorHOCR::clear(bool hide)
{
	if(!m_widget->isVisible()) {
		return true;
	}
	if(getModified()) {
		int response = QMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), QMessageBox::Save, QMessageBox::Discard, QMessageBox::Cancel);
		if(response == QMessageBox::Save) {
			if(!save()) {
				return false;
			}
		} else if(response != QMessageBox::Discard) {
			return false;
		}
	}
	m_document->clear();
	ui.tableWidgetProperties->setRowCount(0);
	ui.plainTextEditOutput->clear();
	m_tool->clearSelection();
	m_modified = false;
	m_filebasename.clear();
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

void OutputEditorHOCR::setLanguage(const Config::Lang &lang) {
	m_document->setDefaultLanguage(lang.code);
}

void OutputEditorHOCR::onVisibilityChanged(bool /*visibile*/) {
	ui.searchFrame->hideSubstitutionsManager();
}

bool OutputEditorHOCR::findReplaceInItem(const QModelIndex& index, const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace, bool& currentSelectionMatchesSearch)
{
	// Check that the item is a word
	const HOCRItem* item = m_document->itemAtIndex(index);
	if(!item || item->itemClass() != "ocrx_word") {
		return false;
	}
	Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
	HOCRTextDelegate* delegate = static_cast<HOCRTextDelegate*>(ui.treeViewHOCR->itemDelegateForColumn(0));
	// If the item is already in edit mode, continue searching inside the text
	if(delegate->getCurrentIndex() == index && delegate->getCurrentEditor()) {
		QLineEdit* editor = delegate->getCurrentEditor();
		bool matchesSearch = editor->selectedText().compare(searchstr, cs) == 0;
		int selStart = editor->selectionStart();
		if(matchesSearch && replace) {
			QString oldText = editor->text();
			editor->setText(oldText.left(selStart) + replacestr + oldText.mid(selStart + searchstr.length()));
			editor->setSelection(selStart, replacestr.length());
			return true;
		}
		bool matchesReplace = editor->selectedText().compare(replacestr, cs) == 0;
		int pos = -1;
		if(backwards) {
			pos = selStart - 1;
			pos = pos < 0 ? -1 : editor->text().lastIndexOf(searchstr, pos, cs);
		} else {
			pos = matchesSearch ? selStart + searchstr.length() : matchesReplace ? selStart + replacestr.length() : selStart;
			pos = editor->text().indexOf(searchstr, pos, cs);
		}
		if(pos != -1) {
			editor->setSelection(pos, searchstr.length());
			return true;
		}
		currentSelectionMatchesSearch = matchesSearch;
		return false;
	}
	// Otherwise, if item contains text, set it in edit mode
	int pos = backwards ? item->text().lastIndexOf(searchstr, -1, cs) : item->text().indexOf(searchstr, 0, cs);
	if(pos != -1) {
		ui.treeViewHOCR->setCurrentIndex(index);
		ui.treeViewHOCR->edit(index);
		Q_ASSERT(delegate->getCurrentIndex() == index && delegate->getCurrentEditor());
		delegate->getCurrentEditor()->setSelection(pos, searchstr.length());
		return true;
	}
	return false;
}

void OutputEditorHOCR::findReplace(const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace)
{
	ui.searchFrame->clearErrorState();
	QModelIndex current = ui.treeViewHOCR->currentIndex();
	if(!current.isValid()) {
		current = m_document->index(backwards ? (m_document->rowCount() - 1) : 0, 0);
	}
	QModelIndex neww = current;
	bool currentSelectionMatchesSearch = false;
	while(!findReplaceInItem(neww, searchstr, replacestr, matchCase, backwards, replace, currentSelectionMatchesSearch)) {
		neww = backwards ? m_document->prevIndex(neww) : m_document->nextIndex(neww);
		if(!neww.isValid() || neww == current) {
			// Break endless loop
			if(!currentSelectionMatchesSearch) {
				ui.searchFrame->setErrorState();
			}
			return;
		}
	}
}

void OutputEditorHOCR::replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase)
{
	MAIN->pushState(MainWindow::State::Busy, _("Replacing..."));
	QModelIndex start = m_document->index(0, 0);
	QModelIndex curr = start;
	Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
	int count = 0;
	do {
		const HOCRItem* item = m_document->itemAtIndex(curr);
		if(item && item->itemClass() == "ocrx_word") {
			if(item->text().contains(searchstr, cs)) {
				++count;
				m_document->setData(curr, item->text().replace(searchstr, replacestr, cs), Qt::EditRole);
			}
		}
		curr = m_document->nextIndex(curr);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
	} while(curr != start);
	if(count == 0) {
		ui.searchFrame->setErrorState();
	}
	MAIN->popState();
}

void OutputEditorHOCR::applySubstitutions(const QMap<QString, QString>& substitutions, bool matchCase)
{
	MAIN->pushState(MainWindow::State::Busy, _("Applying substitutions..."));
	QModelIndex start = m_document->index(0, 0);
	Qt::CaseSensitivity cs = matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
	for(auto it = substitutions.begin(), itEnd = substitutions.end(); it != itEnd; ++it) {
		QString search = it.key();
		QString replace = it.value();
		QModelIndex curr = start;
		do {
			const HOCRItem* item = m_document->itemAtIndex(curr);
			if(item && item->itemClass() == "ocrx_word") {
				m_document->setData(curr, item->text().replace(search, replace, cs), Qt::EditRole);
			}
			curr = m_document->nextIndex(curr);
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		} while(curr != start);
	}
	MAIN->popState();
}
