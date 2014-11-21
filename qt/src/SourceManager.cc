/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.cc
 * Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#include <QClipboard>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QTemporaryFile>

#include "Config.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"

Q_DECLARE_METATYPE(Source*)

SourceManager::SourceManager(const UI_MainWindow& _ui)
	: ui(_ui)
{
	m_recentMenu = new QMenu(MAIN);
	ui.actionSourceRecent->setMenu(m_recentMenu);

	connect(ui.actionSources, SIGNAL(toggled(bool)), ui.dockWidgetSources, SLOT(setVisible(bool)));
	connect(ui.toolButtonSourceAdd, SIGNAL(clicked()), this, SLOT(openSources()));
	connect(ui.menuAddSource, SIGNAL(aboutToShow()), this, SLOT(prepareSourcesMenu()));
	connect(ui.actionSourcePaste, SIGNAL(triggered()), this, SLOT(pasteClipboard()));
	connect(ui.actionSourceScreenshot, SIGNAL(triggered()), this, SLOT(takeScreenshot()));
	connect(ui.actionSourceRemove, SIGNAL(triggered()), this, SLOT(removeSource()));
	connect(ui.actionSourceDelete, SIGNAL(triggered()), this, SLOT(deleteSource()));
	connect(ui.actionSourceClear, SIGNAL(triggered()), this, SLOT(clearSources()));
	connect(ui.listWidgetSources, SIGNAL(itemSelectionChanged()), this, SLOT(currentSourceChanged()));
	connect(&m_fsWatcher, SIGNAL(fileChanged(QString)), this, SLOT(fileChanged(QString)));

	MAIN->getConfig()->addSetting(new VarSetting<QStringList>("recentitems"));
}

SourceManager::~SourceManager()
{
	clearSources();
}

void SourceManager::addSources(const QStringList& files)
{
	QString failed;
	QListWidgetItem* item = nullptr;
	QStringList recentItems = MAIN->getConfig()->getSetting<VarSetting<QStringList>>("recentitems")->getValue();
	for(const QString& filename : files){
		if(!QFile(filename).exists()){
			failed += "\n\t" + filename;
			continue;
		}
		bool contains = false;
		for(int row = 0, nRows = ui.listWidgetSources->count(); row < nRows; ++row){
			if(ui.listWidgetSources->item(row)->toolTip() == filename){
				contains = true;
				break;
			}
		}
		if(contains){
			continue;
		}
		item = new QListWidgetItem(QFileInfo(filename).fileName(), ui.listWidgetSources);
		item->setToolTip(filename);
		Source* source = new Source(filename, QFileInfo(filename).fileName());
		item->setData(Qt::UserRole, QVariant::fromValue(source));
		m_fsWatcher.addPath(filename);
		recentItems.removeAll(filename);
		recentItems.prepend(filename);
	}
	MAIN->getConfig()->getSetting<VarSetting<QStringList>>("recentitems")->setValue(recentItems);
	if(item){
		ui.listWidgetSources->setCurrentItem(item);
	}
	if(!failed.isEmpty()){
		QMessageBox::critical(MAIN, _("Unable to open files"), _("The following files could not be opened:%1").arg(failed));
	}
}

Source* SourceManager::getSelectedSource() const
{
	if(ui.listWidgetSources->currentItem()){
		return ui.listWidgetSources->currentItem()->data(Qt::UserRole).value<Source*>();
	}
	return nullptr;
}

void SourceManager::prepareSourcesMenu()
{
	// Build recent menu
	m_recentMenu->clear();
	for(const QString& filename : MAIN->getConfig()->getSetting<VarSetting<QStringList>>("recentitems")->getValue()){
		if(QFile(filename).exists()){
			QAction* action = new QAction(QFileInfo(filename).fileName(), m_recentMenu);
			action->setToolTip(filename);
			connect(action, SIGNAL(triggered()), this, SLOT(openRecentItem()));
			m_recentMenu->addAction(action);
		}
	}
	ui.actionSourceRecent->setEnabled(!m_recentMenu->isEmpty());

	// Set paste action sensitivity
	ui.actionSourcePaste->setEnabled(!QApplication::clipboard()->pixmap().isNull());
}

void SourceManager::openSources()
{
	Source* current = getSelectedSource();
	QString dir;
	if(current && !current->isTemp){
		dir = QFileInfo(current->path).absolutePath();
	}else{
		dir = Utils::documentsFolder();
	}
	QSet<QString> formats;
	for(const QByteArray& format : QImageReader::supportedImageFormats()){
		formats.insert(QString("*.%1").arg(QString(format).toLower()));
	}
	formats.insert("*.pdf");
	QString filter = QString("%1 (%2)").arg(_("Images and PDFs")).arg(QStringList(formats.toList()).join(" "));
	addSources(QFileDialog::getOpenFileNames(MAIN, _("Select Files"), dir, filter));
}

