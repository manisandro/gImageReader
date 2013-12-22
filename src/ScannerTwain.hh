/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScannerTwain.hh
 * Copyright (C) 2013 Sandro Mani <manisandro@gmail.com>
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

#ifndef SCANNERTWAIN_HH
#define SCANNERTWAIN_HH

#include "Scanner.hh"
#include "MainWindow.hh"
#ifdef G_OS_WIN32
#include <windows.h>
#include <gdk/gdkwin32.h>
#endif
#include <twain.h>
#include <dlfcn.h>

typedef class ScannerTwain ScannerImpl;

class ScannerTwain : public Scanner {
private:
	void*          m_dlHandle       = nullptr;
	DSMENTRYPROC   m_dsmEntry        = nullptr;
	TW_IDENTITY    m_appID           = {};
	TW_ENTRYPOINT  m_entryPoint      = {};

	TW_IDENTITY          m_srcID      = {};
	TW_USERINTERFACE     m_ui         = {};
	TW_UINT16            m_dsMsg = MSG_NULL;
#ifndef G_OS_WIN32
	Glib::Threads::Mutex m_mutex;
	Glib::Threads::Cond  m_cond;
#endif

	bool initBackend();
	void closeBackend();
	std::vector<ScanDevice> detectDevices();
	bool openDevice(const std::string& device);
	void closeDevice();
	void setOptions(ScanJob* job);
	StartStatus startDevice();
	bool getParameters();
	ReadStatus read();
	PageStatus completePage(ScanJob* job);


	struct CapOneVal {
		CapOneVal() = default;
		CapOneVal(TW_UINT16 _type, std::int32_t _integer) : type(_type) { data.integer = _integer; }
		CapOneVal(TW_UINT16 _type, TW_FIX32 _fix32) : type(_type) { data.fix32 = _fix32; }
		CapOneVal(TW_UINT16 _type, TW_FIX32(&_frame)[4]) : type(_type) { std::memcpy(&data.frame[0], &_frame[0], sizeof(data.frame)); }
		CapOneVal(TW_UINT16 _type, const char* _string) : type(_type) { std::strncpy(&data.string[0], &_string[0], sizeof(data.string)); }
		TW_UINT16 type;
		union {
			std::uint32_t integer;
			TW_FIX32 fix32;
			TW_FIX32 frame[4];
			char string[256];
		} data;
	};

	TW_UINT16 call(TW_IDENTITY* idDS, TW_UINT32 dataGroup, TW_UINT16 dataType, TW_UINT16 msg, TW_MEMREF data);
	void setCapability(TW_UINT16 capCode, const CapOneVal& cap);
	static PASCAL TW_UINT16 callback(TW_IDENTITY* origin, TW_IDENTITY* dest, TW_UINT32 DG, TW_UINT16 DAT, TW_UINT16 MSG, TW_MEMREF data);
	static inline float fix32ToFloat(TW_FIX32 fix32);
	static inline TW_FIX32 floatToFix32(float float32);
};

/********** ScannerTwain interface methods **********/

bool ScannerTwain::initBackend()
{
	if(m_dlHandle != nullptr){
		g_critical("Init already called!");
		return false;
	}

#ifdef G_OS_WIN32
	gchar* dir = g_win32_get_package_installation_directory_of_module(0);
	std::string twaindsm = Glib::build_filename(dir, "bin", "twaindsm.dll");
#else
	std::string twaindsm = "libtwaindsm.so.2.2.0";
#endif

	m_dlHandle = dlopen(twaindsm.c_str(), RTLD_LAZY);
	if(m_dlHandle == nullptr){
		g_critical("LoadLibrary failed on %s", twaindsm.c_str());
		return false;
	}

	m_dsmEntry = (DSMENTRYPROC)dlsym(m_dlHandle, "DSM_Entry");
	if(m_dsmEntry == nullptr){
		g_critical("GetProcAddress failed on %s::DSM_Entry", twaindsm.c_str());
		return false;
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

	// State 2 to 3
	TW_MEMREF phwnd = nullptr;
#ifdef G_OS_WIN32
	HWND hwnd = gdk_win32_window_get_impl_hwnd(MAIN->getWindow()->get_window()->gobj());
	phwnd = &hwnd;
#endif
	if(call(nullptr, DG_CONTROL, DAT_PARENT, MSG_OPENDSM, phwnd) != TWRC_SUCCESS){
		return false;
	}
	if((m_appID.SupportedGroups & DF_DSM2) == 0){
		g_critical("Twain 2.x interface is not supported");
		return false;
	}

	// Get entry point of memory management functions
	m_entryPoint.Size = sizeof(TW_ENTRYPOINT);
	if(call(nullptr, DG_CONTROL, DAT_ENTRYPOINT, MSG_GET, &m_entryPoint) != TWRC_SUCCESS){
		return false;
	}
	return true;
}

void ScannerTwain::closeBackend()
{
	closeDevice();
	if(m_appID.Id != 0){
		// State 3 to 2
		call(nullptr, DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, nullptr);
	}
	if(m_dlHandle != nullptr){
		dlclose(m_dlHandle);
		m_dlHandle = nullptr;
	}
}

std::vector<Scanner::ScanDevice> ScannerTwain::detectDevices()
{
	TW_IDENTITY sourceID;
	std::vector<ScanDevice> sources;

	// Get the first source
	TW_UINT16 twRC = call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_GETFIRST, &sourceID);

	// Loop through all the sources
	while (twRC == TWRC_SUCCESS) {
		sources.push_back({sourceID.ProductName, sourceID.ProductName});
		twRC = call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_GETNEXT, &sourceID);
	}
	return sources;
}

