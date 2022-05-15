/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FileTreeModel.cc
 * Copyright (C) 2022 Sandro Mani <manisandro@gmail.com>
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

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QIcon>

#include "FileTreeModel.hh"

FileTreeModel::FileTreeModel(QObject* parent)
	: QAbstractItemModel(parent) {

}

QModelIndex FileTreeModel::insertFile(QString filePath, DataObject* data, const QString& displayName) {
	QFileInfo finfo(filePath);
#ifdef Q_OS_WIN32
	QString fileDir = "/" + finfo.absolutePath().replace("\\", "/");
	filePath = "/" + filePath;
	QString tempPath = "/" + QDir::tempPath();
#else
	QString fileDir = finfo.absolutePath();
	QString tempPath = QDir::tempPath();
#endif
	QString fileName = finfo.fileName();

	if(fileDir.startsWith(tempPath)) {
		int rootRow = (m_root ? 1 : 0);
		if(!m_tmpdir) {
			beginInsertRows(QModelIndex(), rootRow, rootRow);
			m_tmpdir = new DirNode(QFileInfo(tempPath).fileName(), tempPath, nullptr, _("Temporary files"));
			endInsertRows();
		}
		QModelIndex idx = index(rootRow, 0);
		int pos = m_tmpdir->files.insIndex(fileName);
		int row = m_tmpdir->dirs.size() + pos;
		beginInsertRows(idx, row, row);
		m_tmpdir->files.add(new FileNode(fileName, filePath, m_tmpdir, data, displayName), pos);
		endInsertRows();
		return index(row, 0, idx);
	} else if(!m_root) {
		// Set initial root
		m_root = new DirNode(QFileInfo(fileDir).fileName(), fileDir, nullptr);
		beginInsertRows(QModelIndex(), 0, 0);
		m_root->files.add(new FileNode(fileName, filePath, m_root, data, displayName));
		endInsertRows();
		return index(0, 0, index(0, 0));
	} else if(m_root->path == fileDir) {
		// Add to current root
		int pos = m_root->files.insIndex(fileName);
		int row = m_root->dirs.size() + pos;
		beginInsertRows(index(0, 0), row, row);
		m_root->files.add(new FileNode(fileName, filePath, m_root, data, displayName), pos);
		endInsertRows();
		return index(row, 0, index(0, 0));
	} else if(m_root->path.startsWith(fileDir)) {
		// Root below new path, replace root
		QStringList path = m_root->path.mid(fileDir.length()).split("/", QString::SplitBehavior::SkipEmptyParts);
		beginRemoveRows(QModelIndex(), 0, 0);
		DirNode* oldroot = m_root;
		m_root = nullptr;
		endRemoveRows();
		beginInsertRows(QModelIndex(), 0, 0);
		m_root = new DirNode(QFileInfo(fileDir).fileName(), fileDir, nullptr);
		DirNode* cur = m_root;
		for(int i = 0, n = path.length() - 1; i < n; ++i) {
			cur = cur->dirs.add(new DirNode(path[i], cur->path + "/" + path[i], cur));
		}
		cur->dirs.add(oldroot);
		oldroot->parent = cur;
		m_root->files.add(new FileNode(fileName, filePath, m_root, data, displayName));
		endInsertRows();
		return index(1, 0, index(0, 0));
	} else if(fileDir.startsWith(m_root->path)) {
		// New path below root, append to subtree
		QStringList path = fileDir.mid(m_root->path.length()).split("/", QString::SplitBehavior::SkipEmptyParts);
		DirNode* cur = m_root;
		QModelIndex idx = index(0, 0);
		for(const QString& part : path) {
			auto it = cur->dirs.find(part);
			int row = 0;
			if(it == cur->dirs.end()) {
				row = cur->dirs.insIndex(part);
				beginInsertRows(idx, row, row);
				cur = cur->dirs.add(new DirNode(part, cur->path + "/" + part, cur), row);
				endInsertRows();
			} else {
				row = cur->dirs.index(it);
				cur = *it;
			}
			idx = index(row, 0, idx);
		}
		int pos = cur->files.insIndex(fileName);
		int row = cur->dirs.size() + pos;
		beginInsertRows(idx, row, row);
		cur->files.add(new FileNode(fileName, filePath, cur, data, displayName), pos);
		endInsertRows();
		return index(row, 0, idx);
	} else {
		// Unrelated trees, find common ancestor
		QStringList rootPath = m_root->path.split("/", QString::SplitBehavior::SkipEmptyParts);
		QStringList newPath = fileDir.split("/", QString::SplitBehavior::SkipEmptyParts);
		int pos = 0;
		for(int n = qMin(rootPath.length(), newPath.length()); pos < n; ++pos) {
			if(rootPath[pos] != newPath[pos]) {
				break;
			}
		}
		QString newRoot = "/" + rootPath.mid(0, pos).join("/");
		beginRemoveRows(QModelIndex(), 0, 0);
		DirNode* oldroot = m_root;
		m_root = nullptr;
		endRemoveRows();
		m_root = new DirNode(QFileInfo(newRoot).fileName(), newRoot, nullptr);
		// Insert old root and new branch
		beginInsertRows(QModelIndex(), 0, 0);
		// - Old root
		DirNode* cur = m_root;
		QStringList path = rootPath.mid(pos);
		for(int i = 0, n = path.length() - 1; i < n; ++i) {
			cur = cur->dirs.add(new DirNode(path[i], cur->path + "/" + path[i], cur));
		}
		cur->dirs.add(oldroot);
		oldroot->parent = cur;
		// - New branch
		cur = m_root;
		path = newPath.mid(pos);
		QModelIndex idx = index(0, 0);
		for(int i = 0, n = path.length(); i < n; ++i) {
			int pos = cur->dirs.insIndex(path[i]);
			cur = cur->dirs.add(new DirNode(path[i], cur->path + "/" + path[i], cur), pos);
			idx = index(pos, 0, idx);
		}
		cur->files.add(new FileNode(fileName, filePath, cur, data, displayName));
		endInsertRows();
		return index(0, 0, idx);
	}
}

