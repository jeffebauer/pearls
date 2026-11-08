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
// Grain Envelope.

#ifndef BEADS_DSP_GRAIN_ENVELOPE_H_
#define BEADS_DSP_GRAIN_ENVELOPE_H_

#include "stmlib/stmlib.h"
#include "stmlib/dsp/dsp.h"

#include <algorithm>

#include "beads/resources.h"

namespace beads {

#ifdef TEST
// Desktop harness only: lets beads_test.cc log every grain's envelope so the
// measurement tool can report attack times. Compiled out of the firmware.
class GrainEnvelopeProbe {
 public:
  virtual ~GrainEnvelopeProbe() { }
  virtual void OnStart(const void* envelope) = 0;
  virtual void OnRender(const void* envelope, float value) = 0;
};
extern GrainEnvelopeProbe* grain_envelope_probe;
#endif  // TEST

class GrainEnvelope {
 public:
  GrainEnvelope() { }
  ~GrainEnvelope() { }
  
  inline void Init(float sample_rate) {
    stage_ = STAGE_DONE;
    phase_ = 0.0f;
    phase_increment_[STAGE_ATTACK] = 0.5f;
    phase_increment_[STAGE_SUSTAIN] = 0.5f;
    phase_increment_[STAGE_RELEASE] = 0.5f;
    phase_increment_[STAGE_DONE] = 0.0f;
    smoothness_ = 0.0f;
    gaussian_ = 0.0f;

    min_attack_ = 0.003f * sample_rate;
    min_release_ = 0.010f * sample_rate;
  }

  inline void Start(float duration, float shape) {
    stage_ = STAGE_ATTACK;
    phase_ = 0.0f;
#ifdef TEST
    if (grain_envelope_probe) grain_envelope_probe->OnStart(this);
#endif  // TEST
    
    CONSTRAIN(shape, 0.0f, 1.0f);
    const float x = shape * 4.0f;
    const float a = std::min(std::max(0.25f * (x - 1.0f), 0.0f), 0.5f);
    const float attack = a;
    const float sustain = x < 1.0f ? x : 1.0f - 2.0f * a;
    const float release = x < 1.0f ? 1.0f - x : a;

    //             attack  sustain  release  smoothness  gaussian
    // ----------------------------------------------------------
    // 0.00 Decay       0        0        1           0         0
    // 0.25 Square      0        1        0           0         0
    // 0.50 Tukey    0.25      0.5     0.25           1         0
    // 0.75 Hann      0.5        0      0.5           1         0
    // 1.00 Gaussian  0.5        0      0.5           1         1
    //
    // Every column is continuous across the knob, so consecutive grains never
    // jump. SmoothStep stands in for the raised cosine of a true Hann (within
    // about 1%), as it did for stock's bell.

    const float attack_duration = std::max(
        duration * attack,
        min_attack_ * (1.0f + 3.0f * sustain));
    const float sustain_duration = std::max(duration * sustain, 2.0f);
    const float release_duration = std::max(duration * release, min_release_);
    
    phase_increment_[STAGE_ATTACK] = 1.0f / attack_duration;
    phase_increment_[STAGE_SUSTAIN] = 1.0f / sustain_duration;
    phase_increment_[STAGE_RELEASE] = 1.0f / release_duration;
    smoothness_ = std::min(std::max(x - 1.0f, 0.0f), 1.0f);
    gaussian_ = std::min(std::max(x - 3.0f, 0.0f), 1.0f);
  }

  inline void Start(float duration, float shape, float fractional_sample) {
    Start(duration, shape);
    phase_ += phase_increment_[stage_] * fractional_sample;
    WrapPhase();
  }
  
  inline void Stop() {
    stage_ = STAGE_DONE;
  }

  inline void WrapPhase() {
    if (phase_ >= 1.0f) {
      const float t = (phase_ - 1.0f) / phase_increment_[stage_];
      stage_ = Stage(int(stage_) + 1);
      phase_ = t * phase_increment_[stage_];
    }
  }

  inline float Render() {
    if (stage_ == STAGE_DONE) {
      return 0.0f;
    }

    float value;
    if (stage_ == STAGE_SUSTAIN) {
      value = 1.0f;
    } else {
      const bool attack = stage_ == STAGE_ATTACK;
      const float t = attack ? phase_ : 1.0f - phase_;
      const float base = attack ? t : (t * t) * (t * t);
      value = base + (stmlib::SmoothStep(t) - base) * smoothness_;
      if (gaussian_ > 0.0f) {
        value += (stmlib::Interpolate(lut_gaussian, t, 128.0f) - value) *
            gaussian_;
      }
    }

    phase_ += phase_increment_[stage_];
    WrapPhase();
#ifdef TEST
    if (grain_envelope_probe) grain_envelope_probe->OnRender(this, value);
#endif  // TEST
    
    return value;
  }

  inline bool done() const {
    return stage_ >= STAGE_DONE;
  }

 private:
  enum Stage {
    STAGE_ATTACK,
    STAGE_SUSTAIN,
    STAGE_RELEASE,
    STAGE_DONE,
    STAGE_LAST
  };
  
  Stage stage_;
  float phase_;
  float phase_increment_[STAGE_LAST];
  float smoothness_;
  float gaussian_;

  float min_attack_;
  float min_release_;

  DISALLOW_COPY_AND_ASSIGN(GrainEnvelope);
};

}  // namespace beads

#endif  // BEADS_DSP_GRAIN_ENVELOPE_H_
