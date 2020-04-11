/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputEditorText.hh
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

#ifndef OUTPUTEDITORTEXT_HH
#define OUTPUTEDITORTEXT_HH

#include <QtSpell.hpp>

#include "Config.hh"
#include "MainWindow.hh"
#include "OutputEditor.hh"
#include "Ui_OutputEditorText.hh"

class OutputEditorText : public OutputEditor {
	Q_OBJECT
public:
	class TextBatchProcessor : public BatchProcessor {
	public:
		TextBatchProcessor(bool prependPage) : m_prependPage(prependPage) {}
		QString fileSuffix() const override { return QString(".txt"); }
		void appendOutput(QIODevice* dev, tesseract::TessBaseAPI* tess, const PageInfo& pageInfo, bool firstArea) const override;
	private:
		bool m_prependPage = false;
	};

	OutputEditorText();
	~OutputEditorText();

	QWidget* getUI() override {
		return m_widget;
	}
	ReadSessionData* initRead(tesseract::TessBaseAPI& /*tess*/) override {
		return new TextReadSessionData;
	}
	void read(tesseract::TessBaseAPI& tess, ReadSessionData* data) override;
	void readError(const QString& errorMsg, ReadSessionData* data) override;
	BatchProcessor* createBatchProcessor(const QMap<QString, QVariant>& options) const override { return new TextBatchProcessor(options["prependPage"].toBool()); }
	bool getModified() const override;

public slots:
	bool open(const QString& filename) override;
	void onVisibilityChanged(bool visible) override;
	bool clear(bool hide = true) override;
	bool save(const QString& filename = "") override;
	void setLanguage(const Config::Lang& lang) override;

private:
	struct TextReadSessionData : ReadSessionData {
		bool insertText = false;
	};

	enum class InsertMode { Append, Cursor, Replace };

	QWidget* m_widget;
	UI_OutputEditorText ui;

	InsertMode m_insertMode;
	QtSpell::TextEditChecker m_spell;

private slots:
	void addText(const QString& text, bool insert);
	void filterBuffer();
	void findReplace(const QString& searchstr, const QString& replacestr, bool matchCase, bool backwards, bool replace);
	void replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase);
	void applySubstitutions(const QMap<QString, QString>& substitutions, bool matchCase);
	void setFont();
	void setInsertMode(QAction* action);
};

#endif // OUTPUTEDITORTEXT_HH
