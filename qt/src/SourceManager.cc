/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.cc
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

#include <memory>
#include <stdexcept>

#include <QClipboard>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QInputDialog>
#include <QMessageBox>
#include <QString>
#include <QTemporaryFile>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <poppler-qt4.h>
#else
#include <poppler-qt5.h>
#endif

#include "ConfigSettings.hh"
#include "FileDialogs.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"

Q_DECLARE_METATYPE(Source*)

// Only to silence "QVariant::save: unable to save type 'Source*'" warning
QDataStream& operator<<(QDataStream& ds, const Source*&) {
	return ds;
}
QDataStream& operator>>(QDataStream& ds, Source*&) {
	return ds;
}


SourceManager::SourceManager(const UI_MainWindow& _ui)
	: ui(_ui) {
	m_recentMenu = new QMenu(MAIN);
	ui.actionSourceRecent->setMenu(m_recentMenu);

	ui.listWidgetSources->setAcceptDrops(true);
	ui.listWidgetSources->installEventFilter(this);

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

	ADD_SETTING(VarSetting<QStringList>("recentitems"));

	qRegisterMetaType<Source*>("Source*");
	qRegisterMetaTypeStreamOperators<Source*>("Source*");
}

SourceManager::~SourceManager() {
	clearSources();
}

int SourceManager::addSources(const QStringList& files) {
	QString failed;
	QListWidgetItem* item = nullptr;
	QStringList recentItems = ConfigSettings::get<VarSetting<QStringList>>("recentitems")->getValue();
	int added = 0;
	QStringList filesWithText;
	for(const QString& filename : files) {
		if(!QFile(filename).exists()) {
			failed += "\n\t" + filename;
			continue;
		}
		item = ui.listWidgetSources->currentItem();
		bool contains = item ? item->toolTip() == filename : false;
		for(int row = 0, nRows = ui.listWidgetSources->count(); !contains && row < nRows; ++row) {
			if(ui.listWidgetSources->item(row)->toolTip() == filename) {
				item = ui.listWidgetSources->item(row);
				contains = true;
			}
		}
		if(contains) {
			++added;
			continue;
		}
		QByteArray password;
		if(!querySourcePassword(filename, password)) {
			continue;
		}

		// Check text layer.
		if(!checkTextLayer(filename)) {
			filesWithText.push_back(filename);
		}

		Source* source = new Source(filename, QFileInfo(filename).fileName(), password);
		item = new QListWidgetItem(QFileInfo(filename).fileName(), ui.listWidgetSources);
		item->setToolTip(filename);
		item->setData(Qt::UserRole, QVariant::fromValue(source));
		m_fsWatcher.addPath(filename);
		recentItems.removeAll(filename);
		recentItems.prepend(filename);
		++added;
	}
	// Show files with text here:
	if(!filesWithText.empty()) {
		QString messageFiles = filesWithText.join('\n');
		QMessageBox::information(MAIN->getInstance(), _("Searchable PDF"), _("These PDF files already have text inside:\n") + messageFiles);
	}
	ConfigSettings::get<VarSetting<QStringList>>("recentitems")->setValue(recentItems);
	ui.listWidgetSources->blockSignals(true);
	ui.listWidgetSources->clearSelection();
	ui.listWidgetSources->blockSignals(false);
	ui.listWidgetSources->setCurrentItem(item);
	if(!failed.isEmpty()) {
		QMessageBox::critical(MAIN, _("Unable to open files"), _("The following files could not be opened:%1").arg(failed));
	}
	return added;
}

bool SourceManager::querySourcePassword(const QString& filename, QByteArray& password) const {
	bool success = true;
	if(filename.endsWith(".pdf", Qt::CaseInsensitive)) {
		std::unique_ptr<Poppler::Document> document(Poppler::Document::load(filename));
		if(document && document->isLocked()) {
			success = false;
			bool ok = false;
			QString message = QString(_("Enter password for file '%1':")).arg(QFileInfo(filename).fileName());
			QString text;
			while(true) {
				text = QInputDialog::getText(MAIN, _("Protected PDF"), message, QLineEdit::Password, text, &ok);
				if(!ok) {
					break;
				}
				if(!document->unlock(text.toLocal8Bit(), text.toLocal8Bit())) {
					password = text.toLocal8Bit();
					success = true;
					break;
				}
			}
		}
	}
	return success;
}

