/*
* Qt SerialBus plugin for candleLight / gs_usb adapters with CAN FD support
* (CANable 2.5 Candlelight firmware by ElmueSoft, e.g. Jhoinrch RH02 Elite).
*
* Based on candleLight_fw-SavvyCAN-Windows-plugin
* Copyright (c) 2023 Vladimir Alemasov
*
* This program and the accompanying materials are distributed under
* the terms of GNU General Public License version 2
* as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "plugin.hpp"
#include "device.hpp"

QList<QCanBusDeviceInfo> CandleLightFdCanBusPlugin::availableDevices(QString *errorMessage) const
{
    Q_UNUSED(errorMessage);
    return CandleApiDevices::interfaces();
}

QCanBusDevice *CandleLightFdCanBusPlugin::createDevice(const QString &interfaceName, QString *errorMessage) const
{
    const auto info = availableDevices(errorMessage);
    for (const auto &entry : info)
    {
        if (entry.name() == interfaceName)
        {
            return new CandleApiDevice(interfaceName);
        }
    }
    if (errorMessage)
    {
        *errorMessage = QStringLiteral("candleLight adapter %1 not found or in use").arg(interfaceName);
    }
    return nullptr;
}
