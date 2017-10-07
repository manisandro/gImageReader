/***************************************************************************
 *   Copyright (C) 2006 by Pino Toscano <toscano.pino@tiscali.it>          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef _KDJVU_
#define _KDJVU_

#include <QColor>
#include <QImage>
#include <QList>
#include <QPolygon>
#include <QRect>
#include <QVariant>
#include <QVector>

/**
 * @brief Qt (KDE) encapsulation of the DjVuLibre
 */
class KDjVu
{
    public:
        KDjVu();
        ~KDjVu();

        /**
         * A DjVu page.
         */
        class Page
        {
            friend class KDjVu;

            public:
                ~Page();

                int width() const;
                int height() const;
                int dpi() const;
                int orientation() const;

            private:
                Page();

                int m_width;
                int m_height;
                int m_dpi;
                int m_orientation;
        };

        /**
         * Opens the file \p fileName, closing the old one if necessary.
         */
        bool openFile( const QString & fileName );
        /**
         * Close the file currently opened, if any.
         */
        void closeFile();

        /**
         * The pages of the current document, or an empty vector otherwise.
         * \note KDjVu handles the pages, so you don't need to delete them manually
         * \return a vector with the pages of the current document
         */
        const QVector<KDjVu::Page*> &pages() const;

        /**
         * Check if the image for the specified \p page with the specified
         * \p width, \p height and \p rotation is already in cache, and returns
         * it. If not, a null image is returned.
         */
        QImage image( int page, int width, int height, int rotation );

        /**
         * Enable or disable the internal rendered pages cache.
         */
        void setCacheEnabled( bool enable );

        /**
        * Return the page number of the page whose title is \p name.
        */
        int pageCount() const;
    private:
        class Private;
        Private * const d;
};

#endif
