#include "DelayJitterModel.h"
#include <algorithm>
#include <cmath>

namespace chaos {

GaussianJitterModel::GaussianJitterModel(long base_delay_us, double jitter_stddev_us, unsigned seed)
    : base_delay_us_(base_delay_us), rng_(seed), jitter_dist_(0.0, jitter_stddev_us) {}

long GaussianJitterModel::nextDelayMicros() {
    double jitter = jitter_dist_(rng_);
    long delay = base_delay_us_ + static_cast<long>(jitter);
    return std::max<long>(delay, 0);
}

CorrelatedJitterModel::CorrelatedJitterModel(long base_delay_us, double jitter_stddev_us,
                                              double alpha, unsigned seed)
    : base_delay_us_(base_delay_us), alpha_(alpha), rng_(seed),
      noise_dist_(0.0, jitter_stddev_us) {}

long CorrelatedJitterModel::nextDelayMicros() {
    double noise = noise_dist_(rng_);
    // AR(1): blend of previous jitter and fresh noise gives the process
    // "memory". The sqrt(1-alpha^2) factor keeps the stationary stddev at
    // the configured value (variance = (1-a^2)s^2 / (1-a^2) = s^2).
    prev_jitter_ = alpha_ * prev_jitter_ + std::sqrt(1.0 - alpha_ * alpha_) * noise;
    long delay = base_delay_us_ + static_cast<long>(prev_jitter_);
    return std::max<long>(delay, 0);
}

} // namespace chaos
