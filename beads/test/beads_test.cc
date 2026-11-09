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


#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

#include "stmlib/dsp/polyblep.h"
#include "stmlib/test/wav_writer.h"

#include "beads/dsp/processor.h"
#include "beads/dsp/synth.h"
#include "beads/dsp/delay_controller.h"
#include "beads/dsp/delay_line.h"
#include "beads/dsp/fx/wow_and_flutter.h"
#include "beads/dsp/grain.h"
#include "beads/dsp/granular_player.h"
#include "beads/dsp/granular_processor.h"
#include "beads/dsp/randomized_parameter.h"
#include "beads/dsp/multitap_delay.h"
#include "beads/dsp/trigger_generator.h"
#include "beads/dsp/wavetable_grain.h"
#include "beads/dsp/wavetable_granular_player.h"
#include "beads/dsp/wavetable_voice.h"

using namespace beads;
using namespace std;
using namespace stmlib;

const size_t kSampleRate = 48000;
const size_t kBlockSize = 12;

class AudioSource {
 public:
  AudioSource(const string& filename) {
    fp_ = NULL;
    phase_ = 0.0;

    if (filename.length() != 0) {
      // BEADS_TEST_SOURCE swaps in another 48 kHz stereo 16-bit WAV, to hear
      // a render on different material. Unset, every test uses its usual file.
      const char* override_path = getenv("BEADS_TEST_SOURCE");
      fp_ = fopen(override_path ? override_path : filename.c_str(), "rb");
      fseek(fp_, 48, SEEK_SET);
    }
  }
  
  ~AudioSource() {
    if (fp_) {
      fclose(fp_);
    }
  }
  
  void Read(FloatFrame* out, size_t size) {
    if (!fp_) {
      while (size--) {
        phase_ += 261.626f / kSampleRate;
        if (phase_ >= 1.0f) {
          phase_ -= 1.0f;
        }
        out->l = phase_ - 0.5f;
        out->r = sinf(phase_ * 2.0 * M_PI) * 0.5f;
        ++out;
      }
    } else {
      int16_t buffer[size * 2];
      if (fread(buffer, sizeof(int16_t) * 2, size, fp_) != size) {
        FloatFrame null;
        null = 0.0f;
        fill(&out[0], &out[size], null);
      } else {
        for (size_t i = 0; i < size; ++i) {
          out[i].l = static_cast<float>(buffer[2 * i]) / 32768.0f;
          out[i].r = static_cast<float>(buffer[2 * i + 1]) / 32768.0f;
        }
      }
    }
  }
  
 private:
  FILE* fp_;
  float phase_;
};

// Same memory layout as on the hardware.
uint8_t block_1[512 * 1024];
uint8_t block_2[256 * 1024];
uint8_t* buffers[3];

uint16_t reverb_buffer[32768];

namespace beads {
GrainEnvelopeProbe* grain_envelope_probe = NULL;
}  // namespace beads

// Logs the 10-90% rise time of every grain envelope that reaches 0.9, one CSV
// row per grain. Read by beads/test/measure.py.
class AttackLogger : public GrainEnvelopeProbe {
 public:
  AttackLogger(const char* filename) : sample_(0) {
    fp_ = fopen(filename, "w");
    fprintf(fp_, "start_sample,rise_10_90_ms\n");
  }

  ~AttackLogger() {
    for (map<const void*, Record>::iterator it = records_.begin();
         it != records_.end(); ++it) {
      Flush(&it->second);
    }
    fclose(fp_);
  }

  void Tick() { ++sample_; }

  virtual void OnStart(const void* envelope) {
    Record* r = &records_[envelope];
    Flush(r);
    r->active = true;
    r->start = sample_;
    r->values.clear();
  }

  virtual void OnRender(const void* envelope, float value) {
    Record* r = &records_[envelope];
    if (r->active) {
      r->values.push_back(value);
    }
  }

 private:
  struct Record {
    Record() : active(false), start(0) { }
    bool active;
    size_t start;
    vector<float> values;
  };

  // Fractional index at which the envelope first reaches the threshold.
  static float Crossing(const vector<float>& v, float threshold) {
    for (size_t i = 0; i < v.size(); ++i) {
      if (v[i] >= threshold) {
        if (i == 0) return 0.0f;
        return float(i - 1) + (threshold - v[i - 1]) / (v[i] - v[i - 1]);
      }
    }
    return -1.0f;
  }

  void Flush(Record* r) {
    if (!r->active) return;
    r->active = false;
    const float t10 = Crossing(r->values, 0.1f);
    const float t90 = Crossing(r->values, 0.9f);
    if (t10 < 0.0f || t90 < 0.0f) return;  // Cut short before the attack ended.
    fprintf(fp_, "%zu,%.4f\n", r->start, (t90 - t10) * 1000.0f / kSampleRate);
  }

  FILE* fp_;
  size_t sample_;
  map<const void*, Record> records_;
};

void TestDelayLine() {
  WavWriter wav_writer(14, kSampleRate, 30);
  wav_writer.Open("beads_delay_line.wav");
  AudioSource audio_source("audio_samples/kettel.wav");
  
  size_t total_size = 65536 * 4;
  
  uint8_t* delay_buffer[6];
  for (int i = 0; i < 6; ++i) {
    delay_buffer[i] = new uint8_t[total_size];
  }

  beads::DelayLine<RESOLUTION_16_BIT, 65000, 2, 4096, 64> stereo_16;
  beads::DelayLine<RESOLUTION_16_BIT, 130000, 1, 4096, 64> mono_16;
  beads::DelayLine<RESOLUTION_12_BIT, 86666, 2, 4096, 64> stereo_12;
  beads::DelayLine<RESOLUTION_12_BIT, 173333, 1, 4096, 64> mono_12;
  beads::DelayLine<RESOLUTION_8_BIT, 130000, 2, 4096, 64> stereo_8;
  beads::DelayLine<RESOLUTION_8_BIT, 260000, 1, 4096, 64> mono_8;

  stereo_16.Init(delay_buffer[0], total_size);
  mono_16.Init(delay_buffer[1], total_size);
  stereo_12.Init(delay_buffer[2], total_size);
  mono_12.Init(delay_buffer[3], total_size);
  stereo_8.Init(delay_buffer[4], total_size);
  mono_8.Init(delay_buffer[5], total_size);

  float delay = 64999.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    FloatFrame out[6][kBlockSize];

    audio_source.Read(in, kBlockSize);

    for (size_t i = 0; i < kBlockSize; ++i) {
      stereo_16.Write(in[i]);
      out[0][i] = stereo_16.Read<INTERPOLATION_HERMITE>(delay);

      mono_16.Write(in[i]);
      out[1][i] = mono_16.Read<INTERPOLATION_HERMITE>(delay);

      stereo_12.Write(in[i]);
      out[2][i] = stereo_12.Read<INTERPOLATION_HERMITE>(delay);

      mono_12.Write(in[i]);
      out[3][i] = mono_12.Read<INTERPOLATION_HERMITE>(delay);

      stereo_8.Write(in[i]);
      out[4][i] = stereo_8.Read<INTERPOLATION_HERMITE>(delay);

      mono_8.Write(in[i]);
      out[5][i] = mono_8.Read<INTERPOLATION_HERMITE>(delay);
    }
    
    for (size_t i = 0; i < kBlockSize; ++i) {
      float s[14];
      s[0] = in[i].l;
      s[1] = in[i].r;
      for (int j = 0; j < 6; ++j) {
        s[2 + j * 2] = out[j][i].l;
        s[3 + j * 2] = out[j][i].r;
      }
      wav_writer.Write(s, 14);
    }
  }
  
  for (int i = 0; i < 6; ++i) {
    delete delay_buffer[i];
  }
}

void TestGrain() {
  WavWriter wav_writer(4, kSampleRate, 30);
  wav_writer.Open("beads_grain.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  // beads::DelayLine<RESOLUTION_16_BIT, 72000> delay_line;
  beads::DelayLine<RESOLUTION_12_BIT, 9600, 2> delay_line;
  // beads::DelayLine<RESOLUTION_8_BIT, 1440> delay_line;

  delay_line.Init(buffers);

  DelayGrain g;
  g.Init(kSampleRate);
  g.Start(&delay_line, 10.0f, +1.0f, 4800.0f, 0.0f, 1.0f, 0.5f, 0.0f);

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame o;
      o = 0.0f;
      delay_line.Write(in[j], i >= 552000 && ((i % 32768) >= 16384));

      g.Render<HEAD_CROSSING_POLICY_WRAP, true>(&delay_line, &o);

      if (g.done()) {
        g.Start(
            &delay_line,
            72000.0f,
            -1.0f,
            1200.0f * 10,
            wav_writer.triangle(10),
            1.0f,
            0.5f,
            0.0f);
      }
      
      float frame[4];
      frame[0] = in[j].l;
      frame[1] = in[j].r;
      frame[2] = o.l;
      frame[3] = o.r;
      wav_writer.Write(frame, 4);
    }
  }
}

