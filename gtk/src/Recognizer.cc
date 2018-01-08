/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.cc
 * Copyright (C) (\d+)-2018 Sandro Mani <manisandro@gmail.com>
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
#include "Recognizer.hh"
#include "Utils.hh"

#include <gtkspellmm.h>
#include <csignal>
#include <cstring>
#define USE_STD_NAMESPACE
#include <tesseract/baseapi.h>
#include <tesseract/strngs.h>
#include <tesseract/genericvector.h>
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
	ETEXT_DESC desc;

	ProgressMonitor(int nPages) : MainWindow::ProgressMonitor(nPages) {
		desc.progress = 0;
		desc.cancel = cancelCallback;
		desc.cancel_this = this;
	}
	int getProgress() const override {
		Glib::Threads::Mutex::Lock lock(m_mutex);
		return 100.0 * ((m_progress + desc.progress / 100.0) / m_total);
	}
	static bool cancelCallback(void* instance, int /*words*/) {
		ProgressMonitor* monitor = reinterpret_cast<ProgressMonitor*>(instance);
		Glib::Threads::Mutex::Lock lock(monitor->m_mutex);
		return monitor->m_cancelled;
	}
};


class Recognizer::MultilingualMenuItem : public Gtk::RadioMenuItem {
public:
	MultilingualMenuItem(Group& groupx, const Glib::ustring& label)
		: Gtk::RadioMenuItem(groupx, label) {
		CONNECT(this, select, [this] {
			if(!m_selected) {
				m_ignoreNextActivate = true;
			}
			m_selected = true;
		});
		CONNECT(this, deselect, [this] {
			m_selected = false;
			m_ignoreNextActivate = false;
		});
		this->signal_button_press_event().connect([this](GdkEventButton* /*ev*/) {
			set_active(true);
			return true;
		}, false);
	}

protected:
	void on_activate() override {
		if(!m_ignoreNextActivate) {
			Gtk::RadioMenuItem::on_activate();
		}
		m_ignoreNextActivate = false;
	}

private:
	ClassData m_classdata;
	bool m_selected = false;
	bool m_ignoreNextActivate = false;
};

Recognizer::Recognizer(const Ui::MainWindow& _ui)
	: ui(_ui) {
	CONNECT(ui.buttonRecognize, clicked, [this] { recognizeButtonClicked(); });
	CONNECT(ui.menuitemRecognizeCurrent, activate, [this] { recognizeCurrentPage(); });
	CONNECT(ui.menuitemRecognizeMultiple, activate, [this] { recognizeMultiplePages();; });
	CONNECT(ui.entryPageRange, focus_in_event, [this](GdkEventFocus*) {
		Utils::clear_error_state(ui.entryPageRange);
		return false;
	});

	ADD_SETTING(VarSetting<Glib::ustring>("language"));
	ADD_SETTING(ComboSetting("ocrregionstrategy", ui.comboPageRangeRegions));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("ocraddsourcefilename", ui.checkPageRangePrependFile));
	ADD_SETTING(SwitchSettingT<Gtk::CheckButton>("ocraddsourcepage", ui.checkPageRangePrependPage));
	ADD_SETTING(VarSetting<int>("psm"));
}

std::vector<Glib::ustring> Recognizer::getAvailableLanguages() const {
	tesseract::TessBaseAPI tess;
	initTesseract(tess);
	GenericVector<STRING> availLanguages;
	tess.GetAvailableLanguagesAsVector(&availLanguages);
	std::vector<Glib::ustring> result;
	for(int i = 0; i < availLanguages.size(); ++i) {
		result.push_back(availLanguages[i].string());
	}
	return result;
}

static int g_pipe[2];
static jmp_buf g_restore_point;

static void tessCrashHandler(int /*signal*/) {
	fflush(stderr);
	char buf[1025];
	int bytesRead = 0;
	Glib::ustring captured;
	do {
		if((bytesRead = read(g_pipe[0], buf, sizeof(buf)-1)) > 0) {
			buf[bytesRead] = 0;
			captured += buf;
		}
	} while(bytesRead == sizeof(buf)-1);
	tesseract::TessBaseAPI tess;
	Glib::ustring errMsg = Glib::ustring::compose(_("Tesseract crashed with the following message:\n\n"
	                       "%1\n\n"
	                       "This typically happens for one of the following reasons:\n"
	                       "- Outdated traineddata files are used.\n"
	                       "- Auxiliary language data files are missing.\n"
	                       "- Corrupt language data files.\n\n"
	                       "Make sure your language data files are valid and compatible with tesseract %2."), captured, tess.Version());
	Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Error"), errMsg);
	longjmp(g_restore_point, SIGSEGV);
}

