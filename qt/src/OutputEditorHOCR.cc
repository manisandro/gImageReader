/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorHOCR.cc
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

#include <QApplication>
#include <QDir>
#include <QDomDocument>
#include <QGraphicsPixmapItem>
#include <QImage>
#include <QMessageBox>
#include <QFileInfo>
#include <QFileDialog>
#include <QPainter>
#include <QPrinter>
#include <QSyntaxHighlighter>
#include <QTextStream>
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>

#include "DisplayerToolHOCR.hh"
#include "MainWindow.hh"
#include "OutputEditorHOCR.hh"
#include "Recognizer.hh"
#include "SourceManager.hh"
#include "Utils.hh"
#include "ui_PdfExportDialog.h"


const QRegExp OutputEditorHOCR::s_bboxRx = QRegExp("bbox\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)");
const QRegExp OutputEditorHOCR::s_pageTitleRx = QRegExp("image\\s+'(.+)';\\s+bbox\\s+\\d+\\s+\\d+\\s+\\d+\\s+\\d+;\\s+pageno\\s+(\\d+);\\s+rot\\s+(\\d+\\.?\\d*);\\s+res\\s+(\\d+\\.?\\d*)");
const QRegExp OutputEditorHOCR::s_idRx = QRegExp("(\\w+)_\\d+_(\\d+)");
const QRegExp OutputEditorHOCR::s_fontSizeRx = QRegExp("x_fsize\\s+(\\d+)");


class OutputEditorHOCR::HTMLHighlighter : public QSyntaxHighlighter
{
public:
	HTMLHighlighter(QTextDocument *document) : QSyntaxHighlighter(document)
	{
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

	void highlightBlock(const QString &text)
	{
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


Q_DECLARE_METATYPE(QList<QRect>)

OutputEditorHOCR::OutputEditorHOCR(DisplayerToolHOCR* tool)
{
	static int reg = qRegisterMetaType<QList<QRect>>("QList<QRect>");
	Q_UNUSED(reg);

	m_tool = tool;
	m_widget = new QWidget;
	ui.setupUi(m_widget);
	m_highlighter = new HTMLHighlighter(ui.plainTextEditOutput->document());

	m_pdfExportDialog = new QDialog(m_widget);
	m_pdfExportDialogUi.setupUi(m_pdfExportDialog);

	ui.actionOutputSaveHOCR->setShortcut(Qt::CTRL + Qt::Key_S);

	ui.treeWidgetItems->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(ui.actionOutputOpen, SIGNAL(triggered()), this, SLOT(open()));
	connect(ui.actionOutputSaveHOCR, SIGNAL(triggered()), this, SLOT(save()));
	connect(ui.actionOutputExportPDF, SIGNAL(triggered()), this, SLOT(savePDF()));
	connect(ui.actionOutputClear, SIGNAL(triggered()), this, SLOT(clear()));
	connect(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont"), SIGNAL(changed()), this, SLOT(setFont()));
	connect(ui.treeWidgetItems, SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)), this, SLOT(showItemProperties(QTreeWidgetItem*)));
	connect(ui.treeWidgetItems, SIGNAL(itemChanged(QTreeWidgetItem*,int)), this, SLOT(itemChanged(QTreeWidgetItem*,int)));
	connect(ui.treeWidgetItems, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showTreeWidgetContextMenu(QPoint)));
	connect(ui.tableWidgetProperties, SIGNAL(cellChanged(int,int)), this, SLOT(propertyCellChanged(int,int)));
	connect(m_pdfExportDialogUi.buttonFont, SIGNAL(clicked()), &m_pdfFontDialog, SLOT(exec()));
	connect(&m_pdfFontDialog, SIGNAL(fontSelected(QFont)), this, SLOT(updateFontButton(QFont)));
	connect(m_pdfExportDialogUi.comboBoxOutputMode, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.checkBoxFontSize, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.checkBoxUniformizeSpacing, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));
	connect(m_pdfExportDialogUi.checkBoxPreview, SIGNAL(toggled(bool)), this, SLOT(updatePreview()));

