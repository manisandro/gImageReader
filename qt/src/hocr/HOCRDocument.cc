/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.cc
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

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSet>
#include <QTextStream>
#include <QtSpell.hpp>

#include "common.hh"
#include "HOCRDocument.hh"
#include "Utils.hh"


HOCRDocument::HOCRDocument(QtSpell::TextEditChecker* spell, QObject *parent)
	: QAbstractItemModel(parent), m_spell(spell) {
	m_document.setContent(QByteArray("<body></body>"));
}

HOCRDocument::~HOCRDocument() {
	qDeleteAll(m_pages);
}

void HOCRDocument::clear() {
	beginResetModel();
	qDeleteAll(m_pages);
	m_pages.clear();
	m_pageIdCounter = 0;
	m_document.setContent(QByteArray("<body></body>"));
	endResetModel();
}

void HOCRDocument::recheckSpelling() {
	recursiveDataChanged(QModelIndex(), {Qt::DisplayRole}, {"ocrx_word"});
}

QModelIndex HOCRDocument::addPage(const QDomElement& pageElement, bool cleanGraphics) {
	m_document.documentElement().appendChild(pageElement);
	int newRow = m_pages.size();
	beginInsertRows(QModelIndex(), newRow, newRow);
	m_pages.append(new HOCRPage(pageElement, ++m_pageIdCounter, m_defaultLanguage, cleanGraphics, m_pages.size()));
	endInsertRows();
	emit dataChanged(index(0, 0), index(m_pages.size() - 1, 0), {Qt::DisplayRole});
	return index(newRow, 0);
}

bool HOCRDocument::editItemAttribute(const QModelIndex& index, const QString& name, const QString& value, const QString& attrItemClass) {
	HOCRItem* item = mutableItemAtIndex(index);
	if(!item) {
		return false;
	}

	item->setAttribute(name, value, attrItemClass);
	if(name == "title:x_wconf") {
		QModelIndex colIdx = index.sibling(index.row(), 1);
		emit dataChanged(colIdx, colIdx, {Qt::DisplayRole});
	}
	if(name == "lang") {
		recursiveDataChanged(index, {Qt::DisplayRole}, {"ocrx_word"});
	}
	emit itemAttributeChanged(index, name, value);
	if(name == "title:bbox") {
		recomputeParentBBoxes(item);
	}
	return true;
}

// Might be more logical to accept oldParent and oldRow as parameters rather than itemIndex
// (since we need them anyway), if all our callers are likely to know them. Swap does; drag&drop might not.
QModelIndex HOCRDocument::moveItem(const QModelIndex& itemIndex, const QModelIndex& newParent, int newRow) {
	HOCRItem* item = mutableItemAtIndex(itemIndex);
	if(!item) {
		return QModelIndex();
	}
	QModelIndex ancestor = newParent;
	while(ancestor.isValid()) {
		if(ancestor == itemIndex) {
			return QModelIndex();
		}
		ancestor = ancestor.parent();
	}
	int oldRow = itemIndex.row();
	QModelIndex oldParent = itemIndex.parent();
	bool decr = false;
	if(decr = (oldParent == newParent && oldRow < newRow)) {
		--newRow;
	}
	HOCRItem* parentItem = mutableItemAtIndex(newParent);
	if(parentItem) {
		beginRemoveRows(oldParent, oldRow, oldRow);
		takeItem(item);
		endRemoveRows();
		beginInsertRows(newParent, newRow, newRow);
		parentItem->insertChild(item, newRow);
		endInsertRows();
	} else if(item->itemClass() == "ocr_page") {
		beginRemoveRows(QModelIndex(), oldRow, oldRow);
		HOCRPage* pageItem = m_pages.takeAt(oldRow);
		endRemoveRows();
		beginInsertRows(QModelIndex(), newRow, newRow);
		m_pages.insert(newRow, pageItem);
		endInsertRows();
		emit dataChanged(index(std::min(oldRow, newRow), 0), index(std::max(oldRow, (decr ? ++newRow : newRow)), 0), {Qt::DisplayRole});
	} else {
		return QModelIndex();
	}
	return itemIndex;
}