void TestPanMatrix() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_grain_pan_matrix.wav");
  AudioSource audio_source("");

  beads::DelayLine<RESOLUTION_16_BIT, 72000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGrain g;
  g.Init(kSampleRate);
  g.Start(&delay_line, 10.0f, +1.0f, 4800.0f, 0.0f, 1.0f, 0.5f, 0.0f);

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    audio_source.Read(in, kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame o;
      o = 0.0f;
      delay_line.Write(in[j], false);
      g.Render<HEAD_CROSSING_POLICY_WRAP, true>(&delay_line, &o);
      if (g.done()) {
        g.Start(
            &delay_line,
            10.0f,
            +1.0f,
            8000.0f,
            0.25f,
            1.0f,
            wav_writer.triangle(5),
            0.0f);
      }
      wav_writer.Write(&o.l, 2);
    }
  }
}

// The grain window at 65 positions of Shape, for a long grain (nothing on the
// attack and release floors) and a short one (20 ms, on the floors). The CSV is
// read by the checks in beads/test/checks.py (ticket 09). The WAV is the same
// sweep on a 220 Hz sine, to hear or look at, one 100 ms grain per position.
void TestGrainEnvelopeShape() {
  const int kPositions = 65;
  const int kCurvePoints = 129;
  const float kDurations[] = { 48000.0f, 960.0f };

  FILE* fp = fopen("beads_envelope_shape_curves.csv", "w");
  fprintf(fp, "shape,duration,first,last,peak,curve\n");

  GrainEnvelope envelope;
  envelope.Init(float(kSampleRate));

  for (size_t d = 0; d < sizeof(kDurations) / sizeof(kDurations[0]); ++d) {
    for (int i = 0; i < kPositions; ++i) {
      const float shape = float(i) / float(kPositions - 1);
      envelope.Start(kDurations[d], shape);
      vector<float> v;
      while (!envelope.done() && v.size() < 4 * size_t(kDurations[d]) + 48000) {
        v.push_back(envelope.Render());
      }
      fprintf(fp, "%.6f,%.0f,%.6f,%.6f,%.6f", shape, kDurations[d],
              v.front(), v.back(), *max_element(v.begin(), v.end()));
      for (int k = 0; k < kCurvePoints; ++k) {
        const size_t index = size_t(
            float(k) / float(kCurvePoints - 1) * float(v.size() - 1) + 0.5f);
        fprintf(fp, ",%.6f", v[index]);
      }
      fprintf(fp, "\n");
    }
  }
  fclose(fp);

  WavWriter wav_writer(2, kSampleRate, 7);
  wav_writer.Open("beads_envelope_shape.wav");
  float phase = 0.0f;
  for (int i = 0; i < kPositions; ++i) {
    envelope.Start(4800.0f, float(i) / float(kPositions - 1));
    for (int j = 0; j < 4800; ++j) {
      phase += 220.0f / float(kSampleRate);
      if (phase >= 1.0f) phase -= 1.0f;
      float sample = 0.5f * envelope.Render() * sinf(2.0f * 3.14159265f * phase);
      float copy = sample;
      wav_writer.Write(&sample, &copy, 1);
    }
  }
}

void TestGrainFade() {
  WavWriter wav_writer(4, kSampleRate, 1);
  wav_writer.Open("beads_grain_fade.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 48000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGrain g;
  g.Init(kSampleRate);
  g.Start(&delay_line, 100.0f, +1.0f, 96000.0f, 0.0f, 1.0f, 0.5f, 0.0f);

  for (size_t i = 0; i < kSampleRate; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    if (i == 24000) {
      assert(g.remaining_samples() == 72000);
      g.FadeOut(4096);
      assert(g.remaining_samples() == 4096);
    }
    if (i == 24000 + 24 * 100) {
      assert(g.remaining_samples() == 1696);
      g.FadeOut(64);
      assert(g.remaining_samples() == 26);
    }

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame o;
      o = 0.0f;

      delay_line.Write(in[j], false);
      g.Render<HEAD_CROSSING_POLICY_WRAP, true>(&delay_line, &o);

      float frame[4];
      frame[0] = in[j].l;
      frame[1] = in[j].r;
      frame[2] = o.l;
      frame[3] = o.r;
      wav_writer.Write(frame, 4);
    }
  }
}

void TestHeadCrossingPolicies() {
  WavWriter wav_writer(8, kSampleRate, 30);
  wav_writer.Open("beads_head_crossing_policies.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 24000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGrain g[8];
  for (int i = 0; i < 8; ++i) {
    g[i].Init(kSampleRate);
    g[i].Start(&delay_line, 800.0f, +1.0f, 9600000.0f, 0.0f, 1.0f, 0.5f, 0.0f);
  }

  for (size_t i = 0; i < kSampleRate * 30; ++i) {
    FloatFrame in;
    float out[8];

    audio_source.Read(&in, 1);
    delay_line.Write(in, i >= 600000);

    FloatFrame o;
    o = 0.0f;
    g[0].Render<HEAD_CROSSING_POLICY_STOP, false>(&delay_line, &o);
    out[0] = o.l;

    o = 0.0f;
    g[1].Render<HEAD_CROSSING_POLICY_STOP, true>(&delay_line, &o);
    out[1] = o.l;

    o = 0.0f;
    g[2].Render<HEAD_CROSSING_POLICY_WRAP, false>(&delay_line, &o);
    out[2] = o.l;

    o = 0.0f;
    g[3].Render<HEAD_CROSSING_POLICY_WRAP, true>(&delay_line, &o);
    out[3] = o.l;

    o = 0.0f;
    g[4].Render<HEAD_CROSSING_POLICY_PING_PONG, false>(&delay_line, &o);
    out[4] = o.l;

    o = 0.0f;
    g[5].Render<HEAD_CROSSING_POLICY_PING_PONG, true>(&delay_line, &o);
    out[5] = o.l;

    o = 0.0f;
    g[6].Render<HEAD_CROSSING_POLICY_STUTTER, true>(&delay_line, &o);
    out[6] = o.l;

    o = 0.0f;
    g[7].Render<HEAD_CROSSING_POLICY_CROSSFADE, false>(&delay_line, &o);
    out[7] = o.l;

    wav_writer.Write(out, 8);
  }
}

void TestTriggerGenerator() {
  for (int conditions = 0; conditions < 4; ++conditions) {
    const bool locked = conditions & 1;
    const bool has_external_input = conditions & 2;
    const float external_clock_rate = locked ? 60.0f : 1.3f;

    WavWriter wav_writer(7, kSampleRate, 20);

    string name = "beads_trigger_generator";
    name += locked ? "_locked" : "_unlocked";
    name += has_external_input ? "_external" : "_free_running";
    name += ".wav";

    wav_writer.Open(name.c_str());

    TriggerGenerator g;
    g.Init(48000.0f);

    GateFlags previous_flags = GATE_FLAG_LOW;
    float phase = 0.0f;

    for (size_t i = 0; i < kSampleRate * 20; i += kBlockSize) {
      TriggerGenerator::Output out[kBlockSize];
      GateFlags flags[kBlockSize];

      for (size_t j = 0; j < kBlockSize; ++j) {
        phase += external_clock_rate / kSampleRate;
        if (phase >= 1.0f) {
          phase -= 1.0f;
        }
        flags[j] = previous_flags = ExtractGateFlags(
            previous_flags, phase < 0.25f);
      }

      SeedButtonState seed_button_state;
      if (locked) {
        seed_button_state = SEED_BUTTON_STATE_LOCKED;
      } else {
        seed_button_state = wav_writer.triangle(2) < 0.5f ?
            SEED_BUTTON_STATE_ON : SEED_BUTTON_STATE_OFF;
      }

      g.Render(
          seed_button_state,
          has_external_input ? flags : NULL,
          wav_writer.triangle(20), 0.0f, 0.0f,
          out,
          kBlockSize);

      for (size_t j = 0; j < kBlockSize; ++j) {
        float frame[7];
        frame[0] = seed_button_state == SEED_BUTTON_STATE_LOCKED
            ? -0.8f
            : (seed_button_state == SEED_BUTTON_STATE_ON ? 0.8f : 0.0f);
        frame[1] = has_external_input
            ? (flags[j] & GATE_FLAG_HIGH ? 0.8f : 0.0f)
            : -0.8f;
        frame[2] = out[j].trigger ? 0.8f : 0.0f;
        frame[3] = out[j].gate ? 0.8f : 0.0f;
        frame[4] = out[j].fractional_sample;
        frame[5] = out[j].regularity * 0.8f;
        frame[6] = out[j].phase;
        wav_writer.Write(frame, 7);
      }
    }
  }
}

void TestTriggerGeneratorExpoFM() {
  for (int conditions = 0; conditions < 2; ++conditions) {
    const bool linear_fm = conditions == 0;
    WavWriter wav_writer(5, kSampleRate, 20);
    string name = "beads_trigger_generator";
    name += linear_fm ? "_linear_fm" : "_expo_fm";
    name += ".wav";
    wav_writer.Open(name.c_str());

    TriggerGenerator g;
    g.Init(48000.0f);

    for (size_t i = 0; i < kSampleRate * 20; i += kBlockSize) {
      TriggerGenerator::Output out[kBlockSize];
      g.Render(
          SEED_BUTTON_STATE_LOCKED,
          NULL,
          linear_fm ? 0.5f : 0.0f,
          linear_fm ? wav_writer.triangle(10) - 0.5f : 0.0f,
          linear_fm ? 0.0f : wav_writer.triangle(10) - 0.5f,
          out,
          kBlockSize);

      for (size_t j = 0; j < kBlockSize; ++j) {
        float frame[6];
        frame[0] = out[j].trigger ? 0.8f : 0.0f;
        frame[1] = out[j].gate ? 0.8f : 0.0f;
        frame[2] = out[j].fractional_sample;
        frame[3] = out[j].regularity * 0.8f;
        frame[4] = out[j].phase;
        wav_writer.Write(frame, 5);
      }
    }
  }
}

void TestRandomizedParameter() {
  RandomizedParameter p[42];
  RandomizedParameter p_pitch[42];
  RandomWalkModulation w[42];

  for (int i = 0; i < 42; ++i) {
    p[i].base = 0.5f + 0.5f * ((i % 21) / 7) / 2.0f;
    p[i].attenuverter = -1.0f + (i % 7) * 0.333333f;
    p[i].has_cv = i >= 21;
    
    p_pitch[i].base = -12.0f + 24.0f * ((i % 21) / 7) / 2.0f;
    p_pitch[i].attenuverter = -1.0f + (i % 7) * 0.333333f;
    p_pitch[i].has_cv = i >= 21;
    w[i].Init(0.001f);
  }

  for (int sample = 0; sample < 10000; ++sample) {
    float cv = (float(sample % 1000) / 1000.0f) - 0.5f;
    for (int i = 0; i < 42; ++i) {
      p[i].cv = cv;
      printf("%f\t", p[i].value());
    }
    
    for (int i = 0; i < 42; ++i) {
      p[i].cv = cv;
      printf("%f\t", p[i].value(&w[i], 1, 0.5f, false));
    }

    cv = 24.0f * ((float(sample % 1000) / 1000.0f) - 0.5f);
    for (int i = 0; i < 42; ++i) {
      p_pitch[i].cv = cv;
      float semitones = 12.0f * logf(p_pitch[i].pitch_value()) / logf(2.0f);
      printf("%f\t", semitones);
    }
    printf("\n");
  }
}

void TestGranularPlayer() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_granular_player.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 48000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGranularPlayer granuler_player;
  granuler_player.Init(48000.0f);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.5f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0025f;  // Stock's 0.33, the same window.
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.00f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    TriggerGenerator::Output trigger[kBlockSize];
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        /*wav_writer.triangle(20)*/ 0.85f, 0.0f, 0.0f,
        trigger,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      delay_line.Write(in[j], false);
      FloatFrame frame;
      frame.l = 0.0f;
      frame.r = 0.0f;
      granuler_player.Render(&delay_line, trigger[j], p, &frame);
      wav_writer.Write(&frame.l, &frame.r, 1);
    }
  }
}

