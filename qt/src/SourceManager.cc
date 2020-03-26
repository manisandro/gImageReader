/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * SourceManager.cc
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

#include <memory>
#include <QClipboard>
#include <QDesktopWidget>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QInputDialog>
#include <QMessageBox>
#include <QScreen>
#include <QString>
#include <QTemporaryFile>
#include <poppler-qt5.h>

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

	connect(ui.actionSources, &QAction::toggled, ui.dockWidgetSources, &QDockWidget::setVisible);
	connect(ui.toolButtonSourceAdd, &QToolButton::clicked, this, &SourceManager::openSources);
	connect(ui.menuAddSource, &QMenu::aboutToShow, this, &SourceManager::prepareSourcesMenu);
	connect(ui.actionSourceFolder, &QAction::triggered, this, &SourceManager::addFolder);
	connect(ui.actionSourcePaste, &QAction::triggered, this, &SourceManager::pasteClipboard);
	connect(ui.actionSourceScreenshot, &QAction::triggered, this, &SourceManager::takeScreenshot);
	connect(ui.actionSourceRemove, &QAction::triggered, this, &SourceManager::removeSource);
	connect(ui.actionSourceDelete, &QAction::triggered, this, &SourceManager::deleteSource);
	connect(ui.actionSourceClear, &QAction::triggered, this, &SourceManager::clearSources);
	connect(ui.listWidgetSources, &QListWidget::itemSelectionChanged, this, &SourceManager::currentSourceChanged);
	connect(&m_fsWatcher, &QFileSystemWatcher::fileChanged, this, &SourceManager::fileChanged);

	ADD_SETTING(VarSetting<QStringList>("recentitems"));

	qRegisterMetaType<Source*>("Source*");
	qRegisterMetaTypeStreamOperators<Source*>("Source*");
}

SourceManager::~SourceManager() {
	clearSources();
}

int SourceManager::addSources(const QStringList& files, bool suppressTextWarning) {
	QStringList failed;
	QListWidgetItem* item = nullptr;
	QStringList recentItems = ConfigSettings::get<VarSetting<QStringList>>("recentitems")->getValue();
	int added = 0;
	PdfWithTextAction textAction = suppressTextWarning ? PdfWithTextAction::Add : PdfWithTextAction::Ask;

	for(const QString& filename : files) {
		if(!QFile(filename).exists()) {
			failed.append(filename);
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

		Source* source = new Source(filename, QFileInfo(filename).fileName());
		if(source->path.endsWith(".pdf", Qt::CaseInsensitive) && !checkPdfSource(source, textAction, failed)) {
			delete source;
			continue;
		}

		item = new QListWidgetItem(QFileInfo(filename).fileName(), ui.listWidgetSources);
		item->setToolTip(filename);
		item->setData(Qt::UserRole, QVariant::fromValue(source));
		m_fsWatcher.addPath(filename);
		recentItems.removeAll(filename);
		recentItems.prepend(filename);
		++added;
	}
	ConfigSettings::get<VarSetting<QStringList>>("recentitems")->setValue(recentItems.mid(0, sMaxNumRecent));
	ui.listWidgetSources->blockSignals(true);
	ui.listWidgetSources->clearSelection();
	ui.listWidgetSources->blockSignals(false);
	ui.listWidgetSources->setCurrentItem(item);
	if(!failed.isEmpty()) {
		QMessageBox::critical(MAIN, _("Unable to open files"), _("The following files could not be opened:\n%1").arg(failed.join("\n")));
	}
	return added;
}

bool SourceManager::checkPdfSource(Source* source, PdfWithTextAction& textAction, QStringList& failed) const {
	std::unique_ptr<Poppler::Document> document(Poppler::Document::load(source->path));
	if(!document) {
		failed.append(source->path);
		return false;
	}

	// Unlock if necessary
	if(document->isLocked()) {
		bool ok = false;
		QString message = QString(_("Enter password for file '%1':")).arg(QFileInfo(source->path).fileName());
		QString text;
		while(true) {
			text = QInputDialog::getText(MAIN, _("Protected PDF"), message, QLineEdit::Password, text, &ok);
			if(!ok) {
				failed.append(source->path);
				return false;
			}
			if(!document->unlock(text.toLocal8Bit(), text.toLocal8Bit())) {
				source->password = text.toLocal8Bit();
				break;
			}
		}
	}

	// Check whether the PDF already contains text
	if(textAction != PdfWithTextAction::Add) {
		bool haveText = false;
		for (int i = 0, n = document->numPages(); i < n; ++i) {
			if(!document->page(i)->text(QRectF()).isEmpty()) {
				haveText = true;
				break;
			}
		}
		if(haveText && textAction == PdfWithTextAction::Skip) {
			return false;
		}
		int response = QMessageBox::question(MAIN->getInstance(), _("PDF with text"), _("The PDF file already contains text:\n%1\nOpen it regardless?").arg(source->path), QMessageBox::Yes | QMessageBox::YesAll | QMessageBox::No | QMessageBox::NoAll);
		if(response == QMessageBox::No) {
			return false;
		} else if(response == QMessageBox::NoAll) {
			textAction = PdfWithTextAction::Skip;
			return false;
		} else if(response == QMessageBox::YesAll) {
			textAction = PdfWithTextAction::Add;
		}
	}

	// Extract document metadata
	source->author = document->author();
	source->creator = document->creator();
	source->keywords = document->keywords();
	source->producer = document->producer();
	source->title = document->title();
	source->subject = document->subject();
	document->getPdfVersion(&source->pdfVersionMajor, &source->pdfVersionMinor);

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
	int count = 0;
	for(const QString& filename : ConfigSettings::get<VarSetting<QStringList>>("recentitems")->getValue()) {
		if(QFile(filename).exists()) {
			QAction* action = new QAction(QFileInfo(filename).fileName(), m_recentMenu);
			action->setToolTip(filename);
			connect(action, &QAction::triggered, this, &SourceManager::openRecentItem);
			m_recentMenu->addAction(action);
			if(++count >= sMaxNumRecent) {
				break;
			}
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

void SourceManager::addFolder() {
	QString dir = QFileDialog::getExistingDirectory(MAIN, tr("Select folder..."), Utils::documentsFolder());
	if(dir.isEmpty()) {
		return;
	}
	QStringList nameFilters;
	QSet<QString> formats;
	for(const QByteArray& format : QImageReader::supportedImageFormats()) {
		formats.insert(QString("*.%1").arg(QString(format).toLower()));
	}
	formats.insert("*.pdf");
	formats.insert("*.djvu");
	nameFilters = formats.toList();

	QDirIterator it(dir, nameFilters, QDir::Files, QDirIterator::Subdirectories);

	QStringList filenames;
	while(it.hasNext()) {
		filenames.append(it.next());
	}
	if(!filenames.isEmpty()) {
		addSources(filenames);
	}
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
	QPixmap pixmap = QGuiApplication::primaryScreen()->grabWindow(QApplication::desktop()->winId());
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

bool SourceManager::eventFilter(QObject* object, QEvent* event) {
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