QModelIndex HOCRDocument::swapItems(const QModelIndex& parent, int firstRow, int secondRow) {
	moveItem(index(firstRow, 0, parent), parent, secondRow);
	moveItem(index(secondRow, 0, parent), parent, firstRow);
	return index(firstRow, 0, parent);
}

QModelIndex HOCRDocument::mergeItems(const QModelIndex& parent, int startRow, int endRow) {
	if(endRow - startRow <= 0) {
		return QModelIndex();
	}
	QModelIndex targetIndex = parent.child(startRow, 0);
	HOCRItem* targetItem = mutableItemAtIndex(targetIndex);
	if(!targetItem || targetItem->itemClass() == "ocr_page") {
		return QModelIndex();
	}

	QRect bbox = targetItem->bbox();
	if(targetItem->itemClass() == "ocrx_word") {
		// Merge word items: join text, merge bounding boxes
		QString text = targetItem->text();
		beginRemoveRows(parent, startRow + 1, endRow);
		for(int row = ++startRow; row <= endRow; ++row) {
			HOCRItem* item = mutableItemAtIndex(parent.child(startRow, 0));
			Q_ASSERT(item);
			text += item->text();
			bbox = bbox.united(item->bbox());
			deleteItem(item);
		}
		endRemoveRows();
		targetItem->setText(text);
		emit dataChanged(targetIndex, targetIndex, {Qt::DisplayRole, Qt::ForegroundRole});
	} else {
		// Merge other items: merge dom trees and bounding boxes
		QVector<HOCRItem*> moveChilds;
		beginRemoveRows(parent, startRow + 1, endRow);
		for(int row = ++startRow; row <= endRow; ++row) {
			HOCRItem* item = mutableItemAtIndex(parent.child(startRow, 0));
			Q_ASSERT(item);
			moveChilds.append(item->takeChildren());
			bbox = bbox.united(item->bbox());
			deleteItem(item);
		}
		endRemoveRows();
		int pos = targetItem->children().size();
		beginInsertRows(targetIndex, pos, pos + moveChilds.size() - 1);
		for(HOCRItem* child : moveChilds) {
			targetItem->addChild(child);
		}
		endInsertRows();
	}
	QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
	targetItem->setAttribute("title:bbox", bboxstr);
	emit itemAttributeChanged(targetIndex, "title:bbox", bboxstr);
	return targetIndex;
}

QModelIndex HOCRDocument::addItem(const QModelIndex &parent, const QDomElement &element) {
	HOCRItem* parentItem = mutableItemAtIndex(parent);
	if(!parentItem) {
		return QModelIndex();
	}
	HOCRItem* item = new HOCRItem(element, parentItem->page(), parentItem);
	int pos = parentItem->children().size();
	beginInsertRows(parent, pos, pos);
	parentItem->addChild(item);
	recomputeParentBBoxes(item);
	endInsertRows();
	return index(pos, 0, parent);
}

bool HOCRDocument::removeItem(const QModelIndex& index) {
	HOCRItem* item = mutableItemAtIndex(index);
	if(!item) {
		return false;
	}
	beginRemoveRows(index.parent(), index.row(), index.row());
	deleteItem(item);
	recomputeParentBBoxes(item);
	endRemoveRows();
	return true;
}

QModelIndex HOCRDocument::nextIndex(const QModelIndex& current) {
	QModelIndex idx = current;
	// If the current index is invalid return first index
	if(!idx.isValid()) {
		return index(0, 0);
	}
	// If item has children, return first child
	if(rowCount(idx) > 0) {
		return idx.child(0, 0);
	}
	// Return next possible sibling
	QModelIndex next = idx.sibling(idx.row() + 1, 0);
	while(idx.isValid() && !next.isValid()) {
		idx = idx.parent();
		next = idx.sibling(idx.row() + 1, 0);
	}
	if(!idx.isValid()) {
		// Wrap around
		return index(0, 0);
	}
	return next;
}

