/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * FileTreeModel.cc
 * Copyright (C) 2022-2024 Sandro Mani <manisandro@gmail.com>
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

#include "FileTreeModel.hh"
#include "Utils.hh"

#include <iostream>

#define DEBUG(...) //__VA_ARGS__


FileTreeModel::FileTreeModel()
	: Glib::ObjectBase("FileTreeModel") {

}

Gtk::TreePath FileTreeModel::get_root_path(int idx) const {
	Gtk::TreePath path;
	path.push_back(idx);
	return path;
}

void FileTreeModel::recursive_row_inserted(const Gtk::TreeIter& index) {
	DEBUG(std::cout << "Inserted: " << get_path(index).to_string() << std::endl;)
	row_inserted(get_path(index), index);
	for (const Gtk::TreeIter& childIndex : index->children()) {
		recursive_row_inserted(childIndex);
	}
}

Gtk::TreeIter FileTreeModel::insertFile(std::string filePath, DataObject* data, const Glib::ustring& displayName) {
#ifdef G_OS_WIN32
	Glib::ustring fileDir = std::string("/") + Glib::path_get_dirname(filePath);
	Utils::string_replace(fileDir, "\\", "/", true);
	filePath = std::string("/") + filePath;
	std::string tempPath = std::string("/") + Glib::get_tmp_dir();
#else
	std::string fileDir = Glib::path_get_dirname(filePath);
	std::string tempPath = Glib::get_tmp_dir();
#endif
	std::string fileName = Glib::path_get_basename(filePath);

	if (fileDir.substr(0, tempPath.length()) == tempPath) {
		Gtk::TreePath treePath = get_root_path(m_root ? 1 : 0);
		if (!m_tmpdir) {
			m_tmpdir = new DirNode(Glib::path_get_basename(tempPath), tempPath, nullptr, _("Temporary files"));
			row_inserted(treePath, get_iter(treePath));
		}
		int pos = m_tmpdir->files.insIndex(fileName);
		m_tmpdir->files.add(new FileNode(fileName, filePath, m_tmpdir, data, displayName), pos);
		treePath.push_back(m_tmpdir->dirs.size() + pos);
		Gtk::TreeIter iter = get_iter(treePath);
		row_inserted(treePath, iter);
		return iter;
	} else if (!m_root) {
		// Set initial root
		Gtk::TreePath treePath = get_root_path(0);
		m_root = new DirNode(Glib::path_get_basename(fileDir), fileDir, nullptr);
		row_inserted(treePath, get_iter(treePath));

		m_root->files.add(new FileNode(fileName, filePath, m_root, data, displayName));
		treePath.push_back(0);
		Gtk::TreeIter iter = get_iter(treePath);
		row_inserted(treePath, iter);
		return iter;
	} else if (m_root->path == fileDir) {
		// Add to current root
		Gtk::TreePath treePath = get_root_path(0);
		int pos = m_root->files.insIndex(fileName);
		int row = m_root->dirs.size() + pos;
		m_root->files.add(new FileNode(fileName, filePath, m_root, data, displayName), pos);
		treePath.push_back(row);
		Gtk::TreeIter iter = get_iter(treePath);
		row_inserted(treePath, iter);
		return iter;
	} else if (m_root->path.substr(0, fileDir.length()) == fileDir) {
		// Root below new path, replace root
		std::vector<Glib::ustring> path = Utils::string_split(m_root->path.substr(fileDir.length()), '/', false);
		DirNode* oldroot = m_root;
		m_root = nullptr;
		row_deleted(get_root_path(0));

		m_root = new DirNode(Glib::path_get_basename(fileDir), fileDir, nullptr);
		DirNode* cur = m_root;
		for (int i = 0, n = path.size() - 1; i < n; ++i) {
			cur = cur->dirs.add(new DirNode(path[i], cur->path + "/" + path[i], cur));
		}
		cur->dirs.add(oldroot);
		oldroot->parent = cur;
		int pos = m_root->files.insIndex(fileName);
		int row = m_root->dirs.size() + pos;
		m_root->files.add(new FileNode(fileName, filePath, m_root, data, displayName), pos);
		Gtk::TreePath treePath = get_root_path(0);
		recursive_row_inserted(get_iter(treePath));
		treePath.push_back(row);
		return get_iter(treePath);
	} else if (fileDir.substr(0, m_root->path.length()) == m_root->path) {
		// New path below root, append to subtree
		std::vector<Glib::ustring> path = Utils::string_split(fileDir.substr(m_root->path.length()), '/', false);
		DirNode* cur = m_root;
		Gtk::TreePath treePath = get_root_path(0);
		for (const Glib::ustring& part : path) {
			auto it = cur->dirs.find(part);
			int row = 0;
			if (it == cur->dirs.end()) {
				row = cur->dirs.insIndex(part);
				cur = cur->dirs.add(new DirNode(part, cur->path + "/" + part, cur), row);
				treePath.push_back(row);
				row_inserted(treePath, get_iter(treePath));
			} else {
				treePath.push_back(cur->dirs.index(it));
				cur = *it;
			}
		}
		int pos = cur->files.insIndex(fileName);
		int row = cur->dirs.size() + pos;
		cur->files.add(new FileNode(fileName, filePath, cur, data, displayName), pos);
		treePath.push_back(row);
		row_inserted(treePath, get_iter(treePath));
		return get_iter(treePath);
	} else {
		// Unrelated trees, find common ancestor
		std::vector<Glib::ustring> rootPath = Utils::string_split(m_root->path, '/', false);
		std::vector<Glib::ustring> newPath = Utils::string_split(fileDir, '/', false);
		int pos = 0;
		for (int n = std::min(rootPath.size(), newPath.size()); pos < n; ++pos) {
			if (rootPath[pos] != newPath[pos]) {
				break;
			}
		}
		std::string newRoot = std::string("/") + Utils::string_join(Utils::vector_slice(rootPath, 0, pos), "/");
		DirNode* oldroot = m_root;
		m_root = nullptr;
		row_deleted(get_root_path(0));

		m_root = new DirNode(Glib::path_get_basename(newRoot), newRoot, nullptr);
		// Insert old root and new branch
		// - Old root
		DirNode* cur = m_root;
		std::vector<Glib::ustring> path = Utils::vector_slice(rootPath, pos);
		for (int i = 0, n = path.size() - 1; i < n; ++i) {
			cur = cur->dirs.add(new DirNode(path[i], cur->path + "/" + path[i], cur));
		}
		cur->dirs.add(oldroot);
		oldroot->parent = cur;
		// - New branch
		cur = m_root;
		path = Utils::vector_slice(newPath, pos);
		Gtk::TreePath treePath = get_root_path(0);
		for (int i = 0, n = path.size(); i < n; ++i) {
			int pos = cur->dirs.insIndex(path[i]);
			cur = cur->dirs.add(new DirNode(path[i], cur->path + "/" + path[i], cur), pos);
			treePath.push_back(pos);
		}
		cur->files.add(new FileNode(fileName, filePath, cur, data, displayName));
		recursive_row_inserted(get_iter(get_root_path(0)));
		treePath.push_back(0);
		return get_iter(treePath);
	}
}

