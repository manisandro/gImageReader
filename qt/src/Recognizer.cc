/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.hh
 * Copyright (C) 2013-2022 Sandro Mani <manisandro@gmail.com>
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
#include <QDir>
#include <QFileInfo>
#include <QtSpell.hpp>
#include <algorithm>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#include <tesseract/ocrclass.h>
#undef USE_STD_NAMESPACE
#include <QMouseEvent>

#ifdef Q_OS_WIN
#include <fcntl.h>
#define pipe(fds) _pipe(fds, 5000, _O_BINARY)
#endif

#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "MainWindow.hh"
#include "OutputEditor.hh"
#include "RecognitionMenu.hh"
#include "Recognizer.hh"
#include "Utils.hh"

class Recognizer::ProgressMonitor : public MainWindow::ProgressMonitor {
public:
#if TESSERACT_MAJOR_VERSION < 5
	ETEXT_DESC desc;
#else
	tesseract::ETEXT_DESC desc;
#endif

	ProgressMonitor(int nPages) : MainWindow::ProgressMonitor(nPages) {
		desc.progress = 0;
		desc.cancel = cancelCallback;
		desc.cancel_this = this;
	}
	int getProgress() const override {
		QMutexLocker locker(&mMutex);
		return 100.0 * ((mProgress + desc.progress / 100.0) / mTotal);
	}
	static bool cancelCallback(void* instance, int /*words*/) {
		ProgressMonitor* monitor = reinterpret_cast<ProgressMonitor*>(instance);
		QMutexLocker locker(&monitor->mMutex);
		return monitor->mCancelled;
	}
};


Recognizer::Recognizer(const UI_MainWindow& _ui) :
	ui(_ui) {
	QAction* currentPageAction = new QAction(_("Current Page"), this);
	currentPageAction->setData(static_cast<int>(PageSelection::Current));

	QAction* multiplePagesAction = new QAction(_("Multiple Pages..."), this);
	multiplePagesAction->setData(static_cast<int>(PageSelection::Multiple));

	m_actionBatchMode = new QAction(_("Batch mode..."), this);
	m_actionBatchMode->setData(static_cast<int>(PageSelection::Batch));

	m_menuPages = new QMenu(ui.toolButtonRecognize);
	m_menuPages->addAction(currentPageAction);
	m_menuPages->addAction(multiplePagesAction);
	m_menuPages->addAction(m_actionBatchMode);

	m_pagesDialog = new QDialog(MAIN);
	m_pagesDialogUi.setupUi(m_pagesDialog);

	m_batchDialog = new QDialog(MAIN);
	m_batchDialogUi.setupUi(m_batchDialog);
	m_batchDialogUi.comboBoxExisting->addItem(_("Overwrite existing output"), BatchOverwriteOutput);
	m_batchDialogUi.comboBoxExisting->addItem(_("Skip processing source"), BatchSkipSource);

	ui.toolButtonLanguages->setMenu(MAIN->getRecognitionMenu());

	connect(ui.toolButtonRecognize, &QToolButton::clicked, this, &Recognizer::recognizeButtonClicked);
	connect(currentPageAction, &QAction::triggered, this, &Recognizer::recognizeCurrentPage);
	connect(multiplePagesAction, &QAction::triggered, this, &Recognizer::recognizeMultiplePages);
	connect(m_actionBatchMode, &QAction::triggered, this, &Recognizer::recognizeBatch);
	connect(m_pagesDialogUi.lineEditPageRange, &QLineEdit::textChanged, this, &Recognizer::clearLineEditPageRangeStyle);
	connect(MAIN->getRecognitionMenu(), &RecognitionMenu::languageChanged, this, &Recognizer::recognitionLanguageChanged);


	ADD_SETTING(ComboSetting("ocrregionstrategy", m_pagesDialogUi.comboBoxRecognitionArea, 0));
	ADD_SETTING(SwitchSetting("ocraddsourcefilename", m_pagesDialogUi.checkBoxPrependFilename));
	ADD_SETTING(SwitchSetting("ocraddsourcepage", m_pagesDialogUi.checkBoxPrependPage));
}

