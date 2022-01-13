/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputTextEdit.hh
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

#ifndef OUTPUTTEXTEDIT_HH
#define OUTPUTTEXTEDIT_HH

#include <QPlainTextEdit>

class OutputTextEdit : public QPlainTextEdit {
	Q_OBJECT
public:
	explicit OutputTextEdit(QWidget* parent = 0);
	~OutputTextEdit();

	QTextCursor regionBounds() const;
	bool findReplace(bool backwards, bool replace, bool matchCase, const QString& searchstr, const QString& replacestr);
	bool replaceAll(const QString& searchstr, const QString& replacestr, bool matchCase);
	void setFilename(const QString& filename) { m_filename = filename; }
	const QString& filename() const { return m_filename; }

public slots:
	void setDrawWhitespace(bool drawWhitespace);

protected:
	void paintEvent(QPaintEvent* e) override;

private:
	class WhitespaceHighlighter;
	WhitespaceHighlighter* m_wsHighlighter = nullptr;
	bool m_drawWhitespace = false;
	QTextCursor m_regionCursor;
	bool m_entireRegion;
	QString m_filename;

private slots:
	void saveRegionBounds();
};

#endif // OUTPUTTEXTEDIT_HH