Gtk::TreeIter FileTreeModel::findFile(const std::string& filePath, bool isFile) const {
	if (!m_root && !m_tmpdir) {
		return Gtk::TreeIter();
	}

#ifdef G_OS_WIN32
	std::string prefix = "/";
	Glib::ustring fileDir = std::string("/") + Glib::path_get_dirname(filePath);
	Utils::string_replace(fileDir, "\\", "/", true);
	std::string tempPath = std::string("/") + Glib::get_tmp_dir();
#else
	std::string prefix;
	std::string fileDir = Glib::path_get_dirname(filePath);
	std::string tempPath = Glib::get_tmp_dir();
#endif
	std::string fileName = Glib::path_get_basename(filePath);

	// Path is root dir
	if (!isFile && m_root && prefix + filePath == m_root->path) {
		return const_cast<FileTreeModel*> (this)->get_iter(get_root_path(0));
	}
	// Path is temp dir
	else if (!isFile && m_tmpdir && prefix + filePath == m_tmpdir->path) {
		return const_cast<FileTreeModel*> (this)->get_iter(get_root_path(m_root ? 1 : 0));
	}
	// Path is below root or tempdir
	DirNode* cur = nullptr;
	Gtk::TreePath treePath;
	if (m_tmpdir && fileDir.substr(0, m_tmpdir->path.length()) == m_tmpdir->path) {
		cur = m_tmpdir;
		treePath = get_root_path(m_root ? 1 : 0);
	} else if (m_root && fileDir.substr(0, m_root->path.length()) == m_root->path) {
		cur = m_root;
		treePath = get_root_path(0);
	}
	if (!cur) {
		return Gtk::TreeIter();
	}

	std::string relPath = fileDir.substr(cur->path.length());
	std::vector<Glib::ustring> parts = Utils::string_split(relPath, '/', false);
	for (const Glib::ustring& part : parts) {
		auto it = cur->dirs.find(part);
		if (it == cur->dirs.end()) {
			return Gtk::TreeIter();
		}
		treePath.push_back(cur->dirs.index(*it));
		cur = *it;
	}
	if (isFile) {
		auto it = cur->files.find(fileName);
		if (it != cur->files.end()) {
			treePath.push_back(cur->dirs.size() + cur->files.index(*it));
			return const_cast<FileTreeModel*> (this)->get_iter(treePath);
		} else {
			return Gtk::TreeIter();
		}
	} else {
		auto it = cur->dirs.find(fileName);
		if (it != cur->dirs.end()) {
			treePath.push_back(cur->dirs.index(*it));
			return const_cast<FileTreeModel*> (this)->get_iter(treePath);
		} else {
			return Gtk::TreeIter();
		}
	}
}

