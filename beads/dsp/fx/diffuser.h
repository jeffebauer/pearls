// Copyright 2014 Emilie Gillet.
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
// AP diffusion network.

// Ported from Clouds for the wet bus. Clouds runs at 32 kHz; the lines here are
// sized for 48 kHz and read at offsets scaled by the sample rate at runtime, so
// the timing is the same at every Quality setting.

#ifndef BEADS_DSP_FX_DIFFUSER_H_
#define BEADS_DSP_FX_DIFFUSER_H_

#include "stmlib/stmlib.h"

#include "beads/dsp/frame.h"
#include "beads/dsp/fx/fx_engine.h"

namespace beads {

class Diffuser {
 public:
  Diffuser() { }
  ~Diffuser() { }

  void Init(float sample_rate) {
    engine_.Init(buffer_);
    amount_ = 0.0f;
    target_amount_ = 0.0f;
    Reset(sample_rate);
  }

  void Reset(float sample_rate) {
    static const float kLengths[kNumLines] = {
      189.0f, 270.0f, 404.0f, 666.0f, 227.0f, 308.0f, 368.0f, 608.0f
    };
    const float ratio = sample_rate / 48000.0f;
    for (int i = 0; i < kNumLines; ++i) {
      offset_[i] = static_cast<int32_t>(kLengths[i] * ratio + 0.5f) - 1;
    }
    engine_.Clear();
  }

  void Process(FloatFrame* in_out, size_t size) {
    // Costs nothing, and leaves the signal untouched, until it's turned up.
    if (amount_ == 0.0f && target_amount_ == 0.0f) {
      return;
    }

    typedef E::Reserve<189,
      E::Reserve<270,
      E::Reserve<404,
      E::Reserve<666,
      E::Reserve<227,
      E::Reserve<308,
      E::Reserve<368,
      E::Reserve<608> > > > > > > > Memory;
    E::DelayLine<Memory, 0> apl1;
    E::DelayLine<Memory, 1> apl2;
    E::DelayLine<Memory, 2> apl3;
    E::DelayLine<Memory, 3> apl4;
    E::DelayLine<Memory, 4> apr1;
    E::DelayLine<Memory, 5> apr2;
    E::DelayLine<Memory, 6> apr3;
    E::DelayLine<Memory, 7> apr4;
    E::Context c;
    const float kap = 0.625f;
    const float increment = (target_amount_ - amount_) / float(size);
    float amount = amount_;
    while (size--) {
      amount += increment;
      engine_.Start(&c);

      float wet = 0.0f;
      c.Read(in_out->l);
      c.Read(apl1, offset_[0], kap);
      c.WriteAllPass(apl1, -kap);
      c.Read(apl2, offset_[1], kap);
      c.WriteAllPass(apl2, -kap);
      c.Read(apl3, offset_[2], kap);
      c.WriteAllPass(apl3, -kap);
      c.Read(apl4, offset_[3], kap);
      c.WriteAllPass(apl4, -kap);
      c.Write(wet, 0.0f);
      in_out->l += amount * (wet - in_out->l);

      c.Read(in_out->r);
      c.Read(apr1, offset_[4], kap);
      c.WriteAllPass(apr1, -kap);
      c.Read(apr2, offset_[5], kap);
      c.WriteAllPass(apr2, -kap);
      c.Read(apr3, offset_[6], kap);
      c.WriteAllPass(apr3, -kap);
      c.Read(apr4, offset_[7], kap);
      c.WriteAllPass(apr4, -kap);
      c.Write(wet, 0.0f);
      in_out->r += amount * (wet - in_out->r);

      ++in_out;
    }
    amount_ = target_amount_;
  }

  void set_amount(float amount) { target_amount_ = amount; }
  float amount() const { return target_amount_; }

 private:
  enum { kNumLines = 8, kBufferSize = 4096 };

  typedef FxEngine<kBufferSize, FORMAT_32_BIT> E;
  E engine_;
  float buffer_[kBufferSize];
  int32_t offset_[kNumLines];

  float amount_;
  float target_amount_;

  DISALLOW_COPY_AND_ASSIGN(Diffuser);
};

}  // namespace beads

#endif  // BEADS_DSP_FX_DIFFUSER_H_
