/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ScanBackendSane.hh
 * Based on code from Simple Scan, which is:
 *   Copyright (C) 2009-2013 Canonical Ltd.
 *   Author: Robert Ancell <robert.ancell@canonical.com>
 * Modifications are:
 *   Copyright (C) 2013-2014 Sandro Mani <manisandro@gmail.com>
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

#ifndef SCANBACKENDSANE_HH
#define SCANBACKENDSANE_HH

#include <QMap>
#include <QImage>
#include <QStringList>
#include <QVector>
#include <cstring>
#include <sane/sane.h>
#include <sane/saneopts.h>

#include "ScanBackend.hh"

class ScanBackendSane : public ScanBackend {
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
	SANE_Handle m_handle = nullptr;
	QVector<uint8_t> m_lineBuffer;			// Buffer for received line
	int m_nUsed = 0;					// Number of used bytes in the buffer
	int m_lineCount = 0;				// Number of read lines
	SANE_Parameters m_parameters;

	int m_height, m_rowstride;
	QVector<uint8_t> m_imgbuf;

	const SANE_Option_Descriptor* getOptionByName(const QMap<QString, int>& options, SANE_Handle, const QString& name, int& index);
	bool setDefaultOption(SANE_Handle m_handle, const SANE_Option_Descriptor *option, SANE_Int option_index);
	void setBoolOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, bool value, bool*result);
	void setIntOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, int value, int* result);
	void setFixedOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, double value, double* result);
	bool setStringOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, const QString& value, QString* result);
	bool setConstrainedStringOption(SANE_Handle m_handle, const SANE_Option_Descriptor* option, SANE_Int option_index, const QStringList& values, QString *result);
	void logOption(SANE_Int index, const SANE_Option_Descriptor* option);

	static QString getFrameModeString(SANE_Frame frame);
};

/********** ScannerSane Public Methods **********/

bool ScanBackendSane::init()
{
	SANE_Int version_code;
	SANE_Status status = sane_init(&version_code, nullptr);
	qDebug("sane_init() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		qWarning("Unable to initialize SANE backend: %s", sane_strstatus(status));
		return false;
	}
	qDebug("SANE version %d.%d.%d", SANE_VERSION_MAJOR(version_code), SANE_VERSION_MINOR(version_code), SANE_VERSION_BUILD(version_code));
	return true;
}

void ScanBackendSane::close()
{
	qDebug("sane_exit()");
	sane_exit();
}

QList<ScanBackend::Device> ScanBackendSane::detectDevices()
{
	QList<Device> devices;

	const SANE_Device** device_list = nullptr;
	SANE_Status status = sane_get_devices(&device_list, false);
	qDebug("sane_get_devices() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		qWarning("Unable to get SANE devices: %s", sane_strstatus(status));
		return devices;
	}

	for(int i = 0; device_list[i] != nullptr; ++i){
		qDebug("Device: name=\"%s\" vendor=\"%s\" model=\"%s\" type=\"%s\"",
			   device_list[i]->name, device_list[i]->vendor, device_list[i]->model, device_list[i]->type);

		Device scan_device;
		scan_device.name = device_list[i]->name;

		/* Abbreviate HP as it is a long string and does not match what is on the physical scanner */
		QString vendor = device_list[i]->vendor;
		if(vendor == "Hewlett-Packard")
			vendor = "HP";

		scan_device.label = QString("%1 %2").arg(vendor).arg(device_list[i]->model);
		scan_device.label.replace('_', " ");

		devices.append(scan_device);
	}
	return devices;
}

bool ScanBackendSane::openDevice(const QString& device)
{
	SANE_Status status = sane_open(device.toLocal8Bit().data(), &m_handle);
	qDebug("sane_open(\"%s\") -> %s", qPrintable(device), sane_strstatus(status));

	if(status != SANE_STATUS_GOOD){
		qWarning("Unable to get open device: %s", sane_strstatus(status));
		return false;
	}
	return true;
}