bool FileTreeModel::removeIndex(const Gtk::TreeIter& index) {
	if (!index) {
		return false;
	}
	Node* node = static_cast<Node*> (index.gobj()->user_data);
	// Remove entire branch
	bool isFile = dynamic_cast<FileNode*> (node);
	Node* deleteNode = node;
	Gtk::TreeIter deleteIndex = index;
	while (deleteNode->parent && deleteNode->parent->childCount() == 1) {
		isFile = false;
		deleteNode = deleteNode->parent;
		deleteIndex = deleteIndex->parent();
	}
	Gtk::TreePath deletePath = get_path(deleteIndex);
	if (deleteNode == m_root) {
		delete m_root;
		m_root = nullptr;
	} else if (deleteNode == m_tmpdir) {
		delete m_tmpdir;
		m_tmpdir = nullptr;
	} else {
		if (isFile) {
			delete deleteNode->parent->files.take(static_cast<FileNode*> (deleteNode));
		} else {
			delete deleteNode->parent->dirs.take(static_cast<DirNode*> (deleteNode));
		}
	}
	row_deleted(deletePath);
	return true;
}

void FileTreeModel::clear() {
	if (m_root || m_tmpdir) {
		std::vector<Gtk::TreePath> delPaths;
		if (m_root) {
			delPaths.push_back(get_root_path(0));
		}
		if (m_tmpdir) {
			delPaths.push_back(get_root_path(m_root ? 1 : 0));
		}
		delete m_root;
		m_root = nullptr;
		delete m_tmpdir;
		m_tmpdir = nullptr;
		for (const Gtk::TreePath& path : delPaths) {
			row_deleted(path);
		}
	}
}

bool FileTreeModel::isDir(const Gtk::TreeIter& index) const {
	if (!index) {
		return false;
	}
	return dynamic_cast<DirNode*> (static_cast<Node*> (index.gobj()->user_data)) != nullptr;
}

void FileTreeModel::setFileEditable(const Gtk::TreeIter& index, bool editable) {
	Node* node = static_cast<Node*> (index.gobj()->user_data);
	if (dynamic_cast<FileNode*> (node)) {
		static_cast<FileNode*> (node)->editable = editable;
		row_changed(get_path(index), index);
	}
}

bool FileTreeModel::isFileEditable(const Gtk::TreeIter& index) const {
	Node* node = static_cast<Node*> (index.gobj()->user_data);
	return dynamic_cast<FileNode*> (node) && static_cast<FileNode*> (node)->editable;
}

DataObject* FileTreeModel::fileData(const Gtk::TreeIter& index) const {
	FileNode* node = dynamic_cast<FileNode*> (static_cast<Node*> (index.gobj()->user_data));
	return node ? node->data : nullptr;
}

Gtk::TreeModelFlags FileTreeModel::get_flags_vfunc() const {
	return Gtk::TREE_MODEL_ITERS_PERSIST;
}

GType FileTreeModel::get_column_type_vfunc(int index) const {
	if (index == COLUMN_ICON) {
		return Glib::Value<Glib::RefPtr<Gdk::Pixbuf >>::value_type();
	}
	else if (index == COLUMN_TEXT) {
		return Glib::Value<Glib::ustring>::value_type();
	} else if (index == COLUMN_TOOLTIP) {
		return Glib::Value<Glib::ustring>::value_type();
	} else if (index == COLUMN_TEXTSTYLE) {
		return Glib::Value<Pango::Style>::value_type();
	} else if (index == COLUMN_EDITICON) {
		return Glib::Value<Glib::RefPtr<Gdk::Pixbuf >>::value_type();
	}
	return G_TYPE_INVALID;
}

int FileTreeModel::get_n_columns_vfunc() const {
	return NUM_COLUMNS;
}

