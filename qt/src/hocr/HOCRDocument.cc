/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.cc
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

#include <QFileInfo>
#include <QIcon>
#include <QTextStream>
#include <QtSpell.hpp>

#include "common.hh"
#include "HOCRDocument.hh"
#include "Utils.hh"


HOCRDocument::HOCRDocument(QtSpell::TextEditChecker* spell, QObject *parent)
	: QAbstractItemModel(parent), m_spell(spell)
{
	m_document.setContent(QByteArray("<body></body>"));
}

HOCRDocument::~HOCRDocument()
{
	qDeleteAll(m_pages);
}

void HOCRDocument::clear()
{
	beginResetModel();
	qDeleteAll(m_pages);
	m_pages.clear();
	m_pageIdCounter = 0;
	m_document.setContent(QByteArray("<body></body>"));
	endResetModel();
}

void HOCRDocument::recheckSpelling()
{
	recursiveDataChanged(QModelIndex(), {Qt::DisplayRole}, 3); // depth 4: {0=page, 1=block, 2=para, 3=line, 4=word}
}

QModelIndex HOCRDocument::addPage(const QDomElement& pageElement, bool cleanGraphics)
{
	m_document.documentElement().appendChild(pageElement);
	int newRow = m_pages.size();
	beginInsertRows(QModelIndex(), newRow, newRow);
	m_pages.append(new HOCRPage(pageElement, ++m_pageIdCounter, m_defaultLanguage, cleanGraphics));
	endInsertRows();
	return index(newRow, 0);
}

bool HOCRDocument::editItemAttribute(QModelIndex& index, const QString& group, const QString& key, const QString& value)
{
	if(!index.isValid()) {
		return false;
	}
	HOCRItem* item = static_cast<HOCRItem*>(index.internalPointer());

	if(group.isEmpty()) {
		item->m_domElement.setAttribute(key, value);
	} else {
		QMap<QString, QString> attrs = HOCRDocument::deserializeAttrGroup(item->element().attribute(group));
		attrs[key] = value;
		item->m_domElement.setAttribute(group, HOCRDocument::serializeAttrGroup(attrs));
	}
	if(key == "x_wconf") {
		QModelIndex colIdx = index.sibling(index.row(), 1);
		emit dataChanged(colIdx, colIdx, {Qt::DisplayRole});
	}
	emit itemAttributesChanged();
	return true;
}

QModelIndex HOCRDocument::mergeItems(const QModelIndex& parent, int startRow, int endRow)
{
	if(endRow - startRow <= 0) {
		return QModelIndex();
	}
	QModelIndex targetIndex = parent.child(startRow, 0);
	HOCRItem* targetItem = static_cast<HOCRItem*>(targetIndex.internalPointer());
	if(!targetItem || targetItem->itemClass() == "ocr_page") {
		return QModelIndex();
	}

	if(targetItem->itemClass() == "ocrx_word") {
		// Merge word items: join text, merge bounding boxes
		QString text = targetItem->text();
		QRect bbox = targetItem->bbox();
		beginRemoveRows(parent, startRow + 1, endRow);
		for(int row = startRow + 1; row <= endRow; ++row) {
			HOCRItem* item = static_cast<HOCRItem*>(parent.child(row, 0).internalPointer());
			Q_ASSERT(item);
			text += item->text();
			bbox = bbox.united(item->bbox());
			deleteItem(item);
		}
		endRemoveRows();
		targetItem->updateBoundingBox(bbox);
		QDomElement& element = targetItem->m_domElement;
		element.replaceChild(m_document.createTextNode(text), element.firstChild());
		emit dataChanged(targetIndex, targetIndex, {Qt::DisplayRole, Qt::ForegroundRole, BBoxRole});
	} else {
		// Merge other items: merge dom trees and bounding boxes
		QRect bbox = targetItem->bbox();
		QVector<HOCRItem*> moveChilds;
		beginRemoveRows(parent, startRow + 1, endRow);
		for(int row = startRow + 1; row <= endRow; ++row) {
			HOCRItem* item = static_cast<HOCRItem*>(parent.child(row, 0).internalPointer());
			Q_ASSERT(item);
			moveChilds.append(item->m_childItems);
			bbox = bbox.united(item->bbox());
			item->m_childItems.clear();
			deleteItem(item);
		}
		endRemoveRows();
		int pos = targetItem->children().size();
		beginInsertRows(targetIndex, pos, pos + moveChilds.size() - 1);
		for(HOCRItem* child : moveChilds) {
			targetItem->m_childItems.append(child);
			child->m_parentItem = targetItem;
			targetItem->m_domElement.appendChild(child->m_domElement);
		}
		endInsertRows();
		targetItem->updateBoundingBox(bbox);
		emit dataChanged(targetIndex, targetIndex, {BBoxRole});
	}
	return targetIndex;
}

