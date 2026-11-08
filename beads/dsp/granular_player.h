// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// -----------------------------------------------------------------------------
//
// Granular player.

#ifndef BEADS_DSP_GRANULAR_PLAYER_H_
#define BEADS_DSP_GRANULAR_PLAYER_H_

#include "stmlib/stmlib.h"

#include "beads/dsp/grain.h"
#include "beads/dsp/randomized_parameter.h"
#include "beads/dsp/trigger_generator.h"

namespace beads {

// These three constants can be adjusted, but the inequality:
// kInstantCrossfadeTime < kSimpleSchedulerThreshold < kSimpleSchedulerThreshold
// must be preserved (the code relies on that inequality to avoid std::sort).
const float kInstantCrossfadeTime = 0.005f;
const float kSimpleSchedulerThreshold = 0.025f;
const float kPreemptiveCrossfadeTime = 0.1f;

// Wet bus normalisation (ticket 05, ADR 0004). The bus gain drops at once
// when grains pile up and recovers with this time constant when they thin out.
const float kBusGainRecoveryTime = 0.1f;
// How far the bus gain moves from 1/sum towards 1/sqrt(sum) once Time or Pitch
// is randomised, and how quickly it gets there as the randomness rises.
const float kBusGainMaxIncoherence = 0.75f;
const float kBusGainSpreadScale = 100.0f;

struct Parameters {
  RandomizedParameter time;
  RandomizedParameter size;
  RandomizedParameter shape;
  RandomizedParameter pitch;
  bool monophonic_mode;
};

template<typename T, int num_grains>
class GranularPlayer {
 public:
  GranularPlayer() { }
  ~GranularPlayer() { }
  
  enum {
    NUM_GRAINS = num_grains
  };
  
  inline void Init(float sample_rate) {
    instant_crossfade_time_ = int(sample_rate * kInstantCrossfadeTime);
    preemptive_crossfade_time_ = int(sample_rate * kPreemptiveCrossfadeTime);
    simple_scheduler_threshold_ = int(sample_rate * kSimpleSchedulerThreshold);
    
    grain_interval_ = 1;
    
    max_grain_duration_ = sample_rate * 4.0f;
    sample_rate_ = sample_rate;
    one_hertz_ = 1.0f / sample_rate;
    bus_gain_recovery_ = 1.0f / (sample_rate * kBusGainRecoveryTime);
    Reset();
  }
  
  inline void Reset() {
    for (int i = 0; i < num_grains; ++i) {
      grain_[i].Init(sample_rate_);
      grain_priority_list_[i] = &grain_[i];
    }
    bus_gain_ = 1.0f;
  }
  
  inline float ComputeSpread(
      const TriggerGenerator::Output& trigger,
      const Parameters& p) const {
    float total_spread = 0.0f;
    total_spread += p.time.randomness_amount();
    total_spread += p.size.randomness_amount();
    total_spread += p.shape.randomness_amount();
    total_spread += p.pitch.randomness_amount();
    total_spread += 1.0f - trigger.regularity;
    return std::min(total_spread, 1.0f);
  }
  
  inline float ComputeGrainDuration(float duration) {
    const bool reverse = duration < 0.3333333f;
    const float clamped_shortness = reverse
        ? duration * 3.0f * (2.0f - duration * 3.0f)
        : 1.0f - 1.5f * (duration - 0.3333333f);
    const float shortness = std::min(1.0f, 1.025f * clamped_shortness);
    return stmlib::SemitonesToRatio(shortness * -108.0f) * max_grain_duration_;
  }
  
  inline int CountActiveGrains() const {
    int num_active_grains = 0;
    for (int i = 0; i < num_grains; ++i) {
      if (!grain_[i].done()) {
        ++num_active_grains;
      }
    }
    return num_active_grains;
  }
  
  // Pan spread stays count-based, as stock.
  inline float ComputeGrainPan(
      const TriggerGenerator::Output& trigger,
      const Parameters& p,
      float num_active_grains) {
    const float total_spread = ComputeSpread(trigger, p);
    const float pan_spread = 0.5f * num_active_grains * total_spread;
    return BoundedRandomFloat(pan_spread);
  }

  // Stock per-grain gain, fixed at birth to 1/sqrt(active grains + 1). Only
  // the wavetable player still uses it; the delay player normalises the bus.
  inline void ComputeGrainAmplitudeAndPan(
      const TriggerGenerator::Output& trigger,
      const Parameters& p,
      float* amplitude,
      float* pan) {
    const float num_active_grains = float(CountActiveGrains() + 1);
    *amplitude = 1.0f / stmlib::Sqrt(num_active_grains);
    *pan = ComputeGrainPan(trigger, p, num_active_grains);
  }