bool FileTreeModel::iter_next_vfunc(const iterator& iter, iterator& iter_next) const {
	DEBUG(std::cout << "iter_next_vfunc " << get_path(iter).to_string() << ": ";)
	if (!iter) {
		return false;
	}
	Node* node = static_cast<Node*> (iter.gobj()->user_data);
	Node* nextItem = nullptr;
	// Tempdir follows root
	if (node->parent) {
		nextItem = node->parent->nextNode(node);
		if (!nextItem && node->parent == m_root && m_tmpdir) {
			nextItem = m_tmpdir->firstNode();
		}
	} else if (node == m_root && m_tmpdir) {
		nextItem = m_tmpdir;
	}
	iter_next.gobj()->user_data = nextItem;
	iter_next.set_stamp(iter_next.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter_next.gobj()->user_data != nullptr) << std::endl;)
	return iter_next.gobj()->user_data != nullptr;
}

bool FileTreeModel::get_iter_vfunc(const Path& path, iterator& iter) const {
	DEBUG(std::cout << "get_iter_vfunc " << path.to_string() << ": ";)
	if (path.empty() || (!m_root && !m_tmpdir)) {
		DEBUG(std::cout << "0" << std::endl;)
		return false;
	}
	int idx = 0, size = path.size();
	Node* item = path[idx++] == 0 ? m_root ? m_root : m_tmpdir : m_tmpdir;
	while (item && idx < size) {
		item = static_cast<DirNode*> (item)->child(path[idx++]);
	}
	iter.gobj()->user_data = item;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (item != nullptr) << std::endl;)
	return item != nullptr;
}

