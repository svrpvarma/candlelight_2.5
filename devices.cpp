#include "devices.hpp"
#include "interface.hpp"
#include <QDebug>
#include <QSet>

QList<CandleApiInterface *> CandleApiDevices::_ifaces;

/* Port names:
 *   gs_usb_<serial>  when the adapter reports a USB serial number (the STM32
 *                    unique ID on CANable firmware) - stable across USB ports,
 *                    so SavvyCAN finds the same adapter after re-plugging.
 *   gs_usb<n>        fallback, enumeration order.
 */
QList<QCanBusDeviceInfo> CandleApiDevices::interfaces()
{
    QList<QCanBusDeviceInfo> result;
    QSet<QString> usedNames;

    candle_list_handle clist;
    uint8_t num_ifaces;
    candle_handle dev;

    if (candle_list_scan(&clist))
    {
        if (candle_list_length(clist, &num_ifaces))
        {
            for (uint8_t cnt = 0; cnt < num_ifaces; cnt++)
            {
                if (!candle_dev_get(clist, cnt, &dev))
                {
                    continue;
                }
                candle_devstate_t state;
                if (!candle_dev_get_state(dev, &state) || state != CANDLE_DEVSTATE_AVAIL)
                {
                    candle_dev_free(dev);
                    continue;
                }

                const wchar_t *ws = candle_dev_get_serial(dev);
                QString serial = ws ? QString::fromWCharArray(ws) : QString();
                QString dev_name = serial.isEmpty() ? QString("gs_usb%1").arg(cnt)
                                                    : QString("gs_usb_%1").arg(serial);
                if (usedNames.contains(dev_name))
                {
                    dev_name += QString("_%1").arg(cnt);
                }
                usedNames.insert(dev_name);

                const bool fd = candle_channel_has_fd(dev, 0);
                candle_capability_t caps;
                candle_channel_get_capabilities(dev, 0, &caps);
                QString description = QString("candleLight %1 (%2 MHz CAN clock)")
                                          .arg(fd ? "CAN FD" : "CAN")
                                          .arg(caps.fclk_can / 1e6);
                qDebug() << "Found candleLight device:" << dev_name << description;

                CandleApiInterface *iface = findInterface(dev);
                if (iface == nullptr)
                {
                    iface = new CandleApiInterface(dev, dev_name);
                    _ifaces.append(iface);
                }
                else
                {
                    iface->update(dev, dev_name);
                }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                result.append(createDeviceInfo(QStringLiteral(CANDLE_PLUGIN_KEY), dev_name, serial,
                                               description, QString(), 0, false, fd));
#else
                result.append(createDeviceInfo(dev_name, serial, description, 0, false, fd));
#endif
            }
        }
        candle_list_free(clist);
    }
    return result;
}

CandleApiInterface *CandleApiDevices::findInterface(candle_handle dev)
{
    for (CandleApiInterface *iface : _ifaces)
    {
        if (iface->getPath() == std::wstring(candle_dev_get_path(dev)))
        {
            return iface;
        }
    }
    return nullptr;
}

CandleApiInterface *CandleApiDevices::findInterface(QString name)
{
    for (CandleApiInterface *iface : _ifaces)
    {
        if (iface->getName() == name)
        {
            return iface;
        }
    }
    return nullptr;
}
