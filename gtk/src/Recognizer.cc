/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.cc
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

#include "ConfigSettings.hh"
#include "Displayer.hh"
#include "MainWindow.hh"
#include "OutputEditor.hh"
#include "RecognitionMenu.hh"
#include "Recognizer.hh"
#include "Utils.hh"

#include <gtkspellmm.h>
#include <csignal>
#include <cstring>
#include <fstream>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#if TESSERACT_MAJOR_VERSION < 5
#include <tesseract/genericvector.h>
#endif
#include <tesseract/ocrclass.h>
#undef USE_STD_NAMESPACE
#include <unistd.h>
#include <setjmp.h>

#ifdef G_OS_WIN32
#include <fcntl.h>
#define pipe(fds) _pipe(fds, 5000, _O_BINARY)
#endif


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
		std::lock_guard<std::mutex> lock(m_mutex);
		return 100.0 * ((m_progress + desc.progress / 100.0) / m_total);
	}
	static bool cancelCallback(void* instance, int /*words*/) {
		ProgressMonitor* monitor = reinterpret_cast<ProgressMonitor*>(instance);
		std::lock_guard<std::mutex> lock(monitor->m_mutex);
		return monitor->m_cancelled;
	}
};

Recognizer::Recognizer(const Ui::MainWindow& _ui)
	: ui(_ui) {

	ui.menubuttonLanguages->set_menu(*MAIN->getRecognitionMenu());
	ui.comboBoxExisting->append("overwrite", _("Overwrite existing output"));
	ui.comboBoxExisting->append("skip", _("Skip processing source"));
	ui.comboBoxExisting->set_active(0);

	CONNECT(ui.buttonRecognize, clicked, [this] { recognizeButtonClicked(); });
	CONNECT(ui.menuitemRecognizeCurrent, activate, [this] { recognizeCurrentPage(); });
	CONNECT(ui.menuitemRecognizeMultiple, activate, [this] { recognizeMultiplePages(); });
	CONNECT(ui.menuitemRecognizeBatch, activate, [this] { recognizeBatch(); });
	CONNECT(ui.entryPageRange, focus_in_event, [this](GdkEventFocus*) {
		Utils::clear_error_state(ui.entryPageRange);
		return false;
	});
	CONNECT(MAIN->getRecognitionMenu(), languageChanged, [this](const Config::Lang & lang) { recognitionLanguageChanged(lang); });

	ADD_SETTING(ComboSetting("ocrregionstrategy", ui.comboPageRangeRegions));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("ocraddsourcefilename", ui.checkPageRangePrependFile));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("ocraddsourcepage", ui.checkPageRangePrependPage));
}

void Recognizer::setRecognizeMode(const Glib::ustring& mode) {
	ui.labelRecognizeMode->set_markup(Glib::ustring::compose("<small>%1</small>", mode));
}

void Recognizer::recognitionLanguageChanged(const Config::Lang& lang) {
	Glib::ustring langLabel;
	if(!lang.code.empty()) {
		langLabel = Glib::ustring::compose("%1 (%2)", lang.name, lang.code);
	} else if(lang.name == "Multilingual") {
		langLabel = lang.prefix;
	} else {
		langLabel = lang.name;
	}
	ui.labelRecognizeLang->set_markup(Glib::ustring::compose("<small>%1</small>", langLabel));
}