	MAIN->getConfig()->addSetting(new ComboSetting("pdfexportmode", m_pdfExportDialogUi.comboBoxOutputMode));
	MAIN->getConfig()->addSetting(new FontSetting("hocrfont", &m_pdfFontDialog, QFont().toString()));
	MAIN->getConfig()->addSetting(new SwitchSetting("hocrusedetectedfontsizes", m_pdfExportDialogUi.checkBoxFontSize, true));
	MAIN->getConfig()->addSetting(new SwitchSetting("hocruniformizelinespacing", m_pdfExportDialogUi.checkBoxUniformizeSpacing, true));

	setFont();
	updateFontButton(m_pdfFontDialog.currentFont());
}

OutputEditorHOCR::~OutputEditorHOCR()
{
	delete m_widget;
}

void OutputEditorHOCR::setFont()
{
	if(MAIN->getConfig()->getSetting<SwitchSetting>("systemoutputfont")->getValue()){
		ui.plainTextEditOutput->setFont(QFont());
	}else{
		ui.plainTextEditOutput->setFont(MAIN->getConfig()->getSetting<FontSetting>("customoutputfont")->getValue());
	}
}

void OutputEditorHOCR::read(tesseract::TessBaseAPI &tess, ReadSessionData *data)
{
	tess.SetVariable("hocr_font_info", "true");
	char* text = tess.GetHOCRText(data->page);
	QMetaObject::invokeMethod(this, "addPage", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(text)), Q_ARG(ReadSessionData, *data));
	delete[] text;
}

void OutputEditorHOCR::readError(const QString& errorMsg, ReadSessionData *data)
{
	static_cast<HOCRReadSessionData*>(data)->errors.append(QString("%1[%2]: %3").arg(data->file).arg(data->page).arg(errorMsg));
}

void OutputEditorHOCR::finalizeRead(ReadSessionData *data)
{
	HOCRReadSessionData* hdata = static_cast<HOCRReadSessionData*>(data);
	if(!hdata->errors.isEmpty()) {
		QString message = QString(tr("The following pages could not be processed:\n%1").arg(hdata->errors.join("\n")));
		QMessageBox::warning(MAIN, _("Recognition errors"), message);
	}
	OutputEditor::finalizeRead(data);
}

void OutputEditorHOCR::addPage(const QString& hocrText, ReadSessionData data)
{
	QDomDocument doc;
	doc.setContent(hocrText);
	QDomElement pageDiv = doc.firstChildElement("div");
	s_bboxRx.indexIn(pageDiv.attribute("title"));
	int x1 = s_bboxRx.cap(1).toInt();
	int y1 = s_bboxRx.cap(2).toInt();
	int x2 = s_bboxRx.cap(3).toInt();
	int y2 = s_bboxRx.cap(4).toInt();
	QString pageTitle = QString("image '%1'; bbox %2 %3 %4 %5; pageno %6; rot %7; res %8")
			.arg(data.file)
			.arg(x1).arg(y1).arg(x2).arg(y2)
			.arg(data.page)
			.arg(data.angle)
			.arg(data.resolution);
	pageDiv.setAttribute("title", pageTitle);
	addPage(pageDiv, QFileInfo(data.file).fileName(), data.page);
}