bool Recognizer::initTesseract(tesseract::TessBaseAPI& tess, const char* language) const {
	std::string current = setlocale(LC_NUMERIC, NULL);
	setlocale(LC_NUMERIC, "C");
	// unfortunately tesseract creates deliberate segfaults when an error occurs
	std::signal(SIGSEGV, tessCrashHandler);
	pipe(g_pipe);
	fflush(stderr);
	int oldstderr = dup(fileno(stderr));
	dup2(g_pipe[1], fileno(stderr));
	int ret = -1;
	int fault_code = setjmp(g_restore_point);
	if(fault_code == 0) {
		ret = tess.Init(nullptr, language);
	} else {
		ret = -1;
	}
	dup2(oldstderr, fileno(stderr));
	std::signal(SIGSEGV, MainWindow::signalHandler);
	setlocale(LC_NUMERIC, current.c_str());

	close(g_pipe[0]);
	close(g_pipe[1]);
	return ret != -1;
}

void Recognizer::updateLanguagesMenu() {
	ui.menuLanguages->foreach([this](Gtk::Widget& w) {
	ui.menuLanguages->remove(w);
	});
	m_langMenuRadioGroup = Gtk::RadioButtonGroup();
	m_langMenuCheckGroup = std::vector<std::pair<Gtk::CheckMenuItem*, Glib::ustring>>();
	m_psmRadioGroup = Gtk::RadioButtonGroup();
	m_curLang = Config::Lang();
	Gtk::RadioMenuItem* curitem = nullptr;
	Gtk::RadioMenuItem* activeitem = nullptr;
	bool haveOsd = false;

	std::vector<Glib::ustring> parts = Utils::string_split(ConfigSettings::get<VarSetting<Glib::ustring>>("language")->getValue(), ':');
	Config::Lang curlang = {parts.empty() ? "eng" : parts[0], parts.size() < 2 ? "" : parts[1]};

	std::vector<Glib::ustring> dicts = GtkSpell::Checker::get_language_list();

	std::vector<Glib::ustring> availLanguages = getAvailableLanguages();

	if(availLanguages.empty()) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("No languages available"), _("No tesseract languages are available for use. Recognition will not work."));
		ui.labelRecognizeLang->set_text("");
	}

	// Add menu items for languages, with spelling submenu if available
	for(const Glib::ustring& langprefix : availLanguages) {
		if(langprefix == "osd") {
			haveOsd = true;
			continue;
		}
		Config::Lang lang = {langprefix};
		if(!MAIN->getConfig()->searchLangSpec(lang)) {
			lang.name = lang.prefix;
		}
		std::vector<Glib::ustring> spelldicts;
		if(!lang.code.empty()) {
			for(const Glib::ustring& dict : dicts) {
				if(dict.substr(0, 2) == lang.code.substr(0, 2)) {
					spelldicts.push_back(dict);
				}
			}
			std::sort(spelldicts.begin(), spelldicts.end());
		}
		if(!spelldicts.empty()) {
			Gtk::MenuItem* item = Gtk::manage(new Gtk::MenuItem(lang.name));
			Gtk::Menu* submenu = Gtk::manage(new Gtk::Menu);
			for(const Glib::ustring& dict : spelldicts) {
				Config::Lang itemlang = {lang.prefix, dict, lang.name};
				curitem = Gtk::manage(new Gtk::RadioMenuItem(m_langMenuRadioGroup, GtkSpell::Checker::decode_language_code(dict)));
				CONNECT(curitem, toggled, [this, curitem, itemlang] { setLanguage(curitem, itemlang); });
				if(curlang.prefix == lang.prefix && (
				            curlang.code == dict ||
				            (!activeitem && (curlang.code == dict.substr(0, 2) || curlang.code.empty())))) {
					curlang = itemlang;
					activeitem = curitem;
				}
				submenu->append(*curitem);
			}
			item->set_submenu(*submenu);
			ui.menuLanguages->append(*item);
		} else {
			curitem = Gtk::manage(new Gtk::RadioMenuItem(m_langMenuRadioGroup, lang.name));
			CONNECT(curitem, toggled, [this, curitem, lang] { setLanguage(curitem, lang); });
			if(curlang.prefix == lang.prefix) {
				curlang = lang;
				activeitem = curitem;
			}
			ui.menuLanguages->append(*curitem);
		}
	}
	// Add multilanguage menu
	bool isMultilingual = false;
	if(!availLanguages.empty()) {
		ui.menuLanguages->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
		m_multilingualRadio = Gtk::manage(new MultilingualMenuItem(m_langMenuRadioGroup, _("Multilingual")));
		Gtk::Menu* submenu = Gtk::manage(new Gtk::Menu);
		isMultilingual = curlang.prefix.find('+') != curlang.prefix.npos;
		std::vector<Glib::ustring> sellangs = Utils::string_split(curlang.prefix, '+', false);
		for(const Glib::ustring& langprefix : availLanguages) {
			if(langprefix == "osd") {
				continue;
			}
			Config::Lang lang = {langprefix};
			if(!MAIN->getConfig()->searchLangSpec(lang)) {
				lang.name = lang.prefix;
			}
			Gtk::CheckMenuItem* item = Gtk::manage(new Gtk::CheckMenuItem(lang.name));
			item->set_active(isMultilingual && std::find(sellangs.begin(), sellangs.end(), lang.prefix) != sellangs.end());
			CONNECT(item, button_press_event, [this,item](GdkEventButton* ev) {
				return onMultilingualItemButtonEvent(ev, item);
			}, false);
			CONNECT(item, button_release_event, [this,item](GdkEventButton* ev) {
				return onMultilingualItemButtonEvent(ev, item);
			}, false);
			//		CONNECT(item, toggled, [this]{ setMultiLanguage(); });
			submenu->append(*item);
			m_langMenuCheckGroup.push_back(std::make_pair(item, lang.prefix));
		}
		m_multilingualRadio->set_submenu(*submenu);
		CONNECT(m_multilingualRadio, toggled, [this] { if(m_multilingualRadio->get_active()) setMultiLanguage(); });
		CONNECT(submenu, button_press_event, [this](GdkEventButton* ev) {
			return onMultilingualMenuButtonEvent(ev);
		}, false);
		CONNECT(submenu, button_release_event, [this](GdkEventButton* ev) {
			return onMultilingualMenuButtonEvent(ev);
		}, false);
		ui.menuLanguages->append(*m_multilingualRadio);
	}
	if(isMultilingual) {
		activeitem = m_multilingualRadio;
		setMultiLanguage();
	} else if(activeitem == nullptr) {
		activeitem = curitem;
	}

	// Add PSM items
	ui.menuLanguages->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	Gtk::Menu* psmMenu = Gtk::manage(new Gtk::Menu);
	m_currentPsmMode = ConfigSettings::get<VarSetting<int>>("psm")->getValue();

	struct PsmEntry {
		Glib::ustring label;
		tesseract::PageSegMode psmMode;
		bool requireOsd;
	};
	std::vector<PsmEntry> psmModes = {
		PsmEntry{_("Automatic page segmentation"), tesseract::PSM_AUTO, false},
		PsmEntry{_("Page segmentation with orientation and script detection"), tesseract::PSM_AUTO_OSD, true},
		PsmEntry{_("Assume single column of text"), tesseract::PSM_SINGLE_COLUMN, false},
		PsmEntry{_("Assume single block of vertically aligned text"), tesseract::PSM_SINGLE_BLOCK_VERT_TEXT, false},
		PsmEntry{_("Assume a single uniform block of text"), tesseract::PSM_SINGLE_BLOCK, false},
		PsmEntry{_("Assume a line of text"), tesseract::PSM_SINGLE_LINE, false},
		PsmEntry{_("Assume a single word"), tesseract::PSM_SINGLE_WORD, false},
		PsmEntry{_("Assume a single word in a circle"), tesseract::PSM_CIRCLE_WORD, false},
		PsmEntry{_("Sparse text in no particular order"), tesseract::PSM_SPARSE_TEXT, false},
		PsmEntry{_("Sparse text with orientation and script detection"), tesseract::PSM_SPARSE_TEXT_OSD, true}
	};
	for(const auto& entry : psmModes) {
		Gtk::RadioMenuItem* item = Gtk::manage(new Gtk::RadioMenuItem(m_psmRadioGroup, entry.label));
		item->set_sensitive(!entry.requireOsd || haveOsd);
		item->set_active(entry.psmMode == m_currentPsmMode);
		CONNECT(item, toggled, [this, entry, item] {
			if(item->get_active()) {
				m_currentPsmMode = entry.psmMode;
				ConfigSettings::get<VarSetting<int>>("psm")->setValue(entry.psmMode);
			}
		});
		psmMenu->append(*item);
	}

	Gtk::MenuItem* psmItem = Gtk::manage(new Gtk::MenuItem(_("Page segmentation mode")));
	psmItem->set_submenu(*psmMenu);
	ui.menuLanguages->append(*psmItem);

	// Add installer item
	ui.menuLanguages->append(*Gtk::manage(new Gtk::SeparatorMenuItem()));
	Gtk::MenuItem* manageItem = Gtk::manage(new Gtk::MenuItem(_("Manage languages...")));
	CONNECT(manageItem, activate, [this] { MAIN->manageLanguages(); });
	ui.menuLanguages->append(*manageItem);

	ui.menuLanguages->show_all();
	if(activeitem) {
		activeitem->set_active(true);
		activeitem->toggled(); // Ensure signal is emitted
	}
}

