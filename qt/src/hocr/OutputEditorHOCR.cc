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
#include <QImage>
#include <QMessageBox>
#include <QFileInfo>
#include <QSyntaxHighlighter>
#include <cstring>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#undef USE_STD_NAMESPACE

#include "DisplayerToolHOCR.hh"
#include "FileDialogs.hh"
#include "HOCRDocument.hh"
#include "HOCRPdfExporter.hh"
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

	connect(ui.actionOutputOpen, SIGNAL(triggered()), this, SLOT(open()));
	connect(ui.actionOutputSaveHOCR, SIGNAL(triggered()), this, SLOT(save()));
	connect(ui.actionOutputExportPDF, SIGNAL(triggered()), this, SLOT(savePDF()));
	connect(ui.actionOutputClear, SIGNAL(triggered()), this, SLOT(clear()));
	connect(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(ui.treeViewHOCR->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)), this, SLOT(showItemProperties(QModelIndex)));
	connect(ui.treeViewHOCR, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showTreeWidgetContextMenu(QPoint)));
	connect(ui.tableWidgetProperties, SIGNAL(cellChanged(int,int)), this, SLOT(propertyCellChanged(int,int)));
	connect(ui.tabWidget, SIGNAL(currentChanged(int)), this, SLOT(updateSourceText()));
	connect(m_tool, SIGNAL(selectionGeometryChanged(QRect)), this, SLOT(updateCurrentItemBBox(QRect)));
	connect(m_tool, SIGNAL(selectionDrawn(QRect)), this, SLOT(addGraphicRegion(QRect)));
	connect(m_document, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)), this, SLOT(setModified()));
	connect(m_document, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(setModified()));
	connect(m_document, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(setModified()));
	connect(m_document, SIGNAL(itemAttributesChanged()), this, SLOT(setModified()));

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
	attrs["page"] = QString::number(data.page);
	attrs["rot"] = QString::number(data.angle);
	attrs["res"] = QString::number(data.resolution);
	pageDiv.setAttribute("title", HOCRDocument::serializeAttrGroup(attrs));

	QModelIndex index = m_document->addPage(pageDiv, true);

	expandCollapseChildren(index, true);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
	ui.actionOutputSaveHOCR->setEnabled(true);
	ui.actionOutputExportPDF->setEnabled(true);
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
	ui.tableWidgetProperties->blockSignals(true);
	ui.tableWidgetProperties->setRowCount(0);
	ui.tableWidgetProperties->blockSignals(false);
	ui.plainTextEditOutput->setPlainText("");

	const HOCRItem* currentItem = m_document->itemAtIndex(current);
	if(!currentItem) {
		return;
	}
	const HOCRPage* page = currentItem->page();
	showPage(page);

	QDomNamedNodeMap attributes = currentItem->element().attributes();
	int row = -1;
	ui.tableWidgetProperties->blockSignals(true);
	for(int i = 0, n = attributes.count(); i < n; ++i) {
		QDomNode attribNode = attributes.item(i);
		QString nodeName = attribNode.nodeName();
		if(nodeName == "title") {
			QMap<QString, QString> attrs = HOCRDocument::deserializeAttrGroup(attribNode.nodeValue());
			for(auto it = attrs.begin(), itEnd = attrs.end(); it != itEnd; ++it) {
				ui.tableWidgetProperties->insertRow(++row);
				QTableWidgetItem* attrNameItem = new QTableWidgetItem(it.key());
				attrNameItem->setFlags(attrNameItem->flags() & ~Qt::ItemIsEditable);
				attrNameItem->setData(ParentAttrRole, "title");
				ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
				ui.tableWidgetProperties->setItem(row, 1, new QTableWidgetItem(it.value()));
				if(attrNameItem->text() == "bbox") {
					ui.tableWidgetProperties->setProperty("bboxrow", row);
				}
			}
		} else if(nodeName != "class" && nodeName != "id"){
			ui.tableWidgetProperties->insertRow(++row);
			QTableWidgetItem* attrNameItem = new QTableWidgetItem(attribNode.nodeName());
			attrNameItem->setFlags(attrNameItem->flags() & ~Qt::ItemIsEditable);
			ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
			ui.tableWidgetProperties->setItem(row, 1, new QTableWidgetItem(attribNode.nodeValue()));
		}
	}
	ui.tableWidgetProperties->blockSignals(false);

	ui.plainTextEditOutput->setPlainText(currentItem->source());

	m_tool->setSelection(currentItem->bbox());
}