QModelIndex HOCRDocument::addItem(const QModelIndex &parent, const QDomElement &element)
{
	if(!parent.isValid() || !parent.internalPointer()) {
		return QModelIndex();
	}
	HOCRItem* parentItem = static_cast<HOCRItem*>(parent.internalPointer());
	HOCRItem* item = new HOCRItem(element, parentItem->m_pageItem, parentItem);
	parentItem->m_domElement.appendChild(element);
	int pos = parentItem->m_childItems.size();
	beginInsertRows(parent, pos, pos);
	parentItem->m_childItems.append(item);
	endInsertRows();
	return index(pos, 0, parent);
}

bool HOCRDocument::removeItem(const QModelIndex& index)
{
	if(!index.isValid() || !index.internalPointer()) {
		return false;
	}
	HOCRItem* item = static_cast<HOCRItem*>(index.internalPointer());
	beginRemoveRows(index.parent(), index.row(), index.row());
	item->element().parentNode().removeChild(item->element());
	deleteItem(item);
	endRemoveRows();
	return true;
}

QVariant HOCRDocument::data(const QModelIndex &index, int role) const
{
	if (!index.isValid())
		return QVariant();

	HOCRItem *item = static_cast<HOCRItem*>(index.internalPointer());

	if(index.column() == 0) {
		switch (role) {
		case Qt::DisplayRole:
			return displayRoleForItem(item);
		case Qt::DecorationRole:
			return decorationRoleForItem(item);
		case Qt::ForegroundRole:
		{
			bool enabled = item->isEnabled();
			const HOCRItem* parent = item->parent();
			while(enabled && parent) {
				enabled = parent->isEnabled();
				parent = parent->parent();
			}
			if(enabled) {
				return checkItemSpelling(item) ? QVariant() : QVariant(QColor(Qt::red));
			} else {
				return checkItemSpelling(item) ? QVariant(QColor(Qt::gray)) : QVariant(QColor(208, 80, 82));
			}
		}
		case Qt::CheckStateRole:
			return item->isEnabled() ? Qt::Checked : Qt::Unchecked;
		default:
			break;
		}
	} else if(index.column() == 1) {
		if(role == Qt::DisplayRole && item->itemClass() == "ocrx_word") {
			return deserializeAttrGroup(item->element().attribute("title"))["x_wconf"];
		}
	}
	return QVariant();
}

bool HOCRDocument::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(!index.isValid())
		return false;

	HOCRItem *item = static_cast<HOCRItem*>(index.internalPointer());
	if(role == Qt::EditRole && item->itemClass() == "ocrx_word") {
		QDomElement& element = item->m_domElement;
		element.replaceChild(m_document.createTextNode(value.toString()), element.firstChild());
		emit dataChanged(index, index, {Qt::DisplayRole, Qt::ForegroundRole});
		return true;
	} else if(role == BBoxRole) {
		item->updateBoundingBox(value.toRect());
		emit dataChanged(index, index, {BBoxRole});
		return true;
	} else if(role == Qt::CheckStateRole) {
		item->m_enabled = value == Qt::Checked;
		emit dataChanged(index, index, {Qt::CheckStateRole});
		recursiveDataChanged(index, {Qt::CheckStateRole});
		return true;
	}
	return false;
}

void HOCRDocument::recursiveDataChanged(const QModelIndex& parent, const QVector<int>& roles, int fromDepth, int curDepth)
{
	int rows = rowCount(parent);
	if(rows > 0) {
		if(curDepth >= fromDepth) {
			emit dataChanged(index(0, 0, parent), index(rows - 1, 0, parent), roles);
		}
		for(int i = 0; i < rows; ++i) {
			recursiveDataChanged(index(i, 0, parent), roles, fromDepth, curDepth + 1);
		}
	}
}

Qt::ItemFlags HOCRDocument::flags(const QModelIndex &index) const
{
	if (!index.isValid())
		return 0;

	HOCRItem *item = static_cast<HOCRItem*>(index.internalPointer());
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | (item->itemClass() == "ocrx_word" && index.column() == 0 ? Qt::ItemIsEditable : Qt::NoItemFlags);
}

QModelIndex HOCRDocument::index(int row, int column, const QModelIndex &parent) const
{
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	HOCRItem* childItem = nullptr;
	if(!parent.isValid()) {
		childItem = m_pages.value(row);
	} else {
		HOCRItem *parentItem = static_cast<HOCRItem*>(parent.internalPointer());
		childItem = parentItem->m_childItems.value(row);
	}
	return childItem ? createIndex(row, column, childItem) : QModelIndex();
}