void TestMultitapDelay() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_multitap_delay.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 96000, 2> delay_line;
  delay_line.Init(buffers);

  MultitapDelay multitap_delay;
  multitap_delay.Init(48000.0f);

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      bool freeze = (i % (kSampleRate * 8)) > kSampleRate * 4;
      delay_line.Write(in[j], freeze);
      int64_t delay_time = freeze
          ? (kSampleRate - ((i + j) % kSampleRate))
          : 24000;

      FloatFrame frame;
      frame = 0.0f;

      DelayController::Output delay;
      delay.tap[0].delay = delay_time << 16;
      delay.tap[0].gain = 1.0f;

      multitap_delay.Render(
          &delay_line,
          delay,
          1.0f,
          0.5f,
          &frame);
      wav_writer.Write(&frame.l, &frame.r, 1);
    }
  }
}

void TestRandomPanning() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_random_panning.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  beads::DelayLine<RESOLUTION_16_BIT, 48000, 2> delay_line;
  delay_line.Init(buffers);

  DelayGranularPlayer granuler_player;
  granuler_player.Init(48000.0f);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.6f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];

    audio_source.Read(in, kBlockSize);

    TriggerGenerator::Output trigger[kBlockSize];
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        wav_writer.triangle(20), 0.0f, 0.0f,
        trigger,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      delay_line.Write(in[j], false);

      FloatFrame frame;
      frame = 0.0f;
      granuler_player.Render(&delay_line, trigger[j], p, &frame);
      frame *= 0.25f;
      wav_writer.Write(&frame.l, &frame.r, 1);
    }
  }
}

void TestSubsampleTriggering() {
  beads::DelayLine<RESOLUTION_16_BIT, 4800, 2> delay_line;
  delay_line.Init(buffers);

  float phase = 0.0f;
  for (int i = 0; i < 4800; ++i) {
    phase += 1356.0f / 48000.0f;
    if (phase >= 1.0f) {
      phase -= 1.0f;
    }
    FloatFrame s;
    s.r = s.l = sinf(phase * 2.0f * M_PI);
    delay_line.Write(s, i == 4799);
  }

  WavWriter wav_writer(2, kSampleRate, 10);
  wav_writer.Open("beads_subsample.wav");

  DelayGranularPlayer granuler_player;
  granuler_player.Init(48000.0f);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.base = 0.1f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = +0.34f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    TriggerGenerator::Output trigger[kBlockSize];
    p.shape.base = wav_writer.triangle(10);
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        0.02f,
        0.0f,
        0.0f,
        trigger,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame frame;
      frame = 0.0f;
      granuler_player.Render(&delay_line, trigger[j], p, &frame);
      frame *= 0.25f;
      wav_writer.Write(&frame.l, &frame.r, 1);
    }
  }
}

void TestGranularProcessor() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_granular_processor.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  GranularProcessor<RESOLUTION_16_BIT, 48000, 1> proc;
  proc.Init(48000.0f, buffers);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.base = 0.5f;
  p.time.attenuverter = 0.1f;
  p.time.has_cv = false;

  p.size.base = 0.8f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.5f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = +0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame in[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    TriggerGenerator::Output trigger[kBlockSize];
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        0.8f,
        0.0f,
        0.0f,
        trigger, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);

    proc.Process(trigger, NULL, freeze, p, 0.5f, false, in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessor() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.1f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.75f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.025f;  // Stock's 0.3, the same window.
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 0.9f;
  m.reverb_amount = 0.2f;
  m.output_gate = false;
  
  ModulatedParameter d;
  d.base = 0.7f;
  d.cv = 0.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

// The patch the wet renders share. Nothing is randomised, so every grain reads
// about the same spot at the source pitch and the wet sounds like the source
// delayed by about 404 ms.
Parameters FixedWetPatch() {
  Parameters p;
  p.time.base = 0.1f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;
  p.size.base = 0.75f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;
  p.shape.base = 0.025f;  // Stock's 0.3, the same window.
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;
  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  p.monophonic_mode = false;
  return p;
}

// One pass of TestProcessor's patch with reverb and feedback off and Density
// raised to 0.9, a dense random cloud (about 25 grains a second). The wet
// level depends on density, and a sparse patch measures mostly the gaps
// between grains. dry_wet 0 gives the dry path alone and 1 the wet path alone.
// Both mix curves scale by the same factor at 12 o'clock, so the wet/dry level
// difference measured here is the one heard at 12 o'clock.
void RenderProcessorPath(const char* filename, float dry_wet,
                         AttackLogger* logger, float density = 0.9f,
                         int seconds = 30, float input_gain = 1.0f,
                         const Parameters& p = FixedWetPatch()) {
  WavWriter wav_writer(2, kSampleRate, seconds);
  wav_writer.Open(filename);
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = dry_wet;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  ModulatedParameter d;
  d.base = density;
  d.cv = 0.0f;

  grain_envelope_probe = logger;
  for (size_t i = 0; i < kSampleRate * seconds; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);
    for (size_t j = 0; j < kBlockSize; ++j) {
      in[j].l *= input_gain;
      in[j].r *= input_gain;
    }

    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
    // Rise times count rendered samples, so they're exact. Start times are
    // only resolved to the block (12 samples), which is enough to find a grain.
    if (logger) {
      for (size_t j = 0; j < kBlockSize; ++j) logger->Tick();
    }
  }
  grain_envelope_probe = NULL;
}

