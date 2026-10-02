#pragma once
#include "ImpairmentModel.h"
#include <random>

namespace chaos {

// Flat, memoryless packet loss: each packet is dropped independently with
// probability p. Simple, but unrealistic -- real network loss tends to
// come in bursts (a brief Wi-Fi fade, a congested router queue overflowing
// for a few hundred ms), not as independent coin flips.
class BernoulliLossModel : public ILossModel {
public:
    explicit BernoulliLossModel(double drop_probability, unsigned seed = std::random_device{}());

    bool shouldDrop() override;
    const char* name() const override { return "bernoulli"; }
    double expectedLossRate() const override { return p_; }

private:
    double p_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
};

// Gilbert-Elliott: a 2-state Markov chain (GOOD / BAD) modeling bursty
// loss. In GOOD state, loss probability is low (loss_good, often 0); in
// BAD state it's high (loss_bad, e.g. 0.5+). Transition probabilities
// p_good_to_bad and p_bad_to_good control burst length/frequency.
//
// This is a much closer approximation of real link behavior -- e.g. Wi-Fi
// interference or a momentarily congested queue -- than flat Bernoulli
// loss, and is the model worth highlighting in the report as the
// "interesting" contribution.
class GilbertElliottLossModel : public ILossModel {
public:
    struct Params {
        double p_good_to_bad = 0.02;  // chance per packet of entering a burst
        double p_bad_to_good = 0.3;   // chance per packet of a burst ending
        double loss_in_good = 0.0;    // background loss rate outside a burst
        double loss_in_bad = 0.6;     // loss rate during a burst
    };

    explicit GilbertElliottLossModel(Params params, unsigned seed = std::random_device{}());

    bool shouldDrop() override;
    const char* name() const override { return "gilbert-elliott"; }
    // Stationary distribution of the Markov chain:
    //   P(bad) = p_gb / (p_gb + p_bg);  avg loss = P(good)*lg + P(bad)*lb
    double expectedLossRate() const override {
        double denom = params_.p_good_to_bad + params_.p_bad_to_good;
        double p_bad = denom > 0 ? params_.p_good_to_bad / denom : 0.0;
        return (1.0 - p_bad) * params_.loss_in_good + p_bad * params_.loss_in_bad;
    }

    bool inBadState() const { return state_ == State::Bad; }

private:
    enum class State { Good, Bad };

    Params params_;
    State state_ = State::Good;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_{0.0, 1.0};
};

} // namespace chaos