void SourceManager::openRecentItem()
{
	const QString& filename = qobject_cast<QAction*>(QObject::sender())->toolTip();
	addSources(QStringList() << filename);
}

void SourceManager::pasteClipboard()
{
	QPixmap pixmap = QApplication::clipboard()->pixmap();
	if(pixmap.isNull()){
		QMessageBox::critical(MAIN, _("Clipboard Error"),  _("Failed to read the clipboard."));
		return;
	}
	++m_pasteCount;
	QString displayname = _("Pasted %1").arg(m_pasteCount);
	savePixmap(pixmap, displayname);
}

void SourceManager::takeScreenshot()
{
	QPixmap pixmap = QPixmap::grabWindow(QApplication::desktop()->winId());
	if(pixmap.isNull()){
		QMessageBox::critical(MAIN, _("Screenshot Error"),  _("Failed to take screenshot."));
		return;
	}
	++m_screenshotCount;
	QString displayname = _("Screenshot %1").arg(m_pasteCount);
	savePixmap(pixmap, displayname);
}

void SourceManager::savePixmap(const QPixmap& pixmap, const QString& displayname)
{
	MAIN->pushState(MainWindow::State::Busy, _("Saving image..."));
	QString filename;
	bool success = true;
	QTemporaryFile tmpfile(QDir::temp().absoluteFilePath("gimagereader_XXXXXX.png"));
	if(!tmpfile.open()){
		success = false;
	}else{
		tmpfile.setAutoRemove(false);
		filename = tmpfile.fileName();
		success = pixmap.save(filename);
	}
	MAIN->popState();
	if(!success){
		QMessageBox::critical(MAIN, _("Cannot Write File"),  _("Could not write to %1.").arg(filename));
	}else{
		QListWidgetItem* item = new QListWidgetItem(displayname, ui.listWidgetSources);
		item->setToolTip(filename);
		Source* source = new Source(filename, displayname, true);
		item->setData(Qt::UserRole, QVariant::fromValue(source));
		m_fsWatcher.addPath(filename);
		ui.listWidgetSources->setCurrentItem(item);
	}
}

void SourceManager::removeSource(bool deleteFile)
{
	Source* source = getSelectedSource();
	if(!source){
		return;
	}
	if(deleteFile && QMessageBox::Yes != QMessageBox::question(MAIN, _("Delete File?"), _("The following file will be deleted:\n%1").arg(source->path), QMessageBox::Yes, QMessageBox::No)){
		return;
	}
	m_fsWatcher.removePath(source->path);
	if(deleteFile || source->isTemp){
		QFile(source->path).remove();
	}
	delete source;
	delete ui.listWidgetSources->currentItem();
}


void SourceManager::clearSources()
{
	if(!m_fsWatcher.files().isEmpty()){
		m_fsWatcher.removePaths(m_fsWatcher.files());
	}
	for(int row = 0, nRows = ui.listWidgetSources->count(); row < nRows; ++row){
		Source* source = ui.listWidgetSources->item(row)->data(Qt::UserRole).value<Source*>();
		if(source->isTemp){
			QFile(source->path).remove();
		}
		delete source;
		ui.listWidgetSources->item(row)->setData(Qt::UserRole, QVariant::fromValue((Source*)nullptr));
	}
	ui.listWidgetSources->clear();
}

void SourceManager::currentSourceChanged()
{
	bool enabled = getSelectedSource() != nullptr;
	ui.actionSourceRemove->setEnabled(enabled);
	ui.actionSourceDelete->setEnabled(enabled);
	ui.actionSourceClear->setEnabled(enabled);
	emit sourceChanged(getSelectedSource());
}

void SourceManager::fileChanged(const QString& filename)
{
	if(!QFile(filename).exists()){
		for(int row = 0, nRows = ui.listWidgetSources->count(); row < nRows; ++row){
			QListWidgetItem* item = ui.listWidgetSources->item(row);
			Source* source = item->data(Qt::UserRole).value<Source*>();
			if(source->path == filename){
				QMessageBox::warning(MAIN, _("Missing File"), _("The following file has been deleted or moved:\n%1").arg(filename));
				delete item;
				delete source;
				m_fsWatcher.removePath(filename);
				break;
			}
		}
	}
}
