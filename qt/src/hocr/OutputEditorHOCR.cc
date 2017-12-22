/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
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

#include <QApplication>
#include <QDomDocument>
#include <QFontComboBox>
#include <QImage>
#include <QMessageBox>
#include <QFileInfo>
#include <QSyntaxHighlighter>
#include <cmath>
#include <cstring>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#undef USE_STD_NAMESPACE

#include "DisplayerToolHOCR.hh"
#include "FileDialogs.hh"
#include "HOCRDocument.hh"
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

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool) {
	static int reg = qRegisterMetaType<QList<QRect>>("QList<QRect>");
	Q_UNUSED(reg);

	m_tool = tool;
	m_widget = new QWidget;
	ui.setupUi(m_widget);
	m_highlighter = new HTMLHighlighter(ui.plainTextEditOutput->document());

	ui.actionOutputSaveHOCR->setShortcut(Qt::CTRL + Qt::Key_S);

	m_document = new HOCRDocument(&m_spell, ui.treeViewHOCR);
	ui.treeViewHOCR->setModel(m_document);
	ui.treeViewHOCR->setContextMenuPolicy(Qt::CustomContextMenu);
	ui.treeViewHOCR->header()->setStretchLastSection(false);
	ui.treeViewHOCR->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui.treeViewHOCR->setColumnWidth(1, 32);
	ui.treeViewHOCR->setColumnHidden(1, true);

	connect(ui.actionOutputOpen, SIGNAL(triggered()), this, SLOT(open()));
	connect(ui.actionOutputSaveHOCR, SIGNAL(triggered()), this, SLOT(save()));
	connect(ui.actionOutputExportPDF, SIGNAL(triggered()), this, SLOT(exportToPDF()));
	connect(ui.actionOutputExportText, SIGNAL(triggered()), this, SLOT(exportToText()));
	connect(ui.actionOutputClear, SIGNAL(triggered()), this, SLOT(clear()));
	connect(ui.actionToggleWConf, SIGNAL(toggled(bool)), this, SLOT(toggleWConfColumn(bool)));
	connect(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
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

	setFont();
}

OutputEditorHOCR::~OutputEditorHOCR() {
	delete m_widget;
}

void OutputEditorHOCR::setFont() {
	if(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont")->getValue()) {
		ui.plainTextEditOutput->setFont(QFont());
	} else {
		ui.plainTextEditOutput->setFont(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont")->getValue());
	}
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
	QMap<QString, QString> attrs = HOCRDocument::deserializeAttrGroup(pageDiv.attribute("title"));
	attrs["image"] = QString("'%1'").arg(data.file);
	attrs["ppageno"] = QString::number(data.page);
	attrs["rot"] = QString::number(data.angle);
	attrs["res"] = QString::number(data.resolution);
	pageDiv.setAttribute("title", HOCRDocument::serializeAttrGroup(attrs));

	QModelIndex index = m_document->addPage(pageDiv, true);

	expandCollapseChildren(index, true);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
	ui.actionOutputSaveHOCR->setEnabled(true);
	ui.toolButtonOutputExport->setEnabled(true);
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

void OutputEditorHOCR::showItemProperties(const QModelIndex& current) {
	if(ui.treeViewHOCR->currentIndex() != current) {
		ui.treeViewHOCR->blockSignals(true);
		ui.treeViewHOCR->setCurrentIndex(current);
		ui.treeViewHOCR->blockSignals(false);
	}
	ui.tableWidgetProperties->blockSignals(true);
	ui.tableWidgetProperties->setRowCount(0);
	ui.tableWidgetProperties->blockSignals(false);
	ui.plainTextEditOutput->setPlainText("");

	const HOCRItem* currentItem = m_document->itemAtIndex(current);
	if(!currentItem) {
		m_tool->clearSelection();
		return;
	}
	const HOCRPage* page = currentItem->page();
	showPage(page);

	int row = -1;
	ui.tableWidgetProperties->blockSignals(true);
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
		ui.tableWidgetProperties->setCellWidget(row, 1, createAttrWidget(current, attrName, it.value()));
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
			ui.tableWidgetProperties->setCellWidget(row, 1, createAttrWidget(current, attrName, attrValueCount == 1 ? *(attrValues.begin()) : "", it.key(), attrValueCount > 1));
		}
	}
	ui.tableWidgetProperties->blockSignals(false);

	ui.plainTextEditOutput->setPlainText(currentItem->toHtml());

	m_tool->setSelection(currentItem->bbox());
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
		combo->setLineEdit(new HOCRAttributeEditor(attrValue, m_document, itemIndex, attrName, attrItemClass));
		if(multiple) {
			combo->setCurrentIndex(-1);
			combo->lineEdit()->setPlaceholderText(_("Multiple values"));
		} else {
			combo->lineEdit()->setText(attrValue);
		}
		return combo;
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
		bool siblings = true;
		bool graphics = false;
		QVector<int> rows = {indices.first().row()};
		for(int i = 1; i < nIndices && siblings; ++i) {
			siblings &= indices[i].parent() == indices.first().parent();
			const HOCRItem* item = m_document->itemAtIndex(indices[i]);
			graphics |= !item || item->itemClass() == "ocr_graphic";
			rows.append(indices[i].row());
		}
		if(siblings) { // Merging or swapping allowed
			QMenu menu;
			qSort(rows);
			bool consecutive = (rows.last() - rows.first()) == nIndices - 1;
			const HOCRItem* firstItem = m_document->itemAtIndex(indices.first());
			bool pages = firstItem && firstItem->itemClass() == "ocr_page";

			QAction* mergeAction = nullptr;
			QAction* swapAction = nullptr;
			if(consecutive && !graphics && !pages) { // Merging allowed
				mergeAction = menu.addAction(_("Merge"));
			}
			if(nIndices == 2) { // Swapping allowed
				swapAction = menu.addAction(_("Swap"));
			}

			QAction* clickedAction = menu.exec(ui.treeViewHOCR->mapToGlobal(point));
			if(!clickedAction) {
				return;
			}
			ui.treeViewHOCR->selectionModel()->blockSignals(true);
			QModelIndex newIndex;
			if(clickedAction == mergeAction) {
					newIndex = m_document->mergeItems(indices.first().parent(), rows.first(), rows.last());
			}
			if(clickedAction == swapAction) {
					newIndex = m_document->swapItems(indices.first().parent(), rows.first(), rows.last());
			}
			if(newIndex.isValid()) {
			ui.treeViewHOCR->selectionModel()->setCurrentIndex(newIndex, QItemSelectionModel::ClearAndSelect);
				showItemProperties(newIndex);
			}
			ui.treeViewHOCR->selectionModel()->blockSignals(false);
		}
		// Nothing else is allowed with multiple items selected
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
		QString prefix, suffix, trimmedWord = HOCRDocument::trimmedWord(item->text(), &prefix, &suffix);
		QString spellLang = item->lang();
		if(m_spell.getLanguage() != spellLang) {
			m_spell.setLanguage(spellLang);
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
		if(!menu.actions().isEmpty()) {
			menu.addSeparator();
		}
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
	QString filename = m_tool->getDisplayer()->getCurrentImage(pageNr);
	QModelIndex pageIndex = m_document->searchPage(filename, pageNr);
	const HOCRItem* currentItem = m_document->itemAtIndex(pageIndex);
	if(!currentItem) {
		return;
	}
	const HOCRPage* page = currentItem->page();
	// Transform point in coordinate space used when page was OCRed
	double alpha = (page->angle() - m_tool->getDisplayer()->getCurrentAngle()) / 180. * M_PI;
	double scale = double(page->resolution()) / double(m_tool->getDisplayer()->getCurrentResolution());
	QPoint newPoint( scale * (point.x() * std::cos(alpha) - point.y() * std::sin(alpha)) + 0.5 * page->bbox().width(),
					 scale * (point.x() * std::sin(alpha) + point.y() * std::cos(alpha)) + 0.5 * page->bbox().height());
	showItemProperties(m_document->searchAtCanvasPos(pageIndex, newPoint));
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
	m_modified = false;
	m_filebasename = QFileInfo(filename).completeBaseName();
	ui.actionOutputSaveHOCR->setEnabled(true);
	ui.toolButtonOutputExport->setEnabled(true);
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
	file.write(m_document->toHTML().toUtf8());
	file.write("</html>\n");
	m_modified = false;
	m_filebasename = QFileInfo(outname).completeBaseName();
	return true;
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
	ui.plainTextEditOutput->clear();
	m_tool->clearSelection();
	m_modified = false;
	m_filebasename.clear();
	ui.actionOutputSaveHOCR->setEnabled(false);
	ui.toolButtonOutputExport->setEnabled(false);
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

void OutputEditorHOCR::setLanguage(const Config::Lang &lang) {
	m_document->setDefaultLanguage(lang.code);
}
