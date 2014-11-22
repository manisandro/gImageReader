/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScanBackendTwain.hh
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

#ifndef SCANBACKENDTWAIN_HH
#define SCANBACKENDTWAIN_HH

#include <QCoreApplication>
#include <QDir>
#include <QLibrary>
#include <QMutex>
#include <QWaitCondition>
#include <cstring>
#ifdef Q_OS_WIN32
#include <windows.h>
#endif
#include <twain.h>

#include "ScanBackend.hh"
#include "MainWindow.hh"

typedef class ScannerTwain ScannerImpl;

class ScanBackendTwain : public ScanBackend {
public:
	bool init();
	void close();
	QList<Device> detectDevices();
	bool openDevice(const QString& device);
	void closeDevice();
	void setOptions(Job* job);
	StartStatus startDevice();
	bool getParameters();
	ReadStatus read();
	PageStatus completePage(Job* job);

private:
	QLibrary       m_dsmLib;
	DSMENTRYPROC   m_dsmEntry        = nullptr;
	TW_IDENTITY    m_appID           = {};
	TW_ENTRYPOINT  m_entryPoint      = {};

	TW_IDENTITY          m_srcID      = {};
	TW_USERINTERFACE     m_ui         = {};
	TW_UINT16            m_dsMsg = MSG_NULL;