bool FileTreeModel::iter_children_vfunc(const iterator& parent, iterator& iter) const {
	DEBUG(std::cout << "iter_children_vfunc " << get_path(parent).to_string() << ": ";)
	DirNode* parentNode = dynamic_cast<DirNode*> (static_cast<Node*> (parent.gobj()->user_data));
	iter.gobj()->user_data = parentNode ? parentNode->firstNode() : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool FileTreeModel::iter_parent_vfunc(const iterator& child, iterator& iter) const {
	DEBUG(std::cout << "iter_parent_vfunc " << get_path(child).to_string() << ": ";)
	Node* childNode = static_cast<Node*> (child.gobj()->user_data);
	iter.gobj()->user_data = childNode ? childNode->parent : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool FileTreeModel::iter_nth_child_vfunc(const iterator& parent, int n, iterator& iter) const {
	DEBUG(std::cout << "iter_nth_child_vfunc " << get_path(parent).to_string() << "@" << n << ": ";)
	DirNode* parentNode = static_cast<DirNode*> (parent.gobj()->user_data);
	iter.gobj()->user_data = parentNode ? parentNode->child(n) : nullptr;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool FileTreeModel::iter_nth_root_child_vfunc(int n, iterator& iter) const {
	DEBUG(std::cout << "iter_nth_root_child_vfunc " << n << ": ";)
	Node* childNode = nullptr;
	if (m_root && m_tmpdir) {
		childNode = n == 0 ? m_root : n == 1 ? m_tmpdir : nullptr;
	} else if (m_root) {
		childNode = n == 0 ? m_root : nullptr;
	} else if (m_tmpdir) {
		childNode = n == 0 ? m_tmpdir : nullptr;
	}
	iter.gobj()->user_data =  childNode;
	iter.set_stamp(iter.gobj()->user_data != nullptr);
	DEBUG(std::cout << (iter.gobj()->user_data != nullptr) << std::endl;)
	return iter.gobj()->user_data != nullptr;
}

bool FileTreeModel::iter_has_child_vfunc(const iterator& iter) const {
	DEBUG(std::cout << "iter_parent_vfunc " << get_path(iter).to_string() << ": ";)
	Node* node = static_cast<Node*> (iter.gobj()->user_data);
	bool haveChild = dynamic_cast<DirNode*> (node) && static_cast<DirNode*> (node)->childCount() > 0;
	DEBUG(std::cout << haveChild << std::endl;)
	return haveChild;
}

int FileTreeModel::iter_n_children_vfunc(const iterator& iter) const {
	DEBUG(std::cout << "iter_n_children_vfunc " << get_path(iter).to_string() << ": ";)
	Node* node = static_cast<Node*> (iter.gobj()->user_data);
	int numChild = dynamic_cast<DirNode*> (node) ? static_cast<DirNode*> (node)->childCount() : 0;
	DEBUG(std::cout << numChild << std::endl;)
	return numChild;
}

int FileTreeModel::iter_n_root_children_vfunc() const {
	int nRoot = (m_root != nullptr) + (m_tmpdir != nullptr);
	DEBUG(std::cout << "iter_n_root_children_vfunc " << nRoot << std::endl;)
	return nRoot;
}

Gtk::TreeModel::Path FileTreeModel::get_path_vfunc(const iterator& iter) const {
	Gtk::TreeModel::Path path;
	Node* node = static_cast<Node*> (iter.gobj()->user_data);
	while (node) {
		int index = -1;
		if (node->parent) {
			index = node->parent->childIndex(node);
		} else {
			if (node == m_root) {
				index = 0;
			} else if (node == m_tmpdir) {
				index = m_root ? 1 : 0;
			}
		}
		path.push_front(index);
		node = node->parent;
	}
	return path;
}

template<class T>
static void setValue(Glib::ValueBase& gvalue, const T& value) {
	Glib::Value<T> val;
	val.init(val.value_type());
	val.set(value);
	gvalue.init(val.gobj());
}

void FileTreeModel::get_value_vfunc(const iterator& iter, int column, Glib::ValueBase& value) const {
	DEBUG(std::cout << "get_value_vfunc Column " << column << " at path " << get_path(iter).to_string() << std::endl);
	Node* node = static_cast<Node*> (iter.gobj()->user_data);
	if (!node) {
		return;
	} else if (column == COLUMN_ICON) {
		Glib::RefPtr<Gtk::IconTheme> iconTheme = Gtk::IconTheme::get_default();
		bool uncertain = false;
		Glib::ustring contentType = dynamic_cast<DirNode*> (node) ? "inode/directory" : Gio::content_type_guess(node->path, nullptr, 0, uncertain);
		Glib::RefPtr<Gio::Icon> icon = Gio::content_type_get_icon(contentType);
		Glib::RefPtr<Gio::ThemedIcon> themedIcon = Glib::RefPtr<Gio::ThemedIcon>::cast_dynamic(icon);
		if (themedIcon) {
			setValue(value, iconTheme->choose_icon(themedIcon->get_names(), 16).load_icon());
		}
	} else if (column == COLUMN_TEXT) {
		setValue(value, !node->displayName.empty() ? node->displayName : node->fileName);
	} else if (column == COLUMN_TOOLTIP) {
#ifdef G_OS_WIN32
		setValue(value, node->path.substr(1));
#else
		setValue(value, node->path);
#endif
	} else if (column == COLUMN_TEXTSTYLE) {
		setValue(value, m_tmpdir && node->parent == m_tmpdir ? Pango::STYLE_NORMAL : Pango::STYLE_NORMAL);
	} else if (column == COLUMN_EDITICON) {
		bool editable = dynamic_cast<FileNode*> (node) && static_cast<FileNode*> (node)->editable;
		Glib::RefPtr<Gdk::Pixbuf> icon = editable ? Gtk::IconTheme::get_default()->load_icon("document-edit", 16) : Glib::RefPtr<Gdk::Pixbuf> (nullptr);
		setValue(value, icon);
	}
}

void FileTreeModel::set_value_impl(const iterator& /*row*/, int /*column*/, const Glib::ValueBase& /*value*/) {
	// Not editable
}

void FileTreeModel::get_value_impl(const iterator& row, int column, Glib::ValueBase& value) const {
	get_value_vfunc(row, column, value);
}


template<class T>
T FileTreeModel::NodeList<T>::add(T node) {
	auto it = std::lower_bound(this->begin(), this->end(), node->fileName, [](const T & a, const Glib::ustring & b) { return a->fileName.compare(b) < 0; });
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
	if (it != this->end()) {
		this->erase(it);
		return node;
	}
	return nullptr;
}

template<class T>
typename FileTreeModel::NodeList<T>::const_iterator FileTreeModel::NodeList<T>::find(const Glib::ustring& fileName) const {
	auto it = std::lower_bound(this->begin(), this->end(), fileName, [](const T & a, const Glib::ustring & b) { return a->fileName.compare(b) < 0; });
	return it == this->end() ? it : (*it)->fileName == fileName ? it : this->end();
}

template<class T>
int FileTreeModel::NodeList<T>::insIndex(const Glib::ustring& fileName) const {
	auto it = std::lower_bound(this->begin(), this->end(), fileName, [](const T & a, const Glib::ustring & b) { return a->fileName.compare(b) < 0; });
	return std::distance(this->begin(), it);
}

template<class T>
int FileTreeModel::NodeList<T>::index(T node) const {
	auto it = std::lower_bound(this->begin(), this->end(), node->fileName, [](const T & a, const Glib::ustring & b) { return a->fileName.compare(b) < 0; });
	return std::distance(this->begin(), it);
}

template<class T>
int FileTreeModel::NodeList<T>::index(const_iterator it) const {
	return std::distance(this->begin(), it);
}
