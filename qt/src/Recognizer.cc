/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.hh
 * Copyright (C) 2013-2015 Sandro Mani <manisandro@gmail.com>
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
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QtSpell.hpp>
#include <cstring>
#include <tesseract/baseapi.h>
#include <tesseract/strngs.h>
#include <tesseract/genericvector.h>
#include <QMouseEvent>

#include "Displayer.hh"
#include "MainWindow.hh"
#include "OutputEditor.hh"
#include "Recognizer.hh"
#include "Utils.hh"
#include "ui_PageRangeDialog.h"


Recognizer::Recognizer(const UI_MainWindow& _ui) :
	ui(_ui)
{
	QAction* currentPageAction = new QAction(_("Current Page"), this);
	currentPageAction->setData(static_cast<int>(PageSelection::Current));

	QAction* multiplePagesAction = new QAction(_("Multiple Pages..."), this);
	multiplePagesAction->setData(static_cast<int>(PageSelection::Multiple));

	m_menuPages = new QMenu(ui.toolButtonRecognize);
	m_menuPages->addAction(currentPageAction);
	m_menuPages->addAction(multiplePagesAction);

	m_pagesDialog = new QDialog(MAIN);
	Ui::PageRangeDialog uiPageRangeDialog;
	uiPageRangeDialog.setupUi(m_pagesDialog);
	m_pagesLineEdit = uiPageRangeDialog.lineEditPageRange;
	m_pageAreaComboBox = uiPageRangeDialog.comboBoxRecognitionArea;

	m_modeLabel = _("Recognize all");

	ui.toolButtonRecognize->setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
	ui.menuLanguages->installEventFilter(this);

	connect(ui.toolButtonRecognize, SIGNAL(clicked()), this, SLOT(recognizeButtonClicked()));
	connect(currentPageAction, SIGNAL(triggered()), this, SLOT(recognizeCurrentPage()));
	connect(multiplePagesAction, SIGNAL(triggered()), this, SLOT(recognizeMultiplePages()));
	connect(uiPageRangeDialog.lineEditPageRange, SIGNAL(textChanged(QString)), this, SLOT(clearLineEditPageRangeStyle()));

	MAIN->getConfig()->addSetting(new VarSetting<QString>("language", "eng:en_EN"));
	MAIN->getConfig()->addSetting(new ComboSetting("ocrregionstrategy", uiPageRangeDialog.comboBoxRecognitionArea, 0));
	MAIN->getConfig()->addSetting(new VarSetting<bool>("osd", false));
}

bool Recognizer::initTesseract(tesseract::TessBaseAPI& tess, const char* language) const
{
	QByteArray current = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	int ret = tess.Init(nullptr, language);
	setlocale(LC_NUMERIC, current.constData());
	return ret != -1;
}