QModelIndex HOCRDocument::parent(const QModelIndex &child) const
{
	if (!child.isValid())
		return QModelIndex();

	HOCRItem *item = static_cast<HOCRItem*>(child.internalPointer())->m_parentItem;
	if(!item) {
		return QModelIndex();
	}
	int row = -1;
	if(item->m_parentItem) {
		row = item->m_parentItem->m_childItems.indexOf(item);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		row = m_pages.indexOf(page);
	}
	return row >= 0 ? createIndex(row, 0, item) : QModelIndex();
}

int HOCRDocument::rowCount(const QModelIndex &parent) const
{
	if (parent.column() > 0)
		return 0;
	return !parent.isValid() ? m_pages.size() : static_cast<HOCRItem*>(parent.internalPointer())->m_childItems.size();
}

int HOCRDocument::columnCount(const QModelIndex &/*parent*/) const
{
	return 2;
}

QString HOCRDocument::displayRoleForItem(const HOCRItem* item) const
{
	QString itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		const HOCRPage* page = static_cast<const HOCRPage*>(item);
		return page->title();
	} else if(itemClass == "ocr_carea") {
		return _("Text block");
	} else if(itemClass == "ocr_par") {
		return _("Paragraph");
	} else if(itemClass == "ocr_line") {
		return _("Textline");
	} else if(itemClass == "ocrx_word") {
		return item->element().text();
	} else if(itemClass == "ocr_graphic") {
		return _("Graphic");
	}
	return "";
}

QIcon HOCRDocument::decorationRoleForItem(const HOCRItem* item) const
{
	QString itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		return QIcon(":/icons/item_page");
	} else if(itemClass == "ocr_carea") {
		return QIcon(":/icons/item_block");
	} else if(itemClass == "ocr_par") {
		return QIcon(":/icons/item_par");
	} else if(itemClass == "ocr_line") {
		return QIcon(":/icons/item_line");
	} else if(itemClass == "ocrx_word") {
		return QIcon(":/icons/item_word");
	} else if(itemClass == "ocr_graphic") {
		return QIcon(":/icons/item_halftone");
	}
	return QIcon();
}

bool HOCRDocument::checkItemSpelling(const HOCRItem* item) const
{
	if(item->itemClass() == "ocrx_word") {
		QString trimmed = trimmedWord(item->element().text());
		if(!trimmed.isEmpty()) {
			QString lang = item->lang();
			if(m_spell->getLanguage() != lang) {
				m_spell->setLanguage(lang);
			}
			return m_spell->checkWord(trimmed);
		}
	}
	return true;
}

void HOCRDocument::deleteItem(HOCRItem* item)
{
	if(item->m_parentItem) {
		int idx = item->m_parentItem->m_childItems.indexOf(item);
		delete item->m_parentItem->m_childItems.takeAt(idx);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		int idx = m_pages.indexOf(page);
		delete m_pages.takeAt(idx);
	}
}

QMap<QString, QString> HOCRDocument::deserializeAttrGroup(const QString& string)
{
	QMap<QString, QString> attrs;
	for(const QString& attr : string.split(QRegExp("\\s*;\\s*"))) {
		int splitPos = attr.indexOf(QRegExp("\\s+"));
		attrs.insert(attr.left(splitPos), attr.mid(splitPos + 1));
	}
	return attrs;
}

QString HOCRDocument::serializeAttrGroup(const QMap<QString, QString>& attrs)
{
	QStringList list;
	for(auto it = attrs.begin(), itEnd = attrs.end(); it != itEnd; ++it) {
		list.append(QString("%1 %2").arg(it.key(), it.value()));
	}
	return list.join("; ");
}

QString HOCRDocument::trimmedWord(const QString& word, QString* prefix, QString* suffix) {
	QRegExp wordRe("^(\\W*)(\\w*)(\\W*)$");
	if(wordRe.indexIn(word) != -1) {
		if(prefix)
			*prefix = wordRe.cap(1);
		if(suffix)
			*suffix = wordRe.cap(3);
		return wordRe.cap(2);
	}
	return word;
}

///////////////////////////////////////////////////////////////////////////////

const QRegExp HOCRItem::s_bboxRx = QRegExp("bbox\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)\\s+(\\d+)");
const QRegExp HOCRItem::s_fontSizeRx = QRegExp("x_fsize\\s+(\\d+)");
const QRegExp HOCRItem::s_baseLineRx = QRegExp("baseline\\s+(-?\\d+\\.?\\d*)\\s+(-?\\d+)");

QMap<QString,QString> HOCRItem::s_langCache = QMap<QString,QString>();


