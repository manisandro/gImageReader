/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScannerTwain.cc
 * Copyright (C) 2013-2019 Sandro Mani <manisandro@gmail.com>
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

#include "ScannerTwain.hh"
#include "../MainWindow.hh"
#ifdef G_OS_WIN32
#include <gdk/gdkwin32.h>
#endif

#ifdef G_OS_WIN32
bool ScannerTwain::saveDIB(TW_MEMREF hImg, const std::string& filename) {
	PBITMAPINFOHEADER pDIB = reinterpret_cast<PBITMAPINFOHEADER>(m_entryPoint.DSM_MemLock(hImg));
	if(pDIB == nullptr) {
		return false;
	}

	DWORD paletteSize = 0;
	if(pDIB->biBitCount == 1) {
		paletteSize = 2;
	} else if(pDIB->biBitCount == 8) {
		paletteSize = 256;
	} else if(pDIB->biBitCount == 24) {
		paletteSize = 0;
	} else {
		// Unsupported
		m_entryPoint.DSM_MemUnlock(hImg);
		m_entryPoint.DSM_MemFree(hImg);
		return false;
	}

	// If the driver did not fill in the biSizeImage field, then compute it.
	// Each scan line of the image is aligned on a DWORD (32bit) boundary.
	if( pDIB->biSizeImage == 0 ) {
		pDIB->biSizeImage = ((((pDIB->biWidth * pDIB->biBitCount) + 31) & ~31) / 8) * pDIB->biHeight;
		// If a compression scheme is used the result may be larger: increase size.
		if (pDIB->biCompression != 0) {
			pDIB->biSizeImage = (pDIB->biSizeImage * 3) / 2;
		}
	}

	std::uint64_t imageSize = pDIB->biSizeImage + sizeof(RGBQUAD) * paletteSize + sizeof(BITMAPINFOHEADER);

	BITMAPFILEHEADER bmpFIH = {0};
	bmpFIH.bfType = ( (WORD) ('M' << 8) | 'B');
	bmpFIH.bfSize = imageSize + sizeof(BITMAPFILEHEADER);
	bmpFIH.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + (sizeof(RGBQUAD) * paletteSize);

	unsigned char* bmpData = new unsigned char[sizeof(BITMAPFILEHEADER) + imageSize];
	std::memcpy(bmpData, &bmpFIH, sizeof(BITMAPFILEHEADER));
	std::memcpy(bmpData + sizeof(BITMAPFILEHEADER), pDIB, imageSize);

	bool success = false;
	Glib::RefPtr<Gio::MemoryInputStream> stream = Gio::MemoryInputStream::create();
	stream->add_data(bmpData, sizeof(BITMAPFILEHEADER) + imageSize);
	Glib::RefPtr<Gdk::Pixbuf> pixbuf(Gdk::Pixbuf::create_from_stream(stream));
	if(pixbuf) {
		try {
			pixbuf->save(filename, "png");
			success = true;
		} catch(...) {
			success = false;
		}
	}

	delete[] bmpData;
	m_entryPoint.DSM_MemUnlock(hImg);
	m_entryPoint.DSM_MemFree(hImg);

	return success;
}
#endif

ScannerTwain* ScannerTwain::s_instance = nullptr;

void ScannerTwain::init() {
	if(m_dsmLib != nullptr) {
		g_critical("Init already called!");
		m_signal_initFailed.emit();
		return;
	}

#ifdef G_OS_WIN32
	std::string twaindsm = Glib::build_filename(pkgDir, "bin", "twaindsm.dll");
#else
	std::string twaindsm = "libtwaindsm.so.2.2.0";
#endif

	// State 1 to 2
	m_dsmLib = dlopen(twaindsm.c_str(), RTLD_LAZY);
	if(m_dsmLib == nullptr) {
		g_critical("LoadLibrary failed on %s", twaindsm.c_str());
		m_signal_initFailed.emit();
		close();
		return;
	}

	m_dsmEntry = (DSMENTRYPROC)dlsym(m_dsmLib, "DSM_Entry");
	if(m_dsmEntry == nullptr) {
		g_critical("GetProcAddress failed on %s::DSM_Entry", twaindsm.c_str());
		m_signal_initFailed.emit();
		close();
		return;
	}

	m_appID.Id = 0;
	m_appID.Version.MajorNum = 1;
	m_appID.Version.MinorNum = 0;
	m_appID.Version.Language = TWLG_USA;
	m_appID.Version.Country = TWCY_USA;
	std::strncpy(m_appID.Version.Info, "TWAIN Interface", sizeof(m_appID.Version.Info));
	std::strncpy(m_appID.ProductName, "TWAIN Interface", sizeof(m_appID.ProductName));
	std::strncpy(m_appID.Manufacturer, "Sandro Mani", sizeof(m_appID.Manufacturer));
	std::strncpy(m_appID.ProductFamily, "Sandro Mani", sizeof(m_appID.ProductFamily));

	m_appID.SupportedGroups = DF_APP2 | DG_IMAGE | DG_CONTROL;
	m_appID.ProtocolMajor = TWON_PROTOCOLMAJOR;
	m_appID.ProtocolMinor = TWON_PROTOCOLMINOR;

	m_state = 2;

	// State 2 to 3
	TW_MEMREF phwnd = nullptr;
#ifdef G_OS_WIN32
//    HWND hwnd = gdk_win32_window_get_impl_hwnd(MAIN->getWindow()->get_window()->gobj());
//    phwnd = &hwnd;
#endif
	if(call(nullptr, DG_CONTROL, DAT_PARENT, MSG_OPENDSM, phwnd) != TWRC_SUCCESS) {
		m_signal_initFailed.emit();
		close();
		return;
	}
	if((m_appID.SupportedGroups & DF_DSM2) == 0) {
		g_critical("Twain 2.x interface is not supported");
		m_signal_initFailed.emit();
		close();
		return;
	}

	// Get entry point of memory management functions
	m_entryPoint.Size = sizeof(TW_ENTRYPOINT);
	if(call(nullptr, DG_CONTROL, DAT_ENTRYPOINT, MSG_GET, &m_entryPoint) != TWRC_SUCCESS) {
		m_signal_initFailed.emit();
		close();
		return;
	}
	m_state = 3;
	s_instance = this;

	// Redetect on init
	redetect();
}