void Recognizer::updateLanguagesMenu()
{
	ui.menuLanguages->clear();
	delete m_langMenuRadioGroup;
	m_langMenuRadioGroup = new QActionGroup(this);
	delete m_langMenuCheckGroup;
	m_langMenuCheckGroup = new QActionGroup(this);
	m_langMenuCheckGroup->setExclusive(false);
	m_osdAction = nullptr;
	m_menuMultilanguage = nullptr;
	m_curLang = Config::Lang();
	QAction* curitem = nullptr;
	QAction* activeitem = nullptr;
	bool haveOsd = false;

	QStringList parts = MAIN->getConfig()->getSetting<VarSetting<QString>>("language")->getValue().split(":");
	Config::Lang curlang = {parts.empty() ? "eng" : parts[0], parts.size() < 2 ? "" : parts[1], parts.size() < 3 ? "" : parts[2]};

	QList<QString> dicts = QtSpell::Checker::getLanguageList();

	tesseract::TessBaseAPI tess;
	initTesseract(tess);
	GenericVector<STRING> availLanguages;
	tess.GetAvailableLanguagesAsVector(&availLanguages);

	if(availLanguages.empty()){
		QMessageBox::warning(MAIN, _("No languages available"), _("No tesseract languages are available for use. Recognition will not work."));
		m_langLabel = "";
		ui.toolButtonRecognize->setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
		return;
	}

	// Add menu items for languages, with spelling submenu if available
	for(int i = 0; i < availLanguages.size(); ++i){
		if(std::strcmp(availLanguages[i].string(), "osd") == 0){
			haveOsd = true;
			continue;
		}
		Config::Lang lang = {availLanguages[i].string(), QString(), QString()};
		if(!MAIN->getConfig()->searchLangSpec(lang)){
			lang.name = lang.prefix;
		}
		QList<QString> spelldicts;
		if(!lang.code.isEmpty()){
			for(const QString& dict : dicts){
				if(dict.left(2) == lang.code.left(2)){
					spelldicts.append(dict);
				}
			}
			qSort(spelldicts);
		}
		if(!spelldicts.empty()){
			QAction* item = new QAction(lang.name, ui.menuLanguages);
			QMenu* submenu = new QMenu();
			for(const QString& dict : spelldicts){
				Config::Lang itemlang = {lang.prefix, dict, lang.name};
				curitem = new QAction(QtSpell::Checker::decodeLanguageCode(dict), m_langMenuRadioGroup);
				curitem->setCheckable(true);
				curitem->setData(QVariant::fromValue(itemlang));
				connect(curitem, SIGNAL(triggered()), this, SLOT(setLanguage()));
				if((curlang.prefix == lang.prefix) &&
				   (curlang.code.isEmpty() || curlang.code == dict.left(2) || curlang.code == dict))
				{
					curlang = itemlang;
					activeitem = curitem;
				}else if(curlang.prefix == lang.prefix){
					activeitem = curitem;
				}
				submenu->addAction(curitem);
			}
			item->setMenu(submenu);
			ui.menuLanguages->addAction(item);
		}else{
			curitem = new QAction(lang.name, m_langMenuRadioGroup);
			curitem->setCheckable(true);
			curitem->setData(QVariant::fromValue(lang));
			connect(curitem, SIGNAL(triggered()), this, SLOT(setLanguage()));
			if(curlang.prefix == lang.prefix){
				curlang = lang;
				activeitem = curitem;
			}
			ui.menuLanguages->addAction(curitem);
		}
	}

	// Add multilanguage menu
	ui.menuLanguages->addSeparator();
	m_multilingualAction = new QAction(_("Multilingual"), m_langMenuRadioGroup);
	m_multilingualAction->setCheckable(true);
	m_menuMultilanguage = new QMenu();
	bool isMultilingual = curlang.prefix.contains('+');
	QStringList sellangs = curlang.prefix.split('+', QString::SkipEmptyParts);
	for(int i = 0; i < availLanguages.size(); ++i){
		if(std::strcmp(availLanguages[i].string(), "osd") == 0){
			continue;
		}
		Config::Lang lang = {availLanguages[i].string(), "", ""};
		if(!MAIN->getConfig()->searchLangSpec(lang)){
			lang.name = lang.prefix;
		}
		QAction* item = new QAction(lang.name, m_langMenuCheckGroup);
		item->setCheckable(true);
		item->setData(QVariant::fromValue(lang.prefix));
		item->setChecked(isMultilingual && sellangs.contains(lang.prefix));
		connect(item, SIGNAL(triggered()), this, SLOT(setMultiLanguage()));
		m_menuMultilanguage->addAction(item);
	}
	m_menuMultilanguage->installEventFilter(this);
	m_multilingualAction->setMenu(m_menuMultilanguage);
	ui.menuLanguages->addAction(m_multilingualAction);
	if(isMultilingual){
		activeitem = m_multilingualAction;
		setMultiLanguage();
	}else if(activeitem == nullptr){
		activeitem = curitem;
	}
	activeitem->trigger();

	// Add OSD item
	if(haveOsd){
		ui.menuLanguages->addSeparator();
		m_osdAction = new QAction(_("Detect script and orientation"), ui.menuLanguages);
		m_osdAction->setCheckable(true);
		m_osdAction->setChecked(MAIN->getConfig()->getSetting<VarSetting<bool>>("osd")->getValue());
		connect(m_osdAction, SIGNAL(toggled(bool)), this, SLOT(osdToggled(bool)));
		ui.menuLanguages->addAction(m_osdAction);
	}
}

