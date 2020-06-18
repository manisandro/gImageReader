/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.cc
 * Copyright (C) 2013-2020 Sandro Mani <manisandro@gmail.com>
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
#include <QDomElement>
#include <QFileInfo>
#include <QMenu>
#include <QIcon>
#include <QSet>
#include <QTextStream>
#include <cmath>

#include "common.hh"
#include "HOCRDocument.hh"
#include "HOCRSpellChecker.hh"
#include "Utils.hh"


HOCRDocument::HOCRDocument(QObject* parent)
	: QAbstractItemModel(parent) {
	m_spell = new HOCRSpellChecker(this);
}

HOCRDocument::~HOCRDocument() {
	qDeleteAll(m_pages);
}

void HOCRDocument::clear() {
	beginResetModel();
	qDeleteAll(m_pages);
	m_pages.clear();
	m_pageIdCounter = 0;
	endResetModel();
}

void HOCRDocument::resetMisspelled(const QModelIndex& index) {
	if(hasChildren(index)) {
		for(int i = 0, n = rowCount(index); i < n; ++i) {
			resetMisspelled(this->index(i, 0, index));
		}
	} else {
		mutableItemAtIndex(index)->setMisspelled(-1);
		emit dataChanged(index, index, {Qt::ForegroundRole});
	}
}

void HOCRDocument::addSpellingActions(QMenu* menu, const QModelIndex& index) {
	QStringList suggestions;
	QString trimmedWord;
	bool valid = getItemSpellingSuggestions(index, trimmedWord, suggestions, 16);
	for(const QString& suggestion : suggestions) {
		menu->addAction(suggestion, menu, [this, suggestion, index] { setData(index, suggestion, Qt::EditRole); });
	}
	if(!trimmedWord.isEmpty()) {
		if(suggestions.isEmpty()) {
			menu->addAction(_("No suggestions"))->setEnabled(false);
		}
		if(!valid) {
			menu->addSeparator();
			menu->addAction(_("Add to dictionary"), menu, [this, trimmedWord, index] {
				m_spell->addWordToDictionary(trimmedWord);
				resetMisspelled(index);
			});
			menu->addAction(_("Ignore word"), menu, [this, trimmedWord, index] {
				m_spell->ignoreWord(trimmedWord);
				resetMisspelled(index);
			});
		}
	}
}

void HOCRDocument::addWordToDictionary(const QModelIndex& index) {
	const HOCRItem* item = itemAtIndex(index);
	if(item && item->isMisspelled()) {
		QString trimmedWord = HOCRItem::trimmedWord(item->text());
		m_spell->addWordToDictionary(trimmedWord);
		resetMisspelled(index);
	}
}

QString HOCRDocument::toHTML() const {
	QString html = "<body>\n";
	for(const HOCRPage* page : m_pages) {
		html += page->toHtml(1);
	}
	html += "</body>\n";
	return html;
}


QModelIndex HOCRDocument::insertPage(int beforeIdx, const QDomElement& pageElement, bool cleanGraphics, const QString& sourceBasePath) {
	beginInsertRows(QModelIndex(), beforeIdx, beforeIdx);
	m_pages.insert(beforeIdx, new HOCRPage(pageElement, ++m_pageIdCounter, m_defaultLanguage, cleanGraphics, beforeIdx));
	if(!sourceBasePath.isEmpty()) {
		m_pages[beforeIdx]->convertSourcePath(sourceBasePath, true);
	}
	for(int i = beforeIdx + 1; i < m_pages.size(); ++i) {
		m_pages[i]->m_index = i;
	}
	endInsertRows();
	emit dataChanged(index(0, 0), index(m_pages.size() - 1, 0), {Qt::DisplayRole});
	return index(beforeIdx, 0);
}

QModelIndex HOCRDocument::indexAtItem(const HOCRItem* item) const {
	QList<HOCRItem*> parents;
	HOCRItem* parent = item->parent();
	while(parent) {
		parents.prepend(parent);
		parent = parent->parent();
	}
	QModelIndex idx;
	for(HOCRItem* parent : parents) {
		idx = index(parent->index(), 0, idx);
	}
	return index(item->index(), 0, idx);
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
		resetMisspelled(index);
	}
	emit itemAttributeChanged(index, name, value);
	if(name == "title:bbox") {
		recomputeBBoxes(item->parent());
	}
	return true;
}