void Recognizer::setRecognizeMode(const QString& mode) {
	m_modeLabel = mode;
	ui.toolButtonRecognize->setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
}

void Recognizer::recognitionLanguageChanged(const Config::Lang& lang) {
	if(!lang.code.isEmpty()) {
		m_langLabel = QString("%1 (%2)").arg(lang.name, lang.code);
	} else if(lang.name == "Multilingual") {
		m_langLabel = QString("%1").arg(lang.prefix);
	} else {
		m_langLabel = QString("%1").arg(lang.name);
	}
	ui.toolButtonRecognize->setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
}

void Recognizer::clearLineEditPageRangeStyle() {
	qobject_cast<QLineEdit*>(QObject::sender())->setStyleSheet("");
}

QList<int> Recognizer::selectPages(bool& autodetectLayout) {
	int nPages = MAIN->getDisplayer()->getNPages();

	m_pagesDialogUi.lineEditPageRange->setText(QString("1-%1").arg(nPages));
	m_pagesDialogUi.lineEditPageRange->setFocus();
	m_pagesDialogUi.labelRecognitionArea->setVisible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	m_pagesDialogUi.comboBoxRecognitionArea->setVisible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	m_pagesDialogUi.groupBoxPrepend->setVisible(MAIN->getDisplayer()->allowAutodetectOCRAreas());

	m_pagesDialogUi.comboBoxRecognitionArea->setItemText(0, MAIN->getDisplayer()->hasMultipleOCRAreas() ? _("Current selection") : _("Entire page"));

	QRegularExpression validateRegEx("^[\\d,\\-\\s]+$");
	QRegularExpressionMatch match;
	QList<int> pages;
	while(m_pagesDialog->exec() == QDialog::Accepted) {
		pages.clear();
		QString text = m_pagesDialogUi.lineEditPageRange->text();
		if((match = validateRegEx.match(text)).hasMatch()) {
			text.replace(QRegularExpression("\\s+"), "");
			for(const QString& block : text.split(',', Qt::SkipEmptyParts)) {
				QStringList ranges = block.split('-', Qt::SkipEmptyParts);
				if(ranges.size() == 1) {
					int page = ranges[0].toInt();
					if(page > 0 && page <= nPages) {
						pages.append(page);
					}
				} else if(ranges.size() == 2) {
					int start = std::max(1, ranges[0].toInt());
					int end = std::min(nPages, ranges[1].toInt());
					for(int page = start; page <= end; ++page) {
						pages.append(page);
					}
				} else {
					pages.clear();
					break;
				}
			}
		}
		if(pages.empty()) {
			m_pagesDialogUi.lineEditPageRange->setStyleSheet("background: #FF7777; color: #FFFFFF;");
		} else {
			break;
		}
	}
	std::sort(pages.begin(), pages.end());
	autodetectLayout = m_pagesDialogUi.comboBoxRecognitionArea->isVisible() ? m_pagesDialogUi.comboBoxRecognitionArea->currentIndex() == 1 : false;
	return pages;
}

void Recognizer::recognizeButtonClicked() {
	int nPages = MAIN->getDisplayer()->getNPages();
	if(nPages == 1) {
		recognize({MAIN->getDisplayer()->getCurrentPage()});
	} else {
		m_actionBatchMode->setVisible(MAIN->getDisplayer()->getNSources() > 1);
		ui.toolButtonRecognize->setCheckable(true);
		ui.toolButtonRecognize->setChecked(true);
		m_menuPages->popup(ui.toolButtonRecognize->mapToGlobal(QPoint(0, ui.toolButtonRecognize->height())));
		ui.toolButtonRecognize->setChecked(false);
		ui.toolButtonRecognize->setCheckable(false);
	}
}

void Recognizer::recognizeCurrentPage() {
	recognize({MAIN->getDisplayer()->getCurrentPage()});
}