QModelIndex HOCRDocument::prevIndex(const QModelIndex& current) {
	QModelIndex idx = current;
	// If the current index is invalid return last index
	if(!idx.isValid()) {
		return index(rowCount() - 1, 0);
	}
	// Return last possible leaf of previous sibling, if any, or parent
	if(idx.row() > 0) {
		idx = idx.sibling(idx.row() - 1, 0);
	} else {
		if(idx.parent().isValid()) {
			return idx.parent();
		}
		// Wrap around
		idx = index(rowCount() - 1, 0);
	}
	while(rowCount(idx) > 0) {
		idx = idx.child(rowCount(idx) - 1, 0);
	}
	return idx;
}

bool HOCRDocument::indexIsMisspelledWord(const QModelIndex& index) const {
	return !checkItemSpelling(itemAtIndex(index));
}

bool HOCRDocument::referencesSource(const QString& filename) const {
	for(const HOCRPage* page : m_pages) {
		if(page->sourceFile() == filename) {
			return true;
		}
	}
	return false;
}

QModelIndex HOCRDocument::searchPage(const QString& filename, int pageNr) const {
	for(int iPage = 0, nPages = m_pages.size(); iPage < nPages; ++iPage) {
		const HOCRPage* page = m_pages[iPage];
		if(page->sourceFile() == filename && page->pageNr() == pageNr) {
			return index(iPage, 0);
		}
	}
	return QModelIndex();
}

QModelIndex HOCRDocument::searchAtCanvasPos(const QModelIndex& pageIndex, const QPoint& pos) const {
	QModelIndex index = pageIndex;
	if(!index.isValid()) {
		return QModelIndex();
	}
	const HOCRItem* item = itemAtIndex(index);
	while(true) {
		int iChild = 0, nChildren = item->children().size();
		for(; iChild < nChildren; ++iChild) {
			const HOCRItem* childItem = item->children()[iChild];
			if(childItem->bbox().contains(pos)) {
				item = childItem;
				index = index.child(iChild, 0);
				break;
			}
		}
		if(iChild == nChildren) {
			break;
		}
	}
	return index;
}

void HOCRDocument::convertSourcePaths(const QString& basepath, bool absolute) {
	for(HOCRPage* page : m_pages) {
		page->convertSourcePath(basepath, absolute);
	}
}

