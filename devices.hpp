#pragma once

#include <QtSerialBus/QCanBusDevice>
#include <QtSerialBus/QCanBusDeviceInfo>
#include "api/candle.h"

class CandleApiInterface;

#define CANDLE_PLUGIN_KEY "candleLightFD"

class CandleApiDevices : public QCanBusDevice
{
public:
    static QList<QCanBusDeviceInfo> interfaces();
    static CandleApiInterface *findInterface(QString name);

private:
    static CandleApiInterface *findInterface(candle_handle dev);
    static QList<CandleApiInterface *> _ifaces;
};