void ScannerTwain::redetect() {
	TW_IDENTITY sourceID;
	std::vector<Device> sources;

	// Get the first source
	TW_UINT16 twRC = call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_GETFIRST, &sourceID);

	// Loop through all the sources
	while (twRC == TWRC_SUCCESS) {
		sources.push_back({sourceID.ProductName, sourceID.ProductName});
		twRC = call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_GETNEXT, &sourceID);
	}
	m_signal_devicesDetected.emit(sources);
}

void ScannerTwain::scan(const Params& params) {
	if(params.device.empty()) {
		failScan(_("No scanner specified"));
		return;
	}
	/** Open device **/
	m_signal_scanStateChanged.emit(State::OPEN);

	std::strncpy(m_srcID.ProductName, params.device.c_str(), sizeof(m_srcID.ProductName));
	// State 3 to 4
	if(call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_OPENDS, &m_srcID) != TWRC_SUCCESS) {
		failScan(_("Unable to connect to scanner"));
		return;
	}
	m_state = 4;

	// Register callback
	TW_CALLBACK cb = {};
	cb.CallBackProc = reinterpret_cast<TW_MEMREF>(callback);
	if(TWRC_SUCCESS == call(&m_srcID, DG_CONTROL, DAT_CALLBACK, MSG_REGISTER_CALLBACK, &cb)) {
		m_useCallback = true;
	} else {
		m_useCallback = false;
	}

	/** Set options **/
	m_signal_scanStateChanged.emit(State::SET_OPTIONS);

#ifndef G_OS_WIN32
	// Set the filename
	TW_SETUPFILEXFER xferFile = {};
	std::strncpy(xferFile.FileName, params.filename.c_str(), sizeof(xferFile.FileName));
	xferFile.Format = TWFF_PNG;
	xferFile.VRefNum = TWON_DONTCARE16;

	if(call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_SET, &xferFile) != TWRC_SUCCESS) {
		// Possibly unsupported file format. Get the default format, and try again with that one
		call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_GET, &xferFile);
		std::string defaultfile = xferFile.FileName;
		params.filename = params.filename.substr(0, params.filename.rfind('.')) + defaultfile.substr(defaultfile.rfind('.'));
		std::strncpy(xferFile.FileName, params.filename.c_str(), sizeof(xferFile.FileName));
		if(call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_SET, &xferFile) != TWRC_SUCCESS) {
			// If we failed again, just return the default file...
			params.filename = defaultfile;
		}
	}
