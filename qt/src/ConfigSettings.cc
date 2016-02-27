/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ConfigSettings.cc
 * Copyright (C) 2013-2016 Sandro Mani <manisandro@gmail.com>
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

TableSetting::TableSetting(const QString& key, QTableWidget* table)
	: AbstractSetting(key), m_table(table)
{
	QString str = QSettings().value(m_key).toString();
	m_table->setRowCount(0);
	int nCols = m_table->columnCount();

	for(const QString& row : str.split(';')){
		int colidx = 0;
		QStringList cols = row.split(',');
		if(cols.size() != nCols){
			continue;
		}
		int rowidx = m_table->rowCount();
		m_table->insertRow(rowidx);
		for(const QString& col : cols){
			m_table->setItem(rowidx, colidx++, new QTableWidgetItem(col));
		}
	}
}

void TableSetting::serialize()
{
	// Serialized string has format a11,a12,a13;a21,a22,a23;...
	QStringList rows;
	int nCols = m_table->columnCount();
	for(int row = 0, nRows = m_table->rowCount(); row < nRows; ++row){
		QStringList cols;
		for(int col = 0; col < nCols; ++col)
		{
			QTableWidgetItem* item = m_table->item(row, col);
			cols.append(item ? item->text() : QString());
		}
		rows.append(cols.join(","));
	}
	QSettings().setValue(m_key, QVariant::fromValue(rows.join(";")));
	emit changed();
}