void Recognizer::recognizeMultiplePages() {
	bool autodetectLayout = false;
	QList<int> pages = selectPages(autodetectLayout);
	recognize(pages, autodetectLayout);
}

std::unique_ptr<Utils::TesseractHandle> Recognizer::setupTesseract() {
	Config::Lang lang = MAIN->getRecognitionMenu()->getRecognitionLanguage();
	auto tess = std::unique_ptr<Utils::TesseractHandle>(new Utils::TesseractHandle(lang.prefix.toLocal8Bit().constData()));
	if(tess->get()) {
		tess->get()->SetPageSegMode(MAIN->getRecognitionMenu()->getPageSegmentationMode());
		tess->get()->SetVariable("tessedit_char_whitelist", MAIN->getRecognitionMenu()->getCharacterWhitelist().toLocal8Bit());
		tess->get()->SetVariable("tessedit_char_blacklist", MAIN->getRecognitionMenu()->getCharacterBlacklist().toLocal8Bit());
#if TESSERACT_VERSION >= TESSERACT_MAKE_VERSION(5, 0, 0)
		tess->get()->SetVariable("thresholding_method", "1");
#endif
	} else {
		QMessageBox::critical(MAIN, _("Recognition errors occurred"), _("Failed to initialize tesseract"));
	}
	return tess;
}

void Recognizer::recognize(const QList<int>& pages, bool autodetectLayout) {
	bool prependFile = pages.size() > 1 && ConfigSettings::get<SwitchSetting>("ocraddsourcefilename")->getValue();
	bool prependPage = pages.size() > 1 && ConfigSettings::get<SwitchSetting>("ocraddsourcepage")->getValue();
	auto tess = setupTesseract();
	if(!tess->get()) {
		return;
	}
	bool contains = false;
	for(int page : pages) {
		QString source;
		int sourcePage;
		if(MAIN->getDisplayer()->resolvePage(page, source, sourcePage)) {
			if(MAIN->getOutputEditor()->containsSource(source, sourcePage)) {
				contains = true;
				break;
			}
		}
	}
	if(contains) {
		if(QMessageBox::No == QMessageBox::question(MAIN, _("Source already recognized"), _("One or more selected sources were already recognized. Proceed anyway?"))) {
			return;
		}
	}
	QStringList errors;
	OutputEditor::ReadSessionData* readSessionData = MAIN->getOutputEditor()->initRead(*tess->get());
	ProgressMonitor monitor(pages.size());
	MAIN->showProgress(&monitor);
	MAIN->getDisplayer()->setBlockAutoscale(true);
	Utils::busyTask([&] {
		int npages = pages.size();
		int idx = 0;
		QString prevFile;
		for(int page : pages) {
			monitor.desc.progress = 0;
			++idx;
			QMetaObject::invokeMethod(MAIN, "pushState", Qt::QueuedConnection, Q_ARG(MainWindow::State, MainWindow::State::Busy), Q_ARG(QString, _("Recognizing page %1 (%2 of %3)").arg(page).arg(idx).arg(npages)));

			PageData pageData;
			pageData.success = false;
			QMetaObject::invokeMethod(this, "setPage", Qt::BlockingQueuedConnection, Q_RETURN_ARG(PageData, pageData), Q_ARG(int, page), Q_ARG(bool, autodetectLayout));
			if(!pageData.success) {
				errors.append(_("- Page %1: failed to render page").arg(page));
				MAIN->getOutputEditor()->readError(_("\n[Failed to recognize page %1]\n"), readSessionData);
				continue;
			}
			readSessionData->pageInfo = pageData.pageInfo;
			bool firstChunk = true;
			bool newFile = readSessionData->pageInfo.filename != prevFile;
			prevFile = readSessionData->pageInfo.filename;
			for(const QImage& image : pageData.ocrAreas) {
				readSessionData->prependPage = prependPage && firstChunk;
				readSessionData->prependFile = prependFile && (readSessionData->prependPage || newFile);
				firstChunk = false;
				newFile = false;
				tess->get()->SetImage(image.bits(), image.width(), image.height(), 4, image.bytesPerLine());
				tess->get()->SetSourceResolution(MAIN->getDisplayer()->getCurrentResolution());
				tess->get()->Recognize(&monitor.desc);
				if(!monitor.cancelled()) {
					MAIN->getOutputEditor()->read(*tess->get(), readSessionData);
				}
			}
			QMetaObject::invokeMethod(MAIN, "popState", Qt::QueuedConnection);
			monitor.increaseProgress();
			if(monitor.cancelled()) {
				break;
			}
		}
		return true;
	}, _("Recognizing..."));
	MAIN->getDisplayer()->setBlockAutoscale(false);
	MAIN->hideProgress();
	MAIN->getOutputEditor()->finalizeRead(readSessionData);
	if(!errors.isEmpty()) {
		showRecognitionErrorsDialog(errors);
	}
}

