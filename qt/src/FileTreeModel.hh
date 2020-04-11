/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FileTreeModel.hh
 * Copyright (C) 2020 Sandro Mani <manisandro@gmail.com>
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

#ifndef FILETREEMODEL_HH
#define FILETREEMODEL_HH

#include <QAbstractItemModel>
#include <QFileIconProvider>
#include "common.hh"

class FileTreeModel : public QAbstractItemModel {
public:
	FileTreeModel(QObject* parent = nullptr);

	QModelIndex insertFile(QString filePath, DataObject* data, const QString& displayName = QString());
	QModelIndex findFile(const QString& filePath, bool isFile = true) const;
	bool removeIndex(const QModelIndex& index);
	void clear();
	bool isDir(const QModelIndex& index) const;
	void setFileEditable(const QModelIndex& index, bool editable);
	bool isFileEditable(const QModelIndex& index) const;

	template<class T>
	T fileData(const QModelIndex& index) const { return static_cast<T>(fileData(index)); }
	DataObject* fileData(const QModelIndex& index) const;

	QVariant data(const QModelIndex& index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex& child) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent = QModelIndex()) const override;

	static constexpr int OutputFileRole = Qt::UserRole;

private:
	template<class T>
	class NodeList : public std::vector<T> {
	public:
		typedef typename std::vector<T>::iterator iterator;
		typedef typename std::vector<T>::const_iterator const_iterator;

		T add(T node);
		T add(T node, int pos);
		T take(T node);
		const_iterator find(const QString& fileName) const;
		int insIndex(const QString& fileName) const;
		int index(T node) const;
		int index(const_iterator it) const;
	};

	struct DirNode;
	struct Node {
		Node(const QString& _fileName, const QString& _path, DirNode* _parent, const QString& _displayName = QString())
			: fileName(_fileName), path(_path), displayName(_displayName), parent(_parent) {}
		virtual ~Node() = default;
		QString fileName;
		QString path;
		QString displayName;
		DirNode* parent = nullptr;
	};
	struct FileNode : Node {
		FileNode(const QString& _fileName, const QString& _path, DirNode* _parent, DataObject* _data, const QString& _displayName)
			: Node(_fileName, _path, _parent, _displayName), data(_data) {}
		~FileNode() { delete data; }
		bool editable = false;
		DataObject* data = nullptr;
	};
	struct DirNode : Node {
		using Node::Node;
		~DirNode() {
			qDeleteAll(dirs);
			qDeleteAll(files);
		}
		NodeList<DirNode*> dirs;
		NodeList<FileNode*> files;
		int childCount() const { return dirs.size() + files.size(); }
	};

	DirNode* m_root = nullptr;
	DirNode* m_tmpdir = nullptr;
	QFileIconProvider m_iconProvider;
};

#endif // FILETREEMODEL_HH