// Wet and dry paths rendered apart, plus a log of every grain's attack time.
void TestProcessorWetDry() {
  RenderProcessorPath("beads_wetdry_dry.wav", 0.0f, NULL);
  AttackLogger logger("beads_wetdry_wet_attacks.csv");
  RenderProcessorPath("beads_wetdry_wet.wav", 1.0f, &logger);
}

// The wet path alone with the source 12 dB down and 6 dB up (ticket 06). The
// quiet pass stays below the wet saturator's threshold, so it should match
// the code before the saturator. The hot pass drives dense clouds into it.
void TestProcessorWetDrive() {
  RenderProcessorPath("beads_drive_quiet.wav", 1.0f, NULL, 0.9f, 30, 0.25f);
  RenderProcessorPath("beads_drive_hot.wav", 1.0f, NULL, 0.9f, 30, 2.0f);
}

// The wet path alone across both halves of Density (regular below 0.48,
// random above 0.52), 15 s each. How far the levels spread shows how well
// the wet bus is normalised against overlap (ticket 05).
void TestProcessorDensitySweep() {
  const float densities[] = {
    0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.6f, 0.7f, 0.8f, 0.9f, 1.0f
  };
  for (size_t i = 0; i < sizeof(densities) / sizeof(densities[0]); ++i) {
    char filename[64];
    snprintf(filename, sizeof(filename), "beads_density_%03d.wav",
             int(densities[i] * 100.0f + 0.5f));
    RenderProcessorPath(filename, 1.0f, NULL, densities[i], 15);
  }
}

// The wet path with the 32-grain pool overflowing, so voice stealing runs
// (ticket 08). Grains last about 1.5 s at Size 0.9 and 3.3 s at Size 0.98,
// which at about 25 grains a second is roughly 38 and 83 wanting to overlap. At
// the other renders' Size 0.75 only about 9 overlap and nothing is ever stolen.
// Size stays at 0.98 at most: above 0.99 the processor switches to delay mode.
void TestProcessorVoiceSteal() {
  Parameters p = FixedWetPatch();
  p.size.base = 0.9f;
  RenderProcessorPath("beads_steal_s090.wav", 1.0f, NULL, 0.9f, 30, 1.0f, p);
  p.size.base = 0.98f;
  RenderProcessorPath("beads_steal_s098.wav", 1.0f, NULL, 0.9f, 30, 1.0f, p);
}

// A listening patch that sounds granular, unlike FixedWetPatch: fully wet, Time
// spread over about a second per grain, at Hann on Shape. About 9 grains
// overlap at Size 0.75 and Density 0.9. Pitch stays fixed: randomising it made
// the render seasick and hard to judge.
void TestProcessorCloud() {
  Parameters p = FixedWetPatch();
  p.time.attenuverter = 0.25f;
  p.shape.base = 0.75f;
  RenderProcessorPath("beads_cloud.wav", 1.0f, NULL, 0.9f, 30, 1.0f, p);
  // Source 6 dB up, so the blended bus gain drives the wet saturator (ticket 16).
  RenderProcessorPath("beads_cloud_hot.wav", 1.0f, NULL, 0.9f, 30, 2.0f, p);
}

// The cloud's level across Density and across how far Time is randomised, 15 s
// each, for tuning the bus gain's blend (ticket 16).
void TestProcessorCloudLevel() {
  Parameters p = FixedWetPatch();
  p.time.attenuverter = 0.25f;
  p.shape.base = 0.75f;
  const float densities[] = { 0.6f, 0.7f, 0.8f, 0.9f, 1.0f };
  char filename[64];
  for (size_t i = 0; i < sizeof(densities) / sizeof(densities[0]); ++i) {
    snprintf(filename, sizeof(filename), "beads_cloudlevel_d%03d.wav",
             int(densities[i] * 100.0f + 0.5f));
    RenderProcessorPath(filename, 1.0f, NULL, densities[i], 15, 1.0f, p);
  }
  const float spreads[] = { 0.0f, 0.01f, 0.02f, 0.05f, 0.1f, 0.5f };
  for (size_t i = 0; i < sizeof(spreads) / sizeof(spreads[0]); ++i) {
    p.time.attenuverter = spreads[i];
    snprintf(filename, sizeof(filename), "beads_cloudlevel_t%03d.wav",
             int(spreads[i] * 100.0f + 0.5f));
    RenderProcessorPath(filename, 1.0f, NULL, 0.9f, 15, 1.0f, p);
  }
}

// The cloud patch across Shape, 30 s each: the five window positions (ticket
// 09) and halfway up the diffuser (ticket 10).
void TestProcessorShapeSweep() {
  Parameters p = FixedWetPatch();
  p.time.attenuverter = 0.25f;
  const float positions[] = { 0.0f, 0.25f, 0.5f, 0.75f, 0.875f, 1.0f };
  char filename[64];
  for (size_t i = 0; i < sizeof(positions) / sizeof(positions[0]); ++i) {
    p.shape.base = positions[i];
    snprintf(filename, sizeof(filename), "beads_shape_%03d.wav",
             int(positions[i] * 100.0f + 0.5f));
    RenderProcessorPath(filename, 1.0f, NULL, 0.9f, 30, 1.0f, p);
  }
}

// A fully wet render for measuring stereo width (ticket 13). A clock of 25 Hz
// with Density at 0 fires every pulse with no randomness, so without a pan
// floor every grain is dead centre. sum_source feeds the same signal to both
// inputs: most stereo sources already differ between L and R, which hides what
// the grains add.
void RenderWidthPath(const char* filename, bool mono, bool sum_source,
                     bool clocked, float density, const Parameters& p) {
  const int seconds = 15;
  const int clock_period = 1920;
  WavWriter wav_writer(2, kSampleRate, seconds);
  wav_writer.Open(filename);
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, mono);

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  ModulatedParameter d;
  d.base = density;
  d.cv = 0.0f;

  GateFlags previous = GATE_FLAG_LOW;
  int clock = 0;
  for (size_t i = 0; i < kSampleRate * seconds; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    GateFlags flags[kBlockSize];

    audio_source.Read(in, kBlockSize);
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (sum_source) {
        in[j].l = in[j].r = 0.5f * (in[j].l + in[j].r);
      }
      previous = ExtractGateFlags(previous, clock < clock_period / 2);
      flags[j] = previous;
      clock = (clock + 1) % clock_period;
    }

    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        clocked ? flags : NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

// Stereo width (ticket 13): an unrandomised clocked cloud on a source with no
// stereo of its own, the same with a mono input, and a randomised cloud with a
// mono input, which must render as it did before the change. The mono renders
// go first: the stereo ones take more random draws, which would shift them.
void TestProcessorWidth() {
  Parameters p = FixedWetPatch();
  p.shape.base = 0.75f;
  RenderWidthPath("beads_width_mono.wav", true, false, true, 0.0f, p);
  p.time.attenuverter = 0.25f;
  RenderWidthPath("beads_width_mono_cloud.wav", true, false, false, 0.9f, p);
  p.time.attenuverter = 0.0f;
  RenderWidthPath("beads_width_clocked.wav", false, true, true, 0.0f, p);
  // The same patch on the source as it is, for listening.
  RenderWidthPath("beads_width_stereo.wav", false, false, true, 0.0f, p);
}

// Listening patches that sound granular (ticket 13): grains of about 54 ms, Time
// randomised, at Hann, about 25 grains a second. Density's random half first,
// then its regular half, where only the Time spread feeds the pan spread.
void TestProcessorGrains() {
  Parameters p = FixedWetPatch();
  p.size.base = 0.55f;
  p.shape.base = 0.75f;
  p.time.attenuverter = 0.25f;
  RenderProcessorPath("beads_grains_random.wav", 1.0f, NULL, 0.9f, 30, 1.0f, p);
  RenderProcessorPath("beads_grains_regular.wav", 1.0f, NULL, 0.15f, 30, 1.0f, p);
}

// One impulse down the left of a diffuser that's already fully in.
static void DiffuserImpulse(Diffuser* diffuser, int length, vector<float>* out) {
  out->clear();
  for (int block = 0; block < length / int(kBlockSize); ++block) {
    FloatFrame a[kBlockSize];
    fill(&a[0].l, &a[0].l + kBlockSize * 2, 0.0f);
    if (block == 0) a[0].l = 1.0f;
    diffuser->Process(a, kBlockSize);
    for (size_t i = 0; i < kBlockSize; ++i) out->push_back(a[i].l);
  }
}

static void DiffuserSettle(Diffuser* diffuser, int samples) {
  for (int block = 0; block < samples / int(kBlockSize); ++block) {
    FloatFrame a[kBlockSize];
    fill(&a[0].l, &a[0].l + kBlockSize * 2, 0.0f);
    diffuser->Process(a, kBlockSize);
  }
}