void OutputEditorHOCR::addPage(QDomElement pageDiv, const QString& filename, int page)
{
	pageDiv.setAttribute("id", QString("page_%1").arg(++m_idCounter));
	s_bboxRx.indexIn(pageDiv.attribute("title"));
	int x1 = s_bboxRx.cap(1).toInt();
	int y1 = s_bboxRx.cap(2).toInt();
	int x2 = s_bboxRx.cap(3).toInt();
	int y2 = s_bboxRx.cap(4).toInt();

	QTreeWidgetItem* pageItem = new QTreeWidgetItem(QStringList() << QString("%1 [%2]").arg(filename).arg(page));
	pageItem->setData(0, IdRole, pageDiv.attribute("id"));
	pageItem->setData(0, BBoxRole, QRect(x1, y1, x2 - x1, y2 - y1));
	pageItem->setData(0, ClassRole, "ocr_page");
	pageItem->setIcon(0, QIcon(":/icons/item_page"));
	pageItem->setCheckState(0, Qt::Checked);
	ui.treeWidgetItems->addTopLevelItem(pageItem);
	QMap<QString,QString> langCache;

	QDomElement element = pageDiv.firstChildElement("div");
	while(!element.isNull()) {
		// Boxes without text are images
		if(!addChildItems(element.firstChildElement(), pageItem, langCache) && s_bboxRx.indexIn(element.attribute("title")) != -1) {
			QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << _("Graphic"));
			item->setCheckState(0, Qt::Checked);
			item->setIcon(0, QIcon(":/icons/item_halftone"));
			item->setData(0, IdRole, element.attribute("id"));
			item->setData(0, ClassRole, "ocr_graphic");
			x1 = s_bboxRx.cap(1).toInt();
			y1 = s_bboxRx.cap(2).toInt();
			x2 = s_bboxRx.cap(3).toInt();
			y2 = s_bboxRx.cap(4).toInt();
			item->setData(0, BBoxRole, QRect(x1, y1, x2 - x1, y2 - y1));
			pageItem->addChild(item);
		}
		element = element.nextSiblingElement();
	}
	QString str;
	QTextStream ss(&str);
	pageDiv.save(ss, 1);
	pageItem->setData(0, SourceRole, str);
	expandChildren(pageItem);
	MAIN->setOutputPaneVisible(true);
	m_modified = true;
}

void OutputEditorHOCR::expandChildren(QTreeWidgetItem* item) const
{
	if(item->childCount() > 0) {
		item->setExpanded(true);
		for(int i = 0, n = item->childCount(); i < n; ++i) {
			expandChildren(item->child(i));
		}
	}
}

bool OutputEditorHOCR::addChildItems(QDomElement element, QTreeWidgetItem* parentItem, QMap<QString,QString>& langCache)
{
	bool haveWord = false;
	while(!element.isNull()) {
		if(s_idRx.indexIn(element.attribute("id")) != -1) {
			QString newId = QString("%1_%2_%3").arg(s_idRx.cap(1)).arg(m_idCounter).arg(s_idRx.cap(2));
			element.setAttribute("id", newId);
		}

		if(s_bboxRx.indexIn(element.attribute("title")) != -1) {
			QString type = element.attribute("class");
			QString icon;
			QString title;
			int x1 = s_bboxRx.cap(1).toInt();
			int y1 = s_bboxRx.cap(2).toInt();
			int x2 = s_bboxRx.cap(3).toInt();
			int y2 = s_bboxRx.cap(4).toInt();
			if(type == "ocr_par") {
				title = _("Paragraph");
				icon = "par";
			} else if(type == "ocr_line") {
				title = _("Textline");
				icon = "line";
			} else if(type == "ocrx_word") {
				title = element.text();
				icon = "word";
			}
			if(!title.isEmpty()) {
				QTreeWidgetItem* item = new QTreeWidgetItem(QStringList() << title);
				if(type == "ocrx_word" || addChildItems(element.firstChildElement(), item, langCache)) {
					item->setCheckState(0, Qt::Checked);
					item->setData(0, IdRole, element.attribute("id"));
					item->setIcon(0, QIcon(QString(":/icons/item_%1").arg(icon)));
					item->setData(0, BBoxRole, QRect(x1, y1, x2 - x1, y2 - y1));
					item->setData(0, ClassRole, type);
					parentItem->addChild(item);
					haveWord = true;
					if(type == "ocrx_word") {
						if(s_fontSizeRx.indexIn(element.attribute("title")) != -1) {
							item->setData(0, FontSizeRole, s_fontSizeRx.cap(1).toDouble());
						}
						item->setFlags(item->flags() | Qt::ItemIsEditable);
						QString lang = element.attribute("lang");
						auto it = langCache.find(lang);
						if(it == langCache.end()) {
							it = langCache.insert(lang, Utils::getSpellingLanguage(lang));
						}
						QString spellingLang = it.value();
						if(m_spell.getLanguage() != spellingLang) {
							m_spell.setLanguage(spellingLang);
						}
						if(!m_spell.checkWord(title)) {
							item->setForeground(0, Qt::red);
						}
					}
				} else {
					delete item;
				}
			}
		}
		element = element.nextSiblingElement();
	}
	return haveWord;
}

