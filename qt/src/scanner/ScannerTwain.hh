/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScannerTwain.hh
 * Copyright (C) 2013-2024 Sandro Mani <manisandro@gmail.com>
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

#ifndef SCANNER_TWAIN_HH
#define SCANNER_TWAIN_HH

#include "../Scanner.hh"
#include <cstdlib>
#include <cstring>
#include <QLibrary>
#include <QMutex>
#include <QWaitCondition>
#ifdef Q_OS_WIN32
#include <windows.h>
#include <QAbstractNativeEventFilter>
#endif
#include <twain.h>

class ScannerTwain : public Scanner {
	Q_OBJECT
public:
	void init() override;
	void redetect() override;
	void scan(const Params& params) override;
	void cancel() override {
		m_cancel = true;
	}
	void close() override;

private:
	QLibrary m_dsmLib;
	DSMENTRYPROC m_dsmEntry = nullptr;
	TW_IDENTITY m_appID = {};
	TW_ENTRYPOINT m_entryPoint = {};

	TW_IDENTITY  m_srcID = {};
	TW_USERINTERFACE m_ui = {};
	TW_UINT16 m_dsMsg = MSG_NULL;
	bool m_dsQuit = false;
	bool m_cancel = false;
	bool m_useCallback = false;
	int m_state = 0;

	static ScannerTwain* s_instance;

#ifndef Q_OS_WIN32
	QMutex m_mutex;
	QWaitCondition  m_cond;
#endif

	struct CapOneVal {
		CapOneVal() = default;
		CapOneVal(TW_UINT16 _type, std::int32_t _integer) : type(_type) {
			data.integer = _integer;
		}
		CapOneVal(TW_UINT16 _type, TW_FIX32 _fix32) : type(_type) {
			data.fix32 = _fix32;
		}
		CapOneVal(TW_UINT16 _type, TW_FIX32(&_frame)[4]) : type(_type) {
			std::memcpy(&data.frame[0], &_frame[0], sizeof(data.frame));
		}
		CapOneVal(TW_UINT16 _type, const char* _string) : type(_type) {
			std::strncpy(&data.string[0], &_string[0], sizeof(data.string));
		}
		TW_UINT16 type;
		union {
			std::uint32_t integer;
			TW_FIX32 fix32;
			TW_FIX32 frame[4];
			char string[256];
		} data;
	};

	void doStop();
	void failScan(const QString& errorString);
	TW_UINT16 call(TW_IDENTITY* idDS, TW_UINT32 dataGroup, TW_UINT16 dataType, TW_UINT16 msg, TW_MEMREF data);
	void setCapability(TW_UINT16 capCode, const CapOneVal& cap);
#ifdef Q_OS_WIN32
	bool saveDIB(TW_MEMREF hImg, const QString& filename);
	class NativeEventFilter : public QAbstractNativeEventFilter {
	public:
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;
#else
		bool nativeEventFilter(const QByteArray& eventType, void* message, long* result);
#endif
	};
	NativeEventFilter m_eventFilter;
#endif
	static PASCAL TW_UINT16 callback(TW_IDENTITY* origin, TW_IDENTITY* dest, TW_UINT32 DG, TW_UINT16 DAT, TW_UINT16 MSG, TW_MEMREF data);
	static inline float fix32ToFloat(TW_FIX32 fix32);
	static inline TW_FIX32 floatToFix32(float float32);
};

typedef ScannerTwain ScannerImpl;

#endif // SCANNER_TWAIN_HH
