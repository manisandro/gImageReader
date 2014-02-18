/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Recognizer.cc
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

#include "MainWindow.hh"
#include "Config.hh"
#include "Displayer.hh"
#include "OutputManager.hh"
#include "Recognizer.hh"
#include "Utils.hh"

#include <tesseract/baseapi.h>

Recognizer::Recognizer()
{
	m_pagesMenu = Builder("menu:recognize.pages");
	m_pagesDialog = Builder("dialog:recognize.pages");
	m_pagesEntry = Builder("entry:dialog.pages");
	m_regionStrategyCombo = Builder("comboboxtext:dialog.regions");
	Gtk::MenuToolButton* tbbtn = Builder("tbmenu:main.recognize");
	m_recognizeBtn = static_cast<Gtk::ToolButton*>(((Gtk::Container*)tbbtn->get_children()[0])->get_children()[0]);

	CONNECT(m_recognizeBtn, clicked, [this]{ recognizeStart(); });
	CONNECT(Builder("menuitem:recognize.pages.current").as<Gtk::MenuItem>(), activate, [this]{ recognizeStart(PageSelection::Current); });
	CONNECT(Builder("menuitem:recognize.pages.multiple").as<Gtk::MenuItem>(), activate, [this]{ recognizeStart(PageSelection::Multiple); });
	CONNECTS(m_pagesEntry, focus_in_event, [](GdkEventFocus*, Gtk::Entry* e){ Utils::clear_error_state(e); return false; });
	m_textDispatcher.connect([this]{ addText(); });
}

bool Recognizer::recognizeImage(const Cairo::RefPtr<Cairo::ImageSurface> &img, OutputDestination dest)
{
	Glib::ustring lang = MAIN->getConfig()->getSelectedLanguage().prefix;
	Glib::ustring text;
	if(!Utils::busyTask([&img,&lang,&text,this]{
		tesseract::TessBaseAPI tess;
		if(!Utils::initTess(tess, nullptr, lang.c_str())){ return false; }
		tess.SetImage(img->get_data(), img->get_width(), img->get_height(), 4, 4 * img->get_width());
		text = tess.GetUTF8Text();
		return true;
	}, _("Recognizing...")))
	{
		return false;
	}
	if(dest == OutputDestination::Buffer){
		MAIN->getOutputManager()->addText(text, false);
	}else if(dest == OutputDestination::Clipboard){
		Gtk::Clipboard::get()->set_text(text);
	}
	return true;
}

void Recognizer::selectPages(std::vector<int>& pages)
{
	m_pagesEntry->set_text(Glib::ustring::compose("%1-%2", 1, MAIN->getDisplayer()->getNPages()));
	m_pagesEntry->grab_focus();
	int nPages = MAIN->getDisplayer()->getNPages();

	if(m_pagesDialog->run() == Gtk::RESPONSE_OK){
		pages.clear();
		Glib::ustring text = m_pagesEntry->get_text();
		text = Glib::Regex::create("\\s+")->replace(text, 0, "", static_cast<Glib::RegexMatchFlags>(0));
		std::vector<Glib::ustring>&& blocks = Utils::string_split(text, ',');
		for(const Glib::ustring& block : blocks){
			std::vector<Glib::ustring>&& ranges = Utils::string_split(block, '-');
			if(ranges.size() == 1){
				int page = atoi(ranges[0].c_str());
				if(page > 0 && page <= nPages){
					pages.push_back(page);
				}
			}else if(ranges.size() == 2){
				int start = std::max(1, atoi(ranges[0].c_str()));
				int end = std::min(nPages, atoi(ranges[1].c_str()));
				for(int page = start; page <= end; ++page){
					pages.push_back(page);
				}
			}else{
				pages.clear();
				break;
			}
		}
		if(pages.empty()){
			Utils::set_error_state(m_pagesEntry);
		}
	}
	std::sort(pages.begin(), pages.end());
	m_pagesDialog->hide();
}