void Recognizer::setLanguage(const Gtk::RadioMenuItem* item, const Config::Lang &lang) {
	if(item->get_active()) {
		if(!lang.code.empty()) {
			ui.labelRecognizeLang->set_markup(Glib::ustring::compose("<small> %1 (%2)</small>", lang.name, lang.code));
		} else {
			ui.labelRecognizeLang->set_markup(Glib::ustring::compose("<small> %1</small>", lang.name));
		}
		m_curLang = lang;
		ConfigSettings::get<VarSetting<Glib::ustring>>("language")->setValue(lang.prefix + ":" + lang.code);
		m_signal_languageChanged.emit(m_curLang);
	}
}

void Recognizer::setMultiLanguage() {
	m_multilingualRadio->set_active(true);
	Glib::ustring langs;
	for(const auto& pair : m_langMenuCheckGroup) {
		if(pair.first->get_active()) {
			langs += pair.second + "+";
		}
	}
	if(langs.empty()) {
		langs = "eng+";
	}
	langs = langs.substr(0, langs.length() - 1);
	ui.labelRecognizeLang->set_markup("<small>" + langs + "</small>");
	m_curLang = {langs, "", "Multilingual"};
	ConfigSettings::get<VarSetting<Glib::ustring>>("language")->setValue(langs + ":");
	m_signal_languageChanged.emit(m_curLang);
}

