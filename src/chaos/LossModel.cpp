#include "LossModel.h"

namespace chaos {

BernoulliLossModel::BernoulliLossModel(double drop_probability, unsigned seed)
    : p_(drop_probability), rng_(seed) {}

bool BernoulliLossModel::shouldDrop() {
    return dist_(rng_) < p_;
}

GilbertElliottLossModel::GilbertElliottLossModel(Params params, unsigned seed)
    : params_(params), rng_(seed) {}

bool GilbertElliottLossModel::shouldDrop() {
    // Step 1: possibly transition state for this packet.
    double transition_roll = dist_(rng_);
    if (state_ == State::Good) {
        if (transition_roll < params_.p_good_to_bad) {
            state_ = State::Bad;
        }
    } else {
        if (transition_roll < params_.p_bad_to_good) {
            state_ = State::Good;
        }
    }

    // Step 2: apply the loss probability for whichever state we're now in.
    double loss_p = (state_ == State::Good) ? params_.loss_in_good : params_.loss_in_bad;
    return dist_(rng_) < loss_p;
}

} // namespace chaos
