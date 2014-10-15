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

#ifndef RECOGNIZER_HPP
#define RECOGNIZER_HPP

#include <QToolButton>

#include "Config.hh"

namespace tesseract { class TessBaseAPI; }

class Recognizer : public QToolButton
{
	Q_OBJECT
public:
	enum class OutputDestination { Buffer, Clipboard };

	explicit Recognizer(QWidget *parent = 0);
	const Config::Lang& getSelectedLanguage() const{ return m_curLang; }

//	QSize sizeHint() const{ return layout()->sizeHint(); }
//	QSize minimumSizeHint() const{ return layout()->minimumSize(); }

public slots:
	void updateLanguagesMenu();
	void setRecognizeMode(bool haveSelection);
	bool recognizeImage(const QImage& img, OutputDestination dest);

signals:
	void languageChanged(const Config::Lang& lang);

private:
	enum class PageSelection { Prompt, Current, Multiple };
	enum class PageArea { EntirePage, Autodetect };

	QString m_modeLabel;
	QString m_langLabel;
	QMenu* m_languagesMenu;
	QMenu* m_pagesMenu;
	QDialog* m_pagesDialog;
	QLineEdit* m_pagesLineEdit;
	QComboBox* m_pageAreaComboBox;

	QActionGroup* m_langMenuRadioGroup = nullptr;
	QActionGroup* m_langMenuCheckGroup = nullptr;
	QAction* m_multilingualAction = nullptr;
	Config::Lang m_curLang;

	bool initTesseract(tesseract::TessBaseAPI& tess, const char* language = nullptr) const;
	QList<int> selectPages(bool& autodetectLayout);
	void recognize(const QList<int>& pages, bool autodetectLayout = false);

private slots:
	void clearLineEditPageRangeStyle();
	void recognizeButtonClicked();
	void recognizeCurrentPage();
	void recognizeMultiplePages();
	void setLanguage();
	void setMultiLanguage();
	bool setPage(int page, bool autodetectLayout);
	void addText(const QString& text, bool insertText);
};

#endif // RECOGNIZER_HPP
