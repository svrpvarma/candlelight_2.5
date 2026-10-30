#include "timing.hpp"
#include <cmath>

candle_bittiming_t CandleBitTiming::toCandle() const
{
    candle_bittiming_t t;
    // split TSEG1 the same way Linux can_calc_bittiming() does; firmware adds them up
    t.prop_seg   = tseg1 / 2;
    t.phase_seg1 = tseg1 - t.prop_seg;
    t.phase_seg2 = tseg2;
    t.sjw        = sjw;
    t.brp        = brp;
    return t;
}

QString CandleBitTiming::toString() const
{
    return QString("%1 kbit/s @ %2% (BRP=%3 TSEG1=%4 TSEG2=%5 SJW=%6)")
        .arg(bitrate / 1000.0, 0, 'g', 6)
        .arg(samplePoint * 100.0, 0, 'f', 1)
        .arg(brp).arg(tseg1).arg(tseg2).arg(sjw);
}

namespace CandleTiming
{

CandleTimingLimits nominalLimits(const candle_capability_t &c)
{
    return { c.tseg1_min, c.tseg1_max, c.tseg2_min, c.tseg2_max, c.sjw_max,
             c.brp_min, c.brp_max, c.brp_inc ? c.brp_inc : 1 };
}

CandleTimingLimits nominalLimits(const candle_capability_ext_t &c)
{
    return { c.tseg1_min, c.tseg1_max, c.tseg2_min, c.tseg2_max, c.sjw_max,
             c.brp_min, c.brp_max, c.brp_inc ? c.brp_inc : 1 };
}

CandleTimingLimits dataLimits(const candle_capability_ext_t &c)
{
    return { c.dtseg1_min, c.dtseg1_max, c.dtseg2_min, c.dtseg2_max, c.dsjw_max,
             c.dbrp_min, c.dbrp_max, c.dbrp_inc ? c.dbrp_inc : 1 };
}

static bool forBrp(uint32_t fclk, const CandleTimingLimits &lim, uint32_t bitrate, double sp,
                   uint32_t brp, CandleBitTiming &out)
{
    if (brp == 0 || bitrate == 0) {
        return false;
    }
    uint64_t div = uint64_t(brp) * bitrate;
    if (fclk % div) {
        return false;                       // bitrate not exactly reachable with this prescaler
    }
    uint32_t ntq = uint32_t(fclk / div);
    if (ntq < 4) {
        return false;
    }
    int64_t tseg1 = std::llround(sp * ntq) - 1;
    int64_t tseg2 = int64_t(ntq) - 1 - tseg1;
    if (tseg1 < int64_t(lim.tseg1_min) || tseg1 > int64_t(lim.tseg1_max) ||
        tseg2 < int64_t(lim.tseg2_min) || tseg2 > int64_t(lim.tseg2_max) || tseg2 < 1) {
        return false;
    }
    out.brp = brp;
    out.tseg1 = uint32_t(tseg1);
    out.tseg2 = uint32_t(tseg2);
    uint32_t sjw = out.tseg2 < lim.sjw_max ? out.tseg2 : lim.sjw_max;
    out.sjw = sjw ? sjw : 1;
    out.bitrate = double(fclk) / (double(brp) * ntq);
    out.samplePoint = double(1 + tseg1) / ntq;
    return true;
}

bool calc(uint32_t fclk, const CandleTimingLimits &lim, uint32_t bitrate, double samplePoint,
          CandleBitTiming &out, QString *error)
{
    bool found = false;
    double bestErr = 0;
    for (uint32_t brp = lim.brp_min ? lim.brp_min : 1; brp <= lim.brp_max; brp += lim.brp_inc) {
        CandleBitTiming t;
        if (!forBrp(fclk, lim, bitrate, samplePoint, brp, t)) {
            continue;
        }
        double err = std::fabs(t.samplePoint - samplePoint);
        // smallest sample point error first, then the smallest prescaler (finest resync)
        if (!found || err < bestErr - 1e-9) {
            out = t;
            bestErr = err;
            found = true;
        }
    }
    if (!found && error) {
        *error = QString("Bitrate %1 bit/s is not reachable with a %2 MHz CAN clock")
                     .arg(bitrate).arg(fclk / 1e6);
    }
    return found;
}

bool calcFd(const candle_capability_ext_t &caps,
            uint32_t nomBitrate, double nomSamplePoint,
            uint32_t dataBitrate, double dataSamplePoint,
            CandleBitTiming &nominal, CandleBitTiming &data, bool *samePrescaler, QString *error)
{
    const CandleTimingLimits nl = nominalLimits(caps);
    const CandleTimingLimits dl = dataLimits(caps);
    const double tol = 0.005;    // accept sample point within 0.5 %

    uint32_t lo = nl.brp_min > dl.brp_min ? nl.brp_min : dl.brp_min;
    uint32_t hi = nl.brp_max < dl.brp_max ? nl.brp_max : dl.brp_max;
    for (uint32_t brp = lo ? lo : 1; brp <= hi; brp++) {
        if ((brp - nl.brp_min) % nl.brp_inc || (brp - dl.brp_min) % dl.brp_inc) {
            continue;
        }
        CandleBitTiming n, d;
        if (forBrp(caps.fclk_can, nl, nomBitrate, nomSamplePoint, brp, n) &&
            forBrp(caps.fclk_can, dl, dataBitrate, dataSamplePoint, brp, d) &&
            std::fabs(n.samplePoint - nomSamplePoint) <= tol &&
            std::fabs(d.samplePoint - dataSamplePoint) <= tol) {
            nominal = n;
            data = d;
            if (samePrescaler) *samePrescaler = true;
            return true;
        }
    }

    // fall back to independent prescalers
    if (!calc(caps.fclk_can, nl, nomBitrate, nomSamplePoint, nominal, error)) {
        return false;
    }
    if (!calc(caps.fclk_can, dl, dataBitrate, dataSamplePoint, data, error)) {
        return false;
    }
    if (samePrescaler) *samePrescaler = (nominal.brp == data.brp);
    return true;
}

} // namespace CandleTiming