void ScanBackendSane::closeDevice()
{
	if(m_handle != nullptr){
		sane_close(m_handle);
		qDebug("sane_close()");
		m_handle = nullptr;
	}
}

void ScanBackendSane::setOptions(Job* job)
{
	int index = 0;
	const SANE_Option_Descriptor* option;
	QMap<QString, int> options;

	/* Build the option table */
	while((option = sane_get_option_descriptor(m_handle, index)) != nullptr){
		logOption(index, option);
		if(
			/* Ignore groups */
			option->type != SANE_TYPE_GROUP &&
			/* Option disabled */
			(option->cap & SANE_CAP_INACTIVE) == 0 &&
			/* Some options are unnamed (e.g. Option 0) */
			option->name != nullptr && strlen(option->name) > 0
		){
			options.insert(option->name, index);
		}
		++index;
	}

	/* Apply settings */

	/* Pick source */
	option = getOptionByName(options, m_handle, SANE_NAME_SCAN_SOURCE, index);
	if(option != nullptr){
		QStringList flatbed_sources = {
			"Auto",
			SANE_I18N("Auto"),
			"Flatbed",
			SANE_I18N("Flatbed"),
			"FlatBed",
			"Normal",
			SANE_I18N("Normal")
		};
		QStringList adf_sources = {
			"Automatic Document Feeder",
			SANE_I18N("Automatic Document Feeder"),
			"ADF",
			"Automatic Document Feeder(left aligned)", /* Seen in the proprietary brother3 driver */
			"Automatic Document Feeder(centrally aligned)" /* Seen in the proprietary brother3 driver */
		};
		QStringList adf_front_sources = {
			"ADF Front",
			SANE_I18N("ADF Front")
		};
		QStringList adf_back_sources = {
			"ADF Back",
			SANE_I18N("ADF Back")
		};
		QStringList adf_duplex_sources = {
			"ADF Duplex",
			SANE_I18N("ADF Duplex")
		};

		switch(job->type) {
		case ScanType::SINGLE:
			if(!setDefaultOption(m_handle, option, index))
				if(!setConstrainedStringOption(m_handle, option, index, flatbed_sources, nullptr))
					qWarning("Unable to set single page source, please file a bug");
			break;
		case ScanType::ADF_FRONT:
			if(!setConstrainedStringOption(m_handle, option, index, adf_front_sources, nullptr))
				if(!setConstrainedStringOption(m_handle, option, index, adf_sources, nullptr))
					qWarning("Unable to set front ADF source, please file a bug");
			break;
		case ScanType::ADF_BACK:
			if(!setConstrainedStringOption(m_handle, option, index, adf_back_sources, nullptr))
				if(!setConstrainedStringOption(m_handle, option, index, adf_sources, nullptr))
					qWarning("Unable to set back ADF source, please file a bug");
			break;
		case ScanType::ADF_BOTH:
			if(!setConstrainedStringOption(m_handle, option, index, adf_duplex_sources, nullptr))
				if(!setConstrainedStringOption(m_handle, option, index, adf_sources, nullptr))
					qWarning("Unable to set duplex ADF source, please file a bug");
			break;
		}
	}

	/* Scan mode (before resolution as it tends to affect that */
	option = getOptionByName(options, m_handle, SANE_NAME_SCAN_MODE, index);
	if(option != nullptr){
		/* The names of scan modes often used in drivers, as taken from the sane-backends source */
		QStringList color_scan_modes = {
			SANE_VALUE_SCAN_MODE_COLOR,
			"Color",
			"24bit Color" /* Seen in the proprietary brother3 driver */
		};
		QStringList gray_scan_modes = {
			SANE_VALUE_SCAN_MODE_GRAY,
			"Gray",
			"Grayscale",
			SANE_I18N("Grayscale"),
			"True Gray" /* Seen in the proprietary brother3 driver */
		};
		QStringList lineart_scan_modes = {
			SANE_VALUE_SCAN_MODE_LINEART,
			"Lineart",
			"LineArt",
			SANE_I18N("LineArt"),
			"Black & White",
			SANE_I18N("Black & White"),
			"Binary",
			SANE_I18N("Binary"),
			"Thresholded",
			SANE_VALUE_SCAN_MODE_GRAY,
			"Gray",
			"Grayscale",
			SANE_I18N("Grayscale"),
			"True Gray" /* Seen in the proprietary brother3 driver */
		};

		switch(job->scan_mode) {
		case ScanMode::COLOR:
			if(!setConstrainedStringOption(m_handle, option, index, color_scan_modes, nullptr))
				qWarning("Unable to set Color mode, please file a bug");
			break;
		case ScanMode::GRAY:
			if(!setConstrainedStringOption(m_handle, option, index, gray_scan_modes, nullptr))
				qWarning("Unable to set Gray mode, please file a bug");
			break;
		case ScanMode::LINEART:
			if(!setConstrainedStringOption(m_handle, option, index, lineart_scan_modes, nullptr))
				qWarning("Unable to set Lineart mode, please file a bug");
			break;
		default:
			break;
		}
	}

	/* Duplex */
	option = getOptionByName(options, m_handle, "duplex", index);
	if(option != nullptr && option->type == SANE_TYPE_BOOL)
		setBoolOption(m_handle, option, index, job->type == ScanType::ADF_BOTH, nullptr);

	/* Multi-page options */
	option = getOptionByName(options, m_handle, "batch-scan", index);
	if(option != nullptr && option->type == SANE_TYPE_BOOL)
		setBoolOption(m_handle, option, index, job->type != ScanType::SINGLE, nullptr);

	/* Disable compression, we will compress after scanning */
	option = getOptionByName(options, m_handle, "compression", index);
	if(option != nullptr){
		QStringList disable_compression_names = {
			SANE_I18N("None"),
			SANE_I18N("none"),
			"None",
			"none"
		};

		if(!setConstrainedStringOption(m_handle, option, index, disable_compression_names, nullptr))
			qWarning("Unable to disable compression, please file a bug");
	}

	/* Set resolution and bit depth */
	option = getOptionByName(options, m_handle, SANE_NAME_SCAN_RESOLUTION, index);
	if(option != nullptr){
		if(option->type == SANE_TYPE_FIXED)
			setFixedOption(m_handle, option, index, job->dpi, &job->dpi);
		else{
			int dpi;
			setIntOption(m_handle, option, index, job->dpi, &dpi);
			job->dpi = dpi;
		}
		option = getOptionByName(options, m_handle, SANE_NAME_BIT_DEPTH, index);
		if(option != nullptr && job->depth > 0)
			setIntOption(m_handle, option, index, job->depth, nullptr);
	}

	/* Always use maximum scan area - some scanners default to using partial areas.  This should be patched in sane-backends */
	option = getOptionByName(options, m_handle, SANE_NAME_SCAN_BR_X, index);
	if(option != nullptr && option->constraint_type == SANE_CONSTRAINT_RANGE){
		if(option->type == SANE_TYPE_FIXED)
			setFixedOption(m_handle, option, index, SANE_UNFIX(option->constraint.range->max), nullptr);
		else
			setIntOption(m_handle, option, index, option->constraint.range->max, nullptr);
	}
	option = getOptionByName(options, m_handle, SANE_NAME_SCAN_BR_Y, index);
	if(option != nullptr && option->constraint_type == SANE_CONSTRAINT_RANGE){
		if(option->type == SANE_TYPE_FIXED)
			setFixedOption(m_handle, option, index, SANE_UNFIX(option->constraint.range->max), nullptr);
		else
			setIntOption(m_handle, option, index, option->constraint.range->max, nullptr);
	}

	/* Set page dimensions */
	option = getOptionByName(options, m_handle, SANE_NAME_PAGE_WIDTH, index);
	if(option != nullptr && job->page_width > 0.0){
		if(option->type == SANE_TYPE_FIXED)
			setFixedOption(m_handle, option, index, job->page_width / 10.0, nullptr);
		else
			setIntOption(m_handle, option, index, job->page_width / 10, nullptr);
	}
	option = getOptionByName(options, m_handle, SANE_NAME_PAGE_HEIGHT, index);
	if(option != nullptr && job->page_height > 0.0){
		if(option->type == SANE_TYPE_FIXED)
			setFixedOption(m_handle, option, index, job->page_height / 10.0, nullptr);
		else
			setIntOption(m_handle, option, index, job->page_height / 10, nullptr);
	}
}

