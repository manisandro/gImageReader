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
#include "FileTreeModel.hh"
#include "MainWindow.hh"
#include "SourceManager.hh"
#include "Utils.hh"

Source::~Source() {
	if(isTemp) {
		QFile(path).remove();
	}
}

SourceManager::SourceManager(const UI_MainWindow& _ui)
	: ui(_ui) {
	m_recentMenu = new QMenu(MAIN);
	ui.actionSourceRecent->setMenu(m_recentMenu);

	m_fileTreeModel = new FileTreeModel(this);

	ui.treeViewSources->setModel(m_fileTreeModel);
	ui.treeViewSources->setAcceptDrops(true);
	ui.treeViewSources->installEventFilter(this);
	ui.treeViewSources->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui.treeViewSources->header()->setSectionResizeMode(1, QHeaderView::Fixed);
	ui.treeViewSources->header()->resizeSection(1, 16);


	connect(ui.actionSources, &QAction::toggled, ui.dockWidgetSources, &QDockWidget::setVisible);
	connect(ui.toolButtonSourceAdd, &QToolButton::clicked, this, &SourceManager::openSources);
	connect(ui.menuAddSource, &QMenu::aboutToShow, this, &SourceManager::prepareSourcesMenu);
	connect(ui.actionSourceFolder, &QAction::triggered, this, &SourceManager::addFolder);
	connect(ui.actionSourcePaste, &QAction::triggered, this, &SourceManager::pasteClipboard);
	connect(ui.actionSourceScreenshot, &QAction::triggered, this, &SourceManager::takeScreenshot);
	connect(ui.actionSourceRemove, &QAction::triggered, this, &SourceManager::removeSource);
	connect(ui.actionSourceDelete, &QAction::triggered, this, &SourceManager::deleteSource);
	connect(ui.actionSourceClear, &QAction::triggered, this, &SourceManager::clearSources);
	connect(ui.treeViewSources->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SourceManager::currentSourceChanged);
	connect(ui.treeViewSources, &QTreeView::clicked, this, &SourceManager::indexClicked);
	connect(&m_fsWatcher, &QFileSystemWatcher::fileChanged, this, &SourceManager::fileChanged);
	connect(&m_fsWatcher, &QFileSystemWatcher::directoryChanged, this, &SourceManager::directoryChanged);

	ADD_SETTING(VarSetting<QStringList>("recentitems"));
}

SourceManager::~SourceManager() {
	clearSources();
}