QModelIndex FileTreeModel::findFile(const QString& filePath, bool isFile) const {
	if(!m_root && !m_tmpdir) {
		return QModelIndex();
	}

	QFileInfo finfo(filePath);
#ifdef Q_OS_WIN32
	QString prefix = "/";
	QString fileDir = "/" + finfo.absolutePath().replace("\\", "/");
	QString tempPath = "/" + QDir::tempPath();
#else
	QString prefix = "";
	QString fileDir = finfo.absolutePath();
	QString tempPath = QDir::tempPath();
#endif
	QString fileName = finfo.fileName();

	// Path is root dir
	if(!isFile && m_root && prefix + filePath == m_root->path) {
		return index(0, 0);
	}
	// Path is temp dir
	else if(!isFile && m_tmpdir && prefix + filePath == m_tmpdir->path) {
		return index(m_root ? 1 : 0, 0);
	}
	// Path is below root or tempdir
	DirNode* cur = nullptr;
	QModelIndex idx;
	if(m_tmpdir && fileDir.startsWith(m_tmpdir->path)) {
		cur = m_tmpdir;
		idx = index(m_root ? 1 : 0, 0);
	} else if (m_root && fileDir.startsWith(m_root->path)) {
		cur = m_root;
		idx = index(0, 0);
	}
	if(!idx.isValid()) {
		return QModelIndex();
	}
	QString relPath = fileDir.mid(m_root->path.length());
	QStringList parts = relPath.split("/", QString::SplitBehavior::SkipEmptyParts);
	for(const QString& part : parts) {
		auto it = cur->dirs.find(part);
		if(it == cur->dirs.end()) {
			return QModelIndex();
		}
		idx = index(cur->dirs.index(*it), 0, idx);
		cur = *it;
	}
	if(isFile) {
		auto it = cur->files.find(fileName);
		return it != cur->files.end() ? index(cur->dirs.size() + cur->files.index(*it), 0, idx) : QModelIndex();
	} else {
		auto it = cur->dirs.find(fileName);
		return it != cur->dirs.end() ? index(cur->dirs.index(*it), 0, idx) : QModelIndex();
	}
}