ScanBackend::StartStatus ScanBackendSane::startDevice()
{
	SANE_Status status = sane_start(m_handle);
	qDebug("sane_start -> %s", sane_strstatus(status));
	if(status == SANE_STATUS_GOOD)
		return StartStatus::Success;
	else if(status == SANE_STATUS_NO_DOCS)
		return StartStatus::NoDocs;
	qWarning("Unable to start device: %s", sane_strstatus(status));
	return StartStatus::Fail;
}

bool ScanBackendSane::getParameters()
{
	SANE_Status status = sane_get_parameters(m_handle, &m_parameters);
	qDebug("sane_get_parameters() -> %s", sane_strstatus(status));
	if(status != SANE_STATUS_GOOD){
		qWarning("Unable to get device parameters: %s", sane_strstatus(status));
		return false;
	}

	qDebug("Parameters: format=%s last_frame=%s bytes_per_line=%d pixels_per_line=%d lines=%d depth=%d",
			getFrameModeString(m_parameters.format).toLocal8Bit().data(),
			m_parameters.last_frame ? "SANE_TRUE" : "SANE_FALSE",
			m_parameters.bytes_per_line,
			m_parameters.pixels_per_line,
			m_parameters.lines,
			m_parameters.depth);

	m_height = qMax(1, m_parameters.lines);
	m_rowstride = m_parameters.pixels_per_line * 3; // Buffer is for a 24 bit RGB image
	m_imgbuf.resize(m_rowstride * m_parameters.lines);

	/* Prepare for read */
	m_lineBuffer.resize(m_parameters.bytes_per_line);
	m_nUsed = 0;
	m_lineCount = 0;
	return true;
}

