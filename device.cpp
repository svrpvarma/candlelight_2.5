#include "device.hpp"
#include "interface.hpp"
#include "listener.hpp"
#include "timing.hpp"
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QMetaObject>
#include <QtSerialBus/QCanBusFrame>
#include "api/candle.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

/* SavvyCAN passes these bits in QCanBusDevice::UserKey (serialbusconnection.h) */
static const quint32 USERKEY_SILENT_MODE        = 0x00000001;
static const quint32 USERKEY_LOOPBACK_MODE      = 0x00000002;
static const quint32 USERKEY_NO_AUTO_RETRANSMIT = 0x00000004;

CandleApiDevice::CandleApiDevice(QString name) :
    _name(name)
{
    qDebug() << "New CandleApiDevice is created:" << name;
    _iface = findInterface(name);
}

CandleApiDevice::~CandleApiDevice()
{
    if (state() == ConnectedState)
    {
        close();
    }
    qDebug() << "CandleApiDevice is destroyed.";
}

/* Optional settings file next to the plugin DLL:  canbus\qtcandlelightfd.ini
 *
 *   [timing]
 *   NominalSamplePoint=87.5
 *   DataSamplePoint=75.0
 *   [bus]
 *   DataBitrate=2000000     ; used when SavvyCAN does not pass a data rate (Qt5 builds)
 *   BitrateSwitch=true      ; BRS for >8 byte frames that SavvyCAN sends without FD flag
 *   ForceFD=false           ; open in CAN FD mode even if the CAN FD box is not ticked
 *
 * Sample points must match the other nodes on a CAN FD bus. */
void CandleApiDevice::loadSettings()
{
    QString iniPath;
#ifdef Q_OS_WIN
    HMODULE mod = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                           GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&CandleTiming::calcFd), &mod))
    {
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(mod, path, MAX_PATH);
        if (n > 0 && n < MAX_PATH)
        {
            QFileInfo fi(QString::fromWCharArray(path, int(n)));
            iniPath = fi.absolutePath() + "/qtcandlelightfd.ini";
        }
    }
#endif
    if (iniPath.isEmpty() || !QFileInfo::exists(iniPath))
    {
        qDebug() << "candleLightFD: no settings file, using default sample points";
        return;
    }
    QSettings s(iniPath, QSettings::IniFormat);
    double nsp = s.value("timing/NominalSamplePoint", _nomSamplePoint * 100.0).toDouble();
    double dsp = s.value("timing/DataSamplePoint", _dataSamplePoint * 100.0).toDouble();
    if (nsp >= 50.0 && nsp < 100.0) _nomSamplePoint = nsp / 100.0;
    if (dsp >= 50.0 && dsp < 100.0) _dataSamplePoint = dsp / 100.0;
    const uint dbr = s.value("bus/DataBitrate", _iniDataBitrate).toUInt();
    if (dbr) _iniDataBitrate = dbr;
    _iniBitrateSwitch = s.value("bus/BitrateSwitch", _iniBitrateSwitch).toBool();
    _iniForceFd = s.value("bus/ForceFD", _iniForceFd).toBool();
    qDebug() << "candleLightFD settings:" << iniPath << "nominal SP" << nsp << "data SP" << dsp;
}

