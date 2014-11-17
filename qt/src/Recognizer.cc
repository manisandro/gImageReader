/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.hh
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
#include <QGridLayout>
#include <QIcon>
#include <QLabel>
#include <QMessageBox>
#include <QtSpell.hpp>
#include <tesseract/baseapi.h>
#include <tesseract/strngs.h>
#include <tesseract/genericvector.h>

#include "Displayer.hh"
#include "MainWindow.hh"
#include "OutputManager.hh"
#include "Recognizer.hh"
#include "Utils.hh"
#include "ui_PageRangeDialog.h"

Recognizer::Recognizer(QWidget* parent) :
	QToolButton(parent)
{
	m_languagesMenu = new QMenu(this);

	QAction* currentPageAction = new QAction(_("Current Page"), this);
	currentPageAction->setData(static_cast<int>(PageSelection::Current));

	QAction* multiplePagesAction = new QAction(_("Multiple Pages..."), this);
	multiplePagesAction->setData(static_cast<int>(PageSelection::Multiple));

	m_pagesMenu = new QMenu(this);
	m_pagesMenu->addAction(currentPageAction);
	m_pagesMenu->addAction(multiplePagesAction);

	m_pagesDialog = new QDialog(this);
	Ui::PageRangeDialog uiPageRangeDialog;
	uiPageRangeDialog.setupUi(m_pagesDialog);
	m_pagesLineEdit = uiPageRangeDialog.lineEditPageRange;
	m_pageAreaComboBox = uiPageRangeDialog.comboBoxRecognitionArea;

	m_modeLabel = _("Recognize all");
	m_langLabel = _("English (en_US)");

	setIcon(QIcon::fromTheme("insert-text"));
	setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
	setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	QFont font;
	font.setPointSizeF(font.pointSizeF() * 0.9);
	setFont(font);
	setMenu(m_languagesMenu);
	setPopupMode(QToolButton::MenuButtonPopup);

	connect(this, SIGNAL(clicked()), this, SLOT(recognizeButtonClicked()));
	connect(currentPageAction, SIGNAL(triggered()), this, SLOT(recognizeCurrentPage()));
	connect(multiplePagesAction, SIGNAL(triggered()), this, SLOT(recognizeMultiplePages()));
	connect(uiPageRangeDialog.lineEditPageRange, SIGNAL(textChanged(QString)), this, SLOT(clearLineEditPageRangeStyle()));

	MAIN->getConfig()->addSetting(new VarSetting<QString>("language", "eng:en_EN"));
	MAIN->getConfig()->addSetting(new ComboSetting("ocrregionstrategy", uiPageRangeDialog.comboBoxRecognitionArea, 0));
}

bool Recognizer::initTesseract(tesseract::TessBaseAPI& tess, const char* language) const
{
	QByteArray current = std::setlocale(LC_NUMERIC, NULL);
	std::setlocale(LC_NUMERIC, "C");
	int ret = tess.Init(nullptr, language);
	std::setlocale(LC_NUMERIC, current.constData());
	return ret != -1;
}

void Recognizer::updateLanguagesMenu()
{
	m_languagesMenu->clear();
	delete m_langMenuRadioGroup;
	m_langMenuRadioGroup = new QActionGroup(this);
	delete m_langMenuCheckGroup;
	m_langMenuCheckGroup = new QActionGroup(this);
	m_langMenuCheckGroup->setExclusive(false);
	m_curLang = Config::Lang();
	QAction* radioitem = nullptr;
	QAction* activeitem = nullptr;

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
		setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
		return;
	}

	// Add menu items for languages, with spelling submenu if available
	for(int i = 0; i < availLanguages.size(); ++i){
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
			QAction* item = new QAction(lang.name, m_languagesMenu);
			QMenu* submenu = new QMenu();
			for(const QString& dict : spelldicts){
				Config::Lang itemlang = {lang.prefix, dict, lang.name};
				radioitem = new QAction(QtSpell::Checker::decodeLanguageCode(dict), m_langMenuRadioGroup);
				connect(radioitem, SIGNAL(triggered()), this, SLOT(setLanguage()));
				radioitem->setCheckable(true);
				radioitem->setData(QVariant::fromValue(itemlang));
				if((curlang.prefix == lang.prefix) &&
				   (curlang.code.isEmpty() || curlang.code == dict.left(2) || curlang.code == dict))
				{
					curlang = itemlang;
					radioitem->setChecked(true);
					activeitem = radioitem;
				}
				submenu->addAction(radioitem);
			}
			item->setMenu(submenu);
			m_languagesMenu->addAction(item);
		}else{
			radioitem = new QAction(lang.name, m_langMenuRadioGroup);
			connect(radioitem, SIGNAL(triggered()), this, SLOT(setLanguage()));
			radioitem->setCheckable(true);
			radioitem->setData(QVariant::fromValue(lang));
			if(curlang.prefix == lang.prefix){
				activeitem = radioitem;
			}
			m_languagesMenu->addAction(radioitem);
		}
	}

	// Add multilanguage menu
	m_languagesMenu->addSeparator();
	m_multilingualAction = new QAction(_("Multilingual"), m_langMenuRadioGroup);
	m_multilingualAction->setCheckable(true);
	QMenu* submenu = new QMenu();
	bool isMultilingual = curlang.prefix.contains('+');
	QStringList sellangs = curlang.prefix.split('+', QString::SkipEmptyParts);
	for(int i = 0; i < availLanguages.size(); ++i){
		Config::Lang lang = {availLanguages[i].string(), "", ""};
		if(!MAIN->getConfig()->searchLangSpec(lang)){
			lang.name = lang.prefix;
		}
		QAction* item = new QAction(lang.name, m_langMenuCheckGroup);
		connect(item, SIGNAL(triggered()), this, SLOT(setMultiLanguage()));
		item->setCheckable(true);
		radioitem->setData(QVariant::fromValue(lang.prefix));
		item->setChecked(isMultilingual && sellangs.contains(lang.prefix));
		submenu->addAction(item);
	}
	m_multilingualAction->setMenu(submenu);
	m_languagesMenu->addAction(m_multilingualAction);
	if(isMultilingual){
		activeitem = m_multilingualAction;
		setMultiLanguage();
	}

	// Check active item
	if(activeitem == nullptr){
		activeitem = radioitem;
	}
	activeitem->trigger();
