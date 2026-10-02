#pragma once

namespace chaos {

// Common interface every impairment strategy implements. Keeping this
// generic (rather than hardcoding "loss" and "delay" as special cases in
// the engine) means new fault types -- corruption, duplication, whatever --
// plug in without touching ChaosEngine.
class ILossModel {
public:
    virtual ~ILossModel() = default;
    // Returns true if the next packet should be dropped.
    virtual bool shouldDrop() = 0;
    virtual const char* name() const = 0;
    // Long-run average drop probability implied by the configuration
    // (used in the experiment report as the "configured" loss).
    virtual double expectedLossRate() const = 0;
};

class IDelayModel {
public:
    virtual ~IDelayModel() = default;
    // Returns the delay to apply to the next packet, in microseconds.
    virtual long nextDelayMicros() = 0;
    virtual const char* name() const = 0;
};

} // namespace chaos