bool CandleApiDevice::configureTiming(candle_handle handle, bool fd, uint32_t bitrate, uint32_t dataBitrate)
{
    QString err;
    if (fd)
    {
        candle_capability_ext_t caps;
        if (!candle_channel_get_capabilities_ext(handle, 0, &caps))
        {
            setError(tr("This adapter's firmware does not support CAN FD "
                        "(flash the CANable 2.5 Candlelight firmware)."), ConfigurationError);
            return false;
        }
        CandleBitTiming n, d;
        bool same = false;
        if (!CandleTiming::calcFd(caps, bitrate, _nomSamplePoint, dataBitrate, _dataSamplePoint,
                                  n, d, &same, &err))
        {
            setError(err, ConfigurationError);
            return false;
        }
        qDebug() << "candleLightFD nominal:" << n.toString();
        qDebug() << "candleLightFD data:   " << d.toString() << (same ? "(same prescaler)" : "(prescalers differ)");
        candle_bittiming_t tn = n.toCandle();
        candle_bittiming_t td = d.toCandle();
        if (!candle_channel_set_timing(handle, 0, &tn) || !candle_channel_set_data_timing(handle, 0, &td))
        {
            setError(tr("Adapter rejected the bit timing (error %1)").arg(int(candle_dev_last_error(handle))),
                     ConfigurationError);
            return false;
        }
        return true;
    }

    candle_capability_t caps;
    candle_channel_get_capabilities(handle, 0, &caps);
    CandleBitTiming n;
    if (!CandleTiming::calc(caps.fclk_can, CandleTiming::nominalLimits(caps), bitrate, _nomSamplePoint, n, &err))
    {
        setError(err, ConfigurationError);
        return false;
    }
    qDebug() << "candleLightFD classic:" << n.toString();
    candle_bittiming_t tn = n.toCandle();
    if (!candle_channel_set_timing(handle, 0, &tn))
    {
        setError(tr("Adapter rejected the bit timing (error %1)").arg(int(candle_dev_last_error(handle))),
                 ConfigurationError);
        return false;
    }
    return true;
}

bool CandleApiDevice::open()
{
    if (_iface == nullptr)
    {
        _iface = findInterface(_name);
    }
    if (_iface == nullptr)
    {
        setError(tr("Adapter %1 not found").arg(_name), ConnectionError);
        return false;
    }

    setState(ConnectingState);
    loadSettings();

    candle_handle handle = _iface->getHandle();
    if (!candle_dev_open(handle))
    {
        setError(tr("Cannot open %1 - is another program using it? (error %2)")
                     .arg(_name).arg(int(candle_dev_last_error(handle))), ConnectionError);
        setState(UnconnectedState);
        return false;
    }

    QVariant v = configurationParameter(BitRateKey);
    uint32_t bitrate = v.isValid() && v.toUInt() ? v.toUInt() : 500000;
    _fdMode = configurationParameter(CanFdKey).toBool() || _iniForceFd;
    v = configurationParameter(DataBitRateKey);
    uint32_t dataBitrate = v.isValid() && v.toUInt() ? v.toUInt() : _iniDataBitrate;
    _receiveOwn = configurationParameter(ReceiveOwnKey).toBool();
    const quint32 user = configurationParameter(UserKey).toUInt();

    candle_channel_stop(handle, 0);     // make sure the channel is in reset before timing changes

    if (!configureTiming(handle, _fdMode, bitrate, dataBitrate))
    {
        candle_dev_close(handle);
        setState(UnconnectedState);
        return false;
    }

    uint32_t flags = CANDLE_MODE_BERR_REPORTING;
    if (_fdMode)
    {
        flags |= CANDLE_MODE_FD;
    }
    if (user & USERKEY_SILENT_MODE)
    {
        flags |= CANDLE_MODE_LISTEN_ONLY;
    }
    if ((user & USERKEY_LOOPBACK_MODE) || configurationParameter(LoopbackKey).toBool())
    {
        flags |= CANDLE_MODE_LOOP_BACK;
    }
    if (user & USERKEY_NO_AUTO_RETRANSMIT)
    {
        flags |= CANDLE_MODE_ONE_SHOT;
    }

    _deviceTicksStart = 0;
    _lastDevTs = 0;
    _wrapOffset = 0;
    _hwTimestamps = candle_dev_get_timestamp_us(handle, &_deviceTicksStart);
    _lastDevTs = _deviceTicksStart;
    _hostTimer.start();

    if (!candle_channel_start(handle, 0, flags))
    {
        setError(tr("Adapter refused to start the CAN channel (error %1)")
                     .arg(int(candle_dev_last_error(handle))), ConnectionError);
        candle_dev_close(handle);
        setState(UnconnectedState);
        return false;
    }

    _lstn = new CandleApiListener(*this);
    _lstn->startThread();

    setState(ConnectedState);
    return true;
}