QDomElement OutputEditorHOCR::getHOCRElementForItem(QTreeWidgetItem* item, QDomDocument& doc) const
{
	if(!item) {
		return QDomElement();
	}

	QTreeWidgetItem* toplevelItem = item;
	while(toplevelItem->parent()) {
		toplevelItem = toplevelItem->parent();
	}
	QString id = item->data(0, IdRole).toString();
	doc.setContent(toplevelItem->data(0, SourceRole).toString());
	return elementById(doc.firstChildElement(), id);
}

QDomElement OutputEditorHOCR::elementById(QDomElement element, const QString& id) const
{
	QDomElement child;
	while(!element.isNull()) {
		if(element.attribute("id") == id) {
			return element;
		} else if(!(child = elementById(element.firstChildElement(), id)).isNull()) {
			return child;
		} else {
			element = element.nextSiblingElement();
		}
	}
	return QDomElement();
}

void OutputEditorHOCR::showItemProperties(QTreeWidgetItem* item)
{
	ui.tableWidgetProperties->blockSignals(true);
	ui.tableWidgetProperties->setRowCount(0);
	QDomDocument doc;
	QDomElement element = getHOCRElementForItem(item, doc);
	if(!element.isNull()) {
		QDomNamedNodeMap attributes = element.attributes();
		int row = -1;
		for(int i = 0, n = attributes.count(); i < n; ++i) {
			QDomNode attribNode = attributes.item(i);
			if(attribNode.nodeName() == "title") {
				for(const QString& attrib : attribNode.nodeValue().split(QRegExp("\\s*;\\s*"))) {
					int splitPos = attrib.indexOf(QRegExp("\\s+"));
					ui.tableWidgetProperties->insertRow(++row);
					QTableWidgetItem* attrNameItem = new QTableWidgetItem(attrib.left(splitPos));
					attrNameItem->setFlags(attrNameItem->flags() &  ~Qt::ItemIsEditable);
					attrNameItem->setData(ParentAttrRole, "title");
					ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
					ui.tableWidgetProperties->setItem(row, 1, new QTableWidgetItem(attrib.mid(splitPos + 1)));
				}
			} else {
				ui.tableWidgetProperties->insertRow(++row);
				QTableWidgetItem* attrNameItem = new QTableWidgetItem(attribNode.nodeName());
				attrNameItem->setFlags(attrNameItem->flags() &  ~Qt::ItemIsEditable);
				ui.tableWidgetProperties->setItem(row, 0, attrNameItem);
				ui.tableWidgetProperties->setItem(row, 1, new QTableWidgetItem(attribNode.nodeValue()));
			}
		}
		QString str;
		QTextStream stream(&str);
		element.save(stream, 1);
		ui.plainTextEditOutput->setPlainText(str);

		if(setCurrentSource(doc.firstChildElement("div")) && s_bboxRx.indexIn(element.attribute("title")) != -1) {
			int x1 = s_bboxRx.cap(1).toInt();
			int y1 = s_bboxRx.cap(2).toInt();
			int x2 = s_bboxRx.cap(3).toInt();
			int y2 = s_bboxRx.cap(4).toInt();
			m_tool->setSelection(QRect(x1, y1, x2-x1, y2-y1));
		}
	} else {
		m_tool->clearSelection();
	}
	ui.tableWidgetProperties->blockSignals(false);
}

bool OutputEditorHOCR::setCurrentSource(const QDomElement& pageElement, int* pageDpi) const
{
	if(s_pageTitleRx.indexIn(pageElement.attribute("title")) != -1)
	{
		QString filename = s_pageTitleRx.cap(1);
		int page = s_pageTitleRx.cap(2).toInt();
		double angle = s_pageTitleRx.cap(3).toDouble();
		int res = s_pageTitleRx.cap(4).toInt();
		if(pageDpi) {
			*pageDpi = res;
		}

		MAIN->getSourceManager()->addSource(filename);
		QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		int dummy;
		// TODO: Handle this better
		if(MAIN->getDisplayer()->getCurrentImage(dummy) != filename) {
			return false;
		}
		if(MAIN->getDisplayer()->getCurrentPage() != page) {
			MAIN->getDisplayer()->setCurrentPage(page);
		}
		if(MAIN->getDisplayer()->getCurrentAngle() != angle) {
			MAIN->getDisplayer()->setAngle(angle);
		}
		if(MAIN->getDisplayer()->getCurrentResolution() != res) {
			MAIN->getDisplayer()->setResolution(res);
		}
		return true;
	}
	return false;
}