bool FileTreeModel::removeIndex(const QModelIndex& index) {
	if(!index.isValid()) {
		return false;
	}
	Node* node = static_cast<Node*>(index.internalPointer());
	// Remove entire branch
	bool isFile = dynamic_cast<FileNode*>(node);
	Node* deleteNode = node;
	QModelIndex deleteIndex = index;
	while(deleteNode->parent && deleteNode->parent->childCount() == 1) {
		isFile = false;
		deleteNode = deleteNode->parent;
		deleteIndex = deleteIndex.parent();
	}
	if(deleteNode == m_root) {
		beginRemoveRows(QModelIndex(), 0, 0);
		delete m_root;
		m_root = nullptr;
		endRemoveRows();
	} else if(deleteNode == m_tmpdir) {
		int row = (m_root ? 1 : 0);
		beginRemoveRows(QModelIndex(), row, row);
		delete m_tmpdir;
		m_tmpdir = nullptr;
		endRemoveRows();
	} else {
		beginRemoveRows(deleteIndex.parent(), deleteIndex.row(), deleteIndex.row());
		if(isFile) {
			delete deleteNode->parent->files.take(static_cast<FileNode*>(deleteNode));
		} else {
			delete deleteNode->parent->dirs.take(static_cast<DirNode*>(deleteNode));
		}
		endRemoveRows();
	}
	return true;
}

void FileTreeModel::clear() {
	if(m_root || m_tmpdir) {
		beginRemoveRows(QModelIndex(), 0, (m_root != 0) + (m_tmpdir != 0));
		delete m_root;
		m_root = nullptr;
		delete m_tmpdir;
		m_tmpdir = nullptr;
		endRemoveRows();
	}
}

bool FileTreeModel::isDir(const QModelIndex& index) const {
	if(!index.isValid()) {
		return false;
	}
	return dynamic_cast<DirNode*>(static_cast<Node*>(index.internalPointer())) != nullptr;
}

void FileTreeModel::setFileEditable(const QModelIndex& index, bool editable) {
	Node* node = static_cast<Node*>(index.internalPointer());
	if(dynamic_cast<FileNode*>(node)) {
		static_cast<FileNode*>(node)->editable = editable;
		emit dataChanged(index.sibling(index.row(), 1), index.sibling(index.row(), 1), {Qt::DisplayRole});
	}
}

bool FileTreeModel::isFileEditable(const QModelIndex& index) const {
	Node* node = static_cast<Node*>(index.internalPointer());
	return dynamic_cast<FileNode*>(node) && static_cast<FileNode*>(node)->editable;
}

QVariant FileTreeModel::data(const QModelIndex& index, int role) const {
	Node* node = static_cast<Node*>(index.internalPointer());
	if(index.column() == 0) {
		if(role == Qt::FontRole) {
			QFont font;
			if(m_tmpdir && node->parent == m_tmpdir) {
				font.setItalic(true);
			}
			return font;
		} else if(role == Qt::DisplayRole) {
			return !node->displayName.isEmpty() ? node->displayName : node->fileName;
		} else if(role == Qt::DecorationRole) {
			bool isDir = dynamic_cast<DirNode*>(node);
#ifdef Q_OS_WIN32
			return isDir ? m_iconProvider.icon(QFileIconProvider::Folder) : m_iconProvider.icon(QFileInfo(node->path.mid(1)));
#else
			return isDir ? m_iconProvider.icon(QFileIconProvider::Folder) : m_iconProvider.icon(QFileInfo(node->path));
#endif
		} else if(role == Qt::ToolTipRole) {
#ifdef Q_OS_WIN32
			return node->path.mid(1); // Omit leading /
#else
			return node->path;
#endif
		}
	} else if(index.column() == 1) {
		if(role == Qt::DecorationRole) {
			bool editable = dynamic_cast<FileNode*>(node) && static_cast<FileNode*>(node)->editable;
			return editable ? QIcon::fromTheme("document-edit") : QIcon();
		}
	}
	return QVariant();
}

DataObject* FileTreeModel::fileData(const QModelIndex& index) const {
	FileNode* node = dynamic_cast<FileNode*>(static_cast<Node*>(index.internalPointer()));
	return node ? node->data : nullptr;
}