bool ScannerTwain::openDevice(const std::string &device)
{
	closeDevice();

	std::strncpy(m_srcID.ProductName, device.c_str(), sizeof(m_srcID.ProductName));

	// State 3 to 4
	if(call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_OPENDS, &m_srcID) != TWRC_SUCCESS){
		return false;
	}

	// Register callback
	TW_CALLBACK cb = {};
	cb.CallBackProc = reinterpret_cast<TW_MEMREF>(callback);
	call(&m_srcID, DG_CONTROL, DAT_CALLBACK, MSG_REGISTER_CALLBACK, &cb);

	return true;
}

void ScannerTwain::closeDevice()
{
	if(m_srcID.Id != 0){
		TW_PENDINGXFERS twPendingXFers = {};
		// State 7/6 to 5
		call(&m_srcID, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, &twPendingXFers);
		//	State 5 to 4
		call(&m_srcID, DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS, &m_ui);
		//	State 4 to 3
		call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS, &m_srcID);
		m_srcID = {};
	}
}

void ScannerTwain::setOptions(ScanJob* job)
{
	// Set the filename
	TW_SETUPFILEXFER xferFile = {};
	std::strncpy(xferFile.FileName, job->filename.c_str(), sizeof(xferFile.FileName));
	xferFile.Format = TWFF_PNG;
	xferFile.VRefNum = TWON_DONTCARE16;

	if(call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_SET, &xferFile) == TWRC_CHECKSTATUS){
		// Probably unsupported file format. Get the default format, and try again with that one
		call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_GET, &xferFile);
		std::string defaultfile = xferFile.FileName;
		job->filename = job->filename.substr(0, job->filename.rfind('.')) + defaultfile.substr(defaultfile.rfind('.'));
		std::strncpy(xferFile.FileName, job->filename.c_str(), sizeof(xferFile.FileName));
		if(call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_SET, &xferFile) == TWRC_CHECKSTATUS){
			// If we failed again, just return the default file...
			job->filename = defaultfile;
		}
	}

	// Set capabilities
	setCapability(CAP_XFERCOUNT, CapOneVal(TWTY_INT16, 1));
	setCapability(ICAP_XRESOLUTION, CapOneVal(TWTY_FIX32, floatToFix32(job->dpi)));
	setCapability(ICAP_YRESOLUTION, CapOneVal(TWTY_FIX32, floatToFix32(job->dpi)));
	setCapability(ICAP_SUPPORTEDSIZES, CapOneVal(TWTY_UINT16, TWSS_NONE)); // Maximum scan area
	setCapability(ICAP_BITDEPTH, CapOneVal(TWTY_UINT16, job->depth));

	if(job->scan_mode != ScanMode::DEFAULT){
		if(job->scan_mode == ScanMode::COLOR)
			setCapability(ICAP_PIXELTYPE, CapOneVal(TWTY_UINT16, TWPT_RGB));
		else if(job->scan_mode == ScanMode::GRAY)
			setCapability(ICAP_PIXELTYPE, CapOneVal(TWTY_UINT16, TWPT_GRAY));
		else if(job->scan_mode == ScanMode::LINEART)
			setCapability(ICAP_PIXELTYPE, CapOneVal(TWTY_UINT16, TWPT_BW));
	}

	// TODO: ADF, duplex, paper size
}

