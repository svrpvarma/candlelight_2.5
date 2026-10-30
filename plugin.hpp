#pragma once

#include <QObject>
#include <QtSerialBus/QCanBusFactory>
#include <QtSerialBus/QCanBusDevice>

/* Qt 6 plugin (SavvyCAN V220+ Windows builds use Qt 6.10 / MSVC 2022 x64). */
class CandleLightFdCanBusPlugin : public QObject, public QCanBusFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QCanBusFactory" FILE "plugin.json")
    Q_INTERFACES(QCanBusFactory)

public:
    QList<QCanBusDeviceInfo> availableDevices(QString *errorMessage) const override;
    QCanBusDevice *createDevice(const QString &interfaceName, QString *errorMessage) const override;
};