#endif

	// Set capabilities
	setCapability(CAP_XFERCOUNT, CapOneVal(TWTY_INT16, 1));
	setCapability(ICAP_XRESOLUTION, CapOneVal(TWTY_FIX32, floatToFix32(params.dpi)));
	setCapability(ICAP_YRESOLUTION, CapOneVal(TWTY_FIX32, floatToFix32(params.dpi)));
	setCapability(ICAP_SUPPORTEDSIZES, CapOneVal(TWTY_UINT16, TWSS_NONE)); // Maximum scan area
	setCapability(ICAP_BITDEPTH, CapOneVal(TWTY_UINT16, params.depth));

	if(params.scan_mode != ScanMode::DEFAULT) {
		if(params.scan_mode == ScanMode::COLOR) {
			setCapability(ICAP_PIXELTYPE, CapOneVal(TWTY_UINT16, TWPT_RGB));
		} else if(params.scan_mode == ScanMode::GRAY) {
			setCapability(ICAP_PIXELTYPE, CapOneVal(TWTY_UINT16, TWPT_GRAY));
		} else if(params.scan_mode == ScanMode::LINEART) {
			setCapability(ICAP_PIXELTYPE, CapOneVal(TWTY_UINT16, TWPT_BW));
		}
	}

	// TODO: ADF, duplex, paper size

	/** Start device **/
	m_signal_scanStateChanged.emit(State::START);

	m_dsMsg = MSG_NULL;
	m_dsQuit = false;

	m_ui.ShowUI = true;
	m_ui.ModalUI = true;
	m_ui.hParent = nullptr;;
#ifdef G_OS_WIN32
	m_ui.hParent = gdk_win32_window_get_impl_hwnd(MAIN->getWindow()->get_window()->gobj());
#endif

	// State 4 to 5
	if(call(&m_srcID, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, &m_ui) != TWRC_SUCCESS) {
		failScan(_("Unable to start scan"));
		return;
	}
	m_state = 5;

	m_cancel = false;
#ifdef G_OS_WIN32
	MAIN->getWindow()->get_window()->add_filter(eventFilter, nullptr);
	while(m_dsMsg == 0 && !m_dsQuit && !m_cancel) {
		while(Gtk::Main::instance()->events_pending()) {
			Gtk::Main::instance()->iteration();
		}
	}
	MAIN->getWindow()->get_window()->remove_filter(eventFilter, nullptr);
#else
	m_mutex.lock();
	while(m_dsMsg == MSG_NULL) {
		m_cond.wait(m_mutex);
	}
	m_mutex.unlock();
#endif
	if(m_cancel) {
		failScan(_("Scan canceled"));
	}

	if(m_dsMsg != MSG_XFERREADY) {
		failScan(_("Unable to start scan"));
		return;
	}

	/** Read **/
	m_signal_scanStateChanged.emit(State::READ);

	m_state = 6;
	bool saveOk = true;
#ifdef G_OS_WIN32
	TW_MEMREF hImg = nullptr;
	TW_UINT16 twRC = call(&m_srcID, DG_IMAGE, DAT_IMAGENATIVEXFER, MSG_GET, (TW_MEMREF)&hImg);
	if(twRC == TWRC_XFERDONE) {
		saveOk = saveDIB(hImg, params.filename);
	}
#else
	TW_UINT16 twRC = call(&m_srcID, DG_IMAGE, DAT_IMAGEFILEXFER, MSG_GET, nullptr);
#endif
	TW_PENDINGXFERS twPendingXFers = {};
	call(&m_srcID, DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER, &twPendingXFers);
	if(twPendingXFers.Count != 0) {
		// Discard any additional pending transfers
		std::memset(&twPendingXFers, 0, sizeof(twPendingXFers));
		call(&m_srcID, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, &twPendingXFers);
	}
	if(twRC != TWRC_XFERDONE || !saveOk) {
		failScan(_("Error communicating with scanner"));
		return;
	}

	m_signal_pageAvailable.emit(params.filename);

	/** Stop device **/
	doStop();
}

void ScannerTwain::doStop() {
	if(m_state == 6) {
		TW_PENDINGXFERS twPendingXFers = {};
		// State 7/6 to 5
		call(&m_srcID, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, &twPendingXFers);
		m_state = 5;
	}
	if(m_state == 5) {
		//	State 5 to 4
		call(&m_srcID, DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS, &m_ui);
		m_state = 4;
	}
	if(m_state == 4) {
		//	State 4 to 3
		call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS, &m_srcID);
		m_srcID = {};
		m_state = 3;
	}
	m_signal_scanStateChanged.emit(State::IDLE);
}

void ScannerTwain::failScan(const Glib::ustring& errorString) {
	doStop();
	m_signal_scanFailed.emit(errorString);
}

void ScannerTwain::close() {
	doStop();
	if(m_state == 3) {
		// State 3 to 2
		call(nullptr, DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, nullptr);
		m_state = 2;
	}
	if(m_state == 2) {
		dlclose(m_dsmLib);
		m_dsmLib = nullptr;
	}
	m_state = 1;
	s_instance = nullptr;
}