// Where an impulse's energy sits, in samples.
static float EnergyCentroid(const vector<float>& v) {
  float sum = 0.0f, moment = 0.0f;
  for (size_t i = 0; i < v.size(); ++i) {
    sum += v[i] * v[i];
    moment += float(i) * v[i] * v[i];
  }
  return sum > 0.0f ? moment / sum : 0.0f;
}

// Renders separated grains of a DC source and reports the quietest and loudest
// grain and their mean against reference, the peak of a grain with no random
// level. Grains don't overlap, so the bus gain stays at 1 and each peak is the
// grain's level. The delay line is mono so the stereo width stages (ticket 13),
// which also move a grain's level, stay out of the measurement.
float RenderGrainLevels(float level_spread, float reference,
                        float* lowest, float* highest, float* mean,
                        WavWriter* wav) {
  static beads::DelayLine<RESOLUTION_16_BIT, 48000, 1> delay_line;
  static DelayGranularPlayer player;
  delay_line.Init(buffers);
  player.Init(48000.0f);

  Parameters p = FixedWetPatch();
  p.size.base = 0.55f;  // About 54 ms, well inside the 100 ms interval.
  p.shape.base = 0.75f;

  FloatFrame dc;
  dc.l = dc.r = 0.5f;
  for (int i = 0; i < 24000; ++i) {
    delay_line.Write(dc, false);
  }

  float first_peak = 0.0f;
  *lowest = 1e9f;
  *highest = 0.0f;
  float total = 0.0f;
  const int num_grains = 200;
  for (int grain = 0; grain < num_grains; ++grain) {
    float peak = 0.0f;
    for (int i = 0; i < 4800; ++i) {
      TriggerGenerator::Output t;
      t.trigger = i == 0;
      t.gate = false;
      t.fractional_sample = 0.0f;
      t.regularity = 1.0f;
      t.phase = 0.0f;
      t.level_spread = level_spread;
      delay_line.Write(dc, false);
      FloatFrame frame;
      frame.l = frame.r = 0.0f;
      player.Render(&delay_line, t, p, &frame);
      peak = max(peak, fabsf(frame.l) + fabsf(frame.r));
      if (wav) {
        wav->Write(&frame.l, &frame.r, 1);
      }
    }
    if (grain == 0) {
      first_peak = peak;
    }
    const float level = peak / (reference > 0.0f ? reference : first_peak);
    *lowest = min(*lowest, level);
    *highest = max(*highest, level);
    total += level;
  }
  *mean = total / float(num_grains);
  return first_peak;
}

// The random per-grain level (ticket 11): the depth Density asks for, then the
// level grains actually get at no, half and full depth. Read by
// beads/test/checks.py.
void TestGrainLevel() {
  FILE* fp = fopen("beads_grain_level_values.csv", "w");
  fprintf(fp, "key,value\n");

  struct {
    const char* key;
    float density;
    float linear_fm;
    bool clocked;
  } runs[] = {
    { "spread_d000", 0.0f, 0.0f, false },
    { "spread_d040", 0.4f, 0.0f, false },
    { "spread_noon", 0.5f, 0.0f, false },
    { "spread_d076", 0.76f, 0.0f, false },
    { "spread_d100", 1.0f, 0.0f, false },
    { "spread_cv", 0.76f, 0.24f, false },
    { "spread_cv_down", 0.76f, -0.5f, false },
    { "spread_clock_d040", 0.4f, 0.0f, true },
    { "spread_clock_noon", 0.5f, 0.0f, true },
  };
  for (size_t r = 0; r < sizeof(runs) / sizeof(runs[0]); ++r) {
    TriggerGenerator g;
    g.Init(48000.0f);
    GateFlags flags[kBlockSize];
    fill(&flags[0], &flags[kBlockSize], GATE_FLAG_LOW);
    TriggerGenerator::Output out[kBlockSize];
    g.Render(SEED_BUTTON_STATE_LOCKED, runs[r].clocked ? flags : NULL,
             runs[r].density, runs[r].linear_fm, 0.0f, out, kBlockSize);
    fprintf(fp, "%s,%.6f\n", runs[r].key, out[kBlockSize - 1].level_spread);
  }

  float lowest, highest, mean;
  const float reference = RenderGrainLevels(
      0.0f, 0.0f, &lowest, &highest, &mean, NULL);
  fprintf(fp, "level_none_min,%.6f\nlevel_none_max,%.6f\n", lowest, highest);
  fprintf(fp, "level_none_mean,%.6f\n", mean);
  RenderGrainLevels(0.5f, reference, &lowest, &highest, &mean, NULL);
  fprintf(fp, "level_half_min,%.6f\nlevel_half_max,%.6f\n", lowest, highest);
  fprintf(fp, "level_half_mean,%.6f\n", mean);
  WavWriter wav_writer(2, 48000, 20);
  wav_writer.Open("beads_grain_level.wav");
  RenderGrainLevels(1.0f, reference, &lowest, &highest, &mean, &wav_writer);
  fprintf(fp, "level_full_min,%.6f\nlevel_full_max,%.6f\n", lowest, highest);
  fprintf(fp, "level_full_mean,%.6f\n", mean);
  fclose(fp);
}

// The wet diffuser (ticket 10): timing against sample rate, bypass at amount
// 0, whether its delays move, how long it rings and how far it spreads an
// impulse at 48 kHz, and the amount the processor picks from Shape. Read by
// beads/test/checks.py.
void TestDiffuser() {
  FILE* fp = fopen("beads_diffuser_values.csv", "w");
  fprintf(fp, "key,value\n");

  static Diffuser diffuser;
  vector<float> ir32, ir48, ir48_later;

  diffuser.Init(32000.0f);
  diffuser.set_amount(1.0f);
  DiffuserSettle(&diffuser, int(kBlockSize));
  DiffuserImpulse(&diffuser, 6400, &ir32);

  diffuser.Init(48000.0f);
  diffuser.set_amount(1.0f);
  DiffuserSettle(&diffuser, int(kBlockSize));
  DiffuserImpulse(&diffuser, 9600, &ir48);
  fprintf(fp, "timing_ratio_32k_48k,%.6f\n",
          EnergyCentroid(ir32) / EnergyCentroid(ir48));

  // Remaining energy falling to -30 dB of the total.
  float total = 0.0f;
  for (size_t i = 0; i < ir48.size(); ++i) total += ir48[i] * ir48[i];
  float tail = 0.0f;
  size_t ring_end = ir48.size();
  for (size_t i = ir48.size(); i-- > 0;) {
    tail += ir48[i] * ir48[i];
    if (tail >= 0.001f * total) { ring_end = i; break; }
  }
  fprintf(fp, "ring_30db_ms,%.3f\n", float(ring_end) * 1000.0f / 48000.0f);

  // The same impulse 1.25 s later, once the delays have had time to move.
  DiffuserSettle(&diffuser, 60000);
  DiffuserImpulse(&diffuser, 9600, &ir48_later);
  float moved = 0.0f;
  for (size_t i = 0; i < ir48.size(); ++i) {
    moved = max(moved, fabsf(ir48[i] - ir48_later[i]));
  }
  fprintf(fp, "impulse_moved,%.6f\n", moved);

  WavWriter wav_writer(2, 48000, 1);
  wav_writer.Open("beads_diffuser.wav");
  for (size_t i = 0; i < ir48.size(); ++i) {
    float sample = ir48[i];
    float copy = ir48[i];
    wav_writer.Write(&sample, &copy, 1);
  }

  diffuser.Init(48000.0f);
  float bypass_diff = 0.0f;
  for (int block = 0; block < 100; ++block) {
    FloatFrame a[kBlockSize];
    for (size_t i = 0; i < kBlockSize; ++i) {
      a[i].l = a[i].r = float(i) / float(kBlockSize) - 0.5f;
    }
    diffuser.Process(a, kBlockSize);
    for (size_t i = 0; i < kBlockSize; ++i) {
      bypass_diff = max(bypass_diff,
                        fabsf(a[i].l - (float(i) / float(kBlockSize) - 0.5f)));
    }
  }
  fprintf(fp, "bypass_max_diff,%.9f\n", bypass_diff);

  // Settle the amount on silence, then send one impulse down the left.
  diffuser.Init(48000.0f);
  diffuser.set_amount(1.0f);
  float energy = 0.0f, late_energy = 0.0f;
  int n = 0;
  for (int block = -1; block < 4800 / int(kBlockSize); ++block) {
    FloatFrame a[kBlockSize];
    fill(&a[0].l, &a[0].l + kBlockSize * 2, 0.0f);
    if (block == 0) a[0].l = 1.0f;
    diffuser.Process(a, kBlockSize);
    if (block < 0) continue;
    for (size_t i = 0; i < kBlockSize; ++i, ++n) {
      const float e = a[i].l * a[i].l;
      energy += e;
      if (n >= 48) late_energy += e;  // After the first millisecond.
    }
  }
  fprintf(fp, "impulse_energy,%.6f\n", energy);
  fprintf(fp, "smear_fraction,%.6f\n", energy > 0.0f ? late_energy / energy : 0.0f);

  static Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);
  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  ModulatedParameter d;
  d.base = 0.9f;
  d.cv = 0.0f;
  struct Run {
    const char* key;
    float shape, attenuverter, cv, size;
    bool has_cv;
    int blocks;
  } runs[] = {
    { "amount_shape_050", 0.5f, 0.0f, 0.0f, 0.75f, false, 16 },
    { "amount_shape_075", 0.75f, 0.0f, 0.0f, 0.75f, false, 16 },
    { "amount_shape_088", 0.875f, 0.0f, 0.0f, 0.75f, false, 16 },
    { "amount_shape_100", 1.0f, 0.0f, 0.0f, 0.75f, false, 16 },
    { "amount_shape_cv", 0.75f, 0.5f, 0.5f, 0.75f, true, 16 },
    { "amount_delay_mode", 1.0f, 0.0f, 0.0f, 1.0f, false, 8000 },
  };
  for (size_t r = 0; r < sizeof(runs) / sizeof(runs[0]); ++r) {
    Parameters p = FixedWetPatch();
    p.shape.base = runs[r].shape;
    p.shape.attenuverter = runs[r].attenuverter;
    p.shape.cv = runs[r].cv;
    p.shape.has_cv = runs[r].has_cv;
    p.size.base = runs[r].size;
    for (int block = 0; block < runs[r].blocks; ++block) {
      FloatFrame in[kBlockSize];
      FloatFrame out[kBlockSize];
      bool freeze[kBlockSize];
      fill(&in[0].l, &in[0].l + kBlockSize * 2, 0.0f);
      fill(&freeze[0], &freeze[kBlockSize], false);
      proc.Process(SEED_BUTTON_STATE_LOCKED, NULL, freeze, d, p, m, in, out,
                   kBlockSize);
    }
    fprintf(fp, "%s,%.6f\n", runs[r].key, proc.diffuser_amount());
  }
  fclose(fp);
}