void Recognizer::recognizeImage(const QImage& image, OutputDestination dest) {
	auto tess = setupTesseract();
	if(!tess->get()) {
		return;
	}
	tess->get()->SetImage(image.bits(), image.width(), image.height(), 4, image.bytesPerLine());
	ProgressMonitor monitor(1);
	MAIN->showProgress(&monitor);
	if(dest == OutputDestination::Buffer) {
		OutputEditor::ReadSessionData* readSessionData = MAIN->getOutputEditor()->initRead(*tess->get());
		readSessionData->pageInfo.filename = MAIN->getDisplayer()->getCurrentImage(readSessionData->pageInfo.page);
		readSessionData->pageInfo.angle = MAIN->getDisplayer()->getCurrentAngle();
		readSessionData->pageInfo.resolution = MAIN->getDisplayer()->getCurrentResolution();
		Utils::busyTask([&] {
			tess->get()->Recognize(&monitor.desc);
			if(!monitor.cancelled()) {
				MAIN->getOutputEditor()->read(*tess->get(), readSessionData);
			}
			return true;
		}, _("Recognizing..."));
		MAIN->getOutputEditor()->finalizeRead(readSessionData);
	} else if(dest == OutputDestination::Clipboard) {
		QString output;
		if(Utils::busyTask([&] {
		tess->get()->Recognize(&monitor.desc);
			if(!monitor.cancelled()) {
				char* text = tess->get()->GetUTF8Text();
				output = QString::fromUtf8(text);
				delete[] text;
				return true;
			}
			return false;
		}, _("Recognizing..."))) {
			QApplication::clipboard()->setText(output);
		}
	}
	MAIN->hideProgress();
}

