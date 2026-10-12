// Tiny built-in synth so the plugin can be auditioned without routing MIDI to another instrument.
// No JUCE dependency. Channel 10 (index 9) = drums, others = simple oscillators per role.
#pragma once
#include <cmath>
#include <cstdint>

class PreviewSynth
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        atk = 1.0f - std::exp (-1.0f / (0.004f * (float) sr));
        rel = 1.0f - std::exp (-1.0f / (0.090f * (float) sr));
        allOff();
    }

    void allOff() { for (auto& v : voices) v.active = false; }

    void noteOn (int ch, int note, int vel)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) { v = &voices[stealIdx]; stealIdx = (stealIdx + 1) % numVoices; }
        *v = Voice();
        v->active = true; v->held = true; v->ch = ch; v->note = note;
        v->amp = (float) vel / 127.0f;

        if (ch == 9) { setupDrum (*v, note); return; }

        v->inc = 440.0 * std::pow (2.0, (note - 69) / 12.0) / sr;
        switch (ch)
        {
            case 0:  v->kind = 0; v->gain = 0.16f; break;   // melody: saw
            case 1:  v->kind = 1; v->gain = 0.07f; break;   // harmony: square
            case 2:  v->kind = 2; v->gain = 0.30f; break;   // bass: triangle
            default: v->kind = 0; v->gain = 0.05f; break;   // pad: soft saw
        }
    }

    void noteOff (int ch, int note)
    {
        for (auto& v : voices)
            if (v.active && ! v.drum && v.held && v.ch == ch && v.note == note) v.held = false;
    }

    // Adds into left/right (right may be null for mono). Buffers should be pre-cleared.
    void render (float* left, float* right, int n)
    {
        if (n <= 0) return;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            for (int i = 0; i < n; ++i)
            {
                float s = 0.0f;
                if (v.drum)
                {
                    s = drumSample (v);
                    if (! v.active) break;
                }
                else
                {
                    v.env += ((v.held ? 1.0f : 0.0f) - v.env) * (v.held ? atk : rel);
                    if (! v.held && v.env < 0.0005f) { v.active = false; break; }
                    v.phase += v.inc; if (v.phase >= 1.0) v.phase -= 1.0;
                    const float ph = (float) v.phase;
                    float o;
                    if (v.kind == 0)      o = 2.0f * ph - 1.0f;
                    else if (v.kind == 1) o = ph < 0.5f ? 1.0f : -1.0f;
                    else                  o = ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph;
                    s = o * v.env * v.amp * v.gain;
                }
                left[i] += s;
                if (right) right[i] += s;
            }
        }
        for (int i = 0; i < n; ++i)
        {
            left[i] = std::tanh (left[i]);
            if (right) right[i] = std::tanh (right[i]);
        }
    }

private:
    struct Voice
    {
        bool active = false, held = false, drum = false;
        int ch = 0, note = 0, kind = 0;
        float amp = 0, env = 0, gain = 1, decay = 0.1f, freq = 100.0f, lp = 0.0f, hpCoef = 0.2f;
        double phase = 0, phase2 = 0, inc = 0, t = 0;
    };

    static constexpr int numVoices = 32;
    Voice voices[numVoices];
    int stealIdx = 0;
    double sr = 44100.0;
    float atk = 0.01f, rel = 0.01f;
    uint32_t noiseState = 22222u;

    float nextNoise()
    {
        noiseState ^= noiseState << 13; noiseState ^= noiseState >> 17; noiseState ^= noiseState << 5;
        return (float) noiseState / 2147483648.0f - 1.0f;
    }

    void setupDrum (Voice& v, int note)
    {
        v.drum = true;
        auto noise = [&] (int kind, float decay, float cutoff, float gain) {
            v.kind = kind; v.decay = decay; v.gain = gain;
            v.hpCoef = 1.0f - std::exp (-6.2831853f * cutoff / (float) sr);
        };
        switch (note)
        {
            case 35: case 36: v.kind = 0; v.decay = 0.18f; v.gain = 0.9f; break;      // kick
            case 37: noise (1, 0.05f, 2000.0f, 0.40f); break;                          // rim
            case 38: noise (1, 0.16f, 1500.0f, 0.50f); break;                          // snare
            case 39: noise (1, 0.11f, 900.0f, 0.45f); break;                           // clap
            case 42: case 44: noise (2, 0.035f, 7000.0f, 0.22f); break;                // closed hat
            case 46: noise (2, 0.20f, 6500.0f, 0.22f); break;                          // open hat
            case 49: noise (2, 0.90f, 5000.0f, 0.25f); break;                          // crash
            case 51: noise (2, 0.35f, 7000.0f, 0.18f); break;                          // ride
            case 56: v.kind = 4; v.decay = 0.14f; v.gain = 0.25f; break;               // cowbell
            case 43: v.kind = 3; v.freq = 95.0f;  v.decay = 0.25f; v.gain = 0.6f; break;
            case 45: v.kind = 3; v.freq = 120.0f; v.decay = 0.25f; v.gain = 0.6f; break;
            case 48: v.kind = 3; v.freq = 150.0f; v.decay = 0.25f; v.gain = 0.6f; break;
            default: noise (2, 0.05f, 6000.0f, 0.2f); break;
        }
    }

    float drumSample (Voice& v)
    {
        v.t += 1.0 / sr;
        const float e = std::exp (-6.9f * (float) v.t / v.decay);
        if (e < 0.001f) { v.active = false; return 0.0f; }
        float s = 0.0f;
        switch (v.kind)
        {
            case 0:  // kick
                v.phase += (40.0 + 90.0 * std::exp (-v.t * 45.0)) / sr;
                s = std::sin (6.2831853f * (float) v.phase) * e;
                break;
            case 3:  // tom
                v.phase += (v.freq * (1.0 + 0.6 * std::exp (-v.t * 30.0))) / sr;
                s = std::sin (6.2831853f * (float) v.phase) * e;
                break;
            case 4:  // cowbell
                v.phase += 560.0 / sr; v.phase2 += 845.0 / sr;
                v.phase -= std::floor (v.phase); v.phase2 -= std::floor (v.phase2);
                s = ((v.phase < 0.5 ? 1.0f : -1.0f) + (v.phase2 < 0.5 ? 1.0f : -1.0f)) * 0.5f * e;
                break;
            default: // filtered noise
            {
                const float n = nextNoise();
                v.lp += v.hpCoef * (n - v.lp);
                s = (n - v.lp) * e;
                break;
            }
        }
        return s * v.amp * v.gain;
    }
};
