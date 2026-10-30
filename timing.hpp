#pragma once

#include <QString>
#include "api/candle.h"

/* Bit timing calculated from the limits the firmware reports (BT_CONST / BT_CONST_EXT),
   instead of a hard-coded table. Works for any CAN clock (160 MHz CANable 2.5,
   170 MHz CANable 2.0, 48 MHz candleLight, ...). */
struct CandleBitTiming
{
    uint32_t brp = 0;
    uint32_t tseg1 = 0;      // prop_seg + phase_seg1 (sync segment excluded)
    uint32_t tseg2 = 0;
    uint32_t sjw = 0;
    double bitrate = 0;
    double samplePoint = 0;  // 0..1

    candle_bittiming_t toCandle() const;
    QString toString() const;
};

struct CandleTimingLimits
{
    uint32_t tseg1_min, tseg1_max, tseg2_min, tseg2_max, sjw_max, brp_min, brp_max, brp_inc;
};

namespace CandleTiming
{
    CandleTimingLimits nominalLimits(const candle_capability_t &c);
    CandleTimingLimits nominalLimits(const candle_capability_ext_t &c);
    CandleTimingLimits dataLimits(const candle_capability_ext_t &c);

    /* Classic CAN: best timing for bitrate/sample point. */
    bool calc(uint32_t fclk, const CandleTimingLimits &lim, uint32_t bitrate, double samplePoint,
              CandleBitTiming &out, QString *error);

    /* CAN FD: prefers one common, smallest prescaler for both phases
       (ST / CiA recommendation; avoids phase shift at the BRS switch). */
    bool calcFd(const candle_capability_ext_t &caps,
                uint32_t nomBitrate, double nomSamplePoint,
                uint32_t dataBitrate, double dataSamplePoint,
                CandleBitTiming &nominal, CandleBitTiming &data, bool *samePrescaler, QString *error);
}
