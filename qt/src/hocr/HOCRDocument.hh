/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.hh
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

#ifndef HOCRDOCUMENT_HH
#define HOCRDOCUMENT_HH

#include "Config.hh"
#include <QAbstractItemModel>
#include <QRect>

class QDomElement;
class HOCRItem;
class HOCRPage;
class HOCRSpellChecker;

class HOCRDocument : public QAbstractItemModel {
	Q_OBJECT

public:
	HOCRDocument(QObject* parent = nullptr);
	~HOCRDocument();

	void clear();

	void setDefaultLanguage(const QString& language) {
		m_defaultLanguage = language;
	}
	const QString& defaultLanguage() const {
		return m_defaultLanguage;
	}
	void addSpellingActions(QMenu* menu, const QModelIndex& index);

	QString toHTML() const;

	QModelIndex insertPage(int beforeIdx, const QDomElement& pageElement, bool cleanGraphics, const QString& sourceBasePath = QString());
	const HOCRPage* page(int i) const {
		return m_pages.value(i);
	}
	int pageCount() const {
		return m_pages.size();
	}

	const HOCRItem* itemAtIndex(const QModelIndex& index) const {
		return index.isValid() ? static_cast<HOCRItem*>(index.internalPointer()) : nullptr;
	}
	QModelIndex indexAtItem(const HOCRItem* item) const;
	bool editItemAttribute(const QModelIndex& index, const QString& name, const QString& value, const QString& attrItemClass = QString());
	QModelIndex moveItem(const QModelIndex& itemIndex, const QModelIndex& newParent, int row);
	QModelIndex swapItems(const QModelIndex& parent, int startRow, int endRow);
	QModelIndex mergeItems(const QModelIndex& parent, int startRow, int endRow);
	QModelIndex splitItem(const QModelIndex& itemIndex, int startRow, int endRow);
	QModelIndex addItem(const QModelIndex& parent, const QDomElement& element);
	bool removeItem(const QModelIndex& index);

	QModelIndex nextIndex(const QModelIndex& current) const;
	QModelIndex prevIndex(const QModelIndex& current) const;
	QModelIndex prevOrNextIndex(bool next, const QModelIndex& current, const QString& ocrClass, bool misspelled = false) const;
	bool indexIsMisspelledWord(const QModelIndex& index) const;
	bool getItemSpellingSuggestions(const QModelIndex& index, QString& trimmedWord, QStringList& suggestions, int limit) const;

	bool referencesSource(const QString& filename) const;
	QModelIndex searchPage(const QString& filename, int pageNr) const;
	QModelIndex searchAtCanvasPos(const QModelIndex& pageIndex, const QPoint& pos) const;
	void convertSourcePaths(const QString& basepath, bool absolute);

	QVariant data(const QModelIndex& index, int role) const override;
	bool setData(const QModelIndex& index, const QVariant& value, int role) override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;

signals:
	void itemAttributeChanged(const QModelIndex& itemIndex, const QString& name, const QString& value);

private:
	int m_pageIdCounter = 0;
	QString m_defaultLanguage = "en_US";
	HOCRSpellChecker* m_spell;

	QVector<HOCRPage*> m_pages;

	QString displayRoleForItem(const HOCRItem* item) const;
	QIcon decorationRoleForItem(const HOCRItem* item) const;

	void insertItem(HOCRItem* parent, HOCRItem* item, int i);
	void deleteItem(HOCRItem* item);
	void takeItem(HOCRItem* item);
	void resetMisspelled(const QModelIndex& index);
	QList<QModelIndex> recheckItemSpelling(const QModelIndex& index) const;
	void recursiveDataChanged(const QModelIndex& parent, const QVector<int>& roles, const QStringList& itemClasses = QStringList());
	void recomputeBBoxes(HOCRItem* item);
	HOCRItem* mutableItemAtIndex(const QModelIndex& index) const {
		return index.isValid() ? static_cast<HOCRItem*>(index.internalPointer()) : nullptr;
	}
};


class HOCRItem {
public:
	// attrname : attrvalue : occurrences
	typedef QMap<QString, QMap<QString, int>> AttrOccurenceMap_t;