void OutputEditorHOCR::itemChanged(QTreeWidgetItem* item, int col)
{
	ui.treeWidgetItems->blockSignals(true);
	bool isWord = item->data(0, ClassRole).toString() == "ocrx_word";
	if( isWord && item->checkState(col) == Qt::Checked) {
		// Update text
		updateItemText(item);
	}
	else if(item->checkState(col) == Qt::Checked) {
		item->setFlags(item->flags() | Qt::ItemIsSelectable);
		if(!isWord) {
			item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
		}
	} else {
		item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
		item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
		item->setExpanded(false);
	}
	ui.treeWidgetItems->blockSignals(false);
}

void OutputEditorHOCR::propertyCellChanged(int row, int /*col*/)
{
	QTableWidgetItem* keyItem = ui.tableWidgetProperties->item(row, 0);
	QTableWidgetItem* valueItem = ui.tableWidgetProperties->item(row, 1);
	QString parentAttr = keyItem->data(ParentAttrRole).toString();
	if(!parentAttr.isEmpty()) {
		updateItemAttribute(ui.treeWidgetItems->currentItem(), parentAttr, keyItem->text(), valueItem->text());
	} else {
		updateItemAttribute(ui.treeWidgetItems->currentItem(), keyItem->text(), "", valueItem->text());
	}
}

void OutputEditorHOCR::updateItemText(QTreeWidgetItem* item)
{
	QString newText = item->text(0);
	QDomDocument doc;
	QDomElement element = getHOCRElementForItem(item, doc);
	element.replaceChild(doc.createTextNode(newText), element.firstChild());
	updateItem(item, doc, element);
}

void OutputEditorHOCR::updateItemAttribute(QTreeWidgetItem* item, const QString& key, const QString& subkey, const QString& newvalue)
{
	QDomDocument doc;
	QDomElement element = getHOCRElementForItem(item, doc);
	if(subkey.isEmpty()) {
		element.setAttribute(key, newvalue);
	} else {
		QString value = element.attribute(key);
		QStringList subattrs = value.split(QRegExp("\\s*;\\s*"));
		for(int i = 0, n = subattrs.size(); i < n; ++i) {
			int splitPos = subattrs[i].indexOf(QRegExp("\\s+"));
			if(subattrs[i].left(splitPos) == subkey) {
				subattrs[i] = subkey + " " + newvalue;
				break;
			}
		}
		element.setAttribute(key, subattrs.join("; "));
	}
	updateItem(item, doc, element);
}

void OutputEditorHOCR::updateItem(QTreeWidgetItem* item, const QDomDocument& doc, const QDomElement& element)
{
	QString str;
	QTextStream stream(&str);
	doc.save(stream, 1);
	QString spellLang = Utils::getSpellingLanguage(element.attribute("lang"));
	if(m_spell.getLanguage() != spellLang) {
		m_spell.setLanguage(spellLang);
	}
	ui.treeWidgetItems->blockSignals(true); // prevent item changed signal
	item->setForeground(0, m_spell.checkWord(item->text(0)) ? item->parent()->foreground(0) : QBrush(Qt::red));
	ui.treeWidgetItems->blockSignals(false);

	QTreeWidgetItem* toplevelItem = item;
	while(toplevelItem->parent()) {
		toplevelItem = toplevelItem->parent();
	}
	toplevelItem->setData(0, SourceRole, str);

	QString elemstr;
	QTextStream elemstream(&elemstr);
	element.save(elemstream, 1);
	ui.plainTextEditOutput->setPlainText(elemstr);

	if(setCurrentSource(doc.firstChildElement("div")) && s_bboxRx.indexIn(element.attribute("title")) != -1) {
		int x1 = s_bboxRx.cap(1).toInt();
		int y1 = s_bboxRx.cap(2).toInt();
		int x2 = s_bboxRx.cap(3).toInt();
		int y2 = s_bboxRx.cap(4).toInt();
		m_tool->setSelection(QRect(x1, y1, x2-x1, y2-y1));
	}

	m_modified = true;
}