Qt::ItemFlags FileTreeModel::flags(const QModelIndex& /*index*/) const {
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex& parent) const {
	if (!hasIndex(row, column, parent)) {
		return QModelIndex();
	}
	if(parent.isValid()) {
		Node* node = static_cast<Node*>(parent.internalPointer());
		DirNode* dirNode = dynamic_cast<DirNode*>(node);
		int nDirs = dirNode->dirs.size();
		if(row >= nDirs) {
			return createIndex(row, column, dirNode->files[row - nDirs]);
		} else {
			return createIndex(row, column, dirNode->dirs[row]);
		}
	} else if(row == 1) {
		return m_root && m_tmpdir ? createIndex(row, column, m_tmpdir) : QModelIndex();
	} else if(row == 0) {
		return m_root ? createIndex(row, column, m_root) : m_tmpdir ? createIndex(row, column, m_tmpdir) : QModelIndex();
	}
	return QModelIndex();
}

QModelIndex FileTreeModel::parent(const QModelIndex& child) const {
	Node* node = static_cast<Node*>(child.internalPointer());
	if(!node->parent) {
		return QModelIndex();
	} else {
		DirNode* parentNode = static_cast<DirNode*>(node->parent);
		int row = 0;
		if(parentNode->parent) {
			row = parentNode->parent->dirs.index(parentNode);
		} else if(parentNode == m_tmpdir) {
			row = (m_root ? 1 : 0);
		} else if(parentNode == m_root) {
			row = 0;
		} else {
			return QModelIndex();
		}
		return createIndex(row, 0, parentNode);
	}
}

int FileTreeModel::rowCount(const QModelIndex& parent) const {
	if(!parent.isValid()) {
		return (m_root ? 1 : 0) + (m_tmpdir ? 1 : 0);
	}
	Node* node = static_cast<Node*>(parent.internalPointer());
	if(dynamic_cast<DirNode*>(node)) {
		return static_cast<DirNode*>(node)->childCount();
	}
	return 0;
}

int FileTreeModel::columnCount(const QModelIndex& /*parent*/) const {
	return 2;
}

static QCollator initCollator() {
	QCollator c;
	c.setNumericMode(true);
	return c;
}

static const QCollator& collator() {
	static QCollator c = initCollator();
	return c;
}

template<class T>
T FileTreeModel::NodeList<T>::add(T node) {
	auto it = std::lower_bound(this->begin(), this->end(), node->fileName, [](const T & a, const QString & b) { return collator().compare(a->fileName, b) < 0; });
	this->insert(it, node);
	return node;
}

template<class T>
T FileTreeModel::NodeList<T>::add(T node, int pos) {
	auto it = this->begin();
	std::advance(it, pos);
	this->insert(it, node);
	return node;
}

template<class T>
T FileTreeModel::NodeList<T>::take(T node) {
	auto it = find(node->fileName);
	if(it != this->end()) {
		this->erase(it);
		return node;
	}
	return nullptr;
}

template<class T>
typename FileTreeModel::NodeList<T>::const_iterator FileTreeModel::NodeList<T>::find(const QString& fileName) const {
	auto it = std::lower_bound(this->begin(), this->end(), fileName, [](const T & a, const QString & b) { return collator().compare(a->fileName, b) < 0; });
	return it == this->end() ? it : (*it)->fileName == fileName ? it : this->end();
}

template<class T>
int FileTreeModel::NodeList<T>::insIndex(const QString& fileName) const {
	auto it = std::lower_bound(this->begin(), this->end(), fileName, [](const T & a, const QString & b) { return collator().compare(a->fileName, b) < 0; });
	return std::distance(this->begin(), it);
}

template<class T>
int FileTreeModel::NodeList<T>::index(T node) const {
	auto it = std::lower_bound(this->begin(), this->end(), node->fileName, [](const T & a, const QString & b) { return collator().compare(a->fileName, b) < 0; });
	return std::distance(this->begin(), it);
}

template<class T>
int FileTreeModel::NodeList<T>::index(const_iterator it) const {
	return std::distance(this->begin(), it);
}