QVariant HOCRDocument::data(const QModelIndex &index, int role) const {
	if (!index.isValid())
		return QVariant();

	HOCRItem *item = mutableItemAtIndex(index);

	if(index.column() == 0) {
		switch (role) {
		case Qt::DisplayRole:
		case Qt::EditRole:
			return displayRoleForItem(item);
		case Qt::DecorationRole:
			return decorationRoleForItem(item);
		case Qt::ForegroundRole: {
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
			return item->getTitleAttributes()["x_wconf"];
		}
	}
	return QVariant();
}

bool HOCRDocument::setData(const QModelIndex &index, const QVariant &value, int role) {
	if(!index.isValid())
		return false;

	HOCRItem *item = mutableItemAtIndex(index);
	if(role == Qt::EditRole && item->itemClass() == "ocrx_word") {
		item->setText(value.toString());
		emit dataChanged(index, index, {Qt::DisplayRole, Qt::ForegroundRole});
		return true;
	} else if(role == Qt::CheckStateRole) {
		item->setEnabled(value == Qt::Checked);
		emit dataChanged(index, index, {Qt::CheckStateRole});
		recursiveDataChanged(index, {Qt::CheckStateRole});
		return true;
	}
	return false;
}

void HOCRDocument::recursiveDataChanged(const QModelIndex& parent, const QVector<int>& roles, const QStringList& itemClasses) {
	int rows = rowCount(parent);
	if(rows > 0) {
		QModelIndex firstChild = index(0, 0, parent);
		QString childItemClass = itemAtIndex(firstChild)->itemClass();
		if(itemClasses.isEmpty() || itemClasses.contains(childItemClass)) {
			emit dataChanged(firstChild, index(rows - 1, 0, parent), roles);
		}
		for(int i = 0; i < rows; ++i) {
			recursiveDataChanged(index(i, 0, parent), roles, itemClasses);
		}
	}
}

void HOCRDocument::recomputeParentBBoxes(const HOCRItem *item) {
	// Update parent bboxes (except page)
	HOCRItem* parent = item->parent();
	while(parent && parent->parent()) {
		QRect bbox;
		for(const HOCRItem* child : parent->children()) {
			bbox = bbox.united(child->bbox());
		}
		QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
		parent->setAttribute("title:bbox", bboxstr);
		parent = parent->parent();
	}
}

Qt::ItemFlags HOCRDocument::flags(const QModelIndex &index) const {
	if (!index.isValid())
		return 0;

	HOCRItem *item = mutableItemAtIndex(index);
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | (item->itemClass() == "ocrx_word" && index.column() == 0 ? Qt::ItemIsEditable : Qt::NoItemFlags);
}

QModelIndex HOCRDocument::index(int row, int column, const QModelIndex &parent) const {
	if (!hasIndex(row, column, parent))
		return QModelIndex();

	HOCRItem* childItem = nullptr;
	if(!parent.isValid()) {
		childItem = m_pages.value(row);
	} else {
		HOCRItem *parentItem = mutableItemAtIndex(parent);
		childItem = parentItem->children().value(row);
	}
	return childItem ? createIndex(row, column, childItem) : QModelIndex();
}

QModelIndex HOCRDocument::parent(const QModelIndex &child) const {
	if (!child.isValid())
		return QModelIndex();

	HOCRItem *item = mutableItemAtIndex(child)->parent();
	if(!item) {
		return QModelIndex();
	}
	int row = item->index();
	return row >= 0 ? createIndex(row, 0, item) : QModelIndex();
}

int HOCRDocument::rowCount(const QModelIndex &parent) const {
	if (parent.column() > 0)
		return 0;
	return !parent.isValid() ? m_pages.size() : itemAtIndex(parent)->children().size();
}

int HOCRDocument::columnCount(const QModelIndex &/*parent*/) const {
	return 2;
}

QString HOCRDocument::displayRoleForItem(const HOCRItem* item) const {
	QString itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		const HOCRPage* page = static_cast<const HOCRPage*>(item);
		return QString("%1 (%2 %3/%4)").arg(page->title()).arg(tr("Page")).arg(item->index() + 1).arg(m_pages.size());
	} else if(itemClass == "ocr_carea") {
		return _("Text block");
	} else if(itemClass == "ocr_par") {
		return _("Paragraph");
	} else if(itemClass == "ocr_line") {
		return _("Textline");
	} else if(itemClass == "ocrx_word") {
		return item->text();
	} else if(itemClass == "ocr_graphic") {
		return _("Graphic");
	}
	return "";
}