void CandleApiDevice::close()
{
    setState(ClosingState);
    if (_lstn)
    {
        _lstn->stopThread();
        delete _lstn;
        _lstn = nullptr;
    }
    if (_iface)
    {
        candle_handle handle = _iface->getHandle();
        candle_channel_stop(handle, 0);
        candle_dev_close(handle);
    }
    setState(UnconnectedState);
}

bool CandleApiDevice::writeFrame(const QCanBusFrame &msg)
{
    if (state() != ConnectedState || _iface == nullptr)
    {
        setError(tr("Device is not connected"), WriteError);
        return false;
    }
    if (!msg.isValid())
    {
        setError(tr("Invalid CAN frame"), WriteError);
        return false;
    }

    candle_frame_t frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = msg.frameId() & 0x1FFFFFFF;

    switch (msg.frameType())
    {
    case QCanBusFrame::DataFrame:
        break;
    case QCanBusFrame::RemoteRequestFrame:
        frame.can_id |= CANDLE_ID_RTR;
        break;
    default:
        setError(tr("Only data and remote frames can be sent"), WriteError);
        return false;
    }
    if (msg.hasExtendedFrameFormat())
    {
        frame.can_id |= CANDLE_ID_EXTENDED;
    }

    const QByteArray payload = msg.payload();
    const int len = payload.size();
    // Qt5 SavvyCAN may send >8 byte frames without setting the FD flag
    const bool sendAsFd = msg.hasFlexibleDataRateFormat() || (_fdMode && len > 8);
    if (sendAsFd)
    {
        if (!_fdMode)
        {
            setError(tr("CAN FD frame on a bus that is not configured for CAN FD"), WriteError);
            return false;
        }
        if (len > 64)
        {
            setError(tr("CAN FD payload is limited to 64 bytes"), WriteError);
            return false;
        }
        const bool brs = msg.hasFlexibleDataRateFormat() ? msg.hasBitrateSwitch() : _iniBitrateSwitch;
        frame.flags = CANDLE_FLAG_FD | (brs ? CANDLE_FLAG_BRS : 0);
        frame.can_dlc = candle_len_to_dlc(uint8_t(len));
    }
    else
    {
        if (len > 8)
        {
            setError(tr("Classic CAN payload is limited to 8 bytes"), WriteError);
            return false;
        }
        frame.can_dlc = uint8_t(len);
    }
    memcpy(frame.data, payload.constData(), size_t(len));   // FD padding bytes stay 0

    if (!candle_frame_send(_iface->getHandle(), 0, &frame))
    {
        setError(tr("USB write failed (error %1)").arg(int(candle_dev_last_error(_iface->getHandle()))),
                 WriteError);
        return false;
    }
    emit framesWritten(1);
    return true;
}

QString CandleApiDevice::interpretErrorFrame(const QCanBusFrame &errorFrame)
{
    if (errorFrame.frameType() != QCanBusFrame::ErrorFrame)
    {
        return QString();
    }
    const QCanBusFrame::FrameErrors e = errorFrame.error();
    const QByteArray d = errorFrame.payload();
    QStringList parts;
    if (e & QCanBusFrame::BusOffError)              parts << "Bus off";
    if (e & QCanBusFrame::MissingAcknowledgmentError) parts << "No ACK";
    if (e & QCanBusFrame::LostArbitrationError)     parts << "Lost arbitration";
    if (e & QCanBusFrame::ProtocolViolationError)   parts << "Protocol violation";
    if (e & QCanBusFrame::TransceiverError)         parts << "Transceiver error";
    if (e & QCanBusFrame::BusError)                 parts << "Bus error";
    if (e & QCanBusFrame::ControllerRestartError)   parts << "Controller restarted";
    if ((e & QCanBusFrame::ControllerError) && d.size() > 1)
    {
        const quint8 c = quint8(d.at(1));
        if (c & 0x01) parts << "RX overflow";
        if (c & 0x02) parts << "TX overflow";
        if (c & 0x04) parts << "RX warning";
        if (c & 0x08) parts << "TX warning";
        if (c & 0x10) parts << "RX passive";
        if (c & 0x20) parts << "TX passive";
        if (c & 0x40) parts << "Back to error active";
    }
    if (d.size() >= 8)
    {
        parts << QString("TEC=%1 REC=%2").arg(int(quint8(d.at(6)))).arg(int(quint8(d.at(7))));
    }
    return parts.join(", ");
}

