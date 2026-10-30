#pragma once

#include <QObject>
#include <QString>
#include <string>
#include "api/candle.h"

class CandleApiInterface: public QObject
{
public:
    CandleApiInterface(candle_handle handle, QString name);
    virtual ~CandleApiInterface();

    std::wstring getPath() const;
    QString getSerial() const;
    void update(candle_handle handle, QString name);
    QString getName() const;
    candle_handle getHandle() const;

private:
    candle_handle _handle;
    QString _name;
};
