/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.cc
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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
#include <QIcon>
#include <QSet>
#include <QTextStream>
#include <QtSpell.hpp>
#include <cmath>

#include "common.hh"
#include "HOCRDocument.hh"
#include "Utils.hh"


HOCRDocument::HOCRDocument(QtSpell::TextEditChecker* spell, QObject* parent)
	: QAbstractItemModel(parent), m_spell(spell) {
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

void HOCRDocument::recheckSpelling() {
	recursiveDataChanged(QModelIndex(), {Qt::DisplayRole}, {"ocrx_word"});
}

QString HOCRDocument::toHTML() const {
	QString html = "<body>\n";
	for(const HOCRPage* page : m_pages) {
		html += page->toHtml(1);
	}
	html += "</body>\n";
	return html;
}

QModelIndex HOCRDocument::addPage(const QDomElement& pageElement, bool cleanGraphics) {
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

QModelIndex HOCRDocument::splitItem(const QModelIndex& index, int startRow, int endRow) {
	if(endRow - startRow < 0) {
		return QModelIndex();
	}
	HOCRItem* item = mutableItemAtIndex(index);
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
	beginInsertRows(index.parent(), item->index() + 1, item->index() + 1);
	insertItem(item->parent(), newItem, item->index() + 1);
	endInsertRows();
	QModelIndex newIndex = index.sibling(index.row() + 1, 0);
	for(int row = 0; row <= (endRow - startRow); ++row) {
		QModelIndex childIndex = index.child(startRow, 0);
		HOCRItem* child = mutableItemAtIndex(childIndex);
		Q_ASSERT(child);
		moveItem(childIndex, newIndex, row);
	}

	return newIndex;
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
	return !checkItemSpelling(index);
}

bool HOCRDocument::checkItemSpelling(const QModelIndex& index, QStringList* suggestions, int limit) const {
	const HOCRItem* item = itemAtIndex(index);
	if(item->itemClass() != "ocrx_word") { return true; }

	QString prefix, suffix, trimmed = HOCRItem::trimmedWord(item->text(), &prefix, &suffix);
	if(trimmed.isEmpty()) { return true; }
	QString lang = item->spellingLang();
	if(m_spell->getLanguage() != lang && !(m_spell->setLanguage(lang))) { return true; }

	// check word, including (if requested) setting suggestions; handle hyphenated phrases correctly
	if(checkSpelling(trimmed, suggestions, limit)) { return true; }

	// handle some hyphenated words
	// don't bother with words not broken over sibling text lines (ie interrupted by other blocks), it's human hard
	// don't bother with words broken over three or more lines, it's implausible and this treatment is ^ necessarily incomplete
	HOCRItem* parent = item->parent();
	if(!parent) { return false; }
	HOCRItem* grandparent = parent->parent();
	if(!grandparent) { return false; }
	int idx = item->index();
	int parentIdx = parent->index();
	QVector<HOCRItem*> parentSiblings = grandparent->children();
	if(idx == 0 && parentIdx > 0) {
		HOCRItem* parentPrevSibling = parentSiblings.at(parentIdx - 1);
		if(!parentPrevSibling) { return false; }
		QVector<HOCRItem*> cousins = parentPrevSibling->children();
		if(cousins.size() < 1) { return false; }
		HOCRItem* prevCousin = cousins.back();
		if(!prevCousin || prevCousin->itemClass() != "ocrx_word") { return false; }
		QString prevText = prevCousin->text();
		if(prevText.isEmpty() || prevText[prevText.size() - 1] != '-') { return false; }

		// don't bother with (reassembled) suggestions for broken words since we can't re-break them
		return checkSpelling(HOCRItem::trimmedWord(prevText) + trimmed);
	}
	QString text = item->text();
	if(idx + 1 == parent->children().size() && parentIdx + 1 < parentSiblings.size() && !text.isEmpty() && text[text.size() - 1] == '-') {
		HOCRItem* parentNextSibling = parentSiblings.at(parentIdx + 1);
		if(!parentNextSibling) { return false; }
		QVector<HOCRItem*> cousins = parentNextSibling->children();
		if(cousins.size() < 1) { return false; }
		HOCRItem* nextCousin = cousins.front();
		if(!nextCousin || nextCousin->itemClass() != "ocrx_word") { return false; }

		return checkSpelling(trimmed + HOCRItem::trimmedWord(nextCousin->text()));
	}
	return false;
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
				return checkItemSpelling(index) ? QVariant() : QVariant(QColor(Qt::red));
			} else {
				return checkItemSpelling(index) ? QVariant(QColor(Qt::gray)) : QVariant(QColor(208, 80, 82));
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

bool HOCRDocument::checkSpelling(const QString& trimmed, QStringList* suggestions, int limit) const {
	QVector<QStringRef> words = trimmed.splitRef(QRegExp("[\\x2013\\x2014]+"));
	int perWordLimit;
	// s = p^w => w = log_p(c) = log(c)/log(p) => p = 10^(log(c)/w)
	if(limit > 0) { perWordLimit = int(std::pow(10, std::log10(limit) / words.size())); }
	QList<QList<QString>> wordSuggestions;
	bool valid = true;
	bool multipleWords = words.size() > 1;
	for(const QStringRef& word : words) {
		QString wordString = word.toString();
		bool wordValid = m_spell->checkWord(wordString);
		valid &= wordValid;
		if(suggestions) {
			QList<QString> ws = m_spell->getSpellingSuggestions(wordString);
			if(wordValid && multipleWords) { ws.prepend(wordString); }
			if(limit == -1) {
				wordSuggestions.append(ws);
			} else if(limit > 0) {
				wordSuggestions.append(ws.mid(0, perWordLimit));
			}
		}
	}
	if(suggestions) {
		suggestions->clear();
		QList<QList<QString>> suggestionCombinations;
		generateCombinations(wordSuggestions, suggestionCombinations, 0, QList<QString>());
		for(const QList<QString>& combination : suggestionCombinations) {
			QString s = "";
			const QStringRef* originalWord = words.begin();
			int last = 0;
			int next;
			for(const QString& suggestedWord : combination) {
				next = originalWord->position();
				s.append(trimmed.midRef(last, next - last));
				s.append(suggestedWord);
				last = next + originalWord->length();
				originalWord++;
			}
			s.append(trimmed.midRef(last));
			suggestions->append(s);
		}
	}
	return valid;
}

// each suggestion for each word => each word in each suggestion
void HOCRDocument::generateCombinations(const QList<QList<QString>>& lists, QList<QList<QString>>& results, int depth, const QList<QString> c) const {
	if(depth == lists.size()) {
		results.append(c);
		return;
	}
	for(int i = 0; i < lists[depth].size(); ++i) {
		generateCombinations(lists, results, depth + 1, c + QList<QString>({lists[depth][i]}));
	}
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
		html += m_text.toHtmlEscaped();
#else
		html += Qt::escape(m_text);
#endif
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

bool HOCRItem::parseChildren(const QDomElement& element, QString language) {
	// Determine item language (inherit from parent if not specified)
	QString elemLang = element.attribute("lang");
	if(!elemLang.isEmpty()) {
		auto it = s_langCache.find(elemLang);
		if(it == s_langCache.end()) {
			it = s_langCache.insert(elemLang, Utils::getSpellingLanguage(elemLang));
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
		haveWords |= m_childItems.last()->parseChildren(childElement, language);
		childElement = childElement.nextSiblingElement();
	}
	return haveWords;
}

///////////////////////////////////////////////////////////////////////////////

HOCRPage::HOCRPage(const QDomElement& element, int pageId, const QString& language, bool cleanGraphics, int index)
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
	m_resolution = m_titleAttrs["res"].toInt();

	QDomElement childElement = element.firstChildElement("div");
	while(!childElement.isNull()) {
		HOCRItem* item = new HOCRItem(childElement, this, this, m_childItems.size());
		m_childItems.append(item);
		if(!item->parseChildren(childElement, language)) {
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
		m_sourceFile = QString(".%1%2").arg("/").arg(QDir(basepath).relativeFilePath(m_sourceFile));
	}
	m_titleAttrs["image"] = QString("'%1'").arg(m_sourceFile);
}