int SourceManager::addSources(const QStringList& files, bool suppressTextWarning, const QString& parentDir) {
	QStringList failed;
	QItemSelection sel;
	QModelIndex index;
	QStringList recentItems = ConfigSettings::get<VarSetting<QStringList>>("recentitems")->getValue();
	int added = 0;
	PdfWithTextAction textAction = suppressTextWarning ? PdfWithTextAction::Add : PdfWithTextAction::Ask;

	ui.treeViewSources->selectionModel()->blockSignals(true);
	ui.treeViewSources->setUpdatesEnabled(false);
	for(const QString& filename : files) {
		if(!QFile(filename).exists()) {
			failed.append(filename);
			continue;
		}
		index = m_fileTreeModel->findFile(filename);
		if(index.isValid()) {
			sel.select(index, index);
			++added;
			continue;
		}
		QFileInfo finfo(filename);

		Source* source = new Source(filename, finfo.fileName());
		if(source->path.endsWith(".pdf", Qt::CaseInsensitive) && !checkPdfSource(source, textAction, failed)) {
			delete source;
			continue;
		}

		index = m_fileTreeModel->insertFile(filename, source);
		QString base = finfo.absoluteDir().absoluteFilePath(finfo.baseName());
		if(QFile(base + ".txt").exists() || QFile(base + ".html").exists()) {
			m_fileTreeModel->setFileEditable(index, true);
			m_watchedDirectories[finfo.absolutePath()] += 1;
			m_fsWatcher.addPath(finfo.absolutePath());
		}
		sel.select(index, index);
		m_fsWatcher.addPath(filename);
		recentItems.removeAll(filename);
		recentItems.prepend(filename);
		++added;
	}
	ConfigSettings::get<VarSetting<QStringList>>("recentitems")->setValue(recentItems.mid(0, sMaxNumRecent));
	ui.treeViewSources->selectionModel()->blockSignals(false);
	ui.treeViewSources->setUpdatesEnabled(true);
	if(added > 0) {
		if(parentDir.isEmpty()) {
			ui.treeViewSources->selectionModel()->select(sel, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Select | QItemSelectionModel::Rows);
			QModelIndex parent = index.parent();
			while(parent.isValid()) {
				ui.treeViewSources->expand(parent);
				parent = parent.parent();
			}
		} else if(added > 0) {
			QModelIndex idx = m_fileTreeModel->findFile(parentDir, false);
			if(idx.isValid()) {
				ui.treeViewSources->setCurrentIndex(idx);
				ui.treeViewSources->expand(idx);
			} else {
				// If sources were added and the dir was not found as a child, it means that the dir is the root
				ui.treeViewSources->selectAll();
			}
		}
	}
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
	for(QModelIndex index : ui.treeViewSources->selectionModel()->selectedRows()) {
		Source* source = m_fileTreeModel->fileData<Source*>(index);
		if(source) {
			selectedSources.append(source);
		}
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
	QString filter = QString("%1 (%2)").arg(_("Images and PDFs")).arg(QStringList(formats.values()).join(" "));
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
	nameFilters = formats.values();

	QDirIterator it(dir, nameFilters, QDir::Files, QDirIterator::Subdirectories);

	QStringList filenames;
	while(it.hasNext()) {
		filenames.append(it.next());
	}
	if(!filenames.isEmpty()) {
		addSources(filenames, false, dir);
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
		Source* source = new Source(filename, displayname, QByteArray(), true);
		QModelIndex index = m_fileTreeModel->insertFile(filename, source, displayname);
		m_fsWatcher.addPath(filename);
		QFileInfo finfo(filename);
		m_watchedDirectories[finfo.absolutePath()] += 1;
		m_fsWatcher.addPath(finfo.absolutePath());
		ui.treeViewSources->selectionModel()->blockSignals(true);
		ui.treeViewSources->clearSelection();
		ui.treeViewSources->selectionModel()->blockSignals(false);
		ui.treeViewSources->setCurrentIndex(index);
	}
}

void SourceManager::removeSource(bool deleteFile) {
	QStringList paths;
	for(const QModelIndex& index : ui.treeViewSources->selectionModel()->selectedRows()) {
		Source* source = m_fileTreeModel->fileData<Source*>(index);
		if(source) {
			paths.append(source->path);
		}
	}
	if(paths.isEmpty()) {
		return;
	}
	if(deleteFile && QDialogButtonBox::Yes != Utils::messageBox(MAIN, _("Delete Files"), _("Delete the following files?"), paths.join("\n"), QMessageBox::Question, QDialogButtonBox::Yes | QDialogButtonBox::No)) {
		return;
	}
	// Avoid multiple sourceChanged emissions when removing items
	ui.treeViewSources->selectionModel()->blockSignals(true);
	ui.treeViewSources->setUpdatesEnabled(false);
	while(ui.treeViewSources->selectionModel()->hasSelection()) {
		QModelIndex index = ui.treeViewSources->selectionModel()->selectedRows().front();
		if(deleteFile && m_fileTreeModel->isDir(index)) {
			// Skip directories (they are pruned from the tree automatically if empty), otherwise descendant selected file indices are possibly removed without the files getting deleted
			ui.treeViewSources->selectionModel()->select(index, QItemSelectionModel::Deselect);
		} else {
			Source* source = m_fileTreeModel->fileData<Source*>(index);
			if(source) {
				m_fsWatcher.removePath(source->path);
				QString dir = QFileInfo(source->path).absolutePath();
				m_watchedDirectories[dir] -= 1;
				if(m_watchedDirectories[dir] == 0) {
					m_fsWatcher.removePath(dir);
					m_watchedDirectories.remove(dir);
				}
				if(deleteFile) {
					QFile(source->path).remove();
				}
			}
			m_fileTreeModel->removeIndex(index);
		}
	}
	ui.treeViewSources->selectionModel()->clear();
	ui.treeViewSources->selectionModel()->select(ui.treeViewSources->currentIndex(), QItemSelectionModel::Select);
	ui.treeViewSources->selectionModel()->blockSignals(false);
	ui.treeViewSources->setUpdatesEnabled(true);
	currentSourceChanged(QItemSelection(ui.treeViewSources->currentIndex(), ui.treeViewSources->currentIndex()), QItemSelection());
}


void SourceManager::clearSources() {
	if(!m_fsWatcher.files().isEmpty()) {
		m_fsWatcher.removePaths(m_fsWatcher.files());
	}
	if(!m_fsWatcher.directories().isEmpty()) {
		m_fsWatcher.removePaths(m_fsWatcher.directories());
	}
	m_fileTreeModel->clear();
}

void SourceManager::currentSourceChanged(const QItemSelection& /*selected*/, const QItemSelection& deselected) {
	if(m_inCurrentSourceChanged) {
		return;
	}
	m_inCurrentSourceChanged = true;

	// Recursively deselect deselected items
	QItemSelection deselect;
	for(const QModelIndex& index : deselected.indexes()) {
		selectRecursive(deselect, index);
	}
	ui.treeViewSources->selectionModel()->select(deselect, QItemSelectionModel::Deselect | QItemSelectionModel::Rows);

	// Merge selection of all children of selected items
	QItemSelection selection = ui.treeViewSources->selectionModel()->selection();
	QItemSelection extendedSel;
	for(const QModelIndex& index : selection.indexes()) {
		selectRecursive(extendedSel, index);
	}
	extendedSel.merge(selection, QItemSelectionModel::Select);
	ui.treeViewSources->selectionModel()->select(extendedSel, QItemSelectionModel::Select | QItemSelectionModel::Rows);

	bool enabled = !ui.treeViewSources->selectionModel()->selectedRows().isEmpty();
	ui.actionSourceRemove->setEnabled(enabled);
	ui.actionSourceDelete->setEnabled(enabled);
	ui.actionSourceClear->setEnabled(enabled);

	emit sourceChanged();
	m_inCurrentSourceChanged = false;
}

void SourceManager::indexClicked(const QModelIndex& index) {
	Source* source = m_fileTreeModel->fileData<Source*>(index);
	if(index.column() == 1 && source && m_fileTreeModel->isFileEditable(index)) {
		QFileInfo finfo(source->path);
		QString base = finfo.absoluteDir().absoluteFilePath(finfo.baseName());
		bool hasTxt = QFile(base + ".txt").exists();
		bool hasHtml = QFile(base + ".html").exists();
		if(hasTxt && hasHtml) {
			QMessageBox box(QMessageBox::Question, _("Open output"), _("Both a text and a hOCR output were found. Which one do you want to open?"), QMessageBox::Cancel);
			QAbstractButton* textButton = box.addButton(_("Text"), QMessageBox::AcceptRole);
			QAbstractButton* hocrButton = box.addButton(_("hOCR"), QMessageBox::AcceptRole);
			connect(textButton, &QAbstractButton::clicked, this, [base] { MAIN->openOutput(base + ".txt"); });
			connect(hocrButton, &QAbstractButton::clicked, this, [base] { MAIN->openOutput(base + ".html"); });
			box.exec();
		} else if(hasTxt) {
			MAIN->openOutput(base + ".txt");
		} else if(hasHtml) {
			MAIN->openOutput(base + ".html");
		}
	}
}

void SourceManager::selectRecursive(QItemSelection& parentsel, const QModelIndex& index) {
	int rows = m_fileTreeModel->rowCount(index);
	for(int row = 0; row < rows; ++row) {
		QModelIndex child = m_fileTreeModel->index(row, 0, index);
		selectRecursive(parentsel, child);
	}
	QItemSelection sel(m_fileTreeModel->index(0, 0, index), m_fileTreeModel->index(rows - 1, 0, index));
	parentsel.merge(sel, QItemSelectionModel::Select | QItemSelectionModel::Rows);
}

void SourceManager::fileChanged(const QString& filename) {
	if(!QFile(filename).exists()) {
		QModelIndex index = m_fileTreeModel->findFile(filename);
		if(index.isValid()) {
			QMessageBox::warning(MAIN, _("Missing File"), _("The following file has been deleted or moved:\n%1").arg(filename));
			m_fileTreeModel->removeIndex(index);
		}
	}
	m_fsWatcher.removePath(filename);
	m_watchedDirectories.remove(filename); // In case it was a directory
}

void SourceManager::directoryChanged(const QString& dir) {
	// Update editable status of files in directory
	QModelIndex index = m_fileTreeModel->findFile(dir, false);
	for(int row = 0, n = m_fileTreeModel->rowCount(index); row < n; ++row) {
		QModelIndex child = m_fileTreeModel->index(row, 0, index);
		Source* source = m_fileTreeModel->fileData<Source*>(child);
		if(source) {
			QFileInfo finfo(source->path);
			QString base = finfo.absoluteDir().absoluteFilePath(finfo.baseName());
			m_fileTreeModel->setFileEditable(child, QFile(base + ".txt").exists() || QFile(base + ".html").exists());
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
