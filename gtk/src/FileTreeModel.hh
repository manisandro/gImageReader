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

#include "common.hh"

#include <gtkmm.h>

class FileTreeModel : public Gtk::TreeModel, public Glib::Object {
public:
	enum Columns { COLUMN_ICON, COLUMN_TEXT, COLUMN_TOOLTIP, COLUMN_TEXTSTYLE, COLUMN_EDITICON, NUM_COLUMNS};

	FileTreeModel();

	Gtk::TreeIter insertFile(std::string filePath, DataObject* data, const Glib::ustring& displayName = Glib::ustring());
	Gtk::TreeIter findFile(const std::string& filePath, bool isFile = true) const;
	bool removeIndex(const Gtk::TreeIter& index);
	void clear();
	bool isDir(const Gtk::TreeIter& index) const;
	void setFileEditable(const Gtk::TreeIter& index, bool editable);
	bool isFileEditable(const Gtk::TreeIter& index) const;

	template<class T>
	T fileData(const Gtk::TreeIter& index) const { return static_cast<T>(fileData(index)); }
	DataObject* fileData(const Gtk::TreeIter& index) const;

	bool empty() const { return m_root == nullptr && m_tmpdir == nullptr; }

private:
	Gtk::TreePath get_root_path(int idx) const;
	void recursive_row_inserted(const Gtk::TreeIter& index);

	Gtk::TreeModelFlags get_flags_vfunc() const override;
	GType get_column_type_vfunc(int index) const override;
	int get_n_columns_vfunc() const override;
	bool iter_next_vfunc(const iterator& iter, iterator& iter_next) const override;
	bool get_iter_vfunc(const Path& path, iterator& iter) const override;
	bool iter_children_vfunc(const iterator& parent, iterator& iter) const override;
	bool iter_parent_vfunc(const iterator& child, iterator& iter) const override;
	bool iter_nth_child_vfunc(const iterator& parent, int n, iterator& iter) const override;
	bool iter_nth_root_child_vfunc(int n, iterator& iter) const override;
	bool iter_has_child_vfunc(const iterator& iter) const override;
	int iter_n_children_vfunc(const iterator& iter) const override;
	int iter_n_root_children_vfunc() const override;
//	void ref_node_vfunc(const iterator& iter) const override;
//	void unref_node_vfunc(const iterator& iter) const override;
	Gtk::TreeModel::Path get_path_vfunc(const iterator& iter) const override;
	void get_value_vfunc(const iterator& iter, int column, Glib::ValueBase& value) const override;
	void set_value_impl(const iterator& row, int column, const Glib::ValueBase& value) override;
	void get_value_impl(const iterator& row, int column, Glib::ValueBase& value) const override;

private:
	template<class T>
	class NodeList : public std::vector<T> {
	public:
		typedef typename std::vector<T>::iterator iterator;
		typedef typename std::vector<T>::const_iterator const_iterator;

		T add(T node);
		T add(T node, int pos);
		T take(T node);
		const_iterator find(const Glib::ustring& fileName) const;
		int insIndex(const Glib::ustring& fileName) const;
		int index(T node) const;
		int index(const_iterator it) const;
	};

	struct DirNode;
	struct Node {
		Node(const Glib::ustring& _fileName, const std::string& _path, DirNode* _parent, const Glib::ustring& _displayName = Glib::ustring())
			: fileName(_fileName), path(_path), displayName(_displayName), parent(_parent) {}
		virtual ~Node() = default;
		Glib::ustring fileName;
		std::string path;
		Glib::ustring displayName;
		DirNode* parent = nullptr;
	};
	struct FileNode : Node {
		FileNode(const Glib::ustring& _fileName, const std::string& _path, DirNode* _parent, DataObject* _data, const Glib::ustring& _displayName)
			: Node(_fileName, _path, _parent, _displayName), data(_data) {}
		~FileNode() { delete data; }
		bool editable = false;
		DataObject* data = nullptr;
	};
	struct DirNode : Node {
		using Node::Node;
		~DirNode() {
			for(DirNode* dir : dirs) {
				delete dir;
			}
			for(FileNode* file : files) {
				delete file;
			}
		}
		NodeList<DirNode*> dirs;
		NodeList<FileNode*> files;
		int childCount() const { return dirs.size() + files.size(); }
		int childIndex(Node* node) const {
			bool isDir = dynamic_cast<DirNode*>(node);
			return isDir ? dirs.index(static_cast<DirNode*>(node)) : (dirs.size() + files.index(static_cast<FileNode*>(node)));
		}
		Node* firstNode() const {
			return dirs.empty() ? files.empty() ? nullptr : static_cast<Node*>(files[0]) : static_cast<Node*>(dirs[0]);
		}
		Node* child(int index) {
			return index >= int(dirs.size()) ? static_cast<Node*>(files[index - dirs.size()]) : static_cast<Node*>(dirs[index]);
		}
		Node* nextNode(Node* node) const {
			int index = (dynamic_cast<DirNode*>(node) ? dirs.index(static_cast<DirNode*>(node)) : dirs.size() + files.index(static_cast<FileNode*>(node))) + 1;
			return index < int(dirs.size()) ? static_cast<Node*>(dirs[index]) : index < childCount() ? static_cast<Node*>(files[index - dirs.size()]) : nullptr;
		}
	};

	DirNode* m_root = nullptr;
	DirNode* m_tmpdir = nullptr;
};

#endif // FILETREEMODEL_HH