void TestProcessorDelay() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_delay.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.7f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.5f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.7f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.2f;
  m.output_gate = false;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    if (i >= 15 * kSampleRate) {
      d.base = wav_writer.triangle(11);
    }
    
    if (i <= 11 * kSampleRate) {
      m.dry_wet = wav_writer.triangle(4);
      p.shape.base = 0.0f;
    } else {
      m.dry_wet = 1.0f;
      p.shape.base = 0.66f;
    }
    
    fill(&freeze[0], &freeze[kBlockSize], i >= 11.7 * kSampleRate);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorDelayBW() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_delay_bw.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.2f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.9f;
  m.dry_wet = 0.5f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);
    p.size.attenuverter = 2.0f * wav_writer.triangle(20) - 1.0f;

    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorDelayTimeXFade() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_delay_time_xfade.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.has_cv = false;
  p.size.attenuverter = 0.0f;

  p.shape.base = 0.66f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.2f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    p.time.base = wav_writer.triangle(10);
    
    fill(&freeze[0], &freeze[kBlockSize], false);

    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorTapDelay() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_tap_delay.wav");
  AudioSource audio_source("audio_samples/talking_heads.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.5f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  int next_tap_counter = 0;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    GateFlags tap_input[kBlockSize];
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (!next_tap_counter) {
        tap_input[j] = GATE_FLAG_HIGH | GATE_FLAG_RISING;
        next_tap_counter = 21881;
      } else {
        --next_tap_counter;
        tap_input[j] = GATE_FLAG_LOW;
      }
    }
    
    bool frz = (i >= 350108 && i <= 350108 * 3 / 2) || (i >= 350108 * 2);
    
    if (frz) {
      d.base = 0.35f;
      p.time.base = wav_writer.triangle();
      if (i >= kSampleRate * 15) {
        d.base = 0.15f + wav_writer.triangle(7) * 0.35f;
      }
      m.feedback = 0.0f;
      m.dry_wet = 1.0f;
      p.shape.base = wav_writer.triangle(8);
    } else {
      d.base = 0.5f;
      p.time.base = 0.0f;
      m.feedback = 0.3f;
      m.dry_wet = wav_writer.triangle(3);
      p.shape.base = 0.0f;
    }


    fill(&freeze[0], &freeze[kBlockSize], frz);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        tap_input,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorTapDelayGlitch() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_tap_delay_glitch.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.5f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  int next_tap_counter = 0;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    GateFlags tap_input[kBlockSize];
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (!next_tap_counter) {
        tap_input[j] = GATE_FLAG_HIGH | GATE_FLAG_RISING;
        next_tap_counter = 480 + (rand() % 4);
      } else {
        --next_tap_counter;
        tap_input[j] = GATE_FLAG_LOW;
      }
    }
    
    p.time.base = 0.0f;
    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        tap_input,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorTapDelayTempoChange() {
  WavWriter wav_writer(4, kSampleRate, 30);
  wav_writer.Open("beads_processor_tap_delay_tempo_change.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;
  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.5f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  int next_tap_counter = 0;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    GateFlags tap_input[kBlockSize];
    
    p.shape.base = wav_writer.triangle(2);
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (!next_tap_counter) {
        tap_input[j] = GATE_FLAG_HIGH | GATE_FLAG_RISING;
        next_tap_counter = ((i / kSampleRate) & 3) < 2 ? 2120 : 15343;
        next_tap_counter += ((i / kBlockSize) % 3) * 24;
      } else {
        --next_tap_counter;
        tap_input[j] = GATE_FLAG_LOW;
      }
    }
    
    fill(&freeze[0], &freeze[kBlockSize], (i / kSampleRate) >= 15);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        tap_input,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      float out[4];
      out[0] = frames[j].l;
      out[1] = frames[j].r;
      out[2] = tap_input[j] & GATE_FLAG_HIGH ? 0.8f : 0.0f;
      out[3] = freeze[j] ? 0.8f : 0.0f;
      wav_writer.Write(out, 4);
    }
    
  }
}

void TestProcessorTapDelayDensityModulation() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_tap_delay_density_modulation.wav");
  AudioSource audio_source("audio_samples/talking_heads.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 1.0f;
  m.dry_wet = 0.7f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  int next_tap_counter = 0;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    GateFlags tap_input[kBlockSize];
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      if (!next_tap_counter) {
        tap_input[j] = GATE_FLAG_HIGH | GATE_FLAG_RISING;
        next_tap_counter = 21881;
      } else {
        --next_tap_counter;
        tap_input[j] = GATE_FLAG_LOW;
      }
    }
    
    d.base = 1.0f - wav_writer.triangle(11) * 0.5f;
    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        tap_input,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestProcessorDelayGranularXFade() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_delay_granular_xfade.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.2f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  Processor::MixParameters m;
  m.feedback = 0.3f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  ModulatedParameter d;
  d.base = 0.3f;
  d.cv = 0.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);
    
    p.size.base = min(wav_writer.triangle(11) * 2, 1.0f);
    
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestWowAndFlutter() {
  WavWriter wav_writer(2, kSampleRate / 2, 30);
  wav_writer.Open("beads_wow_and_flutter.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  WowAndFlutter wow_and_flutter;
  wow_and_flutter.Init(kSampleRate / 2);

  SampleRateConverter<SRC_DOWN, 2, 48> src_down_;
  src_down_.Init();

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in_stereo[kBlockSize];
    float in[kBlockSize];
    float in_decimated[kBlockSize / 2];
    FloatFrame frames[kBlockSize / 2];
    audio_source.Read(in_stereo, kBlockSize);
    for (size_t i = 0; i < kBlockSize; ++i) {
      in[i] = (in_stereo[i].l + in_stereo[i].r) * 0.5f;
    }
    src_down_.Process(in, in_decimated, kBlockSize);
    for (size_t i = 0; i < kBlockSize / 2; ++i) {
      frames[i].l = frames[i].r = in_decimated[i];
    }
    wow_and_flutter.Process(frames, kBlockSize / 2);
    wav_writer.Write(&(frames[0].l), kBlockSize / 2 * 2);
  }
}