std::vector<int> Recognizer::selectPages(bool& autodetectLayout) {
	int nPages = MAIN->getDisplayer()->getNPages();

	ui.entryPageRange->set_text(Glib::ustring::compose("1-%1", nPages));
	ui.entryPageRange->grab_focus();
	ui.labelPageRangeRegions->set_visible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	ui.comboPageRangeRegions->set_visible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	ui.boxPageRangePrepend->set_visible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	Glib::RefPtr<Gtk::ListStore> store = Glib::RefPtr<Gtk::ListStore>::cast_static(ui.comboPageRangeRegions->get_model());
	int col = ui.comboPageRangeRegions->get_entry_text_column();
	store->children()[0]->set_value<Glib::ustring>(col, MAIN->getDisplayer()->hasMultipleOCRAreas() ? _("Current selection") : _("Entire page"));

	static Glib::RefPtr<Glib::Regex> validateRegEx = Glib::Regex::create("^[\\d,\\-\\s]+$");
	std::vector<int> pages;
	while(ui.dialogPageRange->run() == Gtk::RESPONSE_OK) {
		pages.clear();
		Glib::ustring text = ui.entryPageRange->get_text();
		if(validateRegEx->match(text)) {
			text = Glib::Regex::create("\\s+")->replace(text, 0, "", static_cast<Glib::RegexMatchFlags>(0));
			std::vector<Glib::ustring> blocks = Utils::string_split(text, ',', false);
			for(const Glib::ustring& block : blocks) {
				std::vector<Glib::ustring> ranges = Utils::string_split(block, '-', false);
				if(ranges.size() == 1) {
					int page = Utils::parseInt(ranges[0]);
					if(page > 0 && page <= nPages) {
						pages.push_back(page);
					}
				} else if(ranges.size() == 2) {
					int start = std::max(1, Utils::parseInt(ranges[0]));
					int end = std::min(nPages, Utils::parseInt(ranges[1]));
					for(int page = start; page <= end; ++page) {
						pages.push_back(page);
					}
				} else {
					pages.clear();
					break;
				}
			}
		}
		if(pages.empty()) {
			Utils::set_error_state(ui.entryPageRange);
		} else {
			break;
		}
	}
	autodetectLayout = ui.comboPageRangeRegions->get_visible() ? ui.comboPageRangeRegions->get_active_row_number() == 1 : false;
	ui.dialogPageRange->hide();
	std::sort(pages.begin(), pages.end());
	return pages;
}

void Recognizer::recognizeButtonClicked() {
	int nPages = MAIN->getDisplayer()->getNPages();
	if(nPages == 1) {
		recognize({MAIN->getDisplayer()->getCurrentPage()});
	} else {
		ui.menuitemRecognizeBatch->set_visible(MAIN->getDisplayer()->getNSources() > 1);
		auto positioner = sigc::bind(sigc::ptr_fun(Utils::popup_positioner), ui.buttonRecognize, ui.menuRecognizePages, false, true);
		ui.menuRecognizePages->popup(positioner, 0, gtk_get_current_event_time());
	}
}

void Recognizer::recognizeCurrentPage() {
	recognize({MAIN->getDisplayer()->getCurrentPage()});
}

void Recognizer::recognizeMultiplePages() {
	bool autodetectLayout = false;
	std::vector<int> pages = selectPages(autodetectLayout);
	recognize(pages, autodetectLayout);
}

std::unique_ptr<tesseract::TessBaseAPI> Recognizer::setupTesseract() {
	Config::Lang lang = MAIN->getRecognitionMenu()->getRecognitionLanguage();
	auto tess = Utils::initTesseract(lang.prefix.c_str());
	if(tess) {
		tess->SetPageSegMode(MAIN->getRecognitionMenu()->getPageSegmentationMode());
		tess->SetVariable("tessedit_char_whitelist", MAIN->getRecognitionMenu()->getCharacterWhitelist().c_str());
		tess->SetVariable("tessedit_char_blacklist", MAIN->getRecognitionMenu()->getCharacterBlacklist().c_str());
#if TESSERACT_VERSION >= TESSERACT_MAKE_VERSION(5, 0, 0)
		tess->SetVariable("thresholding_method", "1");
#endif
	} else {
		Utils::messageBox(Gtk::MESSAGE_ERROR, _("Recognition errors occurred"), _("Failed to initialize tesseract"));
	}
	return tess;
}