bool SourceManager::checkTextLayer(const QString& filename) const {
	if(filename.endsWith(".pdf", Qt::CaseInsensitive)) {
		std::unique_ptr<Poppler::Document> document(Poppler::Document::load(filename));
		if(document) {
			const int pagesNbr = document->numPages();

			for (int i = 0; i < pagesNbr; ++i) {
				QString text = document->page(i)->text(QRectF());
				if(!text.isEmpty()) {
					return false;
				}
			}
		}
	}
	return true;
}

QList<Source*> SourceManager::getSelectedSources() const {
	QList<Source*> selectedSources;
	for(const QListWidgetItem* item : ui.listWidgetSources->selectedItems()) {
		selectedSources.append(item->data(Qt::UserRole).value<Source*>());
	}
	return selectedSources;
}

void SourceManager::prepareSourcesMenu() {
	// Build recent menu
	m_recentMenu->clear();
	for(const QString& filename : ConfigSettings::get<VarSetting<QStringList>>("recentitems")->getValue()) {
		if(QFile(filename).exists()) {
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

void SourceManager::openSources() {
	QList<Source*> current = getSelectedSources();
	QString initialFolder;
	if(!current.isEmpty() && !current.front()->isTemp) {
		initialFolder = QFileInfo(current.front()->path).absolutePath();
	}
	QSet<QString> formats;
	for(const QByteArray& format : QImageReader::supportedImageFormats()) {
		formats.insert(QString("*.%1").arg(QString(format).toLower()));
	}
	formats.insert("*.pdf");
	formats.insert("*.djvu");
	QString filter = QString("%1 (%2)").arg(_("Images and PDFs")).arg(QStringList(formats.toList()).join(" "));
	addSources(FileDialogs::openDialog(_("Select Files"), initialFolder, "sourcedir", filter, true));
}

void SourceManager::openRecentItem() {
	const QString& filename = qobject_cast<QAction*>(QObject::sender())->toolTip();
	addSources(QStringList() << filename);
}

void SourceManager::pasteClipboard() {
	QPixmap pixmap = QApplication::clipboard()->pixmap();
	if(pixmap.isNull()) {
		QMessageBox::critical(MAIN, _("Clipboard Error"),  _("Failed to read the clipboard."));
		return;
	}
	++m_pasteCount;
	QString displayname = _("Pasted %1").arg(m_pasteCount);
	savePixmap(pixmap, displayname);
}

void SourceManager::addSourceImage(const QImage& image) {
	++m_pasteCount;
	QString displayname = _("Pasted %1").arg(m_pasteCount);
	savePixmap(QPixmap::fromImage(image), displayname);
}

void SourceManager::takeScreenshot() {
	QPixmap pixmap = QPixmap::grabWindow(QApplication::desktop()->winId());
	if(pixmap.isNull()) {
		QMessageBox::critical(MAIN, _("Screenshot Error"),  _("Failed to take screenshot."));
		return;
	}
	++m_screenshotCount;
	QString displayname = _("Screenshot %1").arg(m_screenshotCount);
	savePixmap(pixmap, displayname);
}

void SourceManager::savePixmap(const QPixmap& pixmap, const QString& displayname) {
	MAIN->pushState(MainWindow::State::Busy, _("Saving image..."));
	QString filename;
	bool success = true;
	QTemporaryFile tmpfile(QDir::temp().absoluteFilePath("gimagereader_XXXXXX.png"));
	if(!tmpfile.open()) {
		success = false;
	} else {
		tmpfile.setAutoRemove(false);
		filename = tmpfile.fileName();
		success = pixmap.save(filename);
	}
	MAIN->popState();
	if(!success) {
		QMessageBox::critical(MAIN, _("Cannot Write File"),  _("Could not write to %1.").arg(filename));
	} else {
		QListWidgetItem* item = new QListWidgetItem(displayname, ui.listWidgetSources);
		item->setToolTip(filename);
		Source* source = new Source(filename, displayname, QByteArray(), true);
		item->setData(Qt::UserRole, QVariant::fromValue(source));
		m_fsWatcher.addPath(filename);
		ui.listWidgetSources->blockSignals(true);
		ui.listWidgetSources->clearSelection();
		ui.listWidgetSources->blockSignals(false);
		ui.listWidgetSources->setCurrentItem(item);
	}
}

void SourceManager::removeSource(bool deleteFile) {
	QString paths;
	for(const QListWidgetItem* item : ui.listWidgetSources->selectedItems()) {
		paths += QString("\n") + item->data(Qt::UserRole).value<Source*>()->path;
	}
	if(paths.isEmpty()) {
		return;
	}
	if(deleteFile && QMessageBox::Yes != QMessageBox::question(MAIN, _("Delete File?"), _("The following files will be deleted:%1").arg(paths), QMessageBox::Yes, QMessageBox::No)) {
		return;
	}
	// Avoid multiple sourceChanged emissions when removing items
	ui.listWidgetSources->blockSignals(true);
	for(const QListWidgetItem* item : ui.listWidgetSources->selectedItems()) {
		Source* source = item->data(Qt::UserRole).value<Source*>();
		m_fsWatcher.removePath(source->path);
		if(deleteFile || source->isTemp) {
			QFile(source->path).remove();
		}
		delete source;
		delete item;
	}
	if(ui.listWidgetSources->selectedItems().isEmpty()) {
		ui.listWidgetSources->selectionModel()->select(ui.listWidgetSources->currentIndex(), QItemSelectionModel::Select);
	}
	ui.listWidgetSources->blockSignals(false);
	emit sourceChanged();
}


void SourceManager::clearSources() {
	if(!m_fsWatcher.files().isEmpty()) {
		m_fsWatcher.removePaths(m_fsWatcher.files());
	}
	for(int row = 0, nRows = ui.listWidgetSources->count(); row < nRows; ++row) {
		Source* source = ui.listWidgetSources->item(row)->data(Qt::UserRole).value<Source*>();
		if(source->isTemp) {
			QFile(source->path).remove();
		}
		delete source;
		ui.listWidgetSources->item(row)->setData(Qt::UserRole, QVariant::fromValue((Source*)nullptr));
	}
	ui.listWidgetSources->clear();
}

void SourceManager::currentSourceChanged() {
	bool enabled = !ui.listWidgetSources->selectedItems().isEmpty();
	ui.actionSourceRemove->setEnabled(enabled);
	ui.actionSourceDelete->setEnabled(enabled);
	ui.actionSourceClear->setEnabled(enabled);
	emit sourceChanged();
}

void SourceManager::fileChanged(const QString& filename) {
	if(!QFile(filename).exists()) {
		for(int row = 0, nRows = ui.listWidgetSources->count(); row < nRows; ++row) {
			QListWidgetItem* item = ui.listWidgetSources->item(row);
			Source* source = item->data(Qt::UserRole).value<Source*>();
			if(source->path == filename) {
				QMessageBox::warning(MAIN, _("Missing File"), _("The following file has been deleted or moved:\n%1").arg(filename));
				delete item;
				delete source;
				m_fsWatcher.removePath(filename);
				break;
			}
		}
	}
}

bool SourceManager::eventFilter(QObject *object, QEvent *event) {
	if(event->type() == QEvent::DragEnter) {
		QDragEnterEvent* dragEnterEvent = static_cast<QDragEnterEvent*>(event);
		if(Utils::handleSourceDragEvent(dragEnterEvent->mimeData())) {
			dragEnterEvent->acceptProposedAction();
		}
		return true;
	} else if(event->type() == QEvent::Drop) {
		QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
		Utils::handleSourceDropEvent(dropEvent->mimeData());
		return true;
	}
	return QObject::eventFilter(object, event);
}
