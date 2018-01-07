/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.hh
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

#ifndef SOURCEMANAGER_HH
#define SOURCEMANAGER_HH

#include <QMetaType>
#include <QStringList>
#include <QFileSystemWatcher>

class QMenu;
class QPixmap;
class UI_MainWindow;


struct Source {
	Source(const QString& _path, const QString& _displayname, const QByteArray& _password, bool _isTemp = false)
		: path(_path), displayname(_displayname), password(_password), isTemp(_isTemp) {}
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
};

class SourceManager : public QObject {
	Q_OBJECT
public:
	SourceManager(const UI_MainWindow& _ui);
	~SourceManager();
	QList<Source*> getSelectedSources() const;
	void addSourceImage(const QImage& image);

	int addSources(const QStringList& files);

public slots:
	bool addSource(const QString& file) {
		return addSources(QStringList() << file) == 1;
	}

signals:
	void sourceChanged();

private:
	const UI_MainWindow& ui;

	QMenu* m_recentMenu;
	QFileSystemWatcher m_fsWatcher;

	int m_screenshotCount = 0;
	int m_pasteCount = 0;

	bool querySourcePassword(const QString& filename, QByteArray& password) const;
	bool checkTextLayer(const QString& filename) const;
	void savePixmap(const QPixmap& pixmap, const QString& displayname);
	void selectionChanged();
	bool eventFilter(QObject* object, QEvent* event) override;

private slots:
	void clearSources();
	void currentSourceChanged();
	void deleteSource() {
		removeSource(true);
	}
	void fileChanged(const QString& filename);
	void openRecentItem();
	void openSources();
	void pasteClipboard();
	void prepareSourcesMenu();
	void removeSource(bool deleteFile = false);
	void takeScreenshot();
};

#endif // SOURCEMANAGER_HH