void Recognizer::recognize(const std::vector<int>& pages, bool autodetectLayout) {
	bool prependFile = pages.size() > 1 && ConfigSettings::get<SwitchSetting>("ocraddsourcefilename")->getValue();
	bool prependPage = pages.size() > 1 && ConfigSettings::get<SwitchSetting>("ocraddsourcepage")->getValue();
	auto tess = setupTesseract();
	if(!tess) {
		return;
	}
	bool contains = false;
	for(int page : pages) {
		std::string source;
		int sourcePage;
		if(MAIN->getDisplayer()->resolvePage(page, source, sourcePage)) {
			if(MAIN->getOutputEditor()->containsSource(source, sourcePage)) {
				contains = true;
				break;
			}
		}
	}
	if(contains) {
		if(Utils::Button::No == Utils::messageBox(Gtk::MESSAGE_QUESTION, _("Source already recognized"), _("One or more selected sources were already recognized. Proceed anyway?"), "", Utils::Button::Yes | Utils::Button::No)) {
			return;
		}
	}
	std::vector<Glib::ustring> errors;
	tess->SetPageSegMode(MAIN->getRecognitionMenu()->getPageSegmentationMode());
	tess->SetVariable("tessedit_char_whitelist", MAIN->getRecognitionMenu()->getCharacterWhitelist().c_str());
	tess->SetVariable("tessedit_char_blacklist", MAIN->getRecognitionMenu()->getCharacterBlacklist().c_str());
	OutputEditor::ReadSessionData* readSessionData = MAIN->getOutputEditor()->initRead(*tess);
	ProgressMonitor monitor(pages.size());
	MAIN->showProgress(&monitor);
	MAIN->getDisplayer()->setBlockAutoscale(true);
	Utils::busyTask([&] {
		int npages = pages.size();
		int idx = 0;
		std::string prevFile;
		for(int page : pages) {
			monitor.desc.progress = 0;
			++idx;
			Glib::signal_idle().connect_once([ = ] { MAIN->pushState(MainWindow::State::Busy, Glib::ustring::compose(_("Recognizing page %1 (%2 of %3)"), page, idx, npages)); });

			PageData pageData;
			Utils::runInMainThreadBlocking([&] { pageData = setPage(page, autodetectLayout); });
			if(!pageData.success) {
				errors.push_back(Glib::ustring::compose(_("- Page %1: failed to render page"), page));
				MAIN->getOutputEditor()->readError(_("\n[Failed to recognize page %1]\n"), readSessionData);
				continue;
			}
			readSessionData->pageInfo = pageData.pageInfo;
			bool firstChunk = true;
			bool newFile = readSessionData->pageInfo.filename != prevFile;
			prevFile = readSessionData->pageInfo.filename;
			for(const Cairo::RefPtr<Cairo::ImageSurface>& image : pageData.ocrAreas) {
				readSessionData->prependPage = prependPage && firstChunk;
				readSessionData->prependFile = prependFile && (readSessionData->prependPage || newFile);
				firstChunk = false;
				newFile = false;
				tess->SetImage(image->get_data(), image->get_width(), image->get_height(), 4, image->get_stride());
				tess->SetSourceResolution(MAIN->getDisplayer()->getCurrentResolution());
				tess->Recognize(&monitor.desc);
				if(!monitor.cancelled()) {
					MAIN->getOutputEditor()->read(*tess, readSessionData);
				}
			}

			Glib::signal_idle().connect_once([] { MAIN->popState(); });
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
	if(!errors.empty()) {
		showRecognitionErrorsDialog(errors);
	}
}

void Recognizer::recognizeImage(const Cairo::RefPtr<Cairo::ImageSurface>& img, OutputDestination dest) {
	auto tess = setupTesseract();
	if(!tess) {
		return;
	}
	tess->SetImage(img->get_data(), img->get_width(), img->get_height(), 4, 4 * img->get_width());
	ProgressMonitor monitor(1);
	MAIN->showProgress(&monitor);
	if(dest == OutputDestination::Buffer) {
		OutputEditor::ReadSessionData* readSessionData = MAIN->getOutputEditor()->initRead(*tess);
		readSessionData->pageInfo.filename = MAIN->getDisplayer()->getCurrentImage(readSessionData->pageInfo.page);
		readSessionData->pageInfo.angle = MAIN->getDisplayer()->getCurrentAngle();
		readSessionData->pageInfo.resolution = MAIN->getDisplayer()->getCurrentResolution();
		Utils::busyTask([&] {
			tess->Recognize(&monitor.desc);
			if(!monitor.cancelled()) {
				MAIN->getOutputEditor()->read(*tess, readSessionData);
			}
			return true;
		}, _("Recognizing..."));
		MAIN->getOutputEditor()->finalizeRead(readSessionData);
	} else if(dest == OutputDestination::Clipboard) {
		Glib::ustring output;
		if(Utils::busyTask([&] {
		tess->Recognize(&monitor.desc);
			if(!monitor.cancelled()) {
				char* text = tess->GetUTF8Text();
				output = text;
				delete[] text;
				return true;
			}
			return false;
		}, _("Recognizing..."))) {
			Gtk::Clipboard::get()->set_text(output);
		}
	}
	MAIN->hideProgress();
}

void Recognizer::recognizeBatch() {
	ui.checkBoxPrependPage->set_visible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	ui.checkBoxAutolayout->set_visible(MAIN->getDisplayer()->allowAutodetectOCRAreas());
	int response = ui.dialogBatch->run();
	ui.dialogBatch->hide();
	if(response != Gtk::RESPONSE_OK) {
		return;
	}
	Glib::ustring existingBehaviour = ui.comboBoxExisting->get_active_id();
	bool prependPage = MAIN->getDisplayer()->allowAutodetectOCRAreas() && ui.checkBoxPrependPage->get_active();
	bool autolayout = MAIN->getDisplayer()->allowAutodetectOCRAreas() && ui.checkBoxAutolayout->get_active();
	int nPages = MAIN->getDisplayer()->getNPages();

	auto tess = setupTesseract();
	if(!tess) {
		return;
	}

	std::map<Glib::ustring, Glib::ustring> batchOptions;
	batchOptions["prependPage"] = prependPage ? "1" : "0";
	OutputEditor::BatchProcessor* batchProcessor = MAIN->getOutputEditor()->createBatchProcessor(batchOptions);

	std::vector<Glib::ustring> errors;
	ProgressMonitor monitor(nPages);
	MAIN->showProgress(&monitor);
	MAIN->getDisplayer()->setBlockAutoscale(true);
	Utils::busyTask([&] {
		int idx = 0;
		std::string currFilename;
		std::ofstream outputFile;
		for(int page = 1; page <= nPages; ++page) {
			monitor.desc.progress = 0;
			++idx;
			Glib::signal_idle().connect_once([ = ] { MAIN->pushState(MainWindow::State::Busy, Glib::ustring::compose(_("Recognizing page %1 (%2 of %3)"), page, idx, nPages)); });

			PageData pageData;
			pageData.success = false;
			Utils::runInMainThreadBlocking([&] { pageData = setPage(page, autolayout); });
			if(!pageData.success) {
				errors.push_back(Glib::ustring::compose(_("- %1:%2: failed to render page"), Glib::path_get_basename(pageData.pageInfo.filename), page));
				continue;
			}
			if(pageData.pageInfo.filename != currFilename) {
				if(outputFile.is_open()) {
					batchProcessor->writeFooter(outputFile);
					outputFile.close();
				}
				currFilename = pageData.pageInfo.filename;
				std::string fileName = Utils::split_filename(currFilename).first + batchProcessor->fileSuffix();
				bool exists = Glib::file_test(fileName, Glib::FILE_TEST_EXISTS);
				if(exists && existingBehaviour == "skip") {
					errors.push_back(Glib::ustring::compose(_("- %1: output already exists, skipping"), Glib::path_get_basename(fileName)));
				} else {
					outputFile.open(fileName);
					if(!outputFile.is_open()) {
						errors.push_back(Glib::ustring::compose(_("- %1: failed to create output file"), Glib::path_get_basename(fileName), page));
					} else {
						batchProcessor->writeHeader(outputFile, tess.get(), pageData.pageInfo);
					}
				}
			}
			if(outputFile.is_open()) {
				bool firstChunk = true;
				for(const Cairo::RefPtr<Cairo::ImageSurface>& image : pageData.ocrAreas) {
					tess->SetImage(image->get_data(), image->get_width(), image->get_height(), 4, image->get_stride());
					tess->SetSourceResolution(MAIN->getDisplayer()->getCurrentResolution());
					tess->Recognize(&monitor.desc);

					if(!monitor.cancelled()) {
						batchProcessor->appendOutput(outputFile, tess.get(), pageData.pageInfo, firstChunk);
					}
					firstChunk = false;
				}
			}
			Glib::signal_idle().connect_once([] { MAIN->popState(); });
			monitor.increaseProgress();
			if(monitor.cancelled()) {
				break;
			}
		}
		if(outputFile.is_open()) {
			batchProcessor->writeFooter(outputFile);
			outputFile.close();
		}
		return true;
	}, _("Recognizing..."));
	MAIN->getDisplayer()->setBlockAutoscale(false);
	MAIN->hideProgress();
	if(!errors.empty()) {
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

void Recognizer::showRecognitionErrorsDialog(const std::vector<Glib::ustring>& errors) {
	Utils::messageBox(Gtk::MESSAGE_WARNING, _("Recognition errors occurred"), _("The following errors occurred:"), Utils::string_join(errors, "\n"));
}