ScanBackendSane::ReadStatus ScanBackendSane::read()
{
	/* Read as many bytes as we need to complete a line */
	int n_to_read = m_lineBuffer.size() - m_nUsed;

	SANE_Int n_read;
	unsigned char* b = &m_lineBuffer[m_nUsed];
	SANE_Status status = sane_read(m_handle, b, n_to_read, &n_read);
	qDebug("sane_read(%d) -> (%s, %d)", n_to_read, sane_strstatus(status), n_read);

	/* Completed read */
	if(status == SANE_STATUS_EOF){
		if(m_parameters.lines > 0 && m_lineCount != m_parameters.lines)
			qWarning("Scan completed with %d lines, expected %d lines", m_lineCount, m_parameters.lines);
		if(m_nUsed > 0)
			qWarning("Scan complete with %d bytes of unused data", m_nUsed);
		return ReadStatus::Done;
	}else if(status != SANE_STATUS_GOOD){
		qWarning("Unable to read frame from device: %s", sane_strstatus(status));
		return ReadStatus::Fail;
	}

	m_nUsed += n_read;

	/* If we completed a line, feed it out */
	if(m_nUsed >= m_lineBuffer.size()){
		Q_ASSERT(m_nUsed == m_parameters.bytes_per_line);
		++m_lineCount;

		// Resize image if necessary
		if(m_lineCount > m_height){
			m_height = m_lineCount;
			m_imgbuf.resize(m_rowstride * m_height);
		}

		// Write data
		int offset = (m_lineCount - 1) * m_rowstride;
		if(m_parameters.format == SANE_FRAME_GRAY){
			for(int i = 0; i < m_lineBuffer.size(); ++i){
				memset(&m_imgbuf[offset + 3*i], m_lineBuffer[i], 3);
			}
		}else if(m_parameters.format == SANE_FRAME_RGB){
			std::memcpy(&m_imgbuf[offset], &m_lineBuffer[0], m_lineBuffer.size());
		}else if(m_parameters.format == SANE_FRAME_RED){
			for(int i = 0; i < m_lineBuffer.size(); ++i){
				m_imgbuf[offset + 3*i + 0] = m_lineBuffer[i];
			}
		}else if(m_parameters.format == SANE_FRAME_GREEN){
			for(int i = 0; i < m_lineBuffer.size(); ++i){
				m_imgbuf[offset + 3*i + 1] = m_lineBuffer[i];
			}
		}else if(m_parameters.format == SANE_FRAME_BLUE){
			for(int i = 0; i < m_lineBuffer.size(); ++i){
				m_imgbuf[offset + 3*i + 2] = m_lineBuffer[i];
			}
		}

		/* Reset buffer */
		m_nUsed = 0;
		m_lineBuffer.resize(m_parameters.bytes_per_line);
	}

	return ReadStatus::Reading;
}