void Recognizer::setRecognizeMode(const Glib::ustring& mode) {
	ui.labelRecognizeMode->set_markup(Glib::ustring::compose("<small>%1</small>", mode));
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

void Recognizer::recognize(const std::vector<int> &pages, bool autodetectLayout) {
	tesseract::TessBaseAPI tess;
	bool prependFile = pages.size() > 1 && ConfigSettings::get<SwitchSetting>("ocraddsourcefilename")->getValue();
	bool prependPage = pages.size() > 1 && ConfigSettings::get<SwitchSetting>("ocraddsourcepage")->getValue();
	if(initTesseract(tess, m_curLang.prefix.c_str())) {
		Glib::ustring failed;
		tess.SetPageSegMode(static_cast<tesseract::PageSegMode>(m_currentPsmMode));
		OutputEditor::ReadSessionData* readSessionData = MAIN->getOutputEditor()->initRead(tess);
		ProgressMonitor monitor(pages.size());
		MAIN->showProgress(&monitor);
		Utils::busyTask([&] {
			int npages = pages.size();
			int idx = 0;
			std::string prevFile;
			for(int page : pages) {
				monitor.desc.progress = 0;
				++idx;
				Glib::signal_idle().connect_once([=] { MAIN->pushState(MainWindow::State::Busy, Glib::ustring::compose(_("Recognizing page %1 (%2 of %3)"), page, idx, npages)); });

				PageData pageData;
				Utils::runInMainThreadBlocking([&] { pageData = setPage(page, autodetectLayout); });
				if(!pageData.success) {
					failed.append(Glib::ustring::compose(_("\n- Page %1: failed to render page"), page));
					MAIN->getOutputEditor()->readError(_("\n[Failed to recognize page %1]\n"), readSessionData);
					continue;
				}
				readSessionData->file = pageData.filename;
				readSessionData->page = pageData.page;
				readSessionData->angle = pageData.angle;
				readSessionData->resolution = pageData.resolution;
				bool firstChunk = true;
				bool newFile = readSessionData->file != prevFile;
				prevFile = readSessionData->file;
				for(const Cairo::RefPtr<Cairo::ImageSurface>& image : pageData.ocrAreas) {
					readSessionData->prependPage = prependPage && firstChunk;
					readSessionData->prependFile = prependFile && (readSessionData->prependPage || newFile);
					firstChunk = false;
					newFile = false;
					tess.SetImage(image->get_data(), image->get_width(), image->get_height(), 4, image->get_stride());
					tess.SetSourceResolution(MAIN->getDisplayer()->getCurrentResolution());
					tess.Recognize(&monitor.desc);
					if(!monitor.cancelled()) {
						MAIN->getOutputEditor()->read(tess, readSessionData);
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
		MAIN->hideProgress();
		MAIN->getOutputEditor()->finalizeRead(readSessionData);
		if(!failed.empty()) {
			Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Recognition errors occurred"), Glib::ustring::compose(_("The following errors occurred:%1"), failed));
		}
	}
}

bool Recognizer::recognizeImage(const Cairo::RefPtr<Cairo::ImageSurface> &img, OutputDestination dest) {
	tesseract::TessBaseAPI tess;
	if(!initTesseract(tess, m_curLang.prefix.c_str())) {
		Utils::message_dialog(Gtk::MESSAGE_ERROR, _("Recognition errors occurred"), _("Failed to initialize tesseract"));
		return false;
	}
	tess.SetImage(img->get_data(), img->get_width(), img->get_height(), 4, 4 * img->get_width());
	ProgressMonitor monitor(1);
	MAIN->showProgress(&monitor);
	if(dest == OutputDestination::Buffer) {
		OutputEditor::ReadSessionData* readSessionData = MAIN->getOutputEditor()->initRead(tess);
		readSessionData->file = MAIN->getDisplayer()->getCurrentImage(readSessionData->page);
		readSessionData->angle = MAIN->getDisplayer()->getCurrentAngle();
		readSessionData->resolution = MAIN->getDisplayer()->getCurrentResolution();
		Utils::busyTask([&] {
			tess.Recognize(&monitor.desc);
			if(!monitor.cancelled()) {
				MAIN->getOutputEditor()->read(tess, readSessionData);
			}
			return true;
		}, _("Recognizing..."));
		MAIN->getOutputEditor()->finalizeRead(readSessionData);
	} else if(dest == OutputDestination::Clipboard) {
		Glib::ustring output;
		if(Utils::busyTask([&] {
		tess.Recognize(&monitor.desc);
			if(!monitor.cancelled()) {
				char* text = tess.GetUTF8Text();
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
	return true;
}

Recognizer::PageData Recognizer::setPage(int page, bool autodetectLayout) {
	PageData pageData;
	pageData.success = MAIN->getDisplayer()->setup(&page);
	if(pageData.success) {
		if(autodetectLayout) {
			MAIN->getDisplayer()->autodetectOCRAreas();
		}
		pageData.filename = MAIN->getDisplayer()->getCurrentImage(pageData.page);
		pageData.angle = MAIN->getDisplayer()->getCurrentAngle();
		pageData.resolution = MAIN->getDisplayer()->getCurrentResolution();
		pageData.ocrAreas = MAIN->getDisplayer()->getOCRAreas();
	}
	return pageData;
}

bool Recognizer::onMultilingualMenuButtonEvent(GdkEventButton* ev) {
	Gtk::Allocation alloc = m_multilingualRadio->get_allocation();
	int radio_x_root, radio_y_root;
	m_multilingualRadio->get_window()->get_root_coords(alloc.get_x(), alloc.get_y(), radio_x_root, radio_y_root);

	if(ev->x_root >= radio_x_root && ev->x_root <= radio_x_root + alloc.get_width() &&
	        ev->y_root >= radio_y_root && ev->y_root <= radio_y_root + alloc.get_height()) {
		if(ev->type == GDK_BUTTON_PRESS) {
			m_multilingualRadio->set_active(true);
		}
		return true;
	}
	return false;
}

bool Recognizer::onMultilingualItemButtonEvent(GdkEventButton* ev, Gtk::CheckMenuItem* item) {
	Gtk::Allocation alloc = item->get_allocation();
	int item_x_root, item_y_root;
	item->get_window()->get_root_coords(alloc.get_x(), alloc.get_y(), item_x_root, item_y_root);

	if(ev->x_root >= item_x_root && ev->x_root <= item_x_root + alloc.get_width() &&
	        ev->y_root >= item_y_root && ev->y_root <= item_y_root + alloc.get_height()) {
		if(ev->type == GDK_BUTTON_RELEASE) {
			item->set_active(!item->get_active());
			setMultiLanguage();
		}
		return true;
	}
	return false;
}
