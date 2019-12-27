/* - DownZemAll! - Copyright (C) 2019 Sebastien Vavassori
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; If not, see <http://www.gnu.org/licenses/>.
 */

#include "abstractsettings.h"

#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtCore/QDebug>

/*
 * Remark:
 * Characters '<' and '>' are unlikely to be used as value for data or directory path.
 * If a collision appears, the only risk is to reset the faulty parameter
 * to its default value.
 */
static const QLatin1String UNDEFINED ("<UNDEFINED>");
static const QLatin1String VALUE_TRUE ("<TRUE>");
static const QLatin1String VALUE_FALSE ("<FALSE>");

/*
 * Helper methods
 */
static QString boolToString(bool b) { return b ? VALUE_TRUE : VALUE_FALSE; }
static bool stringToBool(const QString &str) { return str == VALUE_TRUE; }

static QString intToString(int value) { return QString::number(value); }
static int stringToInt(const QString &str) { return str.toInt(); }


struct AbstractSettings::SettingsItem
{
    AbstractSettings::KeyType keyType;
    QString key;
    QString value;
    QString defaultValue;
};

AbstractSettings::AbstractSettings(QObject *parent) : QObject(parent)
{
}

AbstractSettings::~AbstractSettings()
{
    qDeleteAll(m_items);
}

/******************************************************************************
 ******************************************************************************/
/*!
 * \brief Restore the default settings
 */
void AbstractSettings::beginRestoreDefault()
{
    m_default = true;
}

void AbstractSettings::endRestoreDefault()
{
    m_default = false;
}

void AbstractSettings::readSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("Preference"));
    foreach (auto item, m_items) {
        const QString value = settings.value(uniqueRegisterKey(item), UNDEFINED).toString();
        item->value = (value != UNDEFINED) ? value : item->defaultValue;
    }
    settings.endGroup();
    emit changed();
}

void AbstractSettings::writeSettings()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("Preference"));
    foreach (auto item, m_items) {
        const QString name = uniqueRegisterKey(item);
        if (item->value != item->defaultValue || settings.contains(name)) {
            settings.setValue(name, item->value);
        }
    }
    settings.endGroup();
}

/******************************************************************************
 ******************************************************************************/
QString AbstractSettings::uniqueRegisterKey(const SettingsItem *item) const
{
    Q_ASSERT(item != Q_NULLPTR);
    switch (item->keyType) {
    case BOOL:
        return  QString("%0_bool").arg(item->key);
    case INTEGER:
        return  QString("%0_int").arg(item->key);
    case STRING:
        return item->key;
    default:
        Q_UNREACHABLE();
        break;
    }
}

/******************************************************************************
 ******************************************************************************/
void AbstractSettings::addDefaultSettingBool(const QString &key, bool defaultValue)
{
    _q_addDefaultSetting(key, boolToString(defaultValue), KeyType::BOOL);
}

bool AbstractSettings::getSettingBool(const QString &key) const
{
    return stringToBool(_q_getSetting(key, KeyType::BOOL));
}

void AbstractSettings::setSettingBool(const QString &key, bool value)
{
    _q_setSetting(key, boolToString(value), KeyType::BOOL);
}

/******************************************************************************
 ******************************************************************************/
void AbstractSettings::addDefaultSettingInt(const QString &key, int defaultValue)
{
    _q_addDefaultSetting(key, intToString(defaultValue), KeyType::INTEGER);
}

int AbstractSettings::getSettingInt(const QString &key) const
{
    return stringToInt(_q_getSetting(key, KeyType::INTEGER));
}

void AbstractSettings::setSettingInt(const QString &key, int value)
{
    _q_setSetting(key, intToString(value), KeyType::INTEGER);
}

/******************************************************************************
 ******************************************************************************/
void AbstractSettings::addDefaultSettingString(const QString &key, const QString &defaultValue)
{
    _q_addDefaultSetting(key, defaultValue, KeyType::STRING);
}

QString AbstractSettings::getSettingString(const QString &key) const
{
    return _q_getSetting(key, KeyType::STRING);
}


void AbstractSettings::setSettingString(const QString &key, const QString &value)
{
    _q_setSetting(key, value, KeyType::STRING);
}

/******************************************************************************
 ******************************************************************************/
QStringList AbstractSettings::getSettingStringList(const QString &key) const
{
    QStringList ret;
    for (int i = 0; i < m_items.count(); ++i) {
        const QString subkey = QString("%0%1").arg(key).arg(i);
        foreach (auto item, m_items) {
            if (item->key == subkey) {
                ret << (m_default ? item->defaultValue : item->value);
            }
        }
    }
    return ret;
}

void AbstractSettings::addDefaultSettingStringList(const QString &key, const QStringList &defaultValue)
{
    for (int i = 0; i < defaultValue.count(); ++i) {
        const QString subkey = QString("%0%1").arg(key).arg(i);
        const QString& subvalue = defaultValue.at(i);
        addDefaultSettingString(subkey, subvalue);
    }
}

void AbstractSettings::setSettingStringList(const QString &key, const QStringList &value)
{
    for (int i = 0; i < value.count(); ++i) {
        const QString subkey = QString("%0%1").arg(key).arg(i);
        const QString& subvalue = value.at(i);
        setSettingString(subkey, subvalue);
    }
}

/******************************************************************************
 ******************************************************************************/
void AbstractSettings::_q_addDefaultSetting(const QString &key,
                                            const QString &defaultValue,
                                            KeyType keyType)
{
    if (key.isEmpty() || key == UNDEFINED) {
        throw IllegalKeyException();
    }
    if (defaultValue.isNull() || defaultValue == UNDEFINED) {
        throw IllegalValueException();
    }
    foreach (auto item, m_items) {
        if (item->keyType == keyType && item->key == key) {
            item->defaultValue = defaultValue;
            return;
        }
    }
    auto item = new SettingsItem();
    item->key = key;
    item->keyType = keyType;
    item->defaultValue = defaultValue;
    m_items.append(item);
}

QString AbstractSettings::_q_getSetting(const QString &key, KeyType keyType) const
{
    if (key.isEmpty() || key == UNDEFINED) {
        throw IllegalKeyException();
    }
    foreach (auto item, m_items) {
        if (item->key == key) {
            if (item->keyType != keyType) {
                throw WrongTypeException();
            }
            return m_default ? item->defaultValue : item->value;
        }
    }
    throw MissingKeyException();
}

void AbstractSettings::_q_setSetting(const QString &key,
                                     const QString &value,
                                     KeyType keyType)
{
    if (key.isEmpty() || key == UNDEFINED) {
        throw IllegalKeyException();
    }
    if (value.isNull() || value == UNDEFINED) {
        throw IllegalValueException();
    }
    foreach (auto item, m_items) {
        if (item->key == key) {
            if (item->keyType != keyType) {
                throw WrongTypeException();
            }
            if (item->value != value) {
                item->value = value;
                emit changed();
            }
            return;
        }
    }
    throw MissingKeyException();
}
