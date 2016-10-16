/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ConfigSettings.hh
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

#ifndef CONFIGSETTINGS_HH
#define CONFIGSETTINGS_HH

#include <QAction>
#include <QAbstractButton>
#include <QComboBox>
#include <QFontDialog>
#include <QSettings>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>

class AbstractSetting : public QObject {
	Q_OBJECT
public:
	AbstractSetting(const QString& key)
		: m_key(key) {}
	virtual ~AbstractSetting() {}
	const QString& key() const {
		return m_key;
	}

public slots:
	virtual void serialize() {}

signals:
	void changed();

protected:
	QString m_key;
};

template <class T>
class VarSetting : public AbstractSetting {
public:
	VarSetting(const QString& key, const T& defaultValue = T())
		: AbstractSetting(key), m_defaultValue(QVariant::fromValue(defaultValue)) {}

	T getValue() const {
		return QSettings().value(m_key, m_defaultValue).value<T>();
	}
	void setValue(const T& value) {
		QSettings().setValue(m_key, QVariant::fromValue(value));
		emit changed();
	}

private:
	QVariant m_defaultValue;
};

class FontSetting : public AbstractSetting {
	Q_OBJECT
public:
	FontSetting(const QString& key, QFontDialog* dialog, const QString& defaultValue)
		: AbstractSetting(key), m_dialog(dialog) {
		QFont font;
		if(font.fromString(QSettings().value(m_key, QVariant::fromValue(defaultValue)).toString())) {
			m_dialog->setCurrentFont(font);
		}
		QObject::connect(dialog, SIGNAL(fontSelected(QFont)), this, SLOT(serialize()));
	}
	QFont getValue() const {
		return m_dialog->currentFont();
	}

public slots:
	void serialize() {
		QSettings().setValue(m_key, QVariant::fromValue(m_dialog->selectedFont().toString()));
		emit changed();
	}

private:
	QFontDialog* m_dialog;
};

class SwitchSetting : public AbstractSetting {
	Q_OBJECT
public:
	SwitchSetting(const QString& key, QAbstractButton* button, bool defaultState = false)
		: AbstractSetting(key), m_button(button) {
		button->setChecked(QSettings().value(m_key, QVariant::fromValue(defaultState)).toBool());
		connect(button, SIGNAL(toggled(bool)), this, SLOT(serialize()));
	}
	void setValue(bool value) {
		m_button->setChecked(value);
	}
	bool getValue() const {
		return m_button->isChecked();
	}

public slots:
	void serialize() {
		QSettings().setValue(m_key, QVariant::fromValue(m_button->isChecked()));
		emit changed();
	}

private:
	QAbstractButton* m_button;
};

class ActionSetting : public AbstractSetting {
	Q_OBJECT
public:
	ActionSetting(const QString& key, QAction* button, bool defaultState = false)
		: AbstractSetting(key), m_button(button) {
		button->setChecked(QSettings().value(m_key, QVariant::fromValue(defaultState)).toBool());
		connect(button, SIGNAL(toggled(bool)), this, SLOT(serialize()));
	}
	void setValue(bool value) {
		m_button->setChecked(value);
	}
	bool getValue() const {
		return m_button->isChecked();
	}

public slots:
	void serialize() {
		QSettings().setValue(m_key, QVariant::fromValue(m_button->isChecked()));
		emit changed();
	}

private:
	QAction* m_button;
};

class ComboSetting : public AbstractSetting {
	Q_OBJECT
public:
	ComboSetting(const QString& key, QComboBox* combo, int defaultIndex = 0)
		: AbstractSetting(key), m_combo(combo) {
		combo->setCurrentIndex(QSettings().value(m_key, QVariant::fromValue(defaultIndex)).toInt());
		connect(combo, SIGNAL(currentIndexChanged(int)), this, SLOT(serialize()));
	}

public slots:
	void serialize() {
		QSettings().setValue(m_key, QVariant::fromValue(m_combo->currentIndex()));
		emit changed();
	}

private:
	QComboBox* m_combo;
};

class SpinSetting : public AbstractSetting {
	Q_OBJECT
public:
	SpinSetting(const QString& key, QSpinBox* spin, int defaultValue = 0)
		: AbstractSetting(key), m_spin(spin) {
		spin->setValue(QSettings().value(m_key, QVariant::fromValue(defaultValue)).toInt());
		connect(spin, SIGNAL(valueChanged(int)), this, SLOT(serialize()));
	}

public slots:
	void serialize() {
		QSettings().setValue(m_key, QVariant::fromValue(m_spin->value()));
		emit changed();
	}

private:
	QSpinBox* m_spin;
};

class TableSetting : public AbstractSetting {
	Q_OBJECT
public:
	TableSetting(const QString& key, QTableWidget* table);

public slots:
	void serialize();

private:
	QTableWidget* m_table;
};

#endif // CONFIGSETTINGS_HH