  // Gain for the summed grains, from the sum of the envelope values of every
  // live grain this sample: 1/max(sum, 1). Linear rather than square root:
  // neighbouring grains read near-identical audio, so they add up nearly in
  // phase (ADR 0004). The floor of 1 leaves a lone grain's fade untouched.
  // The gain follows a rise in the sum at once, so a pile-up never
  // overshoots, and recovers slowly when grains die away.
  // Grains scattered in Time or Pitch stop adding up in phase, so their sum
  // grows nearer sqrt(n) than n and 1/sum would turn them down too far. With no
  // randomness this is 0 and the bus gain is exactly 1/sum.
  inline float ComputeBusIncoherence(const Parameters& p) const {
    const float spread = p.time.randomness_amount() + p.pitch.randomness_amount();
    return kBusGainMaxIncoherence * std::min(spread * kBusGainSpreadScale, 1.0f);
  }

  inline float UpdateBusGain(float envelope_sum, float incoherence) {
    const float target = envelope_sum > 1.0f
        ? (1.0f - incoherence) / envelope_sum +
            incoherence / stmlib::Sqrt(envelope_sum)
        : 1.0f;
    if (target < bus_gain_) {
      bus_gain_ = target;
    } else {
      bus_gain_ += bus_gain_recovery_ * (target - bus_gain_);
    }
    return bus_gain_;
  }
  
  inline T* new_grain() { return grain_priority_list_[0]; }
  
  inline void Tick() {
    ++grain_interval_;
  }
  
  inline void SortGrainList(bool monophonic_mode) {
    // Whenever a new grain is started, we subject the first N grains to various
    // crossfades, so they terminate quickly in case a new grain is started
    // afterwards.

    if (monophonic_mode) {
      std::rotate(
          &grain_priority_list_[0],
          &grain_priority_list_[1],
          &grain_priority_list_[num_grains]);
      grain_priority_list_[0]->FadeOut(
          std::min(grain_interval_, instant_crossfade_time_));
      for (int i = 1; i < num_grains - 1; ++i) {
        grain_priority_list_[i]->FadeOut(instant_crossfade_time_);
      }
    } else {
      const int num_safety_grains = 4;
      int fade_time[num_safety_grains];
    
      if (grain_interval_ < simple_scheduler_threshold_) {
        // When the grains are arriving very fast, we simply assume that the
        // first grain will be done or close to completion, and we steal it.
        //
        // The newly created grain is moved back at the end of the list.
        std::rotate(
            &grain_priority_list_[0],
            &grain_priority_list_[1],
            &grain_priority_list_[num_grains]);
        
        fade_time[0] = grain_interval_ + 1;
        fade_time[1] = instant_crossfade_time_;
        fade_time[2] = instant_crossfade_time_;
        fade_time[3] = preemptive_crossfade_time_;
        
        if (grain_interval_ > instant_crossfade_time_) {
          std::swap(fade_time[0], fade_time[2]);
        }
      } else {
        // The grains are generated at a rate not too high to allow the grain
        // list to be resorted. The sorting criterion is how much time
        // is left for a grain to play. Thus, the grains at the beginning of the
        // list will be the best candidates to "steal" without causing a drastic
        // change in texture. If the overlap is not too high, they are very
        // likely to have already completed their course anyway.
        std::partial_sort(
            &grain_priority_list_[0],
            &grain_priority_list_[num_safety_grains],
            &grain_priority_list_[num_grains],
            T::Compare);
      
        fade_time[0] = instant_crossfade_time_;
        fade_time[1] = instant_crossfade_time_;
        fade_time[2] = grain_interval_;
        fade_time[3] = preemptive_crossfade_time_;

        if (grain_interval_ > preemptive_crossfade_time_) {
          std::swap(fade_time[2], fade_time[3]);
        }
      }
    
      // We make sure the grains at the beginning of the list will terminate
      // quickly and that not much damage will happen when they are stolen.
      for (int i = 0; i < num_safety_grains; ++i) {
        grain_priority_list_[i]->FadeOut(fade_time[i]);
      }
    }
    grain_interval_ = 0;
  }
  
 protected:
  T grain_[num_grains];
  T* grain_priority_list_[num_grains];
 
  int preemptive_crossfade_time_;
  int instant_crossfade_time_;
  int simple_scheduler_threshold_;
  
  float max_grain_duration_;
  float sample_rate_;
  float one_hertz_;
  
  int grain_interval_;

  float bus_gain_;
  float bus_gain_recovery_;
  
  DISALLOW_COPY_AND_ASSIGN(GranularPlayer);
};

}  // namespace beads

#endif  // BEADS_DSP_GRANULAR_PLAYER_H_