#ifdef G_OS_WIN32
GdkFilterReturn ScannerTwain::eventFilter(GdkXEvent* xevent, GdkEvent* event, gpointer data) {
	LPMSG msg = static_cast<LPMSG>(xevent);

	if(msg->message == WM_CLOSE) {
		s_instance->m_dsQuit = true;
		return GDK_FILTER_REMOVE;
	}
	TW_EVENT twEvent = {0};
	twEvent.pEvent = (TW_MEMREF)msg;
	twEvent.TWMessage = MSG_NULL;
	TW_UINT16 twRC = s_instance->m_dsmEntry(&s_instance->m_appID, &s_instance->m_srcID, DG_CONTROL, DAT_EVENT, MSG_PROCESSEVENT, (TW_MEMREF)&twEvent);

	if(!s_instance->m_useCallback && twRC == TWRC_DSEVENT) {
		if(twEvent.TWMessage == MSG_XFERREADY || twEvent.TWMessage == MSG_CLOSEDSREQ || twEvent.TWMessage == MSG_CLOSEDSOK || twEvent.TWMessage == MSG_NULL) {
			s_instance->m_dsMsg = twEvent.TWMessage;
		}
		return GDK_FILTER_REMOVE;
	}
	return GDK_FILTER_CONTINUE;
}
#endif

/*********************** ScannerTwain internal methods ***********************/
TW_UINT16 ScannerTwain::call(TW_IDENTITY* idDS, TW_UINT32 dataGroup, TW_UINT16 dataType, TW_UINT16 msg, TW_MEMREF data) {
	TW_UINT16 rc = m_dsmEntry(&m_appID, idDS, dataGroup, dataType, msg, data);
	if(rc == TWRC_FAILURE) {
		TW_STATUS status = {};
		rc = m_dsmEntry(&m_appID, idDS, DG_CONTROL, DAT_STATUS, MSG_GET, &status);
		TW_UINT16 cc = rc == TWRC_SUCCESS ? status.ConditionCode : TWCC_BUMMER;
		g_critical("Call failed with code 0x%x: DataGroup: 0x%x, DataType = 0x%x, Msg = 0x%x", cc, dataGroup, dataType, msg);
		return TWRC_FAILURE;
	}
	return rc;
}

// TWTY_** size:                  INT8, INT16, INT32, UINT8, UINT16, UINT32, BOOL, FIX32, FRAME, STR32, STR64, STR128, STR255;
static std::size_t TWTY_Size[] = {   1,     2,     4,     1,      2,      4,    2,     4,    16,    32,    64,    128,    255};

void ScannerTwain::setCapability(TW_UINT16 capCode, const CapOneVal& cap) {
	TW_CAPABILITY twCapability = {capCode, TWON_DONTCARE16, nullptr};
	TW_UINT16 rc = call(&m_srcID, DG_CONTROL, DAT_CAPABILITY, MSG_GETCURRENT, &twCapability);
	// Unsupported capability
	if(rc != TWRC_SUCCESS) {
		return;
	}
	g_assert(twCapability.ConType == TWON_ONEVALUE);

	pTW_ONEVALUE val = static_cast<pTW_ONEVALUE>(m_entryPoint.DSM_MemLock(twCapability.hContainer));
	g_assert(val->ItemType == cap.type && val->ItemType <= TWTY_STR255);
	// This works because the DSM should return a TW_ONEVALUE container allocated sufficiently large to hold
	// the value of type ItemType.
	std::memcpy(&val->Item, &cap.data, TWTY_Size[cap.type]);
	m_entryPoint.DSM_MemUnlock(twCapability.hContainer);

	call(&m_srcID, DG_CONTROL, DAT_CAPABILITY, MSG_SET, &twCapability);
	m_entryPoint.DSM_MemFree(twCapability.hContainer);
}

TW_UINT16 ScannerTwain::callback(TW_IDENTITY* origin, TW_IDENTITY* /*dest*/, TW_UINT32 /*DG*/, TW_UINT16 /*DAT*/, TW_UINT16 MSG, TW_MEMREF /*data*/) {
	if(origin == nullptr || origin->Id != s_instance->m_srcID.Id) {
		return TWRC_FAILURE;
	}
	if(MSG == MSG_XFERREADY || MSG == MSG_CLOSEDSREQ || MSG == MSG_CLOSEDSOK || MSG == MSG_NULL) {
		s_instance->m_dsMsg = MSG;
#ifndef G_OS_WIN32
		s_instance->m_mutex.lock();
		s_instance->m_cond.wakeOne();
		s_instance->m_mutex.unlock();
#endif
		return TWRC_SUCCESS;
	}
	return TWRC_FAILURE;
}

inline TW_FIX32 ScannerTwain::floatToFix32(float float32) {
	TW_FIX32 fix32;
	TW_INT32 value = (TW_INT32)(float32 * 65536.0 + 0.5);
	fix32.Whole = (TW_INT16)((value >> 16) & 0xFFFFL);
	fix32.Frac = (TW_INT16)(value & 0xFFFFL);
	return fix32;
}

inline float ScannerTwain::fix32ToFloat(TW_FIX32 fix32) {
	return float(fix32.Whole) + float(fix32.Frac) / (1 << 16);
}