ScanBackend::PageStatus ScanBackendSane::completePage(Job* job)
{
	QImage(m_imgbuf.data(), m_rowstride / 3, m_height, m_rowstride, QImage::Format_RGB888).save(job->filename, "png");
	m_imgbuf.clear();
	if(!m_parameters.last_frame){
		return PageStatus::AnotherPass;
	}
	return PageStatus::Done;
}

/*********** ScannerSane Option stuff ***********/
const SANE_Option_Descriptor* ScanBackendSane::getOptionByName(const QMap<QString, int>& options, SANE_Handle, const QString& name, int &index)
{
	QMap<QString, int>::const_iterator it = options.find(name);
	if(it != options.end()){
		index = it.value();
		return sane_get_option_descriptor(m_handle, index);
	}
	index = 0;
	return nullptr;
}

bool ScanBackendSane::setDefaultOption(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index)
{
	/* Check if supports automatic option */
	if((option->cap & SANE_CAP_AUTOMATIC) == 0)
		return false;

	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_AUTO, nullptr, nullptr);
	qDebug("sane_control_option(%d, SANE_ACTION_SET_AUTO) -> %s", option_index, sane_strstatus(status));
	if(status != SANE_STATUS_GOOD)
		qWarning("Error setting default option %s: %s", option->name, sane_strstatus(status));
	return status == SANE_STATUS_GOOD;
}

void ScanBackendSane::setBoolOption(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index, bool value, bool* result)
{
	if(option->type != SANE_TYPE_BOOL){
		return;
	}

	SANE_Bool v = static_cast<SANE_Bool>(value);
	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, &v, nullptr);
	if(result) *result = static_cast<bool>(v);
	qDebug("sane_control_option(%d, SANE_ACTION_SET_VALUE, %s) -> (%s, %s)", option_index, value ? "SANE_TRUE" : "SANE_FALSE", sane_strstatus(status), result ? "SANE_TRUE" : "SANE_FALSE");
}

