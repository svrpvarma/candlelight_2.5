#pragma once

#include <QObject>
#include <atomic>

class CandleApiDevice;
class QThread;

class CandleApiListener : public QObject
{
    Q_OBJECT

public:
    explicit CandleApiListener(CandleApiDevice &dev);
    virtual ~CandleApiListener();

public slots:
    void run();
    void startThread();
    void stopThread();

public:
    std::atomic<bool> _shouldBeRunning;

private:
    CandleApiDevice &_dev;
    QThread *_thread;
};
