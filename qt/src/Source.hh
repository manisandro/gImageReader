/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * Source.hh
 * Copyright (C) 2013-2017 Sandro Mani <manisandro@gmail.com>
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

#include <QString>

class QImage;

class Source {
public:
	Source(const QString& path, const QString& displayname, bool isTemp = false);
	virtual ~Source();

	const QString& getPath() const{ return m_path; }
	const QString& getDisplayName() const{ return m_displayName; }

	virtual int getPageCount() const{ return 1; }

	int getPageRotation(int page) const{ return m_rotation[page]; }
	void setPageRotation(int page, double rotation) { m_rotation[page] = rotation; }

	int getBrightness() const{ return m_brightness; }
	void setBrightness(int brightness){ m_brightness = brightness; }
	int getContrast() const{ return m_contrast; }
	void setContrast(int contrast){ m_contrast = contrast; }
	int getResolution() const{ return m_resolution; }
	void setResolution(int resolution){ m_resolution = resolution; }
	bool getInvertColors() const{ return m_invert; }
	void setInvertColors(bool invert){ m_invert = invert; }

	typedef RenderHandle RenderHandle_t;

	virtual RenderHandle_t getRenderHandle() const = 0;
	QImage renderPage(const RenderHandle_t& handle, int page, int resolution) const;
	virtual void releaseRenderHandle(RenderHandle_t handle) const = 0;

private:
	QString m_path;
	QString m_displayname;
	QByteArray m_password;
	bool m_isTemp;
	int m_brightness = 0;
	int m_contrast = 0;
	int m_resolution = -1;
	int m_page = 1;
	QVector<double> m_rotation;
	bool m_invert = false;

	virtual QImage renderPageImpl(const RenderHandle_t& handle, int page, int resolution) const = 0;

	struct RenderHandle {
		virtual ~RenderHandle() = default;
	};
};

class ImageSource : public Source {

}
