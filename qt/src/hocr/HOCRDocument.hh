/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRDocument.hh
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

#ifndef HOCRDOCUMENT_HH
#define HOCRDOCUMENT_HH

#include <QAbstractItemModel>
#include <QDomDocument>
#include <QRect>

namespace QtSpell { class TextEditChecker; }
class HOCRItem;
class HOCRPage;

class HOCRDocument : public QAbstractItemModel {
	Q_OBJECT

public:
	HOCRDocument(QtSpell::TextEditChecker* spell, QObject *parent = 0);
	~HOCRDocument();

	void clear();

	void setDefaultLanguage(const QString& language) { m_defaultLanguage = language; }
	void recheckSpelling();

	QDomDocument& getDomDocument() { return m_document; }
	QString toHTML(int indent = 1) const { return m_document.toString(indent); }

	QModelIndex addPage(const QDomElement& pageElement, bool cleanGraphics);
	const HOCRPage* page(int i) const{ return m_pages.value(i); }
	int pageCount() const { return m_pages.size(); }

	const HOCRItem* itemAtIndex(const QModelIndex& index) const{ return index.isValid() ? static_cast<HOCRItem*>(index.internalPointer()) : nullptr; }
	bool editItemAttribute(const QModelIndex& index, const QString& name, const QString& value, const QString& attrItemClass = QString());
	QModelIndex mergeItems(const QModelIndex& parent, int startRow, int endRow);
	QModelIndex addItem(const QModelIndex& parent, const QDomElement& element);
	bool removeItem(const QModelIndex& index);

	QModelIndex nextIndex(const QModelIndex& current);
	QModelIndex prevIndex(const QModelIndex& current);

	bool referencesSource(const QString& filename) const;
	QModelIndex searchPage(const QString& filename, int pageNr) const;
	QModelIndex searchAtCanvasPos(const QModelIndex& pageIndex, const QPoint& pos) const;

	QVariant data(const QModelIndex &index, int role) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role) override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &child) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

signals:
	void itemAttributeChanged(const QModelIndex& itemIndex, const QString& name, const QString& value);

private:
	int m_pageIdCounter = 0;
	QDomDocument m_document;
	QString m_defaultLanguage = "en_US";
	QtSpell::TextEditChecker* m_spell;

	QVector<HOCRPage*> m_pages;

	QString displayRoleForItem(const HOCRItem* item) const;
	QIcon decorationRoleForItem(const HOCRItem* item) const;

	bool checkItemSpelling(const HOCRItem* item) const;
	void deleteItem(HOCRItem* item);
	void recursiveDataChanged(const QModelIndex& parent, const QVector<int>& roles, const QStringList& itemClasses = QStringList());
	HOCRItem* mutableItemAtIndex(const QModelIndex& index) const{ return index.isValid() ? static_cast<HOCRItem*>(index.internalPointer()) : nullptr; }
};


class HOCRItem
{
public:
	// attrname : attrvalue : occurences
	typedef QMap<QString, QMap<QString, int>> AttrOccurenceMap_t;

	HOCRItem( QDomElement element, HOCRPage* page, HOCRItem *parent = nullptr);
	virtual ~HOCRItem();
	HOCRPage* page() const{ return m_pageItem; }
	const QVector<HOCRItem*>& children() const{ return m_childItems; }
	HOCRItem* parent() const{ return m_parentItem; }
	const QDomElement& element() const{ return m_domElement; }
	bool isEnabled() const{ return m_enabled; }

	// HOCR specific convenience getters
	QString itemClass() const{ return m_domElement.attribute("class"); }
	const QRect& bbox() const{ return m_bbox; }
	QString text() const{ return m_domElement.text(); }
	QString lang() const{ return m_domElement.attribute("lang"); }
	const QMap<QString, QString> getTitleAttributes() const { return m_titleAttrs; }
	QMap<QString,QString> getAllAttributes() const;
	QMap<QString,QString> getAttributes(const QList<QString>& names) const;
	void getPropagatableAttributes(QMap<QString, QMap<QString, QSet<QString> > >& occurences) const;
	QString toHtml(int indent = 1) const;
	int baseLine() const;
	QString fontFamily() const{ return m_titleAttrs["x_font"]; }
	double fontSize() const{ return m_titleAttrs["x_fsize"].toDouble(); }

	void addChild(HOCRItem* child);
	void removeChild(HOCRItem* child);
	QVector<HOCRItem*> takeChildren();
	void setEnabled(bool enabled) { m_enabled = enabled; }
	void setText(const QString& newText);
	void setAttribute(const QString& name, const QString& value, const QString& attrItemClass = QString());

	static QMap<QString, QString> deserializeAttrGroup(const QString& string);
	static QString serializeAttrGroup(const QMap<QString, QString>& attrs);
	static QString trimmedWord(const QString& word, QString* prefix = nullptr, QString* suffix = nullptr);

protected:
	friend class HOCRPage;

	static QMap<QString,QString> s_langCache;

	QDomElement m_domElement;
	QMap<QString, QString> m_titleAttrs;
	QVector<HOCRItem*> m_childItems;
	HOCRPage* m_pageItem = nullptr;
	HOCRItem* m_parentItem = nullptr;
	bool m_enabled = true;

	QRect m_bbox;

	bool parseChildren(QString language);
};


class HOCRPage : public HOCRItem {
public:
	HOCRPage(QDomElement element, int pageId, const QString& language, bool cleanGraphics);

	const QString& sourceFile() const{ return m_sourceFile; }
	// const-refs here to avoid taking reference from temporaries
	const int& pageNr() const{ return m_pageNr; }
	const double& angle() const{ return m_angle; }
	const int& resolution() const{ return m_resolution; }
	int pageId() const{ return m_pageId; }
	QString title() const;

private:
	friend class HOCRItem;

	int m_pageId;
	QMap<QString, int> m_idCounters;
	QString m_sourceFile;
	int m_pageNr;
	double m_angle;
	int m_resolution;
};


#endif // HOCRDOCUMENT_HH