HOCRItem::HOCRItem(QDomElement element, HOCRPage* page, HOCRItem* parent)
	: m_domElement(element), m_pageItem(page), m_parentItem(parent)
{
	// Adjust item id based on pageId
	if(parent) {
		QString idClass = itemClass().mid(itemClass().indexOf("_") + 1);
		int counter = page->m_idCounters.value(idClass, 0) + 1;
		page->m_idCounters[idClass] = counter;
		QString newId = QString("%1_%2_%3").arg(idClass).arg(page->pageId()).arg(counter);
		m_domElement.setAttribute("id", newId);
	}

	// Parse item bbox
	if(s_bboxRx.indexIn(m_domElement.attribute("title")) != -1) {
		int x1 = s_bboxRx.cap(1).toInt();
		int y1 = s_bboxRx.cap(2).toInt();
		int x2 = s_bboxRx.cap(3).toInt();
		int y2 = s_bboxRx.cap(4).toInt();
		m_bbox.setCoords(x1, y1, x2, y2);
	}

	// For the last word items of the line, ensure the correct hyphen is used
	if(itemClass() == "ocrx_word") {
		QDomElement nextElement = m_domElement.nextSiblingElement();
		if(nextElement.isNull()) {
			QString newText = m_domElement.text().replace(QRegExp("[-\u2014]\\s*$"), "-");
			element.replaceChild(element.ownerDocument().createTextNode(newText), element.firstChild());
		}
	}
}

HOCRItem::~HOCRItem()
{
	qDeleteAll(m_childItems);
}

QString HOCRItem::source() const
{
	QString str;
	QTextStream stream(&str);
	m_domElement.save(stream, 1);
	return str;
}

int HOCRItem::baseLine() const
{
	if(s_baseLineRx.indexIn(m_domElement.attribute("title")) != -1) {
		return s_baseLineRx.cap(2).toInt();
	}
	return 0;
}

double HOCRItem::fontSize() const
{
	if(s_fontSizeRx.indexIn(m_domElement.attribute("title")) != -1) {
		return s_fontSizeRx.cap(1).toDouble();
	}
	return 0;
}

bool HOCRItem::addChildren(QString language)
{
	// Determine item language (inherit from parent if not specified)
	QString elemLang = m_domElement.attribute("lang");
	if(!elemLang.isEmpty()) {
		auto it = s_langCache.find(elemLang);
		if(it == s_langCache.end()) {
			it = s_langCache.insert(elemLang, Utils::getSpellingLanguage(elemLang));
		}
		m_domElement.setAttribute("lang", it.value());
		language = it.value();
	}

	if(itemClass() == "ocrx_word") {
		m_domElement.setAttribute("lang", language);
		return !m_domElement.text().isEmpty();
	}
	bool haveWords = false;
	QDomElement childElement = m_domElement.firstChildElement();
	while(!childElement.isNull()) {
		m_childItems.append(new HOCRItem(childElement, m_pageItem, this));
		haveWords |= m_childItems.last()->addChildren(language);
		childElement = childElement.nextSiblingElement();
	}
	return haveWords;
}

void HOCRItem::updateBoundingBox(const QRect &bbox)
{
	QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
	QMap<QString, QString> attrs = HOCRDocument::deserializeAttrGroup(m_domElement.attribute("title"));
	attrs["bbox"] = bboxstr;
	m_domElement.setAttribute("title", HOCRDocument::serializeAttrGroup(attrs));
	m_bbox = bbox;
}

///////////////////////////////////////////////////////////////////////////////

HOCRPage::HOCRPage(QDomElement element, int pageId, const QString& language, bool cleanGraphics)
	: HOCRItem(element, this), m_pageId(pageId)
{
	m_domElement.setAttribute("id", QString("page_%1").arg(pageId));

	QMap<QString, QString> attrs = HOCRDocument::deserializeAttrGroup(m_domElement.attribute("title"));
	m_sourceFile = attrs["image"].replace(QRegExp("^'"), "").replace(QRegExp("'$"), "");
	m_pageNr = attrs["pageno"].toInt();
	m_angle = attrs["rot"].toDouble();
	m_resolution = attrs["res"].toInt();

	QDomElement childElement = m_domElement.firstChildElement("div");
	while(!childElement.isNull()) {
		HOCRItem* item = new HOCRItem(childElement, this, this);
		m_childItems.append(item);
		if(!item->addChildren(language)) {
			// No word children -> treat as graphic
			if(cleanGraphics && (item->bbox().width() < 10 || item->bbox().height() < 10))
			{
				// Ignore graphics which are less than 10 x 10
				delete m_childItems.takeLast();
			} else {
				childElement.setAttribute("class", "ocr_graphic");
				qDeleteAll(item->m_childItems);
				item->m_childItems.clear();
				// Remove any children since they are not meaningful
				QDomNodeList childNodes = childElement.childNodes();
				for(int i = 0, n = childNodes.size(); i < n; ++i) {
					childElement.removeChild(childNodes.at(i));
				}
			}
		}
		childElement = childElement.nextSiblingElement();
	}
}

QString HOCRPage::title() const {
	return QString("%1 [%2]").arg(QFileInfo(m_sourceFile).fileName()).arg(m_pageNr);
}