	static ScanBackendTwain* s_instance;

#ifndef Q_OS_WIN32
	QMutex m_mutex;
	QWaitCondition  m_cond;
#endif

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

ScanBackendTwain* ScanBackendTwain::s_instance = nullptr;

bool ScanBackendTwain::init()
{
	if(m_dsmLib.isLoaded()){
		qWarning("Init already called!");
		return false;
	}

#ifdef Q_OS_WIN32
	QString twaindsm = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("twaindsm.dll");
#else
	QString twaindsm = "libtwaindsm.so.2.2.0";
#endif

	m_dsmLib.setFileName(twaindsm);
	if(!m_dsmLib.load()){
		qCritical("LoadLibrary failed on %s", qPrintable(twaindsm));
		return false;
	}

	m_dsmEntry = (DSMENTRYPROC)m_dsmLib.resolve("DSM_Entry");
	if(m_dsmEntry == nullptr){
		qCritical("GetProcAddress failed on %s::DSM_Entry", qPrintable(twaindsm));
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
#ifdef Q_OS_WIN32
	HWND hwnd = MAIN->winId();
	phwnd = &hwnd;
#endif
	if(call(nullptr, DG_CONTROL, DAT_PARENT, MSG_OPENDSM, phwnd) != TWRC_SUCCESS){
		return false;
	}
	if((m_appID.SupportedGroups & DF_DSM2) == 0){
		qCritical("Twain 2.x interface is not supported");
		return false;
	}

	// Get entry point of memory management functions
	m_entryPoint.Size = sizeof(TW_ENTRYPOINT);
	if(call(nullptr, DG_CONTROL, DAT_ENTRYPOINT, MSG_GET, &m_entryPoint) != TWRC_SUCCESS){
		return false;
	}
	s_instance = this;
	return true;
}

void ScanBackendTwain::close()
{
	closeDevice();
	if(m_appID.Id != 0){
		// State 3 to 2
		call(nullptr, DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, nullptr);
	}
	if(m_dsmLib.isLoaded()){
		m_dsmLib.unload();
	}
	s_instance = nullptr;
}

QList<ScanBackend::Device> ScanBackendTwain::detectDevices()
{
	TW_IDENTITY sourceID;
	QList<Device> sources;

	// Get the first source
	TW_UINT16 twRC = call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_GETFIRST, &sourceID);

	// Loop through all the sources
	while (twRC == TWRC_SUCCESS) {
		sources.append({sourceID.ProductName, sourceID.ProductName});
		twRC = call(nullptr, DG_CONTROL, DAT_IDENTITY, MSG_GETNEXT, &sourceID);
	}
	return sources;
}

bool ScanBackendTwain::openDevice(const QString& device)
{
	closeDevice();

	std::strncpy(m_srcID.ProductName, device.toLocal8Bit().data(), sizeof(m_srcID.ProductName));

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

void ScanBackendTwain::closeDevice()
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

void ScanBackendTwain::setOptions(Job* job)
{
	// Set the filename
	TW_SETUPFILEXFER xferFile = {};
	std::strncpy(xferFile.FileName, job->filename.toLocal8Bit().data(), sizeof(xferFile.FileName));
	xferFile.Format = TWFF_PNG;
	xferFile.VRefNum = TWON_DONTCARE16;

	if(call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_SET, &xferFile) != TWRC_SUCCESS){
		// Possibly unsupported file format. Get the default format, and try again with that one
		call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_GET, &xferFile);
		QString defaultfile = xferFile.FileName;
		job->filename = job->filename.left(job->filename.lastIndexOf('.')) + defaultfile.right(defaultfile.length() - defaultfile.lastIndexOf('.'));
		std::strncpy(xferFile.FileName, job->filename.toLocal8Bit().data(), sizeof(xferFile.FileName));
		if(call(&m_srcID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_SET, &xferFile) != TWRC_SUCCESS){
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

ScanBackend::StartStatus ScanBackendTwain::startDevice()
{
	m_dsMsg = MSG_NULL;

	m_ui.ShowUI = false;
	m_ui.ModalUI = false;
	m_ui.hParent = nullptr;;
#ifdef Q_OS_WIN32
	m_ui.hParent = MAIN->winId();
#endif

	// State 4 to 5
	if(call(&m_srcID, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, &m_ui) != TWRC_SUCCESS){
		return StartStatus::Fail;
	}

#ifdef Q_OS_WIN32
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
		m_cond.wait(&m_mutex);
	}
	m_mutex.unlock();
#endif

	if(m_dsMsg == MSG_XFERREADY) {
		return StartStatus::Success;
	}
	return StartStatus::Fail;
}

bool ScanBackendTwain::getParameters()
{
	return true;
}

ScanBackend::ReadStatus ScanBackendTwain::read()
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

ScanBackend::PageStatus ScanBackendTwain::completePage(Job* job)
{
	return PageStatus::Done;
}

/********* ScannerTwain internal Methods **********/
TW_UINT16 ScanBackendTwain::call(TW_IDENTITY* idDS, TW_UINT32 dataGroup, TW_UINT16 dataType, TW_UINT16 msg, TW_MEMREF data)
{
	TW_UINT16 rc = m_dsmEntry(&m_appID, idDS, dataGroup, dataType, msg, data);
	if(rc == TWRC_FAILURE){
		TW_STATUS status = {};
		rc = m_dsmEntry(&m_appID, idDS, DG_CONTROL, DAT_STATUS, MSG_GET, &status);
		TW_UINT16 cc = rc == TWRC_SUCCESS ? status.ConditionCode : TWCC_BUMMER;
		qCritical("Call failed with code 0x%x: DataGroup: 0x%lx, DataType = 0x%x, Msg = 0x%x", cc, dataGroup, dataType, msg);
		return TWRC_FAILURE;
	}
	return rc;
}

// TWTY_** size:                  INT8, INT16, INT32, UINT8, UINT16, UINT32, BOOL, FIX32, FRAME, STR32, STR64, STR128, STR255;
static std::size_t TWTY_Size[] = {   1,     2,     4,     1,      2,      4,    2,     4,    16,    32,    64,    128,    255};

void ScanBackendTwain::setCapability(TW_UINT16 capCode, const CapOneVal& cap)
{
	TW_CAPABILITY twCapability = {capCode, TWON_DONTCARE16, nullptr};
	TW_UINT16 rc = call(&m_srcID, DG_CONTROL, DAT_CAPABILITY, MSG_GETCURRENT, &twCapability);
	// Unsupported capability
	if(rc != TWRC_SUCCESS) {
		return;
	}
	Q_ASSERT(twCapability.ConType == TWON_ONEVALUE);

	pTW_ONEVALUE val = static_cast<pTW_ONEVALUE>(m_entryPoint.DSM_MemLock(twCapability.hContainer));
	Q_ASSERT(val->ItemType == cap.type && val->ItemType <= TWTY_STR255);
	// This works because the DSM should return a TW_ONEVALUE container allocated sufficiently large to hold
	// the value of type ItemType.
	std::memcpy(&val->Item, &cap.data, TWTY_Size[cap.type]);
	m_entryPoint.DSM_MemUnlock(twCapability.hContainer);

	call(&m_srcID, DG_CONTROL, DAT_CAPABILITY, MSG_SET, &twCapability);
	m_entryPoint.DSM_MemFree(twCapability.hContainer);
}

TW_UINT16 ScanBackendTwain::callback(TW_IDENTITY* origin, TW_IDENTITY* /*dest*/, TW_UINT32 /*DG*/, TW_UINT16 /*DAT*/, TW_UINT16 MSG, TW_MEMREF /*data*/)
{
	if(origin == nullptr || origin->Id != s_instance->m_srcID.Id) {
		return TWRC_FAILURE;
	}
	if(MSG == MSG_XFERREADY || MSG == MSG_CLOSEDSREQ || MSG == MSG_CLOSEDSOK || MSG == MSG_NULL) {
		s_instance->m_dsMsg = MSG;
#ifndef Q_OS_WIN32
		s_instance->m_mutex.lock();
		s_instance->m_cond.wakeOne();
		s_instance->m_mutex.unlock();
#endif
		return TWRC_SUCCESS;
	}
	return TWRC_FAILURE;
}

inline TW_FIX32 ScanBackendTwain::floatToFix32(float float32)
{
	TW_FIX32 fix32;
	TW_INT32 value = (TW_INT32)(float32 * 65536.0 + 0.5);
	fix32.Whole = (TW_INT16)((value >> 16) & 0xFFFFL);
	fix32.Frac = (TW_INT16)(value & 0xFFFFL);
	return fix32;
}

inline float ScanBackendTwain::fix32ToFloat(TW_FIX32 fix32)
{
	return float(fix32.Whole) + float(fix32.Frac) / (1 << 16);
}

#endif // SCANBACKENDTWAIN_HH