	HOCRItem(const QDomElement& element, HOCRPage* page, HOCRItem* parent, int index = -1);
	virtual ~HOCRItem();
	HOCRPage* page() const {
		return m_pageItem;
	}
	const QVector<HOCRItem*>& children() const {
		return m_childItems;
	}
	HOCRItem* parent() const {
		return m_parentItem;
	}
	int index() const {
		return m_index;
	}
	bool isEnabled() const {
		return m_enabled;
	}

	// HOCR specific convenience getters
	QString itemClass() const {
		return m_attrs["class"];
	}
	const QRect& bbox() const {
		return m_bbox;
	}
	QString text() const {
		return m_text;
	}
	QString lang() const {
		return m_attrs["lang"];
	}
	QString spellingLang() const {
		QString l = lang();
		QString code = Config::lookupLangCode(l);
		return code.isEmpty() ? l : code;
	}
	const QMap<QString, QString> getAttributes() const {
		return m_attrs;
	}
	const QMap<QString, QString> getTitleAttributes() const {
		return m_titleAttrs;
	}
	QMap<QString, QString> getAllAttributes() const;
	QMap<QString, QString> getAttributes(const QList<QString>& names) const;
	void getPropagatableAttributes(QMap<QString, QMap<QString, QSet<QString> > >& occurrences) const;
	QString toHtml(int indent = 0) const;
	QPair<double, double> baseLine() const;
	QString fontFamily() const {
		return m_titleAttrs["x_font"];
	}
	double fontSize() const {
		return m_titleAttrs["x_fsize"].toDouble();
	}
	bool fontBold() const {
		return m_bold;
	}
	bool fontItalic() const {
		return m_italic;
	}

	static QMap<QString, QString> deserializeAttrGroup(const QString& string);
	static QString serializeAttrGroup(const QMap<QString, QString>& attrs);
	static QString trimmedWord(const QString& word, QString* prefix = nullptr, QString* suffix = nullptr);

protected:
	friend class HOCRDocument;
	friend class HOCRPage;

	static QMap<QString, QString> s_langCache;

	QString m_text;
	int m_misspelled = -1;
	bool m_bold;
	bool m_italic;

	QMap<QString, QString> m_attrs;
	QMap<QString, QString> m_titleAttrs;
	QVector<HOCRItem*> m_childItems;
	HOCRPage* m_pageItem = nullptr;
	HOCRItem* m_parentItem = nullptr;
	int m_index;
	bool m_enabled = true;

	QRect m_bbox;

	// All mutations must be done through methods of HOCRDocument
	void addChild(HOCRItem* child);
	void insertChild(HOCRItem* child, int index);
	void removeChild(HOCRItem* child);
	void takeChild(HOCRItem* child);
	QVector<HOCRItem*> takeChildren();
	void setEnabled(bool enabled) {
		m_enabled = enabled;
	}
	void setText(const QString& newText) {
		m_text = newText;
	}
	void setMisspelled(int misspelled) {
		m_misspelled = misspelled;
	}
	int isMisspelled() const {
		return m_misspelled;
	}
	void setAttribute(const QString& name, const QString& value, const QString& attrItemClass = QString());
	bool parseChildren(const QDomElement& element, QString language);
};


class HOCRPage : public HOCRItem {
public:
	HOCRPage(const QDomElement& element, int pageId, const QString& language, bool cleanGraphics, int index);

	const QString& sourceFile() const {
		return m_sourceFile;
	}
	// const-refs here to avoid taking reference from temporaries
	const int& pageNr() const {
		return m_pageNr;
	}
	const double& angle() const {
		return m_angle;
	}
	const int& resolution() const {
		return m_resolution;
	}
	int pageId() const {
		return m_pageId;
	}
	QString title() const;

private:
	friend class HOCRItem;
	friend class HOCRDocument;

	int m_pageId;
	QMap<QString, int> m_idCounters;
	QString m_sourceFile;
	int m_pageNr;
	double m_angle;
	int m_resolution;

	void convertSourcePath(const QString& basepath, bool absolute);
};


#endif // HOCRDOCUMENT_HH
