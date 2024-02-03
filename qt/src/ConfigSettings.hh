/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * ConfigSettings.hh
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

#ifndef CONFIGSETTINGS_HH
#define CONFIGSETTINGS_HH

#include <QAction>
#include <QAbstractButton>
#include <QComboBox>
#include <QFontDialog>
#include <QFontComboBox>
#include <QLineEdit>
#include <QSettings>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>

class AbstractSetting;

#define ADD_SETTING(...) ((new __VA_ARGS__)->setParent(this))

class ConfigSettings {
public:
	template<class T>
	static T* get(const QString& key) {
		auto it = s_settings.find(key);
		return it == s_settings.end() ? nullptr : static_cast<T*>(it.value());
	}

private:
	friend class AbstractSetting;
	static QMap<QString, AbstractSetting*> s_settings;

	static void add(AbstractSetting* setting);
	static void remove(const QString& key);
};


class AbstractSetting : public QObject {
	Q_OBJECT
public:
	AbstractSetting(const QString& key)
		: m_key(key) {
		ConfigSettings::add(this);
	}
	virtual ~AbstractSetting() {
		ConfigSettings::remove(m_key);
	}
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
		return QSettings().value(m_key, m_defaultValue).template value<T>();
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
		connect(dialog, &QFontDialog::fontSelected, this, &FontSetting::serialize);
	}
	QFont getValue() const {
		return m_dialog->currentFont();
	}

public slots:
	void serialize() override {
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
		connect(button, &QAbstractButton::toggled, this, &SwitchSetting::serialize);
	}
	void setValue(bool value) {
		m_button->setChecked(value);
	}
	bool getValue() const {
		return m_button->isChecked();
	}

public slots:
	void serialize() override {
		QSettings().setValue(m_key, QVariant::fromValue(m_button->isChecked()));
		emit changed();
	}

private:
	QAbstractButton* m_button;
};

class ActionSetting : public AbstractSetting {
	Q_OBJECT
public:
	ActionSetting(const QString& key, QAction* action, bool defaultState = false)
		: AbstractSetting(key), m_action(action) {
		action->setChecked(QSettings().value(m_key, QVariant::fromValue(defaultState)).toBool());
		connect(action, &QAction::toggled, this, &ActionSetting::serialize);
	}
	void setValue(bool value) {
		m_action->setChecked(value);
	}
	bool getValue() const {
		return m_action->isChecked();
	}

public slots:
	void serialize() override {
		QSettings().setValue(m_key, QVariant::fromValue(m_action->isChecked()));
		emit changed();
	}

private:
	QAction* m_action;
};

class ComboSetting : public AbstractSetting {
	Q_OBJECT
public:
	ComboSetting(const QString& key, QComboBox* combo, int defaultIndex = 0)
		: AbstractSetting(key), m_combo(combo) {
		combo->setCurrentIndex(QSettings().value(m_key, QVariant::fromValue(defaultIndex)).toInt());
		connect(combo, qOverload<int>(&QComboBox::currentIndexChanged), this, &ComboSetting::serialize);
	}

public slots:
	void serialize() override {
		QSettings().setValue(m_key, QVariant::fromValue(m_combo->currentIndex()));
		emit changed();
	}

private:
	QComboBox* m_combo;
};

class FontComboSetting : public AbstractSetting {
	Q_OBJECT
public:
	FontComboSetting(const QString& key, QFontComboBox* combo, const QFont defaultFont = QFont())
		: AbstractSetting(key), m_combo(combo) {
		combo->setCurrentFont(QFont(QSettings().value(m_key, defaultFont.family()).toString()));
		connect(combo, &QFontComboBox::currentFontChanged, this, &FontComboSetting::serialize);
	}

public slots:
	void serialize() override {
		QSettings().setValue(m_key, m_combo->currentFont().family());
		emit changed();
	}

private:
	QFontComboBox* m_combo;
};

class SpinSetting : public AbstractSetting {
	Q_OBJECT
public:
	SpinSetting(const QString& key, QSpinBox* spin, int defaultValue = 0)
		: AbstractSetting(key), m_spin(spin) {
		spin->setValue(QSettings().value(m_key, QVariant::fromValue(defaultValue)).toInt());
		connect(spin, qOverload<int>(&QSpinBox::valueChanged), this, &SpinSetting::serialize);
	}

public slots:
	void serialize() override {
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
	void serialize() override;

private:
	QTableWidget* m_table;
};

class LineEditSetting : public AbstractSetting {
	Q_OBJECT
public:
	LineEditSetting(const QString& key, QLineEdit* lineEdit, const QString& defaultValue = "")
		: AbstractSetting(key), m_lineEdit(lineEdit) {
		lineEdit->setText(QSettings().value(m_key, QVariant::fromValue(defaultValue)).toString());
		connect(lineEdit, &QLineEdit::textChanged, this, &LineEditSetting::serialize);
	}

public slots:
	void serialize() override {
		QSettings().setValue(m_key, QVariant::fromValue(m_lineEdit->text()));
		emit changed();
	}

private:
	QLineEdit* m_lineEdit;
};

#endif // CONFIGSETTINGS_HH
