/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * OutputTextEdit.cc
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

#include "OutputTextEdit.hh"

#include <QPainter>
#include <QSyntaxHighlighter>


class OutputTextEdit::WhitespaceHighlighter : public QSyntaxHighlighter
{
public:
	WhitespaceHighlighter(QTextDocument* document)
		: QSyntaxHighlighter(document) {}
private:
	void highlightBlock(const QString &text)
	{
		QTextCharFormat fmt;
		fmt.setForeground(Qt::gray);

		QRegExp expression("\\s");
		int index = text.indexOf(expression);
		while (index >= 0) {
			int length = expression.matchedLength();
			setFormat(index, length, fmt);
			index = text.indexOf(expression, index + length);
		}
	}
};


OutputTextEdit::OutputTextEdit(QWidget *parent)
	: QPlainTextEdit(parent)
{
	m_wsHighlighter = new WhitespaceHighlighter(document());
}

OutputTextEdit::~OutputTextEdit()
{
	delete m_wsHighlighter;
}

void OutputTextEdit::setDrawWhitespace(bool drawWhitespace)
{
	m_drawWhitespace = drawWhitespace;
	QTextOption textOption = document()->defaultTextOption();
	if(drawWhitespace){
		textOption.setFlags(QTextOption::ShowTabsAndSpaces | QTextOption::AddSpaceForLineAndParagraphSeparators);
	}else{
		textOption.setFlags(0);
	}
	document()->setDefaultTextOption(textOption);
}

void OutputTextEdit::paintEvent(QPaintEvent *e)
{
	QPlainTextEdit::paintEvent(e);

	if(!m_drawWhitespace){
		return;
	}

	QPainter painter(viewport());
	painter.setPen(Qt::gray);
	QChar visualArrow((ushort)0x21b5);

	QPointF offset = contentOffset();

	QTextBlock block = firstVisibleBlock();
	qreal top = blockBoundingGeometry(block).translated(offset).top();
	qreal bottom = top + blockBoundingRect(block).height();

	// block.next().isValid(): don't draw line break on last block
	while(block.isValid() && block.next().isValid() && top <= e->rect().bottom()){
		if(block.isVisible() && bottom >= e->rect().top()){
			QTextLayout *layout = block.layout();
			// Draw hard line breaks (i.e. those not due to word wrapping)
			QTextLine line = layout->lineAt(layout->lineCount() - 1);
			QRectF lineRect = line.naturalTextRect().translated(offset.x(), top);
			painter.drawText(QPointF(lineRect.right(), lineRect.top() + line.ascent()), visualArrow);
		}
		block = block.next();
		top = bottom;
		bottom = top + blockBoundingRect(block).height();
	}
}