uint64_t CandleApiDevice::timestampUs(uint32_t devTs)
{
    if (!_hwTimestamps || devTs == 0)
    {
        return uint64_t(_hostTimer.nsecsElapsed() / 1000);
    }
    if (devTs < _lastDevTs && (_lastDevTs - devTs) > 0x80000000u)
    {
        _wrapOffset += 0x100000000ull;           // 32 bit microsecond counter wrapped (~71 min)
    }
    _lastDevTs = devTs;
    int64_t t = int64_t(_wrapOffset + devTs) - int64_t(_deviceTicksStart);
    return t > 0 ? uint64_t(t) : 0;
}

void CandleApiDevice::fatalReadError(const QString &text)
{
    // called from the listener thread: hand over to the device's thread
    QMetaObject::invokeMethod(this, [this, text]() {
        setError(text, ConnectionError);
        disconnectDevice();
    }, Qt::QueuedConnection);
}

void CandleApiDevice::readFrames()
{
    candle_frame_t frame;
    const unsigned int timeout_ms = 10;
    candle_handle handle = _iface->getHandle();
    QVector<QCanBusFrame> batch;
    int consecutiveErrors = 0;

    while (_lstn->_shouldBeRunning)
    {
        if (!candle_frame_read(handle, &frame, timeout_ms))
        {
            const candle_err_t e = candle_dev_last_error(handle);
            if (e != CANDLE_ERR_READ_TIMEOUT && ++consecutiveErrors > 50)
            {
                fatalReadError(tr("Adapter %1 stopped responding (unplugged?)").arg(_name));
                break;
            }
            if (!batch.isEmpty())
            {
                enqueueReceivedFrames(batch);
                batch.clear();
            }
            continue;
        }
        consecutiveErrors = 0;

        const candle_frametype_t type = candle_frame_type(&frame);
        if (type == CANDLE_FRAMETYPE_ECHO && !_receiveOwn)
        {
            continue;
        }

        QCanBusFrame msg;
        const uint64_t ts = timestampUs(candle_frame_timestamp_us(&frame));
        msg.setTimeStamp(QCanBusFrame::TimeStamp(qint64(ts / 1000000), qint64(ts % 1000000)));

        if (type == CANDLE_FRAMETYPE_ERROR)
        {
            msg.setFrameType(QCanBusFrame::ErrorFrame);
            msg.setError(QCanBusFrame::FrameErrors(int(candle_frame_id(&frame) & QCanBusFrame::AnyError)));
            msg.setPayload(QByteArray(reinterpret_cast<const char*>(candle_frame_data(&frame)), 8));
        }
        else
        {
            msg.setFrameType(candle_frame_is_rtr(&frame) ? QCanBusFrame::RemoteRequestFrame
                                                         : QCanBusFrame::DataFrame);
            msg.setExtendedFrameFormat(candle_frame_is_extended_id(&frame));
            msg.setFrameId(candle_frame_id(&frame));
            const uint8_t len = candle_frame_data_len(&frame);
            msg.setPayload(QByteArray(reinterpret_cast<const char*>(candle_frame_data(&frame)), len));
            if (candle_frame_is_fd(&frame))
            {
                msg.setFlexibleDataRateFormat(true);
                msg.setBitrateSwitch((frame.flags & CANDLE_FLAG_BRS) != 0);
                msg.setErrorStateIndicator((frame.flags & CANDLE_FLAG_ESI) != 0);
            }
            if (type == CANDLE_FRAMETYPE_ECHO)
            {
                msg.setLocalEcho(true);
            }
        }

        batch.append(msg);
        if (batch.size() >= 64)
        {
            enqueueReceivedFrames(batch);
            batch.clear();
        }
    }
    if (!batch.isEmpty())
    {
        enqueueReceivedFrames(batch);
    }
}