void OutputEditorHOCR::propertyCellChanged(int row, int /*col*/) {
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* currentItem = m_document->itemAtIndex(current);
	if(!currentItem) {
		return;
	}

	QTableWidgetItem* keyItem = ui.tableWidgetProperties->item(row, 0);
	QTableWidgetItem* valueItem = ui.tableWidgetProperties->item(row, 1);
	QString parentAttr = keyItem->data(ParentAttrRole).toString();
	QString key = keyItem->text();
	QString value = valueItem->text().trimmed();
	if(parentAttr == "title" && key == "bbox") {
		// Do some validation for bbox inputs
		static QRegExp bboxRegExp("^(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)$");
		if(bboxRegExp.indexIn(value) == -1) {
			const QRect& bbox = currentItem->bbox();
			ui.tableWidgetProperties->blockSignals(true);
			valueItem->setText(QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom()));
			ui.tableWidgetProperties->blockSignals(false);
		} else {
			int x1 = bboxRegExp.cap(1).toInt();
			int y1 = bboxRegExp.cap(2).toInt();
			int x2 = bboxRegExp.cap(3).toInt();
			int y2 = bboxRegExp.cap(4).toInt();
			QRect bbox;
			bbox.setCoords(x1, y1, x2, y2);
			m_document->setData(current, bbox, HOCRDocument::BBoxRole);
			m_tool->setSelection(bbox);
		}
	} else {
		m_document->editItemAttribute(current, parentAttr, key, value);
	}
}

void OutputEditorHOCR::updateCurrentItemBBox(QRect bbox) {
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	m_document->setData(current, bbox, HOCRDocument::BBoxRole);
	int row = ui.tableWidgetProperties->property("bboxrow").toInt();
	QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
	ui.tableWidgetProperties->blockSignals(true);
	ui.tableWidgetProperties->item(row, 1)->setText(bboxstr);
	ui.tableWidgetProperties->blockSignals(false);

	updateSourceText();
}

void OutputEditorHOCR::updateSourceText() {
	if(ui.tabWidget->currentWidget() == ui.plainTextEditOutput) {
		QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
		const HOCRItem* currentItem = m_document->itemAtIndex(current);
		if(currentItem) {
			ui.plainTextEditOutput->setPlainText(currentItem->source());
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
		// Check if item merging is allowed (no pages, all items with same parent and consecutive)
		const HOCRItem* firstItem = m_document->itemAtIndex(indices.first());
		bool ok = firstItem && firstItem->itemClass() != "ocr_page";
		QVector<int> rows = {indices.first().row()};
		for(int i = 1; i < nIndices && ok; ++i) {
			ok &= indices[i].parent() == indices.first().parent();
			rows.append(indices[i].row());
		}
		qSort(rows);
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
		m_tool->clearSelection();
		m_tool->activateDrawSelection();
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
		m_document->addPage(div, false);
		div = div.nextSiblingElement("div");
	}
	m_modified = false;
	m_filename = filename;
	ui.actionOutputSaveHOCR->setEnabled(true);
	ui.actionOutputExportPDF->setEnabled(true);
}

bool OutputEditorHOCR::save(const QString& filename) {
	QString outname = filename;
	if(outname.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		QString suggestion = m_filename;
		if(suggestion.isEmpty()) {
			suggestion = (!sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output")) + ".html";
		}
		outname = FileDialogs::saveDialog(_("Save hOCR Output..."), suggestion, "outputdir", QString("%1 (*.html)").arg(_("hOCR HTML Files")));
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
						 "  <title></title>\n"
						 "  <meta charset=\"utf-8\" /> \n"
						 "  <meta name='ocr-system' content='tesseract %1' />\n"
						 "  <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
						 " </head>\n").arg(tess.Version());
	file.write(header.toUtf8());
	file.write(m_document->toHTML().toUtf8());
	file.write("</html>\n");
	m_modified = false;
	m_filename = outname;
	return true;
}

bool OutputEditorHOCR::savePDF()
{
	QModelIndex current = ui.treeViewHOCR->selectionModel()->currentIndex();
	const HOCRItem* item = m_document->itemAtIndex(current);
	const HOCRPage* page = item ? item->page() : m_document->page(0);
	if(showPage(page)) {
		return HOCRPdfExporter(m_document, page, m_tool).run();
	}
	return false;
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
	m_filename.clear();
	ui.actionOutputSaveHOCR->setEnabled(false);
	ui.actionOutputExportPDF->setEnabled(false);
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

void OutputEditorHOCR::setLanguage(const Config::Lang &lang) {
	m_document->setDefaultLanguage(lang.code);
}