void Recognizer::recognizeStart(PageSelection pagesel)
{
	int nPages = MAIN->getDisplayer()->getNPages();
	if(nPages == 1 || MAIN->getDisplayer()->getHasSelections()){
		pagesel = PageSelection::Current;
	}
	std::vector<int> pages;
	if(pagesel == PageSelection::Prompt){
		auto positioner = sigc::bind(sigc::ptr_fun(Utils::popup_positioner), m_recognizeBtn, m_pagesMenu, false, true);
		m_pagesMenu->popup(positioner, 0, gtk_get_current_event_time());
		return;
	}else if(pagesel == PageSelection::Current){
		pages.push_back(MAIN->getDisplayer()->getCurrentPage());
	}else if(pagesel == PageSelection::Multiple){
		selectPages(pages);
	}
	if(pages.empty()) {
		return;
	}
	MAIN->pushState(MainWindow::State::Busy, _("Recognizing..."));
	m_textInsert = false;
	Glib::ustring lang = MAIN->getConfig()->getSelectedLanguage().prefix;
	m_thread = Glib::Threads::Thread::create([this,pages,lang]{ recognizeDo(pages, lang); });
}

void Recognizer::recognizeDo(const std::vector<int> &pages, const Glib::ustring& lang)
{
	Glib::ustring failed;
	tesseract::TessBaseAPI tess;
	if(!Utils::initTess(tess, nullptr, lang.c_str())){
		failed.append(_("\n\tFailed to initialize tesseract"));
	}else{
		int npages = pages.size();
		int idx = 0;
		for(int page : pages){
			++idx;
			Glib::signal_idle().connect_once([page, npages, idx]{ MAIN->pushState(MainWindow::State::Busy, Glib::ustring::compose(_("Recognizing page %1 (%2 of %3)"), page, idx, npages)); });
			m_taskState = TaskState::Waiting;
			Glib::signal_idle().connect_once([this,page]{ setPage(page); });
			Glib::Threads::Mutex::Lock lock(m_mutex);
			while(m_taskState == TaskState::Waiting){
				m_cond.wait(m_mutex);
			}
			if(m_taskState == TaskState::Failed){
				failed.append(Glib::ustring::compose(_("\n\tPage %1: failed to render page"), page));
				m_textQueue.push(Glib::ustring::compose(_("\n[Failed to recognize page %1]\n"), page));
				m_textDispatcher.emit();
				continue;
			}
			std::vector<Cairo::RefPtr<Cairo::ImageSurface>>&& selections = MAIN->getDisplayer()->getSelections();
			for(Cairo::RefPtr<Cairo::ImageSurface> image : selections){
				tess.SetImage(image->get_data(), image->get_width(), image->get_height(), 4, image->get_stride());
				m_textQueue.push(tess.GetUTF8Text());
				m_textDispatcher.emit();
			}
			Glib::signal_idle().connect_once([]{ MAIN->popState(); });
		}
	}
	Glib::signal_idle().connect_once([this, failed]{ recognizeDone(failed); });
}

void Recognizer::recognizeDone(const Glib::ustring &errors)
{
	m_thread->join();
	MAIN->popState();
	if(!errors.empty()){
		Utils::error_dialog(_("Recognition errors occured"), Glib::ustring::compose(_("The following errors occured:%1"), errors));
	}
}

void Recognizer::setPage(int page)
{
	bool success = MAIN->getDisplayer()->setCurrentPage(page);
	if(m_regionStrategyCombo->get_active_row_number() == RegionStrategy::Autodetect) {
		MAIN->getDisplayer()->autodetectLayout();
	}
	Glib::Threads::Mutex::Lock lock(m_mutex);
	m_taskState = success ? TaskState::Succeeded : TaskState::Failed;
	m_cond.signal();
}

void Recognizer::addText()
{
	Glib::ustring text = m_textQueue.pop();
	MAIN->getOutputManager()->addText(text, m_textInsert);
	m_textInsert = true;
}
