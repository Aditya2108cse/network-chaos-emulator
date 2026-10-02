#pragma once
#include "ImpairmentModel.h"
#include <random>

namespace chaos {

// Fixed base delay plus independent random jitter, drawn from a normal
// distribution and clamped to be non-negative. This is the simple case:
// each packet's jitter is IID, uncorrelated with the previous packet's.
// Good enough for most application-level testing (e.g. does the app's
// timeout logic tolerate 100ms +/- 20ms of latency).
class GaussianJitterModel : public IDelayModel {
public:
    // base_delay_us: mean delay in microseconds.
    // jitter_stddev_us: standard deviation of the jitter added on top.
    GaussianJitterModel(long base_delay_us, double jitter_stddev_us,
                         unsigned seed = std::random_device{}());

    long nextDelayMicros() override;
    const char* name() const override { return "gaussian-jitter"; }

private:
    long base_delay_us_;
    std::mt19937 rng_;
    std::normal_distribution<double> jitter_dist_;
};

// Correlated jitter via an AR(1) (first-order autoregressive) process:
//   jitter[n] = alpha * jitter[n-1] + sqrt(1 - alpha^2) * noise[n]
// The sqrt(1 - alpha^2) scaling keeps the long-run standard deviation of
// the jitter equal to jitter_stddev_us no matter what alpha is, so the
// configured jitter is also the jitter you measure.
// With alpha close to 1, consecutive packets' delays drift smoothly
// instead of jumping independently -- much closer to how latency actually
// behaves on a real, moderately congested or wireless link, where delay
// this millisecond is a strong predictor of delay next millisecond.
class CorrelatedJitterModel : public IDelayModel {
public:
    // alpha in [0,1): 0 = behaves like independent Gaussian jitter,
    // closer to 1 = slower-changing, more "drifting" delay.
    CorrelatedJitterModel(long base_delay_us, double jitter_stddev_us, double alpha,
                          unsigned seed = std::random_device{}());

    long nextDelayMicros() override;
    const char* name() const override { return "correlated-jitter"; }

private:
    long base_delay_us_;
    double alpha_;
    double prev_jitter_ = 0.0;
    std::mt19937 rng_;
    std::normal_distribution<double> noise_dist_;
};

} // namespace chaos