void ScanBackendSane::setIntOption(SANE_Handle handle, const SANE_Option_Descriptor* option, SANE_Int option_index, int value, int* result)
{
	if(option->type != SANE_TYPE_INT){
		return;
	}

	SANE_Word v = static_cast<SANE_Word>(value);
	if(option->constraint_type == SANE_CONSTRAINT_RANGE){
		if(option->constraint.range->quant != 0)
			v *= option->constraint.range->quant;
		v = qMax(option->constraint.range->min, qMin(option->constraint.range->max, v));
	}else if(option->constraint_type == SANE_CONSTRAINT_WORD_LIST){
		int distance = std::numeric_limits<int>::max();
		int nearest = 0;

		/* Find nearest value to requested */
		for(int i = 0; i < option->constraint.word_list[0]; ++i){
			int x = option->constraint.word_list[i+1];
			int d = qAbs(x - v);
			if(d < distance){
				distance = d;
				nearest = x;
			}
		}
		v = nearest;
	}

	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, &v, nullptr);
	qDebug("sane_control_option(%d, SANE_ACTION_SET_VALUE, %d) -> (%s, %d)", option_index, value, sane_strstatus(status), v);
	if(result) *result = v;
}

void ScanBackendSane::setFixedOption(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, double value, double* result)
{
	if(option->type != SANE_TYPE_FIXED){
		return;
	}

	double v = value;
	if(option->constraint_type == SANE_CONSTRAINT_RANGE){
		double min = SANE_UNFIX(option->constraint.range->min);
		double max = SANE_UNFIX(option->constraint.range->max);
		v = qMax(min, qMin(max, v));
	}else if(option->constraint_type == SANE_CONSTRAINT_WORD_LIST){
		double distance = std::numeric_limits<double>::max();
		double nearest = 0.0;

		/* Find nearest value to requested */
		for(int i = 0; i < option->constraint.word_list[0]; ++i){
			double x = SANE_UNFIX(option->constraint.word_list[i+1]);
			double d = qAbs(x - v);
			if(d < distance){
				distance = d;
				nearest = x;
			}
		}
		v = nearest;
	}

	SANE_Fixed v_fixed = SANE_FIX(v);
	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, &v_fixed, nullptr);
	qDebug("sane_control_option(%d, SANE_ACTION_SET_VALUE, %f) -> (%s, %f)", option_index, value, sane_strstatus(status), SANE_UNFIX(v_fixed));

	if(result) *result = SANE_UNFIX(v_fixed);
}

bool ScanBackendSane::setStringOption(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, const QString& value, QString* result)
{
	if(option->type != SANE_TYPE_STRING){
		return false;
	}

	char* v = new char[value.size() + 1]; // +1: \0
	QByteArray val = value.toLocal8Bit();
	strncpy(v, val.data(), val.size()+1);
	SANE_Status status = sane_control_option(handle, option_index, SANE_ACTION_SET_VALUE, v, nullptr);
	qDebug("sane_control_option(%d, SANE_ACTION_SET_VALUE, \"%s\") -> (%s, \"%s\")", option_index, val.data(), sane_strstatus(status), v);
	if(result) *result = v;
	delete[] v;
	return status == SANE_STATUS_GOOD;
}

bool ScanBackendSane::setConstrainedStringOption(SANE_Handle handle, const SANE_Option_Descriptor *option, SANE_Int option_index, const QStringList& values, QString* result)
{
	if(option->type != SANE_TYPE_STRING){
		return false;
	}
	if(option->constraint_type != SANE_CONSTRAINT_STRING_LIST){
		return false;
	}

	for(const QString& value : values){
		for(int j = 0; option->constraint.string_list[j] != nullptr; ++j){
			if(value == option->constraint.string_list[j]){
				return setStringOption(handle, option, option_index, value, result);
			}
		}
	}
	if(result) *result = "";
	return false;
}