Scanner::StartStatus ScannerTwain::startDevice()
{
	m_dsMsg = MSG_NULL;

	m_ui.ShowUI = false;
	m_ui.ModalUI = false;
	m_ui.hParent = nullptr;;
#ifdef G_OS_WIN32
	m_ui.hParent = gdk_win32_window_get_impl_hwnd(MAIN->getWindow()->get_window()->gobj());
#endif

	// State 4 to 5
	if(call(&m_srcID, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, &m_ui) != TWRC_SUCCESS){
		return StartStatus::Fail;
	}

#ifdef G_OS_WIN32
	while(m_dsMsg == 0) {
		TW_EVENT twEvent = {0};

		MSG msg;
		if(GetMessage((LPMSG)&msg, NULL, 0, 0) == 0) {
			break; // WM_QUIT
		}
		twEvent.pEvent = (TW_MEMREF)&msg;

		twEvent.TWMessage = MSG_NULL;
		TW_UINT16 twRC = m_dsmEntry(&m_appID, &m_srcID, DG_CONTROL, DAT_EVENT, MSG_PROCESSEVENT, (TW_MEMREF)&twEvent);

		if(twRC == TWRC_DSEVENT) {
			if(twEvent.TWMessage == MSG_XFERREADY || twEvent.TWMessage == MSG_CLOSEDSREQ || twEvent.TWMessage == MSG_CLOSEDSOK || twEvent.TWMessage == MSG_NULL){
				m_dsMsg = twEvent.TWMessage;
			}
		}
	}
#else
	m_mutex.lock();
	while(m_dsMsg == 0){
		m_cond.wait(m_mutex);
	}
	m_mutex.unlock();
#endif

	if(m_dsMsg == MSG_XFERREADY) {
		return StartStatus::Success;
	}
	return StartStatus::Fail;
}

bool ScannerTwain::getParameters()
{
	return true;
}

Scanner::ReadStatus ScannerTwain::read()
{
	TW_UINT16 twRC = call(&m_srcID, DG_IMAGE, DAT_IMAGEFILEXFER, MSG_GET, nullptr);
	TW_PENDINGXFERS twPendingXFers = {};
	call(&m_srcID, DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER, &twPendingXFers);
	if(twPendingXFers.Count != 0) {
		// Discard any additional pending transfers
		std::memset(&twPendingXFers, 0, sizeof(twPendingXFers));
		call(&m_srcID, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, &twPendingXFers);
	}
	return twRC == TWRC_FAILURE ? ReadStatus::Fail : ReadStatus::Done;
}

Scanner::PageStatus ScannerTwain::completePage(ScanJob* job)
{
	return Scanner::PageStatus::Done;
}

/********* ScannerTwain internal Methods **********/
TW_UINT16 ScannerTwain::call(TW_IDENTITY* idDS, TW_UINT32 dataGroup, TW_UINT16 dataType, TW_UINT16 msg, TW_MEMREF data)
{
	TW_UINT16 rc = m_dsmEntry(&m_appID, idDS, dataGroup, dataType, msg, data);
	if(rc == TWRC_FAILURE){
		TW_STATUS status = {};
		rc = m_dsmEntry(&m_appID, idDS, DG_CONTROL, DAT_STATUS, MSG_GET, &status);
		TW_UINT16 cc = rc == TWRC_SUCCESS ? status.ConditionCode : TWCC_BUMMER;
		g_critical("Call failed with code 0x%x: DataGroup: 0x%x, DataType = 0x%x, Msg = 0x%x", cc, dataGroup, dataType, msg);
	}
	return rc;
}

// TWTY_** size:                  INT8, INT16, INT32, UINT8, UINT16, UINT32, BOOL, FIX32, FRAME, STR32, STR64, STR128, STR255;
static std::size_t TWTY_Size[] = {   1,     2,     4,     1,      2,      4,    2,     4,    16,    32,    64,    128,    255};

void ScannerTwain::setCapability(TW_UINT16 capCode, const CapOneVal& cap)
{
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

TW_UINT16 ScannerTwain::callback(TW_IDENTITY* origin, TW_IDENTITY* /*dest*/, TW_UINT32 /*DG*/, TW_UINT16 /*DAT*/, TW_UINT16 MSG, TW_MEMREF /*data*/)
{
	ScannerTwain* instance = static_cast<ScannerTwain*>(Scanner::get_instance());

	if(origin == nullptr || origin->Id != instance->m_srcID.Id) {
		return TWRC_FAILURE;
	}
	if(MSG == MSG_XFERREADY || MSG == MSG_CLOSEDSREQ || MSG == MSG_CLOSEDSOK || MSG == MSG_NULL) {
		instance->m_dsMsg = MSG;
#ifndef G_OS_WIN32
		instance->m_mutex.lock();
		instance->m_cond.signal();
		instance->m_mutex.unlock();
#endif
		return TWRC_SUCCESS;
	}
	return TWRC_FAILURE;
}

inline TW_FIX32 ScannerTwain::floatToFix32(float float32)
{
	TW_FIX32 fix32;
	TW_INT32 value = (TW_INT32)(float32 * 65536.0 + 0.5);
	fix32.Whole = (TW_INT16)((value >> 16) & 0xFFFFL);
	fix32.Frac = (TW_INT16)(value & 0xFFFFL);
	return fix32;
}

inline float ScannerTwain::fix32ToFloat(TW_FIX32 fix32)
{
	return float(fix32.Whole) + float(fix32.Frac) / (1 << 16);
}

#endif // SCANNERTWAIN_HH
