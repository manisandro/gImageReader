/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * HOCRTextExporter.cc
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

#include "ConfigSettings.hh"
#include "HOCRDocument.hh"
#include "HOCRTextExporter.hh"
#include "MainWindow.hh"
#include "FileDialogs.hh"
#include "SourceManager.hh"

#include <QDesktopServices>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>

bool HOCRTextExporter::run(const HOCRDocument *hocrdocument, QString &filebasename)
{
	QString suggestion = filebasename;
	if(suggestion.isEmpty()) {
		QList<Source*> sources = MAIN->getSourceManager()->getSelectedSources();
		suggestion = !sources.isEmpty() ? QFileInfo(sources.first()->displayname).baseName() : _("output");
	}

	QString outname = FileDialogs::saveDialog(_("Save Text Output..."), suggestion + ".txt", "outputdir", QString("%1 (*.txt)").arg(_("Text Files")));
	if(outname.isEmpty()) {
		return false;
	}
	filebasename = QFileInfo(outname).completeBaseName();

	QFile outputFile(outname);
	if(!outputFile.open(QIODevice::WriteOnly)) {
		QMessageBox::warning(MAIN, _("Export failed"), _("The text export failed: unable to write output file."));
		return false;
	}

	QString output;
	QTextStream outputStream(&output, QIODevice::WriteOnly);
	for(int i = 0, n = hocrdocument->pageCount(); i < n; ++i) {
		const HOCRPage* page = hocrdocument->page(i);
		if(!page->isEnabled()) {
			continue;
		}
		printItem(outputStream, page);
	}
	outputFile.write(MAIN->getConfig()->useUtf8() ? output.toUtf8() : output.toLocal8Bit());
	outputFile.close();
	bool openAfterExport = ConfigSettings::get<SwitchSetting>("openafterexport")->getValue();
	if(openAfterExport) {
		QDesktopServices::openUrl(QUrl::fromLocalFile(outname));
	}
	return true;
}

void HOCRTextExporter::printItem(QTextStream& outputStream, const HOCRItem* item, bool lastChild)
{
	if(!item->isEnabled()) {
		return;
	}
	QString itemClass = item->itemClass();
	if(itemClass == "ocrx_word") {
		outputStream << item->text();
		if(!lastChild) {
			outputStream << " ";
		}
	} else {
		for(int i = 0, n = item->children().size(); i < n; ++i) {
			printItem(outputStream, item->children()[i], i == n - 1);
		}
	}
	if(itemClass == "ocr_line" || itemClass == "ocr_par") {
		outputStream << "\n";
	}
}