void TestProcessorMonophonicMode() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_mono_mode.wav");
  AudioSource audio_source("audio_samples/kettel.wav");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.1f;
  p.time.attenuverter = 0.3f;
  p.time.has_cv = false;

  p.size.base = 0.8f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.66f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.1f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = true;
  
  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  ModulatedParameter d;
  d.base = 0.8f;
  d.cv = 0.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], false);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestWavetableGrain() {
  WavWriter wav_writer(2, 48000.0f, 10);
  wav_writer.Open("beads_wavetable_grain.wav");
  
  WavetableGrain g[2];
  g[0].Init(kSampleRate);
  g[1].Init(kSampleRate);
  
  int grain = 0;
  float phase = 0.0f;
  float frequency = 69.3f / kSampleRate;
  
  for (size_t i = 0; i < kSampleRate * 10; ++i) {
    phase += frequency;
    if (phase >= 1.0f) {
      phase -= 1.0f;
      float fractional_sample = phase / frequency;
      grain = (grain + 1) % 2;
      
      g[grain].Start(
          2, 16,
          wav_writer.triangle(5),
          4.0f * frequency,
          1.2f / frequency,
          wav_writer.triangle(7),
          0.5f,
          0.5f,
          fractional_sample);
    }
    FloatFrame s;
    s = 0.0f;
    g[0].Render(&s);
    g[1].Render(&s);
    wav_writer.Write(&s.l, 2);
  }
}

void TestWavetableGranularPlayer() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_wavetable_granular_player.wav");

  WavetableGranularPlayer granuler_player;
  granuler_player.Init(48000.0f);

  TriggerGenerator g;
  g.Init(48000.0f);

  Parameters p;

  p.time.attenuverter = 0.05f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.66f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.04f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    p.time.base = wav_writer.triangle(30);
    TriggerGenerator::Output trigger[kBlockSize];
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        1.0f,
        0.0f,
        0.0f,
        trigger,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      FloatFrame frame;
      frame = 0.0f;
      granuler_player.Render(
          2,
          16,
          trigger[j], false, p, &frame);
      frame *= 0.5f;
      wav_writer.Write(&frame.l, 2);
    }
  }
}