void OutputEditorHOCR::showTreeWidgetContextMenu(const QPoint &point){
	QTreeWidgetItem* item = ui.treeWidgetItems->itemAt(point);
	QString itemClass = item->data(0, ClassRole).toString();
	if(itemClass.isEmpty()) {
		return;
	}
	if(itemClass == "ocr_page") {
		QMenu menu;
		QAction* actionRemove = menu.addAction(_("Remove"));
		if(menu.exec(ui.treeWidgetItems->mapToGlobal(point)) == actionRemove) {
			delete item;
		}
	} else if(itemClass == "ocrx_word") {
		QMenu menu;
		for(const QString& suggestion : m_spell.getSpellingSuggestions(item->text(0))) {
			menu.addAction(suggestion);
		}
		if(menu.actions().isEmpty()) {
			menu.addAction(_("No suggestions"))->setEnabled(false);
		}
		QAction* addAction = nullptr;
		QAction* ignoreAction = nullptr;
		if(!m_spell.checkWord(item->text(0))) {
			menu.addSeparator();
			addAction = menu.addAction(_("Add to dictionary"));
			ignoreAction = menu.addAction(_("Ignore word"));
		}
		QAction* clickedAction = menu.exec(ui.treeWidgetItems->mapToGlobal(point));
		if(clickedAction) {
			if(clickedAction == addAction) {
				m_spell.addWordToDictionary(item->text(0));
				item->setForeground(0, item->parent()->foreground(0));
			} else if(clickedAction == ignoreAction) {
				m_spell.ignoreWord(item->text(0));
				item->setForeground(0, item->parent()->foreground(0));
			} else {
				item->setText(0, clickedAction->text());
			}
		}
	}
}

void OutputEditorHOCR::updateFontButton(const QFont& font)
{
	m_pdfExportDialogUi.buttonFont->setText(QString("%1 %2").arg(font.family()).arg(font.pointSize()));
	updatePreview();
}

void OutputEditorHOCR::open()
{
	if(!clear(false)) {
		return;
	}
	QString dir = MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->getValue();
	QString filename = QFileDialog::getOpenFileName(m_widget, _("Open hOCR File"), dir, QString("%1 (*.html)").arg(_("hOCR HTML Files")));
	if(filename.isEmpty()) {
		return;
	}
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
	int page = 0;
	while(!div.isNull()) {
		++page;
		addPage(div, QFileInfo(filename).fileName(), page);
		div = div.nextSiblingElement("div");
	}
}

bool OutputEditorHOCR::save(const QString& filename)
{
	QString outname = filename;
	if(outname.isEmpty()){
		outname = QDir(MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->getValue()).absoluteFilePath(_("output") + ".html");
		outname = QFileDialog::getSaveFileName(MAIN, _("Save hOCR Output..."), outname, QString("%1 (*.html)").arg(_("hOCR HTML Files")));
		if(outname.isEmpty()){
			return false;
		}
		MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->setValue(QFileInfo(outname).absolutePath());
	}
	QFile file(outname);
	if(!file.open(QIODevice::WriteOnly)){
		QMessageBox::critical(MAIN, _("Failed to save output"), _("Check that you have writing permissions in the selected folder."));
		return false;
	}
	tesseract::TessBaseAPI tess;
	QString header = QString(
			"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
			"<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
			" <head>\n"
			"  <title></title>\n"
			"  <meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" />\n"
			"    <meta name='ocr-system' content='tesseract %1' />\n"
			"    <meta name='ocr-capabilities' content='ocr_page ocr_carea ocr_par ocr_line ocrx_word'/>\n"
			"  </head>\n"
			"<body>\n").arg(tess.Version());
	file.write(header.toUtf8());
	for(int i = 0, n = ui.treeWidgetItems->topLevelItemCount(); i < n; ++i) {
		file.write(ui.treeWidgetItems->topLevelItem(i)->data(0, SourceRole).toString().toUtf8());
	}
	file.write("</body>\n</html>\n");
	m_modified = false;
	return true;
}

