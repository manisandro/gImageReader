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
#include "twain.h"
#include "MainWindow.hh"
#include <dlfcn.h>
#include <cstdint>
#include <vector>

#ifdef G_OS_WIN32
#include <gdk/gdkwin32.h>
typedef HWND WindowHandle_t;
#else
#include <gdk/gdkx.h>
typedef XID WindowHandle_t;
#endif

class ScannerTwain : public Scanner {
private:
	void* m_dllHandle = nullptr;
	DSMENTRYPROC m_dsmAddr;
	TW_IDENTITY m_appID;
	WindowHandle_t m_hwnd;

	TW_IDENTITY m_objID;
	TW_USERINTERFACE m_twUI;

	struct CapVal {
		TW_UINT16 type;
		union {
			std::uint32_t integer;
			char string[256];
			TW_FIX32 fix32;
			TW_FIX32 frame[4];
		} data;
	};

	bool setTransferFilename(const std::string& filename, TW_UINT16 format = TWFF_PNG);
	bool setCapability(TW_UINT16 capability, CapVal value);
	bool requestAcquire(bool showUI, bool modalUI);
	bool getImageInfo(TW_IMAGEINFO& twImageInfo);
	bool transferImageByFile(TW_UINT16& remaining);
	static TW_FIX32 floatToFix32(double x);

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
};

/********** ScannerTwain Public Methods **********/

bool ScannerTwain::initBackend()
{
	m_dllHandle = dlopen("TWAIN_32.DLL", RTLD_LAZY);
	if(m_dllHandle == NULL){
		g_debug("LoadLibrary failed");
		return false;
	}
	m_dsmAddr = (DSMENTRYPROC)dlsym(m_dllHandle, (char*)1);
	if(m_dsmAddr == NULL) {
		g_debug("GetProcAddress failed");
		return false;
	}

	m_appID.Id = 0; // SourceManager will assign proper value
	m_appID.Version.MajorNum = 1;
	m_appID.Version.MinorNum = 0;
	m_appID.Version.Language = TWLG_ENGLISH_USA;
	m_appID.Version.Country  = TWCY_USA;
	std::strcpy(m_appID.Version.Info, PACKAGE_VERSION);
	m_appID.ProtocolMajor = TWON_PROTOCOLMAJOR;
	m_appID.ProtocolMinor = TWON_PROTOCOLMINOR;
	m_appID.SupportedGroups =  DG_IMAGE | DG_CONTROL;
	std::strcpy(m_appID.Manufacturer, "Sandro Mani");
	std::strcpy(m_appID.ProductFamily, "-");
	std::strcpy(m_appID.ProductName, "gImageReader");

#ifdef G_OS_WIN32
	m_hwnd = gdk_win32_window_get_impl_hwnd(MAIN->getWindow()->get_window()->gobj());
#else
	m_hwnd = gdk_x11_window_get_xid(MAIN->getWindow()->get_window()->gobj());
#endif
	TW_UINT16 twRC = m_dsmAddr(&m_appID, NULL, DG_CONTROL, DAT_PARENT, MSG_OPENDSM, (TW_MEMREF)&m_hwnd);
	if(twRC != TWRC_SUCCESS) {
		g_debug("DAT_PARENT:OPENDSM failed: %d", twRC);
		return false;
	}
	return true;
}

void ScannerTwain::closeBackend()
{
	m_dsmAddr(&m_appID, NULL, DG_CONTROL, DAT_PARENT, MSG_CLOSEDSM, (TW_MEMREF)&m_hwnd);
	dlclose(m_dllHandle);
}

std::vector<Scanner::ScanDevice> ScannerTwain::detectDevices()
{
	std::vector<ScanDevice> devices;

	//  Get the first source
	int twRC = m_dsmAddr(&m_appID, NULL, DG_CONTROL, DAT_IDENTITY, MSG_GETFIRST, NULL);

	//  Loop through all the sources
	while(twRC == TWRC_SUCCESS){
		devices.push_back({m_appID.ProductName, m_appID.ProductName});
		twRC = m_dsmAddr(&m_appID, NULL, DG_CONTROL, DAT_IDENTITY, MSG_GETNEXT, NULL);
	}
	return devices;
}

bool ScannerTwain::openDevice(const std::string &device)
{
	if(m_objID.Id != 0){
		g_debug("There is already an opened source");
		return false;
	}
	std::strcpy(m_objID.ProductName, device.c_str());

	TW_UINT16 twRC = m_dsmAddr(&m_appID, NULL, DG_CONTROL, DAT_IDENTITY, MSG_OPENDS, (TW_MEMREF)&m_objID);
	if(twRC != TWRC_SUCCESS) {
		g_debug("DAT_IDENTITY:OPENDS failed for source %s: %d", m_objID.ProductName, twRC);
		m_objID.Id = 0;
		return false;
	}
	return true;
}

void ScannerTwain::closeDevice()
{
	if(m_objID.Id == 0){
		g_debug("There is no source to close");
		return;
	}
	TW_PENDINGXFERS twPendingXFers;

	// State 7/6->5
	m_dsmAddr(&m_appID, &m_objID, DG_CONTROL, DAT_PENDINGXFERS, MSG_RESET, (TW_MEMREF)&twPendingXFers);
	//  State 5->4
	m_dsmAddr(&m_appID, &m_objID, DG_CONTROL, DAT_USERINTERFACE, MSG_DISABLEDS, (TW_MEMREF)&m_twUI);
	//  State 4->3
	m_dsmAddr(&m_appID, NULL, DG_CONTROL, DAT_IDENTITY, MSG_CLOSEDS, (TW_MEMREF)&m_objID);

	m_objID.Id = 0;
}