void Recognizer::setLanguage()
{
	QAction* item = qobject_cast<QAction*>(QObject::sender());
	if(item->isChecked()) {
		Config::Lang lang = item->data().value<Config::Lang>();
		if(!lang.code.isEmpty()){
			m_langLabel = QString("%1 (%2)").arg(lang.name, lang.code);
		}else{
			m_langLabel = QString("%1").arg(lang.name);
		}
		ui.toolButtonRecognize->setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
		m_curLang = lang;
		MAIN->getConfig()->getSetting<VarSetting<QString>>("language")->setValue(lang.prefix + ":" + lang.code);
		emit languageChanged(m_curLang);
	}
}

void Recognizer::setMultiLanguage()
{
	m_multilingualAction->setChecked(true);
	QString langs;
	for(QAction* action : m_langMenuCheckGroup->actions()) {
		if(action->isChecked()) {
			langs += action->data().value<QString>() + "+";
		}
	}
	if(langs.isEmpty()) {
		langs = "eng+";
	}
	langs = langs.left(langs.length() - 1);
	m_langLabel = langs;
	ui.toolButtonRecognize->setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
	m_curLang = {langs, "", "Multilingual"};
	MAIN->getConfig()->getSetting<VarSetting<QString>>("language")->setValue(langs + ":");
	emit languageChanged(m_curLang);
}

void Recognizer::setRecognizeMode(bool haveSelection)
{
	m_modeLabel = haveSelection ? _("Recognize selection") : _("Recognize all");
	ui.toolButtonRecognize->setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
}

void Recognizer::clearLineEditPageRangeStyle()
{
	qobject_cast<QLineEdit*>(QObject::sender())->setStyleSheet("");
}

void Recognizer::osdToggled(bool state)
{
	MAIN->getConfig()->getSetting<VarSetting<bool>>("osd")->setValue(state);
}

QList<int> Recognizer::selectPages(bool& autodetectLayout)
{
	int nPages = MAIN->getDisplayer()->getNPages();

	m_pagesLineEdit->setText(QString("1-%1").arg(nPages));
	m_pagesLineEdit->setFocus();

	QList<int> pages;
	if(m_pagesDialog->exec() == QDialog::Accepted){
		QString text = m_pagesLineEdit->text();
		text.replace(QRegExp("\\s+"), "");
		for(const QString& block : text.split(',')){
			QStringList ranges = block.split('-');
			if(ranges.size() == 1){
				int page = ranges[0].toInt();
				if(page > 0 && page <= nPages){
					pages.append(page);
				}
			}else if(ranges.size() == 2){
				int start = qMax(1, ranges[0].toInt());
				int end = qMin(nPages, ranges[1].toInt());
				for(int page = start; page <= end; ++page){
					pages.append(page);
				}
			}else{
				pages.clear();
				break;
			}
		}
		if(pages.empty()){
			m_pagesLineEdit->setStyleSheet("background: #FF7777; color: #FFFFFF;");
		}
	}
	qSort(pages);
	autodetectLayout = m_pageAreaComboBox->currentIndex() == 1;
	return pages;
}

void Recognizer::recognizeButtonClicked()
{
	int nPages = MAIN->getDisplayer()->getNPages();
	if(nPages == 1 || MAIN->getDisplayer()->getHasSelections()){
		recognize({MAIN->getDisplayer()->getCurrentPage()});
	}else{
		ui.toolButtonRecognize->setCheckable(true);
		ui.toolButtonRecognize->setChecked(true);
		m_menuPages->popup(ui.toolButtonRecognize->mapToGlobal(QPoint(0, ui.toolButtonRecognize->height())));
		ui.toolButtonRecognize->setChecked(false);
		ui.toolButtonRecognize->setCheckable(false);
	}
}

void Recognizer::recognizeCurrentPage()
{
	recognize({MAIN->getDisplayer()->getCurrentPage()});
}

void Recognizer::recognizeMultiplePages()
{
	bool autodetectLayout = false;
	QList<int> pages = selectPages(autodetectLayout);
	recognize(pages, autodetectLayout);
}

