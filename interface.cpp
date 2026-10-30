#include "interface.hpp"

CandleApiInterface::CandleApiInterface(candle_handle handle, QString name) :
    _handle(handle),
    _name(name)
{
}

CandleApiInterface::~CandleApiInterface()
{
}

std::wstring CandleApiInterface::getPath() const
{
    return std::wstring(candle_dev_get_path(_handle));
}

QString CandleApiInterface::getSerial() const
{
    const wchar_t *s = candle_dev_get_serial(_handle);
    return s ? QString::fromWCharArray(s) : QString();
}

void CandleApiInterface::update(candle_handle handle, QString name)
{
    if (handle != _handle) {
        candle_dev_free(_handle);
    }
    _handle = handle;
    _name = name;
}

QString CandleApiInterface::getName() const
{
    return _name;
}

candle_handle CandleApiInterface::getHandle() const
{
    return _handle;
}