QIcon HOCRDocument::decorationRoleForItem(const HOCRItem* item) const {
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

bool HOCRDocument::checkItemSpelling(const HOCRItem* item) const {
	if(item->itemClass() == "ocrx_word") {
		QString trimmed = HOCRItem::trimmedWord(item->text());
		if(!trimmed.isEmpty()) {
			QString lang = item->lang();
			bool haveLanguage = true;
			if(m_spell->getLanguage() != lang) {
				haveLanguage = m_spell->setLanguage(lang);
			}
			return !haveLanguage || m_spell->checkWord(trimmed);
		}
	}
	return true;
}

void HOCRDocument::deleteItem(HOCRItem* item) {
	if(item->parent()) {
		item->parent()->removeChild(item);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		int idx = page->index();
		delete m_pages.takeAt(idx);
		for(int i = idx, n = m_pages.size(); i < n; ++i) {
			m_pages[i]->m_index = i;
		}
		emit dataChanged(index(0, 0), index(m_pages.size() - 1, 0), {Qt::DisplayRole});
	}
}

void HOCRDocument::takeItem(HOCRItem* item) {
	if(item->parent()) {
		item->parent()->takeChild(item);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		int idx = m_pages.indexOf(page);
		m_pages.takeAt(idx);
	}
}

///////////////////////////////////////////////////////////////////////////////

QMap<QString,QString> HOCRItem::s_langCache = QMap<QString,QString>();

QMap<QString, QString> HOCRItem::deserializeAttrGroup(const QString& string) {
	QMap<QString, QString> attrs;
	for(const QString& attr : string.split(QRegExp("\\s*;\\s*"))) {
		int splitPos = attr.indexOf(QRegExp("\\s+"));
		attrs.insert(attr.left(splitPos), attr.mid(splitPos + 1));
	}
	return attrs;
}

QString HOCRItem::serializeAttrGroup(const QMap<QString, QString>& attrs) {
	QStringList list;
	for(auto it = attrs.begin(), itEnd = attrs.end(); it != itEnd; ++it) {
		list.append(QString("%1 %2").arg(it.key(), it.value()));
	}
	return list.join("; ");
}

QString HOCRItem::trimmedWord(const QString& word, QString* prefix, QString* suffix) {
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

HOCRItem::HOCRItem(QDomElement element, HOCRPage* page, HOCRItem* parent, int index)
	: m_domElement(element), m_pageItem(page), m_parentItem(parent), m_index(index) {
	// Adjust item id based on pageId
	if(parent) {
		QString idClass = itemClass().mid(itemClass().indexOf("_") + 1);
		int counter = page->m_idCounters.value(idClass, 0) + 1;
		page->m_idCounters[idClass] = counter;
		QString newId = QString("%1_%2_%3").arg(idClass).arg(page->pageId()).arg(counter);
		m_domElement.setAttribute("id", newId);
	}

	// Deserialize title attrs
	m_titleAttrs = deserializeAttrGroup(m_domElement.attribute("title"));

	// Parse item bbox
	QStringList bbox = m_titleAttrs["bbox"].split(QRegExp("\\s+"));
	if(bbox.size() == 4) {
		m_bbox.setCoords(bbox[0].toInt(), bbox[1].toInt(), bbox[2].toInt(), bbox[3].toInt());
	}

	// For the last word items of the line, ensure the correct hyphen is used
	if(itemClass() == "ocrx_word") {
		QDomElement nextElement = m_domElement.nextSiblingElement();
		if(nextElement.isNull()) {
			setText(m_domElement.text().replace(QRegExp("[-\u2014]\\s*$"), "-"));
		}
	}


}

HOCRItem::~HOCRItem() {
	qDeleteAll(m_childItems);
}

void HOCRItem::addChild(HOCRItem* child) {
	m_domElement.appendChild(child->m_domElement);
	m_childItems.append(child);
	child->m_parentItem = this;
	child->m_pageItem = m_pageItem;
	child->m_index = m_childItems.size() - 1;
}

void HOCRItem::insertChild(HOCRItem* child, int i) {
	m_domElement.insertBefore(child->m_domElement, m_domElement.childNodes().at(i));
	m_childItems.insert(i, child);
	child->m_parentItem = this;
	child->m_pageItem = m_pageItem;
}

void HOCRItem::removeChild(HOCRItem *child) {
	m_domElement.removeChild(child->m_domElement);
	int idx = child->index();
	delete m_childItems.takeAt(idx);
	for(int i = idx, n = m_childItems.size(); i < n ; ++i) {
		m_childItems[i]->m_index = i;
	}
}

void HOCRItem::takeChild(HOCRItem* child) {
	m_domElement.removeChild(child->m_domElement);
	m_childItems.takeAt(m_childItems.indexOf(child));
}

QVector<HOCRItem*> HOCRItem::takeChildren() {
	QVector<HOCRItem*> children(m_childItems);
	m_childItems.clear();
	return children;
}

void HOCRItem::setText(const QString& newText) {
	QDomElement leaf = m_domElement;
	while(!leaf.firstChildElement().isNull()) {
		leaf = leaf.firstChildElement();
	}
	leaf.replaceChild(leaf.ownerDocument().createTextNode(newText), leaf.firstChild());
}

QMap<QString,QString> HOCRItem::getAllAttributes() const {
	QMap<QString,QString> attrValues;
	QDomNamedNodeMap attributes = m_domElement.attributes();
	for(int i = 0, n = attributes.count(); i < n; ++i) {
		QDomNode attribNode = attributes.item(i);
		QString attrName = attribNode.nodeName();
		if(attrName == "title") {
			for(auto it = m_titleAttrs.begin(), itEnd = m_titleAttrs.end(); it != itEnd; ++it) {
				attrValues.insert(QString("title:%1").arg(it.key()), it.value());
			}
		} else {
			attrValues.insert(attrName, attribNode.nodeValue());
		}
	}
	if(itemClass() == "ocrx_word") {
		if(!attrValues.contains("title:x_font")) {
			attrValues.insert("title:x_font", "");
		}
		attrValues.insert("bold", fontBold() ? "1" : "0");
		attrValues.insert("italic", fontItalic() ? "1" : "0");
	}
	return attrValues;
}

QMap<QString,QString> HOCRItem::getAttributes(const QList<QString>& names = QList<QString>()) const {
	QMap<QString,QString> attrValues;
	for(const QString& attrName : names) {
		QStringList parts = attrName.split(":");
		if(parts.size() > 1) {
			Q_ASSERT(parts[0] == "title");
			attrValues.insert(attrName, m_titleAttrs.value(parts[1]));
		} else if(attrName == "bold") {
			attrValues.insert(attrName, fontBold() ? "1" : "0");
		} else if(attrName == "italic") {
			attrValues.insert(attrName, fontItalic() ? "1" : "0");
		} else {
			attrValues.insert(attrName, m_domElement.attribute(attrName));
		}
	}
	return attrValues;
}

void HOCRItem::getPropagatableAttributes(QMap<QString, QMap<QString, QSet<QString>>>& occurences) const {
	static QMap<QString,QStringList> s_propagatableAttributes = {
		{"ocr_line", {"title:baseline"}},
		{"ocrx_word", {"lang", "title:x_fsize", "title:x_font", "bold", "italic"}}
	};

	QString childClass = m_childItems.isEmpty() ? "" : m_childItems.front()->itemClass();
	auto it = s_propagatableAttributes.find(childClass);
	if(it != s_propagatableAttributes.end()) {
		for(HOCRItem* child : m_childItems) {
			QMap<QString, QString> attrs = child->getAttributes(it.value());
			for(auto attrIt = attrs.begin(), attrItEnd = attrs.end(); attrIt != attrItEnd; ++attrIt) {
				occurences[childClass][attrIt.key()].insert(attrIt.value());
			}
		}
	}
	if(childClass != "ocrx_word") {
		for(HOCRItem* child : m_childItems) {
			child->getPropagatableAttributes(occurences);
		}
	}
}

void HOCRItem::setAttribute(const QString& name, const QString& value, const QString& attrItemClass) {
	if(!attrItemClass.isEmpty() && itemClass() != attrItemClass) {
		for(HOCRItem* child : m_childItems) {
			child->setAttribute(name, value, attrItemClass);
		}
		return;
	}
	QStringList parts = name.split(":");
	if(name == "bold" || name == "italic") {
		QString elemName = (name == "bold" ? "strong" : "em");
		bool currentState = (name == "bold" ? fontBold() : fontItalic());
		if(value == "1" && !currentState) {
			QDomElement elem = m_domElement.ownerDocument().createElement(elemName);
			QDomNodeList children = m_domElement.childNodes();
			for(int i = 0, n = children.size(); i < n; ++i) {
				elem.appendChild(children.at(i));
			}
			m_domElement.appendChild(elem);
		} else if(value == "0" && currentState) {
			QDomElement elem = m_domElement.elementsByTagName(elemName).at(0).toElement();
			QDomNodeList children = elem.childNodes();
			for(int i = 0, n = children.size(); i < n; ++i) {
				elem.parentNode().appendChild(children.at(i));
			}
			elem.parentNode().removeChild(elem);
		}
	} else if(parts.size() < 2) {
		m_domElement.setAttribute(name, value);
	} else {
		Q_ASSERT(parts[0] == "title");
		m_titleAttrs[parts[1]] = value;
		m_domElement.setAttribute("title", serializeAttrGroup(m_titleAttrs));
		if(name == "title:bbox") {
			QStringList bbox = value.split(QRegExp("\\s+"));
			Q_ASSERT(bbox.size() == 4);
			m_bbox.setCoords(bbox[0].toInt(), bbox[1].toInt(), bbox[2].toInt(), bbox[3].toInt());
		}
	}
}

QString HOCRItem::toHtml(int indent) const {
	QString str;
	QTextStream stream(&str);
	m_domElement.save(stream, indent);
	return str;
}

int HOCRItem::baseLine() const {
	static const QRegExp baseLineRx = QRegExp("([+-]?\\d+\\.?\\d*)\\s+([+-]?\\d+)");
	if(baseLineRx.indexIn(m_titleAttrs["baseline"]) != -1) {
		return baseLineRx.cap(2).toInt();
	}
	return 0;
}

bool HOCRItem::parseChildren(QString language) {
	// Determine item language (inherit from parent if not specified)
	QString elemLang = m_domElement.attribute("lang");
	if(!elemLang.isEmpty()) {
		auto it = s_langCache.find(elemLang);
		if(it == s_langCache.end()) {
			it = s_langCache.insert(elemLang, Utils::getSpellingLanguage(elemLang));
		}
		m_domElement.removeAttribute("lang");
		language = it.value();
	}

	if(itemClass() == "ocrx_word") {
		m_domElement.setAttribute("lang", language);
		return !m_domElement.text().isEmpty();
	}
	bool haveWords = false;
	QDomElement childElement = m_domElement.firstChildElement();
	while(!childElement.isNull()) {
		m_childItems.append(new HOCRItem(childElement, m_pageItem, this, m_childItems.size()));
		haveWords |= m_childItems.last()->parseChildren(language);
		childElement = childElement.nextSiblingElement();
	}
	return haveWords;
}

///////////////////////////////////////////////////////////////////////////////

HOCRPage::HOCRPage(QDomElement element, int pageId, const QString& language, bool cleanGraphics, int index)
	: HOCRItem(element, this, nullptr, index), m_pageId(pageId) {
	m_domElement.setAttribute("id", QString("page_%1").arg(pageId));

	m_sourceFile = m_titleAttrs["image"].replace(QRegExp("^['\"]"), "").replace(QRegExp("['\"]$"), "");
	m_pageNr = m_titleAttrs["ppageno"].toInt();
	// Code to handle pageno -> ppageno typo in previous versions of gImageReader
	if(m_pageNr == 0) {
		m_pageNr = m_titleAttrs["pageno"].toInt();
		m_titleAttrs["ppageno"] = m_titleAttrs["pageno"];
		m_titleAttrs.remove("pageno");
	}
	m_angle = m_titleAttrs["rot"].toDouble();
	m_resolution = m_titleAttrs["res"].toInt();

	QDomElement childElement = m_domElement.firstChildElement("div");
	while(!childElement.isNull()) {
		HOCRItem* item = new HOCRItem(childElement, this, this, m_childItems.size());
		m_childItems.append(item);
		if(!item->parseChildren(language)) {
			// No word children -> treat as graphic
			if(cleanGraphics && (item->bbox().width() < 10 || item->bbox().height() < 10)) {
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

void HOCRPage::convertSourcePath(const QString& basepath, bool absolute) {
	if(absolute && !QFileInfo(m_sourceFile).isAbsolute()) {
		m_sourceFile = QDir::cleanPath(QDir(basepath).absoluteFilePath(m_sourceFile));
	} else if(!absolute && QFileInfo(m_sourceFile).isAbsolute() && m_sourceFile.startsWith(basepath)) {
		m_sourceFile = QString(".%1%2").arg("/").arg(QDir(basepath).relativeFilePath(m_sourceFile));
	}
	m_titleAttrs["image"] = QString("'%1'").arg(m_sourceFile);
	m_domElement.setAttribute("title", serializeAttrGroup(m_titleAttrs));
}