void Recognizer::recognize(const QList<int> &pages, bool autodetectLayout)
{
	QString failed;
	tesseract::TessBaseAPI tess;
	if(!initTesseract(tess, m_curLang.prefix.toLocal8Bit().constData())){
		failed.append(_("\n\tFailed to initialize tesseract"));
	}else{
		if(MAIN->getConfig()->getSetting<VarSetting<bool>>("osd")->getValue() == true){
			tess.SetPageSegMode(tesseract::PSM_AUTO_OSD);
		}
		OutputEditor::ReadSessionData* readSessionData = MAIN->getOutputEditor()->initRead();
		Utils::busyTask([&]{
			int npages = pages.size();
			int idx = 0;
			for(int page : pages){
				readSessionData->currentPage = page;
				++idx;
				QMetaObject::invokeMethod(MAIN, "pushState", Qt::QueuedConnection, Q_ARG(MainWindow::State, MainWindow::State::Busy), Q_ARG(QString, _("Recognizing page %1 (%2 of %3)").arg(page).arg(idx).arg(npages)));

				bool success = false;
				QMetaObject::invokeMethod(this, "setPage", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, success), Q_ARG(int, page), Q_ARG(bool, autodetectLayout));
				if(!success){
					failed.append(_("\n\tPage %1: failed to render page").arg(page));
					MAIN->getOutputEditor()->readError(_("\n[Failed to recognize page %1]\n"), readSessionData);
					continue;
				}
				for(const QImage& image : MAIN->getDisplayer()->getSelections()){
					tess.SetImage(image.bits(), image.width(), image.height(), 4, image.bytesPerLine());
					MAIN->getOutputEditor()->read(tess, readSessionData);
				}
				QMetaObject::invokeMethod(MAIN, "popState", Qt::QueuedConnection);
			}
			return true;
		}, _("Recognizing..."));
		MAIN->getOutputEditor()->finalizeRead(readSessionData);
	}
	if(!failed.isEmpty()){
		QMessageBox::critical(MAIN, _("Recognition errors occurred"), _("The following errors occurred:%1").arg(failed));
	}
}

bool Recognizer::recognizeImage(const QImage& img, OutputDestination dest)
{
	tesseract::TessBaseAPI tess;
	if(!initTesseract(tess, m_curLang.prefix.toLocal8Bit().constData())){
		QMessageBox::critical(MAIN, _("Recognition errors occurred"), _("Failed to initialize tesseract"));
		return false;
	}
	tess.SetImage(img.bits(), img.width(), img.height(), 4, img.bytesPerLine());
	if(dest == OutputDestination::Buffer){
		OutputEditor::ReadSessionData* readSessionData = MAIN->getOutputEditor()->initRead();
		Utils::busyTask([&]{
			MAIN->getOutputEditor()->read(tess, readSessionData);
			return true;
		}, _("Recognizing..."));
		MAIN->getOutputEditor()->finalizeRead(readSessionData);
	}else if(dest == OutputDestination::Clipboard){
		QString output;
		Utils::busyTask([&]{
			char* text = tess.GetUTF8Text();
			output = QString::fromUtf8(text);
			delete[] text;
			return true;
		}, _("Recognizing..."));
		QApplication::clipboard()->setText(output);
	}
	return true;
}

bool Recognizer::setPage(int page, bool autodetectLayout)
{
	bool success = MAIN->getDisplayer()->setCurrentPage(page);
	if(success && autodetectLayout) {
		MAIN->getDisplayer()->autodetectLayout();
	}
	return success;
}

bool Recognizer::eventFilter(QObject* obj, QEvent* ev)
{
	if(obj == ui.menuLanguages && ev->type() == QEvent::MouseButtonPress) {
		QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(ev);
		if(ui.menuLanguages->actionAt(mouseEvent->pos()) == m_multilingualAction) {
			m_multilingualAction->toggle();
			if(m_multilingualAction->isChecked()) {
				setMultiLanguage();
			}
			return true;
		}
	} else if(obj == m_menuMultilanguage && (ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::MouseButtonRelease)) {
		QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(ev);
		QAction* action = m_menuMultilanguage->actionAt(mouseEvent->pos());
		if(action) {
			if(ev->type() == QEvent::MouseButtonRelease) {
				action->trigger();
			}
			return true;
		}
	}
	return QObject::eventFilter(obj, ev);
}