void Recognizer::recognizeBatch() {
	m_batchDialogUi.checkBoxPrependPage->setVisible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	m_batchDialogUi.checkBoxAutolayout->setVisible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	if(m_batchDialog->exec() != QDialog::Accepted) {
		return;
	}
	BatchExistingBehaviour existingBehaviour = static_cast<BatchExistingBehaviour>(m_batchDialogUi.comboBoxExisting->currentData().toInt());
	bool prependPage = MAIN->getDisplayer()->allowAutodetectOCRAreas() && m_batchDialogUi.checkBoxPrependPage->isChecked();
	bool autolayout = MAIN->getDisplayer()->allowAutodetectOCRAreas() && m_batchDialogUi.checkBoxAutolayout->isChecked();
	int nPages = MAIN->getDisplayer()->getNPages();

	auto tess = setupTesseract();
	if(!tess->get()) {
		return;
	}

	QMap<QString, QVariant> batchOptions;
	batchOptions["prependPage"] = prependPage;
	OutputEditor::BatchProcessor* batchProcessor = MAIN->getOutputEditor()->createBatchProcessor(batchOptions);

	QStringList errors;
	ProgressMonitor monitor(nPages);
	MAIN->showProgress(&monitor);
	MAIN->getDisplayer()->setBlockAutoscale(true);
	Utils::busyTask([&] {
		int idx = 0;
		QString currFilename;
		QFile outputFile;
		for(int page = 1; page <= nPages; ++page) {
			monitor.desc.progress = 0;
			++idx;
			QMetaObject::invokeMethod(MAIN, "pushState", Qt::QueuedConnection, Q_ARG(MainWindow::State, MainWindow::State::Busy), Q_ARG(QString, _("Recognizing page %1 (%2 of %3)").arg(page).arg(idx).arg(nPages)));

			PageData pageData;
			pageData.success = false;
			QMetaObject::invokeMethod(this, "setPage", Qt::BlockingQueuedConnection, Q_RETURN_ARG(PageData, pageData), Q_ARG(int, page), Q_ARG(bool, autolayout));
			if(!pageData.success) {
				errors.append(_("- %1:%2: failed to render page").arg(QFileInfo(pageData.pageInfo.filename).fileName()).arg(page));
				continue;
			}
			if(pageData.pageInfo.filename != currFilename) {
				if(outputFile.isOpen()) {
					batchProcessor->writeFooter(&outputFile);
					outputFile.close();
				}
				currFilename = pageData.pageInfo.filename;
				QFileInfo finfo(pageData.pageInfo.filename);
				QString fileName = QDir(finfo.absolutePath()).absoluteFilePath(finfo.baseName() + batchProcessor->fileSuffix());
				bool exists = QFileInfo(fileName).exists();
				if(exists && existingBehaviour == BatchSkipSource) {
					errors.append(_("- %1: output already exists, skipping").arg(finfo.fileName()));
				} else {
					outputFile.setFileName(fileName);
					if(!outputFile.open(QIODevice::WriteOnly)) {
						errors.append(_("- %1: failed to create output file").arg(finfo.fileName()).arg(page));
					} else {
						batchProcessor->writeHeader(&outputFile, tess->get(), pageData.pageInfo);
					}
				}
			}
			if(outputFile.isOpen()) {
				bool firstChunk = true;
				for(const QImage& image : pageData.ocrAreas) {
					tess->get()->SetImage(image.bits(), image.width(), image.height(), 4, image.bytesPerLine());
					tess->get()->SetSourceResolution(MAIN->getDisplayer()->getCurrentResolution());
					tess->get()->Recognize(&monitor.desc);

					if(!monitor.cancelled()) {
						batchProcessor->appendOutput(&outputFile, tess->get(), pageData.pageInfo, firstChunk);
					}
					firstChunk = false;
				}
			}
			QMetaObject::invokeMethod(MAIN, "popState", Qt::QueuedConnection);
			monitor.increaseProgress();
			if(monitor.cancelled()) {
				break;
			}
		}
		if(outputFile.isOpen()) {
			batchProcessor->writeFooter(&outputFile);
			outputFile.close();
		}
		return true;
	}, _("Recognizing..."));
	MAIN->getDisplayer()->setBlockAutoscale(false);
	MAIN->hideProgress();
	if(!errors.isEmpty()) {
		showRecognitionErrorsDialog(errors);
	}
	delete batchProcessor;
}

Recognizer::PageData Recognizer::setPage(int page, bool autodetectLayout) {
	PageData pageData;
	pageData.success = MAIN->getDisplayer()->setup(&page);
	if(pageData.success) {
		if(autodetectLayout) {
			MAIN->getDisplayer()->autodetectOCRAreas();
		}
		pageData.pageInfo.filename = MAIN->getDisplayer()->getCurrentImage(pageData.pageInfo.page);
		pageData.pageInfo.angle = MAIN->getDisplayer()->getCurrentAngle();
		pageData.pageInfo.resolution = MAIN->getDisplayer()->getCurrentResolution();
		pageData.ocrAreas = MAIN->getDisplayer()->getOCRAreas();
	}
	return pageData;
}

void Recognizer::showRecognitionErrorsDialog(const QStringList& errors) {
	Utils::messageBox(MAIN, _("Recognition errors occurred"), _("The following errors occurred:"), errors.join("\n"), QMessageBox::Warning, QDialogButtonBox::Close);
}
