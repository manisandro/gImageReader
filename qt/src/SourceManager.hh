/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.hh
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

#ifndef SOURCEMANAGER_HH
#define SOURCEMANAGER_HH

#include <QMap>
#include <QMetaType>
#include <QStringList>
#include <QFileSystemWatcher>
#include "common.hh"

class FileTreeModel;
class QMenu;
class QPixmap;
class UI_MainWindow;

class Source : public DataObject {
public:
	Source(const QString& _path, const QString& _displayname, const QByteArray& _password = "", bool _isTemp = false)
		: path(_path), displayname(_displayname), password(_password), isTemp(_isTemp) {}
	~Source();
	QString path;
	QString displayname;
	QByteArray password;
	bool isTemp;
	int brightness = 0;
	int contrast = 0;
	int resolution = -1;
	int page = 1;
	QVector<double> angle;
	bool invert = false;

	//Additional info from original file
	QString author, title, creator, producer, keywords, subject;
	int pdfVersionMajor = -1, pdfVersionMinor = -1;
};

class SourceManager : public QObject {
	Q_OBJECT
public:
	SourceManager(const UI_MainWindow& _ui);
	~SourceManager();
	QList<Source*> getSelectedSources() const;
	void addSourceImage(const QImage& image);

	int addSources(const QStringList& files, bool suppressWarnings = false);

public slots:
	bool addSource(const QString& file, bool suppressWarnings = false) {
		return addSources(QStringList() << file, suppressWarnings) == 1;
	}

signals:
	void sourceChanged();

private:
	enum class PdfWithTextAction {Ask, Add, Skip};
	const UI_MainWindow& ui;

	FileTreeModel* m_fileTreeModel = nullptr;
	QMenu* m_recentMenu = nullptr;
	QFileSystemWatcher m_fsWatcher;
	QMap<QString, int> m_watchedDirectories;

	static constexpr int sMaxNumRecent = 15;
	int m_screenshotCount = 0;
	int m_pasteCount = 0;
	bool m_inCurrentSourceChanged = false;

	bool checkPdfSource(Source* source, PdfWithTextAction& textAction, QStringList& failed) const;
	void savePixmap(const QPixmap& pixmap, const QString& displayname);
	bool eventFilter(QObject* object, QEvent* event) override;
	void selectRecursive(QItemSelection& parentsel, const QModelIndex& index);

private slots:
	void clearSources();
	void currentSourceChanged(const QItemSelection&, const QItemSelection& deselected);
	void indexClicked(const QModelIndex& index);
	void deleteSource() {
		removeSource(true);
	}
	void fileChanged(const QString& filename);
	void directoryChanged(const QString& dir);
	void addFolder();
	void openRecentItem();
	void openSources();
	void pasteClipboard();
	void prepareRecentMenu();
	void removeSource(bool deleteFile = false);
	void takeScreenshot();
};

#endif // SOURCEMANAGER_HH