void TestSynth() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_synth.wav");

  Synth synth;
  synth.Init(reverb_buffer);

  Parameters p;
  
  p.time.base = 0.0f;
  p.time.attenuverter = 0.1f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.52f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.2f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 1.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    
    p.size.base = 0.7f + wav_writer.triangle(5) * 0.3f;
    p.time.base = wav_writer.triangle(25);

    fill(&freeze[0], &freeze[kBlockSize], ((i / kSampleRate) % 4) >= 3);
    synth.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        Synth::RESOLUTION_16_BIT, d, p, m,
        frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestSynthWavetableScanGlitch() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_synth_scan.wav");

  Synth synth;
  synth.Init(reverb_buffer);

  Parameters p;
  
  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.7f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.25f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.2f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.dry_wet = 0.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    
    m.feedback = wav_writer.triangle(5);

    fill(&freeze[0], &freeze[kBlockSize], ((i / kSampleRate) % 8) >= 6);
    synth.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        Synth::RESOLUTION_16_BIT, d, p, m,
        frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestSynthSimpleSweep() {
  WavWriter wav_writer(2, kSampleRate, 68);
  wav_writer.Open("beads_synth_sweep.wav");

  Synth synth;
  synth.Init(reverb_buffer);

  Parameters p;
  
  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.7f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.33f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = -12.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.25f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  for (size_t i = 0; i < kSampleRate * 68; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    
    m.feedback = float(i) / float(kSampleRate * 64);
    p.time.base = wav_writer.triangle(2);

    fill(&freeze[0], &freeze[kBlockSize], false);
    synth.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        Synth::RESOLUTION_16_BIT, d, p, m,
        frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestSynthFrequencySweep() {
  WavWriter wav_writer(2, kSampleRate, 68);
  wav_writer.Open("beads_synth_frequency_sweep.wav");

  Synth synth;
  synth.Init(reverb_buffer);

  Parameters p;
  
  p.time.base = 0.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.7f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.40f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 0.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;

  for (size_t i = 0; i < kSampleRate * 68; i += kBlockSize) {
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];
    
    float ramp = float(i % (2 * kSampleRate)) / float(2 * kSampleRate);
    p.pitch.base = ramp * 120.0f - 60.0f;

    fill(&freeze[0], &freeze[kBlockSize], false);
    synth.Render(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        Synth::RESOLUTION_16_BIT, d, p, m,
        frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

template<int R, int N>
class CicDecimator {
 public:
  int32_t hi[N];
  int32_t hc[N];
  
  void Init() {
    fill(&hi[0], &hi[N], 0.0f);
    fill(&hc[0], &hc[N], 0.0f);
  }
  
  void Process(float* input, float* output, size_t size) {
    for (int i = 0; i < size / R; ++i) {
      // N integrators
      for (int r=0; r<R; r++) {
        hi[0] += (*input++) * 32768.0f;
        for(int n=1; n<N; n++) {
          hi[n] += hi[n-1];
        }
      }
      // N combs
      int32_t v = hi[N-1];
      for (int n=0; n<N; n++) {
        int32_t in = v;
        v -= hc[n];
        hc[n] = in;
      }
      *output++ = static_cast<float>(v / powf(R, N) / 32768.0f);
    }
  }
};

template<int R, int N>
class CicInterpolator {
 public:
  int32_t hi[N];
  int32_t hc[N];
  
  void Init() {
    fill(&hi[0], &hi[N], 0.0f);
    fill(&hc[0], &hc[N], 0.0f);
  }
  
  void Process(float* input, float* output, size_t size) {
    for (int i = 0; i < size; ++i) {
      int32_t v = static_cast<float>(*input++ * R * 32768.0f);
      for (int n=0; n<N; n++) {
        int32_t in = v;
        v -= hc[n];
        hc[n] = in;
      }
      for (int r=0; r<R; r++) {
        hi[0] += v;
        for(int n=1; n<N; n++) {
          hi[n] += hi[n-1];
        }
        *output++ = static_cast<float>(hi[N - 1] / powf(R, N) / 32768.0f);
        v = 0;
      }
    }
  }
};

void TestSRCDown(void) {
  int ratio = 3;
  
  WavWriter wav_writer(2, kSampleRate, 10);
  wav_writer.Open("beads_src_down.wav");
  AudioSource audio_source("");
  
  stmlib::SampleRateConverter<stmlib::SRC_DOWN, 3, 48> src_down_;
  stmlib::SampleRateConverter<stmlib::SRC_UP, 3, 48> src_up_;
  src_down_.Init();
  src_up_.Init();
  
  CicDecimator<3, 8> cic_down_;
  CicInterpolator<3, 4> cic_up_;
  cic_down_.Init();
  cic_up_.Init();
  
  float phase = 0.0f;
  for (size_t i = 0; i < 48000 * 10; i += kBlockSize) {
    float input[kBlockSize];
    
    float frequency = powf(2.0f, wav_writer.triangle(2) * -10.0f) * 0.5f;
    for (size_t j = 0; j < kBlockSize; ++j) {
      phase += frequency;
      if (phase >= 1.0f) {
        phase -= 1.0f;
      }
      input[j] = sinf(M_PI * 2 * phase);
    }
    
    float down_fir[kBlockSize / ratio];
    float down_cic[kBlockSize / ratio];
    float up_fir[kBlockSize];
    float up_cic[kBlockSize];
    
    src_down_.Process(input, down_fir, kBlockSize);
    src_up_.Process(down_fir, up_fir, kBlockSize / ratio);
    
    cic_down_.Process(input, down_cic, kBlockSize);
    cic_up_.Process(down_cic, up_cic, kBlockSize / ratio);
    
    for (size_t j = 0; j < kBlockSize; ++j) {
      float s[2];
      s[0] = up_fir[j];
      s[1] = up_cic[j];
      wav_writer.Write(s, 2);
    }
  }
}

void TestGateOut() {
  WavWriter wav_writer(4, kSampleRate, 20);

  wav_writer.Open("beads_gate_out.wav");

  TriggerGenerator g;
  g.Init(48000.0f);

  GateFlags previous_flags = GATE_FLAG_LOW;
  float phase = 0.0f;

  for (size_t i = 0; i < kSampleRate * 20; i += kBlockSize) {
    TriggerGenerator::Output out[kBlockSize];
    GateFlags flags[kBlockSize];

    for (size_t j = 0; j < kBlockSize; ++j) {
      phase += 1.0f / kSampleRate;
      if (phase >= 1.0f) {
        phase -= 1.0f;
      }
      flags[j] = previous_flags = ExtractGateFlags(
          previous_flags, phase < 0.5f);
    }

    g.Render(
        SEED_BUTTON_STATE_OFF,
        flags,
        wav_writer.triangle(10) * 0.7f + 0.15f, 0.0f, 0.0f,
        out,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      float frame[4];
      frame[0] = flags[j] & GATE_FLAG_HIGH;
      frame[1] = out[j].trigger ? 0.8f : 0.0f;
      frame[2] = out[j].gate ? 0.8f : 0.0f;
      frame[3] = out[j].phase;
      wav_writer.Write(frame, 4);
    }
  }
}

void TestAudioRateTriggerGenerator() {
  WavWriter wav_writer(6, kSampleRate, 20);
  wav_writer.Open("beads_audio_rate_trigger.wav");

  TriggerGenerator g;
  g.Init(48000.0f);

  GateFlags previous_flags = GATE_FLAG_LOW;
  float phase = 0.0f;
  
  float this_sample = 0.0f;
  float next_sample;
  float previous_phase = 0.0f;

  for (size_t i = 0; i < kSampleRate * 20; i += kBlockSize) {
    const float frequency = ((i / (kSampleRate / 4)) % 2 == 0)
        ? 432.0f / kSampleRate
        : 865.3f / kSampleRate;
    GateFlags flags[kBlockSize];
    for (size_t j = 0; j < kBlockSize; ++j) {
      phase += frequency;
      if (phase >= 1.0f) {
        phase -= 1.0f;
      }
      flags[j] = previous_flags = ExtractGateFlags(
          previous_flags, phase < 0.25f);
    }
    
    TriggerGenerator::Output out[kBlockSize];
    
    g.Render(
        SEED_BUTTON_STATE_LOCKED,
        flags,
        wav_writer.triangle(13) * 1.0f, 0.0f, 0.0f,
        out,
        kBlockSize);

    for (size_t j = 0; j < kBlockSize; ++j) {
      float frame[6];
      frame[0] = flags[j] & GATE_FLAG_HIGH ? 0.8f : 0.0f;
      frame[1] = out[j].trigger ? 0.8f : 0.0f;
      frame[2] = out[j].gate ? 0.8f : 0.0f;
      frame[3] = out[j].fractional_sample;
      frame[4] = out[j].regularity * 0.8f;
      
      next_sample = 0.0f;
      if (out[j].trigger) {
        float d = out[j].phase - previous_phase;
        this_sample += stmlib::ThisBlepSample(out[j].fractional_sample) * d;
        next_sample += stmlib::NextBlepSample(out[j].fractional_sample) * d;
      }
      next_sample += out[j].phase;
      previous_phase = out[j].phase;
      frame[5] = this_sample;
      wav_writer.Write(frame, 6);

      this_sample = next_sample;
    }
  }
}

void TestDelayModulation() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_processor_delay_glitch.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;
  p.time.base = 1.0f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 1.0f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.0f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = +0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  ModulatedParameter d;
  d.base = 0.125f;
  d.cv = 0.0f;

  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    if (i < kSampleRate * 15) {
      d.cv = 0.25f * (cosf(wav_writer.triangle(7) * 2 * M_PI) + 1.0f);
    } else {
      d.cv = wav_writer.triangle(1) > 0.5f ? 3.0f / 5.0f : 1.0f / 5.0f;
    }
    
    fill(&freeze[0], &freeze[kBlockSize], i > kSampleRate * 15);

    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

void TestFreezeGlitch() {
  WavWriter wav_writer(2, kSampleRate, 30);
  wav_writer.Open("beads_freeze_glitch.wav");
  AudioSource audio_source("");

  Processor proc;
  proc.Init(buffers, reverb_buffer);
  proc.Configure(Processor::MEDIA_DIGITAL, false);

  Parameters p;

  p.time.base = 0.25f;
  p.time.attenuverter = 0.0f;
  p.time.has_cv = false;

  p.size.base = 0.85f;
  p.size.attenuverter = 0.0f;
  p.size.has_cv = false;

  p.shape.base = 0.333f;
  p.shape.attenuverter = 0.0f;
  p.shape.has_cv = false;

  p.pitch.base = 0.0f;
  p.pitch.attenuverter = 0.0f;
  p.pitch.has_cv = false;
  
  p.monophonic_mode = false;
  
  Processor::MixParameters m;
  m.feedback = 0.0f;
  m.dry_wet = 1.0f;
  m.reverb_amount = 0.0f;
  m.output_gate = false;
  
  ModulatedParameter d;
  d.base = 0.45f;
  d.cv = 0.0f;

  for (size_t i = 0; i < kSampleRate * 30; i += kBlockSize) {
    FloatFrame in[kBlockSize];
    bool freeze[kBlockSize];
    FloatFrame frames[kBlockSize];

    audio_source.Read(in, kBlockSize);

    fill(&freeze[0], &freeze[kBlockSize], (i % 4000) <= 2000);
    proc.Process(
        SEED_BUTTON_STATE_LOCKED,
        NULL,
        freeze,
        d, p, m,
        in, frames, kBlockSize);
    wav_writer.Write(&(frames[0].l), kBlockSize * 2);
  }
}

struct NamedTest {
  const char* name;
  void (*fn)();
};

const NamedTest kTests[] = {
  { "TestDelayLine", &TestDelayLine },
  { "TestGrain", &TestGrain },
  { "TestPanMatrix", &TestPanMatrix },
  { "TestGrainEnvelopeShape", &TestGrainEnvelopeShape },
  { "TestProcessorShapeSweep", &TestProcessorShapeSweep },
  { "TestProcessorWidth", &TestProcessorWidth },
  { "TestProcessorGrains", &TestProcessorGrains },
  { "TestDiffuser", &TestDiffuser },
  { "TestGrainLevel", &TestGrainLevel },
  { "TestGrainFade", &TestGrainFade },
  { "TestHeadCrossingPolicies", &TestHeadCrossingPolicies },
  { "TestTriggerGenerator", &TestTriggerGenerator },
  { "TestTriggerGeneratorExpoFM", &TestTriggerGeneratorExpoFM },
  { "TestRandomizedParameter", &TestRandomizedParameter },
  { "TestGranularPlayer", &TestGranularPlayer },
  { "TestMultitapDelay", &TestMultitapDelay },
  { "TestRandomPanning", &TestRandomPanning },
  { "TestSubsampleTriggering", &TestSubsampleTriggering },
  { "TestGranularProcessor", &TestGranularProcessor },
  { "TestProcessor", &TestProcessor },
  { "TestProcessorWetDry", &TestProcessorWetDry },
  { "TestProcessorDensitySweep", &TestProcessorDensitySweep },
  { "TestProcessorWetDrive", &TestProcessorWetDrive },
  { "TestProcessorVoiceSteal", &TestProcessorVoiceSteal },
  { "TestProcessorCloud", &TestProcessorCloud },
  { "TestProcessorCloudLevel", &TestProcessorCloudLevel },
  { "TestProcessorDelay", &TestProcessorDelay },
  { "TestProcessorDelayBW", &TestProcessorDelayBW },
  { "TestProcessorDelayTimeXFade", &TestProcessorDelayTimeXFade },
  { "TestProcessorTapDelay", &TestProcessorTapDelay },
  { "TestProcessorTapDelayGlitch", &TestProcessorTapDelayGlitch },
  { "TestProcessorTapDelayTempoChange", &TestProcessorTapDelayTempoChange },
  { "TestProcessorTapDelayDensityModulation", &TestProcessorTapDelayDensityModulation },
  { "TestProcessorDelayGranularXFade", &TestProcessorDelayGranularXFade },
  { "TestWowAndFlutter", &TestWowAndFlutter },
  { "TestProcessorMonophonicMode", &TestProcessorMonophonicMode },
  { "TestWavetableGrain", &TestWavetableGrain },
  { "TestWavetableGranularPlayer", &TestWavetableGranularPlayer },
  { "TestSynth", &TestSynth },
  { "TestSynthWavetableScanGlitch", &TestSynthWavetableScanGlitch },
  { "TestSynthSimpleSweep", &TestSynthSimpleSweep },
  { "TestSynthFrequencySweep", &TestSynthFrequencySweep },
  { "TestGateOut", &TestGateOut },
  { "TestAudioRateTriggerGenerator", &TestAudioRateTriggerGenerator },
  { "TestDelayModulation", &TestDelayModulation },
  { "TestFreezeGlitch", &TestFreezeGlitch },
};

// Usage: beads_test [TestName ...]
// With no arguments, runs TestProcessor. "list" prints the available tests.
int main(int argc, char** argv) {
  buffers[0] = block_1;
  buffers[1] = block_1 + (256 * 1024);
  buffers[2] = block_2;

  const size_t num_tests = sizeof(kTests) / sizeof(kTests[0]);
  if (argc < 2) {
    TestProcessor();
    return 0;
  }
  for (int a = 1; a < argc; ++a) {
    if (string(argv[a]) == "list") {
      for (size_t t = 0; t < num_tests; ++t) {
        printf("%s\n", kTests[t].name);
      }
      continue;
    }
    bool found = false;
    for (size_t t = 0; t < num_tests; ++t) {
      if (string(argv[a]) == kTests[t].name) {
        printf("Running %s\n", kTests[t].name);
        kTests[t].fn();
        found = true;
      }
    }
    if (!found) {
      fprintf(stderr, "Unknown test: %s\n", argv[a]);
      return 1;
    }
  }
  return 0;
}