//	activeitem->setChecked(true);
}

void Recognizer::setRecognizeMode(bool haveSelection)
{
	m_modeLabel = haveSelection ? _("Recognize selection") : _("Recognize all");
	setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
}

void Recognizer::setLanguage()
{
	QAction* item = qobject_cast<QAction*>(QObject::sender());
	if(item->isChecked()) {
		Config::Lang lang = item->data().value<Config::Lang>();
		m_langLabel = QString("%1 (%2)").arg(lang.name, lang.code);
		setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
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
	langs = langs.left(langs.length() - 1);
	if(langs.isEmpty()) {
		langs = "eng+";
	}
	m_langLabel = langs.left(langs.length() - 1);
	setText(QString("%1\n%2").arg(m_modeLabel).arg(m_langLabel));
	m_curLang = {langs, "", "Multilingual"};
	MAIN->getConfig()->getSetting<VarSetting<QString>>("language")->setValue(langs + ":");
	emit languageChanged(m_curLang);
}

void Recognizer::clearLineEditPageRangeStyle()
{
	qobject_cast<QLineEdit*>(QObject::sender())->setStyleSheet("");
}

QList<int> Recognizer::selectPages(bool& autodetectLayout)
{
	int nPages = MAIN->getDisplayer()->getNPages();

	m_pagesLineEdit->setText(QString("%1-%2").arg(1).arg(nPages));
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
		recognize(QList<int>() << MAIN->getDisplayer()->getCurrentPage());
	}else{
		setCheckable(true);
		setChecked(true);
		m_pagesMenu->popup(mapToGlobal(QPoint(0, height())));
		setChecked(false);
		setCheckable(false);
	}
}

void Recognizer::recognizeCurrentPage()
{
	recognize(QList<int>() << MAIN->getDisplayer()->getCurrentPage());
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
		Utils::busyTask([&pages, &failed, &tess, autodetectLayout, this]{
			int npages = pages.size();
			int idx = 0;
			bool insertText = false;
			for(int page : pages){
				++idx;
				QMetaObject::invokeMethod(MAIN, "pushState", Qt::QueuedConnection, Q_ARG(MainWindow::State, MainWindow::State::Busy), Q_ARG(QString, _("Recognizing page %1 (%2 of %3)").arg(page).arg(idx).arg(npages)));

				bool success = false;
				QMetaObject::invokeMethod(this, "setPage", Qt::BlockingQueuedConnection, Q_RETURN_ARG(bool, success), Q_ARG(int, page), Q_ARG(bool, autodetectLayout));
				if(!success){
					failed.append(_("\n\tPage %1: failed to render page").arg(page));
					QMetaObject::invokeMethod(this, "addText", Qt::QueuedConnection, Q_ARG(QString, _("\n[Failed to recognize page %1]\n")));
					continue;
				}
				for(const QImage& image : MAIN->getDisplayer()->getSelections()){
					tess.SetImage(image.bits(), image.width(), image.height(), 4, image.bytesPerLine());
					QMetaObject::invokeMethod(this, "addText", Qt::QueuedConnection, Q_ARG(QString, QString::fromUtf8(tess.GetUTF8Text())), Q_ARG(bool, insertText));
					insertText = false;
				}
				QMetaObject::invokeMethod(MAIN, "popState", Qt::QueuedConnection);
			}
			return true;
		}, _("Recognizing..."));
	}
	if(!failed.isEmpty()){
		QMessageBox::critical(this, _("Recognition errors occured"), _("The following errors occured:%1").arg(failed));
	}
}

bool Recognizer::recognizeImage(const QImage& img, OutputDestination dest)
{
	tesseract::TessBaseAPI tess;
	if(!initTesseract(tess, m_curLang.prefix.toLocal8Bit().constData())){
		return false;
	}
	QString text;
	if(!Utils::busyTask([&img,&tess,&text]{
		tess.SetImage(img.bits(), img.width(), img.height(), 4, img.bytesPerLine());
		text = QString::fromUtf8(tess.GetUTF8Text());
		return true;
	}, _("Recognizing...")))
	{
		return false;
	}
	if(dest == OutputDestination::Buffer){
		MAIN->getOutputManager()->addText(text, false);
	}else if(dest == OutputDestination::Clipboard){
		QApplication::clipboard()->setText(text);
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

void Recognizer::addText(const QString& text, bool insertText)
{
	MAIN->getOutputManager()->addText(text, insertText);
}