// Might be more logical to accept oldParent and oldRow as parameters rather than itemIndex
// (since we need them anyway), if all our callers are likely to know them. Swap does; drag&drop might not.
QModelIndex HOCRDocument::moveItem(const QModelIndex& itemIndex, const QModelIndex& newParent, int newRow) {
	HOCRItem* item = mutableItemAtIndex(itemIndex);
	HOCRItem* parentItem = mutableItemAtIndex(newParent);
	if(!item || (!parentItem && item->itemClass() != "ocr_page")) {
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
	if(oldParent == newParent && oldRow < newRow) {
		--newRow;
	}
	beginRemoveRows(oldParent, oldRow, oldRow);
	takeItem(item);
	endRemoveRows();
	recomputeBBoxes(mutableItemAtIndex(oldParent));
	beginInsertRows(newParent, newRow, newRow);
	insertItem(parentItem, item, newRow);
	endInsertRows();
	recomputeBBoxes(parentItem);
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
	QModelIndex targetIndex = index(startRow, 0, parent);
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
			HOCRItem* item = mutableItemAtIndex(index(startRow, 0, parent));
			Q_ASSERT(item);
			text += item->text();
			bbox = bbox.united(item->bbox());
			deleteItem(item);
		}
		endRemoveRows();
		targetItem->setText(text);
		for(QModelIndex changedIndex : recheckItemSpelling(targetIndex)) {
			emit dataChanged(changedIndex, changedIndex, {Qt::DisplayRole, Qt::ForegroundRole});
		}
	} else {
		// Merge other items: merge dom trees and bounding boxes
		QVector<HOCRItem*> moveChilds;
		beginRemoveRows(parent, startRow + 1, endRow);
		for(int row = ++startRow; row <= endRow; ++row) {
			HOCRItem* item = mutableItemAtIndex(index(startRow, 0, parent));
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

QModelIndex HOCRDocument::splitItem(const QModelIndex& itemIndex, int startRow, int endRow) {
	if(endRow - startRow < 0) {
		return QModelIndex();
	}
	HOCRItem* item = mutableItemAtIndex(itemIndex);
	if(!item) {
		return QModelIndex();
	}
	QString itemClass = item->itemClass();
	QDomDocument doc;
	QDomElement newElement;
	if(itemClass == "ocr_carea") {
		newElement = doc.createElement("div");
	} else if(itemClass == "ocr_par") {
		newElement = doc.createElement("p");
	} else if(itemClass == "ocr_line") {
		newElement = doc.createElement("span");
	} else {
		return QModelIndex();
	}
	newElement.setAttribute("class", itemClass);
	newElement.setAttribute("title", HOCRItem::serializeAttrGroup(item->getTitleAttributes()));
	HOCRItem* newItem = new HOCRItem(newElement, item->page(), item->parent(), item->index() + 1);
	beginInsertRows(itemIndex.parent(), item->index() + 1, item->index() + 1);
	insertItem(item->parent(), newItem, item->index() + 1);
	endInsertRows();
	QModelIndex newIndex = itemIndex.sibling(itemIndex.row() + 1, 0);
	for(int row = 0; row <= (endRow - startRow); ++row) {
		QModelIndex childIndex = index(startRow, 0, itemIndex);
		HOCRItem* child = mutableItemAtIndex(childIndex);
		Q_ASSERT(child);
		moveItem(childIndex, newIndex, row);
	}

	return newIndex;
}

QModelIndex HOCRDocument::splitItemText(const QModelIndex& itemIndex, int pos) {
	HOCRItem* item = mutableItemAtIndex(itemIndex);
	if(!item || item->itemClass() != "ocrx_word") {
		return QModelIndex();
	}
	// Don't split if it would create empty words
	if(pos == 0 || pos == item->text().length()) {
		return itemIndex;
	}
	// Compute new bounding box using font metrics with default font
	QFontMetrics metrics((QFont()));
	double fraction = metrics.horizontalAdvance(item->text().left(pos)) / double(metrics.horizontalAdvance(item->text()));
	QRect bbox = item->bbox();
	QRect leftBBox = bbox;
	leftBBox.setWidth(bbox.width() * fraction);
	QRect rightBBox = bbox;
	rightBBox.setLeft(bbox.left() + bbox.width() * fraction);

	QDomDocument doc;
	QDomElement newElement = doc.createElement("span");
	newElement.appendChild(doc.createTextNode(item->text().mid(pos)));
	newElement.setAttribute("class", "ocrx_word");
	QMap<QString, QString> attrs = item->getTitleAttributes();
	attrs["bbox"] = QString("%1 %2 %3 %4").arg(rightBBox.left()).arg(rightBBox.top()).arg(rightBBox.right()).arg(rightBBox.bottom());
	newElement.setAttribute("title", HOCRItem::serializeAttrGroup(attrs));
	HOCRItem* newItem = new HOCRItem(newElement, item->page(), item->parent(), item->index() + 1);
	beginInsertRows(itemIndex.parent(), item->index() + 1, item->index() + 1);
	insertItem(item->parent(), newItem, item->index() + 1);
	endInsertRows();
	setData(itemIndex, item->text().left(pos), Qt::EditRole);
	editItemAttribute(itemIndex, "title:bbox", QString("%1 %2 %3 %4").arg(leftBBox.left()).arg(leftBBox.top()).arg(leftBBox.right()).arg(leftBBox.bottom()));
	return itemIndex.sibling(itemIndex.row() + 1, 0);
}

QModelIndex HOCRDocument::mergeItemText(const QModelIndex& itemIndex, bool mergeNext) {
	HOCRItem* item = mutableItemAtIndex(itemIndex);
	if(!item || item->itemClass() != "ocrx_word") {
		return QModelIndex();
	}
	if((!mergeNext && item->index() == 0) || (mergeNext && item->index() == item->parent()->children().size() - 1)) {
		return QModelIndex();
	}
	int offset = mergeNext ? 1 : -1;
	HOCRItem* otherItem = item->parent()->children()[item->index() + offset];
	QString newText = mergeNext ? item->text() + otherItem->text() : otherItem->text() + item->text();
	QRect newBbox = item->bbox().united(otherItem->bbox());
	setData(itemIndex, newText, Qt::EditRole);
	editItemAttribute(itemIndex, "title:bbox", QString("%1 %2 %3 %4").arg(newBbox.left()).arg(newBbox.top()).arg(newBbox.right()).arg(newBbox.bottom()));
	beginRemoveRows(itemIndex.parent(), itemIndex.row() + offset, itemIndex.row() + offset);
	deleteItem(otherItem);
	endRemoveRows();
	return itemIndex;
}

QModelIndex HOCRDocument::addItem(const QModelIndex& parent, const QDomElement& element) {
	HOCRItem* parentItem = mutableItemAtIndex(parent);
	if(!parentItem) {
		return QModelIndex();
	}
	HOCRItem* item = new HOCRItem(element, parentItem->page(), parentItem);
	int pos = parentItem->children().size();
	beginInsertRows(parent, pos, pos);
	parentItem->addChild(item);
	recomputeBBoxes(parentItem);
	endInsertRows();
	return index(pos, 0, parent);
}

bool HOCRDocument::removeItem(const QModelIndex& index) {
	HOCRItem* item = mutableItemAtIndex(index);
	if(!item) {
		return false;
	}
	HOCRItem* parentItem = item->parent();
	beginRemoveRows(index.parent(), index.row(), index.row());
	deleteItem(item);
	endRemoveRows();
	recomputeBBoxes(parentItem);
	return true;
}

QModelIndex HOCRDocument::nextIndex(const QModelIndex& current) const {
	QModelIndex idx = current;
	// If the current index is invalid return first index
	if(!idx.isValid()) {
		return index(0, 0);
	}
	// If item has children, return first child
	if(rowCount(idx) > 0) {
		return index(0, 0, idx);
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

QModelIndex HOCRDocument::prevIndex(const QModelIndex& current) const {
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
		idx = index(rowCount(idx) - 1, 0, idx);
	}
	return idx;
}

QModelIndex HOCRDocument::prevOrNextIndex(bool next, const QModelIndex& current, const QString& ocrClass, bool misspelled) const {
	QModelIndex start = current;
	if(!start.isValid()) {
		start = index(0, 0);
	}
	QModelIndex curr = next ? nextIndex(start) : prevIndex(start);
	while(curr != start) {
		const HOCRItem* item = itemAtIndex(curr);
		if(item && item->itemClass() == ocrClass && (!misspelled || indexIsMisspelledWord(curr))) {
			break;
		}
		curr = next ? nextIndex(curr) : prevIndex(curr);
	};
	return curr;
}

bool HOCRDocument::indexIsMisspelledWord(const QModelIndex& index) const {
	const HOCRItem* item = itemAtIndex(index);
	if(item->isMisspelled() == -1) {
		recheckItemSpelling(index);
	}
	return item->isMisspelled() == 1;
}

QList<QModelIndex> HOCRDocument::recheckItemSpelling(const QModelIndex& index) const {
	HOCRItem* item = mutableItemAtIndex(index);
	if(item->itemClass() != "ocrx_word") { return {}; }

	QString prefix, suffix, trimmed = HOCRItem::trimmedWord(item->text(), &prefix, &suffix);
	if(trimmed.isEmpty()) {
		item->setMisspelled(false);
		return {index};
	}
	QString lang = item->spellingLang();
	if(m_spell->getLanguage() != lang && !(m_spell->setLanguage(lang))) {
		item->setMisspelled(false);
		return {index};
	}

	// check word, including (if requested) setting suggestions; handle hyphenated phrases correctly
	if(m_spell->checkSpelling(trimmed)) {
		item->setMisspelled(false);
		return {index};
	}
	item->setMisspelled(true);

	// handle some hyphenated words
	// don't bother with words not broken over sibling text lines (ie interrupted by other blocks), it's human hard
	// don't bother with words broken over three or more lines, it's implausible and this treatment is ^ necessarily incomplete
	HOCRItem* line = item->parent();
	if(!line) { return {index}; }
	HOCRItem* paragraph = line->parent();
	if(!paragraph) { return {index}; }
	int lineIdx = line->index();
	const QVector<HOCRItem*>& paragraphLines = paragraph->children();
	if(item == line->children().front() && lineIdx > 0) {
		HOCRItem* prevLine = paragraphLines.at(lineIdx - 1);
		if(!prevLine || prevLine->children().isEmpty()) { return {index}; }
		HOCRItem* prevWord = prevLine->children().back();
		if(!prevWord || prevWord->itemClass() != "ocrx_word") { return {index}; }
		QString prevText = prevWord->text();
		if(!prevText.endsWith("-")) { return {index}; }

		// don't bother with (reassembled) suggestions for broken words since we can't re-break them
		bool valid = m_spell->checkSpelling(HOCRItem::trimmedWord(prevText) + trimmed);
		item->setMisspelled(!valid);
		prevWord->setMisspelled(!valid);
		return {index, indexAtItem(prevWord)};
	} else if(item == line->children().back() && lineIdx + 1 < paragraphLines.size() && item->text().endsWith("-")) {
		HOCRItem* nextLine = paragraphLines.at(lineIdx + 1);
		if(!nextLine || nextLine->children().isEmpty()) { return {index}; }
		HOCRItem* nextWord = nextLine->children().front();
		if(!nextWord || nextWord->itemClass() != "ocrx_word") { return {index}; }

		bool valid = m_spell->checkSpelling(trimmed + HOCRItem::trimmedWord(nextWord->text()));
		item->setMisspelled(!valid);
		nextWord->setMisspelled(!valid);
		return {index, indexAtItem(nextWord)};
	}
	return {index};
}

bool HOCRDocument::getItemSpellingSuggestions(const QModelIndex& index, QString& trimmedWord, QStringList& suggestions, int limit) const {
	const HOCRItem* item = itemAtIndex(index);
	if(item->itemClass() != "ocrx_word") { return true; }

	QString prefix, suffix;
	trimmedWord = HOCRItem::trimmedWord(item->text(), &prefix, &suffix);
	if(trimmedWord.isEmpty()) {
		return true;
	}
	QString lang = item->spellingLang();
	if(m_spell->getLanguage() != lang && !(m_spell->setLanguage(lang))) {
		return true;
	}

	bool valid = m_spell->checkSpelling(trimmedWord, &suggestions, limit);
	for(int i = 0, n = suggestions.size(); i < n; ++i) {
		suggestions[i] = prefix + suggestions[i] + suffix;
	}
	return valid;
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
	QModelIndex idx = pageIndex;
	if(!idx.isValid()) {
		return QModelIndex();
	}
	const HOCRItem* item = itemAtIndex(idx);
	while(true) {
		int iChild = 0, nChildren = item->children().size();
		for(; iChild < nChildren; ++iChild) {
			const HOCRItem* childItem = item->children()[iChild];
			if(childItem->bbox().contains(pos)) {
				item = childItem;
				idx = index(iChild, 0, idx);
				break;
			}
		}
		if(iChild == nChildren) {
			break;
		}
	}
	return idx;
}

void HOCRDocument::convertSourcePaths(const QString& basepath, bool absolute) {
	for(HOCRPage* page : m_pages) {
		page->convertSourcePath(basepath, absolute);
	}
}

QVariant HOCRDocument::data(const QModelIndex& index, int role) const {
	if (!index.isValid()) {
		return QVariant();
	}

	HOCRItem* item = mutableItemAtIndex(index);

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
				return indexIsMisspelledWord(index) ? QVariant(QColor(Qt::red)) : QVariant();
			} else {
				return indexIsMisspelledWord(index) ? QVariant(QColor(208, 80, 82)) : QVariant(QColor(Qt::gray));
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

bool HOCRDocument::setData(const QModelIndex& index, const QVariant& value, int role) {
	if(!index.isValid()) {
		return false;
	}

	HOCRItem* item = mutableItemAtIndex(index);
	if(role == Qt::EditRole && item->itemClass() == "ocrx_word") {
		item->setText(value.toString());
		for(QModelIndex changedIndex : recheckItemSpelling(index)) {
			emit dataChanged(changedIndex, changedIndex, {Qt::DisplayRole, Qt::ForegroundRole});
		}

		return true;
	} else if(role == Qt::CheckStateRole) {
		item->setEnabled(value == Qt::Checked);
		// Get leaf
		QModelIndex leaf = index;
		while(hasChildren(leaf)) {
			leaf = this->index(rowCount(leaf) - 1, 0, leaf);
		}
		emit dataChanged(index, leaf, {Qt::CheckStateRole});
		return true;
	}
	return false;
}

void HOCRDocument::recomputeBBoxes(HOCRItem* item) {
	// Update parent bboxes (except page)
	while(item && item->parent()) {
		QRect bbox;
		for(const HOCRItem* child : item->children()) {
			bbox = bbox.united(child->bbox());
		}
		QString bboxstr = QString("%1 %2 %3 %4").arg(bbox.left()).arg(bbox.top()).arg(bbox.right()).arg(bbox.bottom());
		item->setAttribute("title:bbox", bboxstr);
		item = item->parent();
	}
}

Qt::ItemFlags HOCRDocument::flags(const QModelIndex& index) const {
	if (!index.isValid()) {
		return 0;
	}

	HOCRItem* item = mutableItemAtIndex(index);
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | (item->itemClass() == "ocrx_word" && index.column() == 0 ? Qt::ItemIsEditable : Qt::NoItemFlags);
}

QModelIndex HOCRDocument::index(int row, int column, const QModelIndex& parent) const {
	if (!hasIndex(row, column, parent)) {
		return QModelIndex();
	}

	HOCRItem* childItem = nullptr;
	if(!parent.isValid()) {
		childItem = m_pages.value(row);
	} else {
		HOCRItem* parentItem = mutableItemAtIndex(parent);
		childItem = parentItem->children().value(row);
	}
	return childItem ? createIndex(row, column, childItem) : QModelIndex();
}

QModelIndex HOCRDocument::parent(const QModelIndex& child) const {
	if (!child.isValid()) {
		return QModelIndex();
	}

	HOCRItem* item = mutableItemAtIndex(child)->parent();
	if(!item) {
		return QModelIndex();
	}
	int row = item->index();
	return row >= 0 ? createIndex(row, 0, item) : QModelIndex();
}

int HOCRDocument::rowCount(const QModelIndex& parent) const {
	if (parent.column() > 0) {
		return 0;
	}
	return !parent.isValid() ? m_pages.size() : itemAtIndex(parent)->children().size();
}

int HOCRDocument::columnCount(const QModelIndex& /*parent*/) const {
	return 2;
}

QString HOCRDocument::displayRoleForItem(const HOCRItem* item) const {
	QString itemClass = item->itemClass();
	if(itemClass == "ocr_page") {
		const HOCRPage* page = static_cast<const HOCRPage*>(item);
		return QString("%1 (%2 %3/%4)").arg(page->title()).arg(_("Page")).arg(item->index() + 1).arg(m_pages.size());
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

void HOCRDocument::insertItem(HOCRItem* parent, HOCRItem* item, int i) {
	if(parent) {
		parent->insertChild(item, i);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		page->m_index = i;
		m_pages.insert(i++, page);
		for(int n = m_pages.size(); i < n; ++i) {
			m_pages[i]->m_index = i;
		}
		emit dataChanged(index(page->index(), 0), index(m_pages.size() - 1, 0), {Qt::DisplayRole});
	}
}

void HOCRDocument::deleteItem(HOCRItem* item) {
	takeItem(item);
	delete item;
}

void HOCRDocument::takeItem(HOCRItem* item) {
	if(item->parent()) {
		item->parent()->takeChild(item);
	} else if(HOCRPage* page = dynamic_cast<HOCRPage*>(item)) {
		int i = page->index();
		m_pages.remove(i);
		for(int n = m_pages.size(); i < n; ++i) {
			m_pages[i]->m_index = i;
		}
		emit dataChanged(index(page->index(), 0), index(m_pages.size() - 1, 0), {Qt::DisplayRole});
	}
}

///////////////////////////////////////////////////////////////////////////////

QMap<QString, QString> HOCRItem::s_langCache = QMap<QString, QString>();

QMap<QString, QString> HOCRItem::deserializeAttrGroup(const QString& string) {
	QMap<QString, QString> attrs;
	for(const QString& attr : string.split(QRegExp("\\s*;\\s*"))) {
		int splitPos = attr.indexOf(QRegExp("\\s+"));
		attrs.insert(attr.left(splitPos), splitPos > 0 ? attr.mid(splitPos + 1) : "");
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
	// correctly trim words with apostrophes or hyphens within them, phrases with dashes, initialisms/acronyms, and numeric citations
	QRegExp wordRe("^(\\W*)(\\w?|\\w(\\w|[-\\x2013\\x2014'’])*\\w|(\\w+\\.){2,})([\\W\\x00b2\\x00b3\\x00b9\\x2070-\\x207e]*)$");
	if(wordRe.indexIn(word) != -1) {
		if(prefix) {
			*prefix = wordRe.cap(1);
		}
		if(suffix) {
			*suffix = wordRe.cap(5);
		}
		return wordRe.cap(2);
	}
	return word;
}

HOCRItem::HOCRItem(const QDomElement& element, HOCRPage* page, HOCRItem* parent, int index)
	: m_pageItem(page), m_parentItem(parent), m_index(index) {
	// Read attrs
	QDomNamedNodeMap attributes = element.attributes();
	for(int i = 0, n = attributes.size(); i < n; ++i) {
		QString attrName = attributes.item(i).nodeName();
		if(attrName == "title") {
			m_titleAttrs = deserializeAttrGroup(attributes.item(i).nodeValue());
		} else {
			m_attrs[attrName] = attributes.item(i).nodeValue();
		}
	}
	// Map ocr_header/ocr_caption to ocr_line
	if(m_attrs["class"] == "ocr_header" || m_attrs["class"] == "ocr_caption") {
		m_attrs["class"] = "ocr_line";
	}
	// Adjust item id based on pageId
	if(parent) {
		QString idClass = itemClass().mid(itemClass().indexOf("_") + 1);
		int counter = page->m_idCounters.value(idClass, 0) + 1;
		page->m_idCounters[idClass] = counter;
		QString newId = QString("%1_%2_%3").arg(idClass).arg(page->pageId()).arg(counter);
		m_attrs["id"] = newId;
	}

	// Parse item bbox
	QStringList bbox = m_titleAttrs["bbox"].split(QRegExp("\\s+"));
	if(bbox.size() == 4) {
		m_bbox.setCoords(bbox[0].toInt(), bbox[1].toInt(), bbox[2].toInt(), bbox[3].toInt());
	}

	if(itemClass() == "ocrx_word") {
		m_text = element.text();
		m_bold = !element.elementsByTagName("strong").isEmpty();
		m_italic = !element.elementsByTagName("em").isEmpty();
	} else if(itemClass() == "ocr_line") {
		// Depending on the locale, tesseract can use a comma instead of a dot as decimal separator in the baseline...
		m_titleAttrs["baseline"] = m_titleAttrs["baseline"].replace(",", ".");
	}
}

HOCRItem::~HOCRItem() {
	qDeleteAll(m_childItems);
}

void HOCRItem::addChild(HOCRItem* child) {
	m_childItems.append(child);
	child->m_parentItem = this;
	child->m_pageItem = m_pageItem;
	child->m_index = m_childItems.size() - 1;
}

void HOCRItem::insertChild(HOCRItem* child, int i) {
	m_childItems.insert(i, child);
	child->m_parentItem = this;
	child->m_pageItem = m_pageItem;
	child->m_index = i++;
	for(int n = m_childItems.size(); i < n; ++i) {
		m_childItems[i]->m_index = i;
	}
}

void HOCRItem::removeChild(HOCRItem* child) {
	takeChild(child);
	delete child;
}

void HOCRItem::takeChild(HOCRItem* child) {
	if(this != child->parent()) {
		return;
	}
	int i = child->index();
	m_childItems.remove(i);
	for(int n = m_childItems.size(); i < n; ++i) {
		m_childItems[i]->m_index = i;
	}
}

QVector<HOCRItem*> HOCRItem::takeChildren() {
	QVector<HOCRItem*> children(m_childItems);
	m_childItems.clear();
	return children;
}

QMap<QString, QString> HOCRItem::getAllAttributes() const {
	QMap<QString, QString> attrValues;
	for(auto it = m_attrs.begin(), itEnd = m_attrs.end(); it != itEnd; ++it) {
		attrValues.insert(it.key(), it.value());
	}
	for(auto it = m_titleAttrs.begin(), itEnd = m_titleAttrs.end(); it != itEnd; ++it) {
		attrValues.insert(QString("title:%1").arg(it.key()), it.value());
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

QMap<QString, QString> HOCRItem::getAttributes(const QList<QString>& names = QList<QString>()) const {
	QMap<QString, QString> attrValues;
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
			attrValues.insert(attrName, m_attrs[attrName]);
		}
	}
	return attrValues;
}

// ocr_class:attr_key:attr_values
void HOCRItem::getPropagatableAttributes(QMap<QString, QMap<QString, QSet<QString>>>& occurrences) const {
	static QMap<QString, QStringList> s_propagatableAttributes = {
		{"ocr_line", {"title:baseline"}},
		{"ocrx_word", {"lang", "title:x_fsize", "title:x_font", "bold", "italic"}}
	};

	QString childClass = m_childItems.isEmpty() ? "" : m_childItems.front()->itemClass();
	auto it = s_propagatableAttributes.find(childClass);
	if(it != s_propagatableAttributes.end()) {
		for(HOCRItem* child : m_childItems) {
			QMap<QString, QString> attrs = child->getAttributes(it.value());
			for(auto attrIt = attrs.begin(), attrItEnd = attrs.end(); attrIt != attrItEnd; ++attrIt) {
				occurrences[childClass][attrIt.key()].insert(attrIt.value());
			}
		}
	}
	if(childClass != "ocrx_word") {
		for(HOCRItem* child : m_childItems) {
			child->getPropagatableAttributes(occurrences);
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
	if(name == "bold") {
		m_bold = value == "1";
	} else if(name == "italic") {
		m_italic = value == "1";
	} else if(parts.size() < 2) {
		m_attrs[name] = value;
	} else {
		Q_ASSERT(parts[0] == "title");
		m_titleAttrs[parts[1]] = value;
		if(name == "title:bbox") {
			QStringList bbox = value.split(QRegExp("\\s+"));
			Q_ASSERT(bbox.size() == 4);
			m_bbox.setCoords(bbox[0].toInt(), bbox[1].toInt(), bbox[2].toInt(), bbox[3].toInt());
		}
	}
}

QString HOCRItem::toHtml(int indent) const {
	QString cls = itemClass();
	QString tag;
	if(cls == "ocr_page" || cls == "ocr_carea" || cls == "ocr_graphic") {
		tag = "div";
	} else if(cls == "ocr_par") {
		tag = "p";
	} else {
		tag = "span";
	}
	QString html = QString(indent, ' ') + "<" + tag;
	html += QString(" title=\"%1\"").arg(serializeAttrGroup(m_titleAttrs));
	for(auto it = m_attrs.begin(), itEnd = m_attrs.end(); it != itEnd; ++it) {
		html += QString(" %1=\"%2\"").arg(it.key(), it.value());
	}
	html += ">";
	if(cls == "ocrx_word") {
		if(m_bold) {
			html += "<strong>";
		}
		if(m_italic) {
			html += "<em>";
		}
		html += m_text.toHtmlEscaped();
		if(m_italic) {
			html += "</em>";
		}
		if(m_bold) {
			html += "</strong>";
		}
	} else {
		html += "\n";
		for(const HOCRItem* child : m_childItems) {
			html += child->toHtml(indent + 1);
		}
		html += QString(indent, ' ');
	}
	html += "</" + tag + ">\n";
	return html;
}

QPair<double, double> HOCRItem::baseLine() const {
	static const QRegExp baseLineRx = QRegExp("([+-]?\\d+\\.?\\d*)\\s+([+-]?\\d+\\.?\\d*)");
	if(baseLineRx.indexIn(m_titleAttrs["baseline"]) != -1) {
		return qMakePair(baseLineRx.cap(1).toDouble(), baseLineRx.cap(2).toDouble());
	}
	return qMakePair(0.0, 0.0);
}

bool HOCRItem::parseChildren(const QDomElement& element, QString language, const QString& defaultLanguage) {
	// Determine item language (inherit from parent if not specified)
	QString elemLang = element.attribute("lang");
	if(!elemLang.isEmpty()) {
		auto it = s_langCache.find(elemLang);
		if(it == s_langCache.end()) {
			it = s_langCache.insert(elemLang, Utils::getSpellingLanguage(elemLang, defaultLanguage));
		}
		m_attrs.remove("lang");
		language = it.value();
	}

	if(itemClass() == "ocrx_word") {
		m_attrs["lang"] = language;
		return !m_text.isEmpty();
	}
	bool haveWords = false;
	QDomElement childElement = element.firstChildElement();
	while(!childElement.isNull()) {
		m_childItems.append(new HOCRItem(childElement, m_pageItem, this, m_childItems.size()));
		haveWords |= m_childItems.last()->parseChildren(childElement, language, defaultLanguage);
		childElement = childElement.nextSiblingElement();
	}
	return haveWords;
}

///////////////////////////////////////////////////////////////////////////////

HOCRPage::HOCRPage(const QDomElement& element, int pageId, const QString& defaultLanguage, bool cleanGraphics, int index)
	: HOCRItem(element, this, nullptr, index), m_pageId(pageId) {
	m_attrs["id"] = QString("page_%1").arg(pageId);

	m_sourceFile = m_titleAttrs["image"].replace(QRegExp("^['\"]"), "").replace(QRegExp("['\"]$"), "");
	m_pageNr = m_titleAttrs["ppageno"].toInt();
	// Code to handle pageno -> ppageno typo in previous versions of gImageReader
	if(m_pageNr == 0) {
		m_pageNr = m_titleAttrs["pageno"].toInt();
		m_titleAttrs["ppageno"] = m_titleAttrs["pageno"];
		m_titleAttrs.remove("pageno");
	}
	m_angle = m_titleAttrs["rot"].toDouble();
	m_resolution = m_titleAttrs.value("res", "100").toInt();

	QDomElement childElement = element.firstChildElement("div");
	while(!childElement.isNull()) {
		HOCRItem* item = new HOCRItem(childElement, this, this, m_childItems.size());
		m_childItems.append(item);
		if(!item->parseChildren(childElement, defaultLanguage, defaultLanguage)) {
			// No word children -> treat as graphic
			if(cleanGraphics && (item->bbox().width() < 10 || item->bbox().height() < 10)) {
				// Ignore graphics which are less than 10 x 10
				delete m_childItems.takeLast();
			} else {
				item->setAttribute("class", "ocr_graphic");
				qDeleteAll(item->m_childItems);
				item->m_childItems.clear();
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
		m_sourceFile = QString("./%1").arg(QDir(basepath).relativeFilePath(m_sourceFile));
	}
	m_titleAttrs["image"] = QString("'%1'").arg(m_sourceFile);
}