void OutputEditorHOCR::savePDF()
{
	m_preview = new QGraphicsPixmapItem();
	m_preview->setTransformationMode(Qt::SmoothTransformation);
	updatePreview();
	MAIN->getDisplayer()->scene()->addItem(m_preview);

	bool accepted = (m_pdfExportDialog->exec() == QDialog::Accepted);

	MAIN->getDisplayer()->scene()->removeItem(m_preview);
	delete m_preview;
	m_preview = nullptr;

	if(!accepted) {
		return;
	}

	QString	outname = QDir(MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->getValue()).absoluteFilePath(_("output") + ".pdf");
	outname = QFileDialog::getSaveFileName(MAIN, _("Save PDF Output..."), outname, QString("%1 (*.pdf)").arg(_("PDF Files")));
	if(outname.isEmpty()){
		return;
	}
	MAIN->getConfig()->getSetting<VarSetting<QString>>("outputdir")->setValue(QFileInfo(outname).absolutePath());

	QPrinter printer;
	printer.setOutputFileName(outname);
	printer.setOutputFormat(QPrinter::PdfFormat);
	printer.setFontEmbeddingEnabled(true);
	int outputDpi = 300;
	printer.setFullPage(true);
	printer.setResolution(outputDpi);
	QPainter painter;
	bool useDetectedFontSizes = m_pdfExportDialogUi.checkBoxFontSize->isChecked();
	bool uniformizeLineSpacing = m_pdfExportDialogUi.checkBoxUniformizeSpacing->isChecked();
	bool overlay = m_pdfExportDialogUi.comboBoxOutputMode->currentIndex() == 1;
	QStringList failed;
	for(int i = 0, n = ui.treeWidgetItems->topLevelItemCount(); i < n; ++i) {
		QTreeWidgetItem* item = ui.treeWidgetItems->topLevelItem(i);
		if(item->checkState(0) != Qt::Checked) {
			continue;
		}
		QRect bbox = item->data(0, BBoxRole).toRect();
		QDomDocument doc;
		doc.setContent(item->data(0, SourceRole).toString());
		int pageDpi = 0;
		if(setCurrentSource(doc.firstChildElement("div"), &pageDpi)) {
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
			printer.setPaperSize(QSizeF(bbox.width() / double(pageDpi), bbox.height() / double(pageDpi)), QPrinter::Inch);
#else
			printer.setPageSize(QPageSize(QSizeF(bbox.width() / double(pageDpi), bbox.height() / double(pageDpi)), QPageSize::Inch));
#endif
			printer.newPage();
			if(i == 0) {
				painter.begin(&printer);
				painter.setFont(m_pdfFontDialog.currentFont());
			}
			painter.save();
			painter.scale(outputDpi / double(pageDpi), outputDpi / double(pageDpi));
			if(overlay) {
				painter.setPen(QPen(QColor(0, 0, 0, 0)));
			}
			printChildren(painter, item, overlay, useDetectedFontSizes, uniformizeLineSpacing);
			if(overlay) {
				painter.drawPixmap(bbox, QPixmap::fromImage(m_tool->getSelection(bbox)));
			}
			painter.restore();
		} else {
			failed.append(item->text(0));
		}
	}
	if(!failed.isEmpty()) {
		QMessageBox::warning(m_widget, _("Errors occurred"), _("The following pages could not be rendered:\n%1").arg(failed.join("\n")));
	}
}