void ScanBackendSane::logOption(SANE_Int index, const SANE_Option_Descriptor *option)
{
	QString s = QString("Option %1:").arg(index);

	if(option->name && strlen(option->name) > 0)
		s += QString(" name='%1'").arg(option->name);

	if(option->title && strlen(option->title) > 0)
		s += QString(" title='%1'").arg(option->title);

	QString typestr[] = {"bool", "int", "fixed", "string", "button", "group"};
	if(option->type <= SANE_TYPE_GROUP){
		s += QString(" type=%1").arg(typestr[option->type]);
	}else{
		s += QString(" type=%1").arg(option->type);
	}

	s += QString(" size=%1").arg(option->size);

	QString unitstr[] = {"none", "pixel", "bit", "mm", "dpi", "percent", "microseconds"};
	if(option->unit <= SANE_UNIT_MICROSECOND){
		s += QString(" unit=%1").arg(unitstr[option->unit]);
	}else{
		s += QString(" unit=%1").arg(option->unit);
	}

	switch(option->constraint_type){
	case SANE_CONSTRAINT_RANGE:
		if(option->type == SANE_TYPE_FIXED)
			s += QString(" min=%1, max=%2, quant=%3").arg(SANE_UNFIX(option->constraint.range->min)).arg(SANE_UNFIX(option->constraint.range->max)).arg(option->constraint.range->quant);
		else
			s += QString(" min=%1, max=%2, quant=%3").arg(option->constraint.range->min).arg(option->constraint.range->max).arg(option->constraint.range->quant);
		break;
	case SANE_CONSTRAINT_WORD_LIST:
		s += " values=[";
		for(int i = 0; i < option->constraint.word_list[0]; ++i){
			if(i > 0)
				s += ", ";
			if(option->type == SANE_TYPE_INT)
				s += QString("%1").arg(option->constraint.word_list[i+1]);
			else
				s += QString("%1").arg(SANE_UNFIX(option->constraint.word_list[i+1]));
		}
		s += "]";
		break;
	case SANE_CONSTRAINT_STRING_LIST:
		s += " values=[";
		for(int i = 0; option->constraint.string_list[i] != nullptr; ++i){
			if(i > 0)
				s += ", ";
			s += QString("\"%1\"").arg(option->constraint.string_list[i]);
		}
		s += "]";
		break;
	default:
		break;
	}

	int cap = option->cap;
	if(cap != 0){
		s += " cap=";
		if((cap & SANE_CAP_SOFT_SELECT) != 0){
			s += "soft-select,";
			cap &= ~SANE_CAP_SOFT_SELECT;
		}
		if((cap & SANE_CAP_HARD_SELECT) != 0){
			s += "hard-select,";
			cap &= ~SANE_CAP_HARD_SELECT;
		}
		if((cap & SANE_CAP_SOFT_DETECT) != 0){
			s += "soft-detect,";
			cap &= ~SANE_CAP_SOFT_DETECT;
		}
		if((cap & SANE_CAP_EMULATED) != 0){
			s += "emulated,";
			cap &= ~SANE_CAP_EMULATED;
		}
		if((cap & SANE_CAP_AUTOMATIC) != 0){
			s += "automatic,";
			cap &= ~SANE_CAP_AUTOMATIC;
		}
		if((cap & SANE_CAP_INACTIVE) != 0){
			s += "inactive,";
			cap &= ~SANE_CAP_INACTIVE;
		}
		if((cap & SANE_CAP_ADVANCED) != 0){
			s += "advanced,";
			cap &= ~SANE_CAP_ADVANCED;
		}
		/* Unknown capabilities */
		if(cap != 0){
			s += QString("unknown (%1)").arg(cap);
		}
	}

	qDebug("%s", qPrintable(s));
	if(option->desc != nullptr)
		qDebug("  Description: %s", option->desc);
}


/****************** Enum to string ******************/

QString ScanBackendSane::getFrameModeString(SANE_Frame frame)
{
	QString framestr[] = {"SANE_FRAME_GRAY", "SANE_FRAME_RGB", "SANE_FRAME_RED", "SANE_FRAME_GREEN", "SANE_FRAME_BLUE"};
	if(frame <= SANE_FRAME_BLUE){
		return framestr[frame];
	}else{
		return QString("SANE_FRAME(%1)").arg(frame);
	}
}

#endif // SCANBACKENDSANE_HH
