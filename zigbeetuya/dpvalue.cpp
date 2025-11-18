// SPDX-License-Identifier: GPL-3.0-or-later

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*
* Copyright (C) 2013 - 2024, nymea GmbH
* Copyright (C) 2024 - 2025, chargebyte austria GmbH
*
* This file is part of nymea-plugins-zigbee.
*
* nymea-plugins-zigbee is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* nymea-plugins-zigbee is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with nymea-plugins-zigbee. If not, see <https://www.gnu.org/licenses/>.
*
* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include "dpvalue.h"

#include <QDataStream>

#include "extern-plugininfo.h"

DpValue::DpValue()
{

}

DpValue::DpValue(quint8 dp, Type type, const QVariant &value, quint8 length, quint16 sequence):
    m_sequence(sequence),
    m_dp(dp),
    m_type(type),
    m_value(value),
    m_length(length)
{
}

quint16 DpValue::sequence() const
{
    return m_sequence;
}

quint8 DpValue::dp() const
{
    return m_dp;
}

DpValue::Type DpValue::type() const
{
    return m_type;
}

QVariant DpValue::value() const
{
    return m_value;
}

quint16 DpValue::length() const
{
    return m_length;
}

DpValue DpValue::fromData(const QByteArray &data)
{
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::BigEndian);
    quint16 sequence;
    stream >> sequence;
    quint8 dp;
    stream >> dp;
    quint8 dataType;
    stream >> dataType;
    quint16 len;
    stream >> len;
    QVariant value;
    switch (dataType) {
    case TypeBool: {
        quint8 tmp;
        stream >> tmp;
        value = tmp == 1 ? true : false;
        break;
    }
    case TypeUInt32: {
        quint32 tmp;
        stream >> tmp;
        value = tmp;
        break;
    }
    case TypeRaw:
    case TypeString: {
        char tmp[len];
        stream.readRawData(tmp, len);
        value = QByteArray(tmp, len);
        break;
    }
    case TypeEnum: {
        quint8 tmp;
        stream >> tmp;
        value = tmp;
        break;
    }
    case TypeFlags: {
        quint32 tmp = 0;
        switch (len) {
        case 1: {
            quint8 tmp1;
            stream >> tmp1;
            tmp = tmp1;
            break;
        }
        case 2: {
            quint16 tmp1;
            stream >> tmp1;
            tmp = tmp1;
            break;
        }
        case 4:
            stream >> tmp;
            break;
        }
        value = tmp;
        break;
    }
    default:
        qCWarning(dcZigbeeTuya()) << "Unhandled data type" << dataType << data.toHex();
        break;
    }

    return DpValue(dp, static_cast<Type>(dataType), value, len, sequence);

}

QByteArray DpValue::toData() const
{
    QByteArray ret;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QDataStream stream(&ret, QDataStream::WriteOnly);
#else
    QDataStream stream(&ret, QIODevice::WriteOnly);
#endif
    stream << m_sequence;
    stream << m_dp;
    stream << static_cast<quint8>(m_type);
    switch (m_type) {
    case TypeRaw:
    case TypeString:
        stream << m_value.toByteArray().length();
        stream.writeRawData(m_value.toByteArray().data(), m_value.toByteArray().length());
        break;
    case TypeBool:
    case TypeEnum:
        stream << static_cast<quint16>(1);
        stream << static_cast<quint8>(m_value.toUInt());
        break;
    case TypeUInt32:
        stream << static_cast<quint16>(4);
        stream << static_cast<quint32>(m_value.toUInt());
        break;
    case TypeFlags:
        stream << m_length;
        switch (m_length) {
        case 0:
            stream << static_cast<quint8>(m_value.toUInt());
            break;
        case 1:
            stream << static_cast<quint16>(m_value.toUInt());
            break;
        case 4:
            stream << static_cast<quint32>(m_value.toUInt());
            break;
        default:
            Q_ASSERT_X(m_length == 1 || m_length == 2 || m_length == 4, "DpValue", "Invalid data type length. Enum must be 1, 2 or 4 bytes length");
        }
        break;
    }

    return ret;
}

QDebug operator<<(QDebug dbg, const DpValue &value)
{
    QDebugStateSaver s(dbg);
    dbg.nospace() << "DpValue(" << value.dp() << ", Type: " << value.type() << ", Value: " << value.value() << ", length: " << value.length() << ", seq: " << value.sequence() << ")";
    return dbg;
}