void OutputEditorHOCR::printChildren(QPainter& painter, QTreeWidgetItem *item, bool overlayMode, bool useDetectedFontSizes, bool uniformizeLineSpacing) const
{
	if(item->checkState(0) != Qt::Checked) {
		return;
	}
	QString itemClass = item->data(0, ClassRole).toString();
	QRect itemRect = item->data(0, BBoxRole).toRect();
	if(itemClass == "ocr_line" && uniformizeLineSpacing) {
		int curSize = painter.font().pointSize();
		int x = itemRect.x();
		int prevWordRight = itemRect.x();
		for(int iWord = 0, nWords = item->childCount(); iWord < nWords; ++iWord) {
			QTreeWidgetItem* wordItem = item->child(iWord);
			if(wordItem->checkState(0) == Qt::Checked) {
				QRect wordRect = wordItem->data(0, BBoxRole).toRect();
				if(useDetectedFontSizes) {
					double wordSize = wordItem->data(0, FontSizeRole).toDouble();
					if(wordSize != curSize) {
						QFont font = painter.font();
						font.setPointSize(wordSize);
						painter.setFont(font);
						curSize = wordSize;
					}
				}
				// If distance from previous word is large, keep the space
				if(wordRect.x() - prevWordRight > 4 * painter.fontMetrics().averageCharWidth()) {
					x = wordRect.x();
				}
				prevWordRight = wordRect.right();
				painter.drawText(x, itemRect.bottom(), wordItem->text(0));
				x += painter.fontMetrics().width(wordItem->text(0) + " ");
			}
		}
	} else if(itemClass == "ocrx_word" && !uniformizeLineSpacing) {
		if(useDetectedFontSizes) {
			QFont font = painter.font();
			font.setPointSize(item->data(0, FontSizeRole).toDouble());
			painter.setFont(font);
		}
		painter.drawText(itemRect.x(), itemRect.bottom(), item->text(0));
	} else if(itemClass == "ocr_graphic" && !overlayMode) {
		painter.drawPixmap(itemRect, QPixmap::fromImage(m_tool->getSelection(itemRect)));
	} else {
		for(int i = 0, n = item->childCount(); i < n; ++i) {
			printChildren(painter, item->child(i), overlayMode, useDetectedFontSizes, uniformizeLineSpacing);
		}
	}
}

void OutputEditorHOCR::updatePreview()
{
	if(!m_preview) {
		return;
	}
	m_preview->setVisible(m_pdfExportDialogUi.checkBoxPreview->isChecked());
	if(ui.treeWidgetItems->topLevelItemCount() == 0 || !m_pdfExportDialogUi.checkBoxPreview->isChecked()) {
		return;
	}
	QTreeWidgetItem* item = ui.treeWidgetItems->topLevelItem(0);
	QRect bbox = item->data(0, BBoxRole).toRect();
	QDomDocument doc;
	doc.setContent(item->data(0, SourceRole).toString());
	setCurrentSource(doc.firstChildElement("div"));

	QImage image(bbox.size(), QImage::Format_ARGB32);
	image.setDotsPerMeterX(300 / 0.0254);
	image.setDotsPerMeterY(300 / 0.0254);
	QPainter painter(&image);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setFont(m_pdfFontDialog.currentFont());
	bool overlay = m_pdfExportDialogUi.comboBoxOutputMode->currentIndex() == 1;
	if(overlay) {
		painter.drawPixmap(bbox, QPixmap::fromImage(m_tool->getSelection(bbox)));
		image.fill(QColor(255, 255, 255, 127));
	} else {
		image.fill(Qt::white);
	}
	printChildren(painter, item, overlay, m_pdfExportDialogUi.checkBoxFontSize->isChecked(), m_pdfExportDialogUi.checkBoxUniformizeSpacing->isChecked());
	m_preview->setPixmap(QPixmap::fromImage(image));
	m_preview->setPos(-0.5 * bbox.width(), -0.5 * bbox.height());
}

bool OutputEditorHOCR::clear(bool hide)
{
	if(!m_widget->isVisible()){
		return true;
	}
	if(getModified()){
		int response = QMessageBox::question(MAIN, _("Output not saved"), _("Save output before proceeding?"), QMessageBox::Save, QMessageBox::Discard, QMessageBox::Cancel);
		if(response == QMessageBox::Save){
			if(!save()){
				return false;
			}
		}else if(response != QMessageBox::Discard){
			return false;
		}
	}
	m_idCounter = 0;
	ui.treeWidgetItems->clear();
	ui.tableWidgetProperties->setRowCount(0);
	ui.plainTextEditOutput->clear();
	m_tool->clearSelection();
	m_modified = false;
	if(hide)
		MAIN->setOutputPaneVisible(false);
	return true;
}

bool OutputEditorHOCR::getModified() const
{
	return m_modified;
}
