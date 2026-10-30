#pragma once

#include <QObject>
#include <QtSerialBus/QCanBusFactory>
#include <QtSerialBus/QCanBusDevice>

/* Builds for Qt 5.15 (SavvyCAN V213/V220: Qt 5.15.2 MSVC 2019 x64) and Qt 6.
   CANDLE_QT5 is set by CMakeLists.txt (moc needs a plain define, not QT_VERSION). */
#ifdef CANDLE_QT5
class CandleLightFdCanBusPlugin : public QObject, public QCanBusFactoryV2
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QCanBusFactory" FILE "plugin.json")
    Q_INTERFACES(QCanBusFactoryV2)
#else
class CandleLightFdCanBusPlugin : public QObject, public QCanBusFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QCanBusFactory" FILE "plugin.json")
    Q_INTERFACES(QCanBusFactory)
#endif

public:
    QList<QCanBusDeviceInfo> availableDevices(QString *errorMessage) const override;
    QCanBusDevice *createDevice(const QString &interfaceName, QString *errorMessage) const override;
};
