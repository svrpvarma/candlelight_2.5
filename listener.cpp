#include "listener.hpp"
#include "device.hpp"
#include <QThread>

CandleApiListener::CandleApiListener(CandleApiDevice &dev) :
    _shouldBeRunning(false),
    _dev(dev)
{
    _thread = new QThread();
}

CandleApiListener::~CandleApiListener()
{
    delete _thread;
}

void CandleApiListener::run()
{
    _dev.readFrames();
    _thread->quit();
}

void CandleApiListener::startThread()
{
    _shouldBeRunning = true;
    moveToThread(_thread);
    connect(_thread, SIGNAL(started()), this, SLOT(run()));
    _thread->start();
}

void CandleApiListener::stopThread()
{
    _shouldBeRunning = false;
    if (_thread->isRunning())
    {
        _thread->wait();
    }
}