void ScannerTwain::setOptions(ScanJob* job)
{
	setTransferFilename(job->filename);

	CapVal res; res.type = TWTY_FIX32; res.data.fix32 = floatToFix32(job->dpi);
	setCapability(ICAP_XRESOLUTION, res);
	setCapability(ICAP_YRESOLUTION, res);

	CapVal paper; res.type = TWTY_UINT16; res.data.integer = TWSS_A4;
	setCapability(ICAP_SUPPORTEDSIZES, paper);

	if(job->scan_mode != ScanMode::DEFAULT){
		CapVal pixel; pixel.type = TWTY_UINT16;
		if(job->scan_mode == ScanMode::COLOR)
			pixel.data.integer = TWPT_RGB;
		else if(job->scan_mode == ScanMode::GRAY)
			pixel.data.integer = TWPT_GRAY;
		else if(job->scan_mode == ScanMode::LINEART)
			pixel.data.integer = TWPT_BW;
		setCapability(ICAP_PIXELTYPE, pixel);
	}

	CapVal depth; depth.type = TWTY_UINT16; depth.data.integer = job->depth;
	setCapability(ICAP_BITDEPTH, depth);

	// TODO: ADF, duplex, paper size
}

Scanner::StartStatus ScannerTwain::startDevice()
{
	if(requestAcquire(true, true)){
		return StartStatus::Success;
	}
	return StartStatus::Fail;
}

bool ScannerTwain::getParameters()
{
//	TW_IMAGEINFO twImageInfo;
//	 getImageInfo(twImageInfo);
	return true;
}

Scanner::ReadStatus ScannerTwain::read()
{
	TW_UINT16 remaining = 0;
	if(!transferImageByFile(remaining)){
		return ReadStatus::Fail;
	}
	return ReadStatus::Done;
}

Scanner::PageStatus ScannerTwain::completePage(ScanJob* job)
{
	return Scanner::PageStatus::Done;
}

/********* ScannerTwain Private Methods **********/

bool ScannerTwain::setTransferFilename(const std::string& filename, TW_UINT16 format)
{
	TW_SETUPFILEXFER twSetupFileXfer;
	twSetupFileXfer.Format = format;
	std::strcpy(twSetupFileXfer.FileName, filename.c_str());

	TW_UINT16 twRC = m_dsmAddr(&m_appID, &m_objID, DG_CONTROL, DAT_SETUPFILEXFER, MSG_SET, (TW_MEMREF)&twSetupFileXfer);
	if(twRC != TWRC_SUCCESS){
		g_debug("DAT_SETUPFILEXFER:MSG_SET failed: %d", twRC);
		return false;
	}
	return true;
}

bool ScannerTwain::setCapability(TW_UINT16 capability, CapVal value)
{
	TW_CAPABILITY twCapability;
	twCapability.Cap = capability;
	twCapability.ConType = TWON_ONEVALUE;
	twCapability.hContainer = &value;
	TW_UINT16 twRC = m_dsmAddr(&m_appID, &m_objID, DG_CONTROL, DAT_CAPABILITY, MSG_SET, (TW_MEMREF)&twCapability);
	if(twRC != TWRC_SUCCESS){
		g_debug("DAT_CAPABILITY:MSG_SET failed: %d (Cap = %d, Type = %d)", twRC, capability, value.type);
		return false;
	}
	return true;
}

bool ScannerTwain::requestAcquire(bool showUI, bool modalUI)
{
	m_twUI.ShowUI = showUI;
	m_twUI.ModalUI = modalUI;
	m_twUI.hParent = m_hwnd;
	TW_UINT16 twRC = m_dsmAddr(&m_appID, &m_objID, DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDS, (TW_MEMREF)&m_twUI);
	if(twRC != TWRC_SUCCESS) {
		g_debug("DAT_USERINTERFACE:MSG_ENABLEDS failed: %d", twRC);
		return false;
	}
	return true;
}

bool ScannerTwain::getImageInfo(TW_IMAGEINFO& twImageInfo)
{
	TW_UINT16 twRC = m_dsmAddr(&m_appID, &m_objID, DG_IMAGE, DAT_IMAGEINFO, MSG_GET, (TW_MEMREF)&twImageInfo);
	if(twRC != TWRC_SUCCESS){
		g_debug("DAT_IMAGEINFO:MSG_GET failed: %d", twRC);
		return false;
	}
	return true;
}

bool ScannerTwain::transferImageByFile(TW_UINT16& remaining)
{
	// Transfer the image
	TW_UINT16 twRC = m_dsmAddr(&m_appID, &m_objID, DG_IMAGE, DAT_IMAGEFILEXFER, MSG_GET, NULL);
	if(twRC != TWRC_XFERDONE && twRC != TWRC_CANCEL){
		g_debug("DAT_IMAGEFILEXFER:MSG_GET failed: %d", twRC);
		return false;
	}

	// Acknowledge end of the transfer
	TW_PENDINGXFERS twPendingXFers;
	m_dsmAddr(&m_appID, &m_objID, DG_CONTROL, DAT_PENDINGXFERS, MSG_ENDXFER, (TW_MEMREF)&twPendingXFers);
	remaining = (twRC != TWRC_XFERDONE ? 0 : twPendingXFers.Count);
	return true;
}

TW_FIX32 ScannerTwain::floatToFix32(double x){
	TW_FIX32 fix;
	TW_INT32 value = (TW_INT32) (x * 65536.0 + 0.5);
	fix.Whole = (TW_INT16)((value >> 16) & 0xFFFFL);
	fix.Frac = (TW_INT16)(value & 0x0000FFFFL);
	return fix;
}

#endif // SCANNERTWAIN_HH
