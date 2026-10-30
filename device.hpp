#pragma once

#include <QtSerialBus/QCanBusDevice>
#include <QElapsedTimer>
#include "devices.hpp"

class CandleApiListener;
class CandleApiInterface;

class CandleApiDevice : public CandleApiDevices
{
    Q_OBJECT

public:
    explicit CandleApiDevice(QString name);
    virtual ~CandleApiDevice();

public:
    bool open() override;
    void close() override;
    bool writeFrame(const QCanBusFrame &msg) override;
    QString interpretErrorFrame(const QCanBusFrame &errorFrame) override;

    void readFrames();           // runs in the listener thread

private:
    bool configureTiming(candle_handle handle, bool fd, uint32_t bitrate, uint32_t dataBitrate);
    void loadSettings();
    uint64_t timestampUs(uint32_t devTs);
    void fatalReadError(const QString &text);

    CandleApiListener *_lstn = nullptr;
    CandleApiInterface *_iface = nullptr;
    QString _name;

    bool _fdMode = false;
    bool _receiveOwn = false;
    double _nomSamplePoint = 0.875;
    double _dataSamplePoint = 0.75;
    uint32_t _iniDataBitrate = 2000000;   // used when SavvyCAN does not pass DataBitRateKey (Qt5 builds)
    bool _iniBitrateSwitch = true;        // BRS for >8 byte frames sent without FD flag
    bool _iniForceFd = false;

    uint32_t _deviceTicksStart = 0;
    uint32_t _lastDevTs = 0;
    uint64_t _wrapOffset = 0;
    bool _hwTimestamps = false;
    QElapsedTimer _hostTimer;
};
