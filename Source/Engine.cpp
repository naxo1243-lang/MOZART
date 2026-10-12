#include "Engine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <random>

namespace mozart
{
namespace
{
template <class T> inline T clampv (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int rnd (double x) { return (int) std::floor (x + 0.5); }   // JS Math.round

/* ---------------- tables ---------------- */

struct ScaleDef { const char* name; std::vector<int> iv; };

const std::vector<ScaleDef>& scales()
{
    static const std::vector<ScaleDef> s = {
        { "Major (Ionian)",    { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural Minor",     { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor",    { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",     { 0, 2, 3, 5, 7, 9, 11 } },
        { "Major Pentatonic",  { 0, 2, 4, 7, 9 } },
        { "Minor Pentatonic",  { 0, 3, 5, 7, 10 } },
        { "Blues",             { 0, 3, 5, 6, 7, 10 } },
        { "Dorian",            { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",          { 0, 1, 3, 5, 7, 8, 10 } },
        { "Phrygian Dominant", { 0, 1, 4, 5, 7, 8, 10 } },
        { "Mixolydian",        { 0, 2, 4, 5, 7, 9, 10 } },
        { "Lydian",            { 0, 2, 4, 6, 7, 9, 11 } },
        { "Whole Tone",        { 0, 2, 4, 6, 8, 10 } },
        { "Diminished (W-H)",  { 0, 2, 3, 5, 6, 8, 9, 11 } },
        { "Chromatic",         { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    return s;
}

const char* const ROOTS[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

struct BodySpec { const char* t; int b; double e; };
using Prog = std::vector<int>;
using Progs = std::vector<Prog>;

struct Genre
{
    const char* name; const char* scale; const char* root;
    int bpm; int subdiv; bool swing; int oct;
    bool chords7; const char* bass; const char* chordStyle; const char* drum;
    double density; bool ornaments; bool hasPad;
    int intro; int outro;
    std::vector<BodySpec> body;
    Progs verse, chorus, bridge;
    int gmLead, gmChord, gmBass, gmPad;
};

const std::vector<Genre>& genres()
{
    static const std::vector<Genre> g = {
    { "Pop", "Major (Ionian)", "C", 108, 8, false, 3, false, "pop", "block", "pop", 0.72, false, true, 4, 4,
      { { "Verse", 8, 0.55 }, { "PreChorus", 4, 0.72 }, { "Chorus", 8, 0.9 } },
      { { 0, 4, 5, 3 } }, { { 0, 4, 5, 3 } }, { { 3, 4, 5, 3 } }, 0, 5, 33, 91 },
    { "Synthwave", "Natural Minor", "A", 104, 16, false, 3, false, "eighths", "arpeggio", "synthwave", 0.6, false, true, 4, 4,
      { { "Verse", 8, 0.5 }, { "Build", 4, 0.65 }, { "Chorus", 8, 0.9 } },
      { { 0, 5, 2, 6 } }, { { 0, 5, 2, 6 }, { 0, 4, 5, 3 } }, { { 2, 5, 0, 4 } }, 81, 91, 33, 95 },
    { "Trap / Hip-Hop", "Natural Minor", "A", 140, 16, true, 2, false, "trap", "stabs", "trap", 0.5, false, true, 4, 4,
      { { "Verse", 8, 0.5 }, { "Hook", 8, 0.85 } },
      { { 0, 2 } }, { { 0, 2, 5, 4 } }, { { 2, 5, 4 } }, 4, 91, 37, 90 },
    { "Drill", "Natural Minor", "A#", 143, 16, false, 2, false, "trap", "stabs", "drill", 0.5, false, false, 4, 0,
      { { "Verse", 8, 0.45 }, { "Hook", 8, 0.85 } },
      { { 0 } }, { { 0, 2, 4, 5 } }, { { 5, 4 } }, 4, 91, 37, 0 },
    { "Reggaet\xC3\xB3n", "Natural Minor", "C#", 96, 8, false, 2, false, "dem", "skank", "reggaeton", 0.55, false, false, 4, 4,
      { { "Verse", 4, 0.6 }, { "Chorus", 4, 0.85 } },
      { { 0, 5, 4 } }, { { 0, 4, 5 }, { 0, 5, 4 } }, { { 5, 4, 3 } }, 28, 0, 33, 0 },
    { "Afrobeats", "Dorian", "F", 104, 8, false, 3, false, "four", "block", "afro", 0.6, false, true, 4, 4,
      { { "Verse", 8, 0.6 }, { "Chorus", 8, 0.85 } },
      { { 0, 3, 4, 3 } }, { { 0, 3, 5, 3 } }, { { 3, 4, 0 } }, 108, 13, 33, 96 },
    { "House", "Natural Minor", "A", 124, 16, false, 2, false, "eighths", "arpeggio", "house", 0.55, false, true, 8, 8,
      { { "Build", 8, 0.55 }, { "Drop", 8, 1.0 }, { "Break", 4, 0.35 } },
      { { 0, 5, 2, 6 } }, { { 0, 5, 2, 6 } }, { { 2, 5 } }, 81, 91, 33, 95 },
    { "Disco / Funk", "Dorian", "A", 112, 16, false, 3, true, "funk", "comp", "disco", 0.72, false, true, 4, 4,
      { { "Verse", 8, 0.65 }, { "Chorus", 8, 0.9 } },
      { { 1, 4, 0, 0 } }, { { 0, 3, 4, 3 } }, { { 4, 3, 0, 0 } }, 28, 4, 37, 48 },
    { "R&B / Soul", "Dorian", "C#", 76, 8, true, 3, true, "four", "comp", "rnb", 0.6, false, true, 4, 4,
      { { "Verse", 8, 0.5 }, { "Chorus", 8, 0.8 } },
      { { 0, 4, 3, 4 } }, { { 0, 4, 5, 3 } }, { { 1, 4, 0 } }, 4, 4, 33, 90 },
    { "Rock", "Minor Pentatonic", "E", 124, 8, false, 2, false, "eighths", "power", "rock", 0.66, false, false, 4, 4,
      { { "Verse", 8, 0.55 }, { "Chorus", 8, 0.9 } },
      { { 0, 3, 4 } }, { { 0, 3, 4 }, { 4, 3, 0, 0 } }, { { 5, 0, 3, 4 } }, 30, 30, 33, 0 },
    { "Pop-Punk", "Major (Ionian)", "C", 175, 8, false, 3, false, "eighths", "power", "poppunk", 0.6, false, false, 4, 4,
      { { "Verse", 8, 0.55 }, { "Chorus", 8, 0.9 } },
      { { 0, 4, 5, 3 } }, { { 0, 4, 5, 3 }, { 3, 4, 5, 0 } }, { { 5, 4, 3, 4 } }, 30, 30, 33, 0 },
    { "Metal", "Harmonic Minor", "E", 160, 16, false, 2, false, "eighths", "power", "metal", 0.78, false, false, 4, 4,
      { { "Verse", 8, 0.6 }, { "Chorus", 8, 0.95 } },
      { { 0, 5, 3 } }, { { 0, 2, 4, 5 } }, { { 5, 4, 3 } }, 30, 30, 33, 0 },
    { "Jazz", "Dorian", "C", 120, 8, true, 3, true, "walking", "comp", "jazz", 0.72, false, false, 0, 4,
      { { "A", 8, 0.7 }, { "B", 8, 0.6 } },
      { { 1, 4, 0, 0 } }, { { 1, 4, 0, 0 } }, { { 0, 3, 4, 3 } }, 66, 11, 32, 0 },
    { "Blues", "Blues", "E", 96, 8, true, 2, true, "walking", "comp", "blues", 0.68, false, false, 0, 0,
      { { "Verse", 12, 0.6 } },
      { { 0, 0, 0, 0, 3, 3, 0, 0, 4, 3, 0, 0 } }, { { 0, 0, 0, 0, 3, 3, 0, 0, 4, 3, 0, 0 } }, { { 0, 3, 4 } }, 30, 4, 33, 0 },
    { "Country / Pop", "Major (Ionian)", "G", 118, 8, false, 3, false, "four", "block", "country", 0.62, false, true, 4, 4,
      { { "Verse", 8, 0.55 }, { "Chorus", 8, 0.85 } },
      { { 0, 3, 4 } }, { { 0, 3, 4 }, { 0, 4, 3, 2 } }, { { 3, 4, 0 } }, 25, 0, 33, 48 },
    { "Bossa Nova", "Major (Ionian)", "D", 132, 8, false, 3, true, "bossa", "comp", "bossa", 0.58, false, false, 4, 4,
      { { "A", 8, 0.65 }, { "B", 8, 0.6 } },
      { { 1, 4, 0, 0 } }, { { 1, 4, 0, 0 } }, { { 0, 4, 3, 4 } }, 24, 4, 32, 0 },
    { "Classical", "Major (Ionian)", "C", 110, 16, false, 3, false, "albertiLow", "alberti", "classical", 0.78, true, true, 4, 4,
      { { "A", 8, 0.7 }, { "B", 8, 0.6 } },
      { { 0, 3, 4, 0 } }, { { 0, 3, 4, 0 } }, { { 0, 5, 3, 4, 0 } }, 40, 0, 42, 48 },
    { "Flamenco", "Phrygian Dominant", "E", 120, 8, false, 3, false, "rasgueado", "rasgueado", "flamenco", 0.72, false, false, 4, 4,
      { { "Verse", 8, 0.7 }, { "Chorus", 8, 0.85 } },
      { { 0, 6, 0, 5 } }, { { 0, 4, 5, 4 } }, { { 0, 2, 0, 3 } }, 24, 24, 33, 0 },
    { "City Pop", "Major (Ionian)", "F", 112, 16, false, 3, true, "city", "comp", "city", 0.7, false, true, 4, 4,
      { { "Verse", 8, 0.65 }, { "Chorus", 8, 0.9 } },
      { { 3, 4, 2, 5 } }, { { 0, 4, 5, 3 } }, { { 2, 5, 0, 4 } }, 66, 5, 33, 91 },
    { "Latin / Salsa", "Mixolydian", "G", 104, 8, false, 3, true, "montuno", "montuno", "latin", 0.68, false, false, 4, 4,
      { { "Verse", 8, 0.7 }, { "Chorus", 8, 0.9 } },
      { { 0, 3, 4, 3 } }, { { 0, 4, 0, 3 } }, { { 5, 4, 0, 2 } }, 56, 0, 33, 0 },
    { "Ambient", "Natural Minor", "A", 70, 8, false, 3, true, "pedal", "pad", "ambient", 0.38, false, true, 8, 8,
      { { "Verse", 8, 0.3 } },
      { { 0, 2, 5, 4 } }, { { 0, 5, 3, 4 } }, { { 2, 5, 0 } }, 92, 95, 96, 93 },
    { "Lo-fi", "Dorian", "D#", 78, 8, true, 3, true, "four", "comp", "lofi", 0.55, false, true, 4, 4,
      { { "Verse", 8, 0.45 }, { "Chorus", 8, 0.7 } },
      { { 1, 4, 0, 0 } }, { { 0, 4, 5, 3 } }, { { 3, 4, 5, 0 } }, 4, 4, 33, 96 },
    { "Classic Rock", "Mixolydian", "A", 118, 8, false, 2, false, "eighths", "power", "rock", 0.66, false, false, 4, 4,
      { { "Verse", 8, 0.55 }, { "Chorus", 8, 0.9 } },
      { { 0, 3, 4 } }, { { 0, 5, 3, 4 } }, { { 4, 3, 0 } }, 30, 30, 33, 0 },
    };
    return g;
}

int scaleIndexByName (const char* n)
{
    for (size_t i = 0; i < scales().size(); ++i) if (std::strcmp (scales()[i].name, n) == 0) return (int) i;
    return 0;
}
int rootIndexByName (const char* n)
{
    for (int i = 0; i < 12; ++i) if (std::strcmp (ROOTS[i], n) == 0) return i;
    return 0;
}

/* ---------------- PRNG (bit-exact mulberry32) ---------------- */

struct Rng
{
    uint32_t a;
    explicit Rng (uint32_t s) : a (s) {}
    double next()
    {
        a += 0x6D2B79F5u;
        uint32_t t = (a ^ (a >> 15)) * (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
    int randi (int lo, int hi) { return lo + (int) std::floor (next() * (hi - lo + 1)); }
    int choice (const std::vector<int>& v) { return v[(size_t) std::floor (next() * (double) v.size())]; }
    const Prog& choiceProg (const Progs& v) { return v[(size_t) std::floor (next() * (double) v.size())]; }
};

/* ---------------- helpers ---------------- */

int scalePitch (int rootM, const std::vector<int>& iv, int step, int extraOct = 0)
{
    const int n = (int) iv.size();
    const int o = step >= 0 ? step / n : -((-step + n - 1) / n);   // floor division
    const int i = ((step % n) + n) % n;
    return rootM + 12 * o + iv[(size_t) i] + 12 * extraOct;
}

void put (Track& t, int tick, int dur, int note, int vel)
{
    const uint8_t v = (uint8_t) clampv (vel, 1, 127);
    t.events.push_back ({ std::max (0, tick), true, (uint8_t) t.channel, (uint8_t) (note & 127), v, (uint8_t) t.role });
    t.events.push_back ({ std::max (0, tick + dur), false, (uint8_t) t.channel, (uint8_t) (note & 127), 0, (uint8_t) t.role });
}

Track newTrack (const char* name, int ch, Role role, int program)
{
    Track t; t.name = name; t.channel = ch; t.role = role; t.program = program;
    return t;
}

std::vector<Section> sectionize (const Genre& g, int bars)
{
    std::vector<Section> secs;
    int used = 0;
    const int introBars = (g.intro && bars >= g.intro + g.outro + 2) ? g.intro : 0;
    if (introBars) { secs.push_back ({ "Intro", introBars, 0.4 }); used += introBars; }
    int bodyLen = 0; for (auto& s : g.body) bodyLen += s.b;
    const int outroBars = (g.outro && bars >= used + bodyLen + g.outro) ? g.outro : 0;
    int remain = bars - used - outroBars;
    size_t bi = 0;
    while (remain > 0)
    {
        const BodySpec& spec = g.body[bi % g.body.size()];
        const int take = std::min (spec.b, remain);
        if (take <= 0) break;
        secs.push_back ({ spec.t, take, spec.e });
        remain -= take; used += take; ++bi;
    }
    if (outroBars) secs.push_back ({ "Outro", outroBars, 0.35 });
    return secs;
}

const Progs& progForKey (const Genre& g, const std::string& t)
{
    if (t == "Intro" || t == "Outro" || t == "Verse" || t == "Break") return g.verse;
    if (t == "PreChorus" || t == "Build" || t == "Bridge" || t == "B") return g.bridge.empty() ? g.verse : g.bridge;
    return g.chorus;
}

int registerFor (const std::string& t)
{
    if (t == "Chorus" || t == "Hook" || t == "Drop" || t == "A") return 7;
    if (t == "Bridge" || t == "B") return -2;
    return 0;
}

/* ---------------- drums ---------------- */

void generateDrums (Track& t, const std::string& style, int bars, bool swing,
                    const std::vector<std::string>& barType, const std::vector<double>& barEnergy,
                    double energyMul, Rng& rng)
{
    const int K = 36, S = 38, C = 39, R = 37, HH = 42, OH = 46, RD = 51, CR = 49, CB = 56, T1 = 48, T2 = 45, T3 = 43;
    std::string prevType;
    bool havePrev = false;
    for (int b = 0; b < bars; ++b)
    {
        const int bt = b * BAR_TICKS;
        const std::string& type = barType[(size_t) b];
        const double e = clampv (barEnergy[(size_t) b] * energyMul, 0.3, 1.1);
        const bool intoChorus = havePrev && prevType != type && (type == "Chorus" || type == "Hook" || type == "Drop");

        auto hit = [&] (int tick, int note, int vel, int len) { put (t, bt + tick, len, note, vel); };
        auto kick  = [&] (int tick, int vel) { hit (tick, K, vel, 8); };
        auto snare = [&] (int tick, int vel) { hit (tick, S, vel, 8); };
        auto clap  = [&] (int tick, int vel) { hit (tick, C, vel, 8); };
        auto hat   = [&] (int tick, bool open, int v) { hit (tick, open ? OH : HH, open ? 64 : v, 6); };
        auto ride  = [&] (int tick) { hit (tick, RD, 74, 6); };
        auto ghost = [&] (int tick) { hit (tick, S, 30, 6); };
        auto crash = [&] () { hit (0, CR, 88, 40); };
        auto fill  = [&] (bool intoC) {
            const int start = rnd (TPQ * 3.5);
            const int seq[6] = { S, T3, T2, T1, T2, T3 };
            for (int i = 0; i < 6; ++i)
                hit (start + i * rnd (TPQ / 8.0), seq[i], intoC ? 90 : 78, 20);
        };
        const bool isBoundary = (b > 0 && (b % 4 == 3));
        const bool notLast = b < bars - 1;

        if (style == "rock")
        {
            kick (0, rnd (100 * e)); kick (TPQ * 2, rnd (85 * e));
            snare (TPQ, rnd (95 * e)); snare (TPQ * 3, rnd (95 * e));
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2, i % 2 == 1 && rng.next() < 0.25, rnd (78 * e));
            if (notLast && (intoChorus || isBoundary)) { fill (intoChorus); if (intoChorus) crash(); }
        }
        else if (style == "pop")
        {
            kick (0, 98); kick (TPQ * 2, 90);
            snare (TPQ, rnd (90 * e)); snare (TPQ * 3, rnd (90 * e));
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2, false, rnd (72 * e));
            if (notLast && (intoChorus || isBoundary)) { fill (intoChorus); if (intoChorus) crash(); }
        }
        else if (style == "metal")
        {
            for (int i = 0; i < 4; ++i) kick (i * TPQ, rnd (100 * e));
            snare (TPQ, rnd (100 * e)); snare (TPQ * 3, rnd (100 * e));
            for (int i = 0; i < 16; ++i) hat (i * TPQ / 4, false, rnd (70 * e));
            if (notLast && (intoChorus || isBoundary)) fill (intoChorus);
        }
        else if (style == "edm" || style == "house" || style == "synthwave" || style == "disco")
        {
            for (int i = 0; i < 4; ++i) kick (i * TPQ, 100);
            clap (TPQ, rnd (92 * e)); clap (TPQ * 3, rnd (92 * e));
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2, i % 2 == 1, rnd (78 * e));
            if (notLast && (intoChorus || isBoundary)) { for (int i = 0; i < 4; ++i) snare (i * TPQ, rnd (60 * e)); crash(); }
        }
        else if (style == "poppunk")
        {
            for (int i = 0; i < 4; ++i) kick (i * TPQ, 96);
            snare (TPQ, 98); snare (TPQ * 3, 98);
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2, false, 84);
            if (notLast && (intoChorus || isBoundary)) fill (intoChorus);
        }
        else if (style == "trap")
        {
            kick (0, 100); kick (rnd (TPQ * 1.5), 78);
            if (rng.next() < 0.5) kick (rnd (TPQ * 2.5), 70);
            snare (TPQ, rnd (90 * e)); snare (TPQ * 3, rnd (90 * e));
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2 + ((swing && i % 2) ? rnd (TPQ / 3.0) : 0), false, rnd (70 * e));
            if (intoChorus) crash();
        }
        else if (style == "drill")
        {
            kick (0, 100); kick (rnd (TPQ * 1.5), 80);
            snare (TPQ, rnd (95 * e));
            for (int i = 0; i < 12; ++i) hat (rnd (i * TPQ * 2 / 3.0), false, rnd (66 * e));
        }
        else if (style == "reggaeton")
        {
            kick (0, 100); kick (TPQ * 2 + TPQ / 2, 70);
            clap (TPQ, 90); clap (TPQ * 3, 90);
            for (int i = 1; i < 8; i += 2) hat (i * TPQ / 2, false, 70);
        }
        else if (style == "afro")
        {
            kick (0, 95); kick (TPQ * 2, 72); kick (rnd (TPQ * 3.5), 60);
            snare (TPQ, rnd (80 * e)); snare (TPQ * 3, rnd (80 * e));
            hat (rnd (TPQ * 0.5), true, 60);
            for (int i = 0; i < 8; i += 2) hat (i * TPQ / 2, false, 64);
        }
        else if (style == "rnb" || style == "lofi")
        {
            kick (0, 90); kick (rnd (TPQ * 2.5), 72);
            snare (TPQ, rnd (80 * e)); snare (TPQ * 3, rnd (80 * e));
            if (rng.next() < 0.4) ghost (TPQ * 2 + TPQ / 2);
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2 + ((swing && i % 2) ? rnd (TPQ / 3.0) : 0), false, rnd (60 * e));
        }
        else if (style == "country")
        {
            kick (0, 90); kick (TPQ * 2, 76);
            snare (TPQ, 88); snare (TPQ * 3, 88);
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2, i % 2 == 1, 66);
        }
        else if (style == "city")
        {
            kick (0, 90); kick (TPQ * 2, 70);
            snare (TPQ, 85); snare (TPQ * 3, 85);
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2, false, 70);
        }
        else if (style == "jazz")
        {
            const int rd[6] = { 0, rnd (TPQ * 2 / 3.0), rnd (TPQ * 4 / 3.0), TPQ * 2,
                                TPQ * 2 + rnd (TPQ * 2 / 3.0), TPQ * 2 + rnd (TPQ * 4 / 3.0) };
            for (int x : rd) ride (x);
            if (swing) kick (0, 60); else { kick (0, 70); kick (TPQ * 2, 60); }
            if (rng.next() < 0.5) { const int s = rng.next() < 0.5 ? TPQ : TPQ * 3; hit (s, R, 70, 6); }
        }
        else if (style == "blues")
        {
            kick (0, 84); kick (TPQ * 2, 84);
            snare (TPQ, 92); snare (TPQ * 3, 92);
            for (int i = 0; i < 4; ++i) hat (i * TPQ + ((i % 2) ? rnd (TPQ * 2 / 3.0) : 0), i % 2 == 1, 74);
        }
        else if (style == "bossa")
        {
            hit (TPQ, R, 60, 6);
            ride (rnd (TPQ * 1.5)); ride (rnd (TPQ * 2.5));
            for (int i = 0; i < 8; i += 2) hat (i * TPQ / 2, false, 50);
        }
        else if (style == "latin")
        {
            const int cb[3] = { 0, rnd (TPQ * 1.5), TPQ * 3 };
            for (int tk : cb) hit (tk, CB, 80, 6);
            kick (0, 86); kick (rnd (TPQ * 2.5), 78);
            hit (TPQ * 2, R, 72, 6);
            for (int i = 0; i < 8; ++i) hat (i * TPQ / 2, false, 60);
        }
        else if (style == "flamenco")
        {
            for (int i = 0; i < 4; ++i) clap (i * TPQ, 80);
            kick (0, 80); kick (TPQ * 2, 80);
            hit (rnd (TPQ * 1.5), R, 60, 6);
        }
        else if (style == "ambient")
        {
            if (b % 4 == 0) kick (0, 40);
            if (b % 2 == 1 && rng.next() < 0.5) hit (TPQ * 2, CR, 40, 60);
        }
        else if (style == "classical")
        {
            if (b % 2 == 0) kick (0, 34);
        }
        prevType = type; havePrev = true;
    }
}

bool eventLess (const Event& a, const Event& b)
{
    if (a.tick != b.tick) return a.tick < b.tick;
    return (!a.on && b.on);     // note-offs before note-ons on the same tick
}

} // namespace

/* ---------------- public API ---------------- */

int Track::noteCount() const
{
    int n = 0; for (auto& e : events) if (e.on) ++n;
    return n;
}
const Track* Song::find (Role r) const
{
    for (auto& t : tracks) if (t.role == r) return &t;
    return nullptr;
}

int genreCount() { return (int) genres().size(); }
const char* genreName (int i) { return genres()[(size_t) clampv (i, 0, genreCount() - 1)].name; }
int scaleCount() { return (int) scales().size(); }
const char* scaleName (int i) { return scales()[(size_t) clampv (i, 0, scaleCount() - 1)].name; }
int rootCount() { return 12; }
const char* rootName (int i) { return ROOTS[clampv (i, 0, 11)]; }

GenreDefaults genreDefaults (int genre)
{
    const Genre& g = genres()[(size_t) clampv (genre, 0, genreCount() - 1)];
    int bodyLen = 0; for (auto& s : g.body) bodyLen += s.b;
    return { scaleIndexByName (g.scale), rootIndexByName (g.root), g.oct, g.bpm,
             g.intro + g.outro + bodyLen * 2, (int) std::lround (g.density * 100.0) };
}

static uint32_t toUint32 (double d)
{
    if (! std::isfinite (d)) return 0;
    d = std::trunc (d);
    d = std::fmod (d, 4294967296.0);
    if (d < 0) d += 4294967296.0;
    return (uint32_t) d;
}

std::shared_ptr<Song> generate (const Options& optsIn)
{
    auto song = std::make_shared<Song>();
    Options opts = optsIn;
    opts.genre  = clampv (opts.genre, 0, genreCount() - 1);
    opts.scale  = clampv (opts.scale, 0, scaleCount() - 1);
    opts.root   = clampv (opts.root, 0, 11);
    opts.octave = clampv (opts.octave, 0, 6);
    opts.bpm    = clampv (opts.bpm, 40, 240);
    opts.bars   = clampv (opts.bars, 4, 128);
    opts.density = clampv (opts.density, 0, 100);
    opts.energy  = clampv (opts.energy, 0, 100);

    const Genre& g = genres()[(size_t) opts.genre];
    const std::vector<int>& intervals = scales()[(size_t) opts.scale].iv;
    const int rootM = 12 * (opts.octave + 1) + opts.root;
    const bool swing = opts.swing == 0 ? g.swing : (opts.swing == 1);
    const int bars = opts.bars;
    const double density = opts.density / 100.0;
    const double energyMul = 0.6 + (opts.energy / 100.0) * 0.8;
    const bool humanize = opts.humanize;

    // seed
    uint32_t seedU = 0;
    {
        std::string s = opts.seed;
        while (! s.empty() && std::isspace ((unsigned char) s.back())) s.pop_back();
        size_t st = 0; while (st < s.size() && std::isspace ((unsigned char) s[st])) ++st;
        s = s.substr (st);
        bool ok = false;
        if (! s.empty())
        {
            char* end = nullptr;
            const double d = std::strtod (s.c_str(), &end);
            if (end && *end == '\0' && ! std::isnan (d)) { ok = true; seedU = toUint32 (d); }
        }
        if (! ok)
        {
            std::random_device rd;
            seedU = rd() & 0x7fffffffu;
            s = std::to_string (seedU);
        }
        song->seedText = s;
    }
    Rng rng (seedU);

    const int total = bars * BAR_TICKS;
    song->sections = sectionize (g, bars);

    std::vector<std::string> barType; std::vector<int> chords; std::vector<double> barEnergy;
    for (auto& sec : song->sections)
    {
        const Prog& p = rng.choiceProg (progForKey (g, sec.type));
        for (int i = 0; i < sec.bars; ++i)
        {
            barType.push_back (sec.type);
            chords.push_back (p[(size_t) i % p.size()]);
            barEnergy.push_back (sec.energy);
        }
    }

    auto chordSteps = [&] (int deg, bool seventh) {
        std::vector<int> s = { deg, deg + 2, deg + 4 };
        if (seventh) s.push_back (deg + 6);
        return s;
    };
    auto chordNotes = [&] (int deg, int octShift) {
        std::vector<int> out;
        for (int st : chordSteps (deg, g.chords7)) out.push_back (clampv (scalePitch (rootM, intervals, st, octShift), 21, 108));
        return out;
    };

    int contour[8];
    {
        const std::vector<int> opts5 = { -2, -1, 0, 1, 2 };
        for (int i = 0; i < 8; ++i) contour[i] = rng.choice (opts5);
    }

    /* ---- Melody ---- */
    Track mel = newTrack ("Melody", 0, RoleMelody, g.gmLead);
    {
        const int cells = g.subdiv == 16 ? 16 : 8;
        const int cell = BAR_TICKS / cells;
        for (int b = 0; b < bars; ++b)
        {
            const int deg = chords[(size_t) b];
            const std::string& type = barType[(size_t) b];
            const double energy = barEnergy[(size_t) b] * energyMul;
            const int reg = registerFor (type);
            const std::vector<int> ct = chordSteps (deg, g.chords7);
            const double dLoc = clampv (density * (0.4 + energy * 0.8), 0.15, 1.0);
            int step = deg;
            for (int c = 0; c < cells; ++c)
            {
                const bool strong = (c % (cells / 4)) == 0;
                if (rng.next() > dLoc) { if (rng.next() < 0.2) step += rng.randi (-2, 2); continue; }
                if (strong && rng.next() < 0.6)
                    step = rng.choice (ct);
                else
                {
                    step += contour[c % 8];
                    if (rng.next() < 0.35)
                        step = ct[(size_t) std::min ((int) ct.size() - 1, (int) std::floor (rng.next() * (double) ct.size()))];
                }
                if (c == cells - 1 && ! strong) step = rng.choice (ct);
                step = clampv (step, deg - 7, deg + 12);
                const int pitch = clampv (scalePitch (rootM, intervals, step) + reg, 40, 98);
                int tick = b * BAR_TICKS;
                if (swing && cells == 8) tick += (c / 2) * TPQ + ((c % 2) ? rnd (TPQ * 2 / 3.0) : 0);
                else tick += c * cell;
                const int dur = (swing && cells == 8) ? rnd (cell * 0.6) : rnd (cell * 0.85);
                int vel = strong ? rng.randi (92, 108) : rng.randi (58, 82);
                vel = rnd (vel * clampv (0.7 + energy * 0.45, 0.7, 1.15));
                if (g.ornaments && rng.next() < 0.25)
                {
                    const int upper = clampv (scalePitch (rootM, intervals, step + 1) + reg, 40, 98);
                    const int n = 6;
                    const int sp = std::max (20, rnd (dur / (double) n));
                    for (int k = 0; k < n; ++k)
                        put (mel, tick + k * sp, sp - 2, (k % 2 == 0) ? pitch : upper, std::max (1, vel - 10));
                    continue;
                }
                if (humanize) { tick += rng.randi (-2, 2); vel = clampv (vel + rng.randi (-5, 5), 1, 127); }
                put (mel, tick, dur, pitch, vel);
            }
        }
    }

    /* ---- Harmony ---- */
    Track chordTrack = newTrack ("Harmony", 1, RoleChord, g.gmChord);
    {
        const std::string p = g.chordStyle;
        for (int b = 0; b < bars; ++b)
        {
            const int deg = chords[(size_t) b];
            const double energy = barEnergy[(size_t) b] * energyMul;
            const int bt = b * BAR_TICKS;
            const std::vector<int> notes = chordNotes (deg, 0);
            const int vel = clampv (rnd (60 + energy * 30), 55, 100);
            if (p == "block" || p == "pad" || p == "stabs")
            {
                const int whole = p == "stabs" ? rnd (TPQ * 0.5) : BAR_TICKS;
                const int off = p == "stabs" ? (swing ? rnd (TPQ * 2 / 3.0) : TPQ) : 0;
                for (int n : notes) put (chordTrack, bt + off, whole, n, vel);
            }
            else if (p == "alberti")
            {
                const int seq[8] = { notes[0] - 12, notes[0], notes[1], notes[0], notes[0] - 12, notes[0], notes[1], notes[0] };
                const int cell = BAR_TICKS / 8;
                for (int i = 0; i < 8; ++i)
                    put (chordTrack, bt + i * cell, cell - 6, clampv (seq[i], 30, 96), vel - 10);
            }
            else if (p == "arpeggio")
            {
                const int cell = BAR_TICKS / 16;
                std::vector<int> arp = notes; arp.push_back (notes[0] + 12);
                std::sort (arp.begin(), arp.end());
                for (int i = 0; i < 16; ++i)
                    put (chordTrack, bt + i * cell, cell - 4, clampv (arp[(size_t) i % arp.size()], 36, 96), vel);
            }
            else if (p == "comp")
            {
                const int stabs[2] = { swing ? rnd (TPQ * 2 / 3.0) + TPQ : rnd (TPQ * 1.5),
                                       swing ? rnd (TPQ * 2 / 3.0) + TPQ * 3 : rnd (TPQ * 3.5) };
                for (int off : stabs)
                {
                    std::vector<int> noteset (notes.begin(), notes.begin() + 3);
                    if (rng.next() < 0.5 && notes.size() > 3) noteset.push_back (notes[3]);
                    for (int n : noteset) put (chordTrack, bt + off, rnd (TPQ * 0.45), n, vel);
                }
            }
            else if (p == "power")
            {
                put (chordTrack, bt, BAR_TICKS - 4, notes[0], vel + 6);
                put (chordTrack, bt, BAR_TICKS - 4, notes[1], vel + 6);
            }
            else if (p == "skank" || p == "montuno")
            {
                const int cell = BAR_TICKS / 8;
                for (int i = 1; i < 8; i += 2)
                    for (int n : notes) put (chordTrack, bt + i * cell, rnd (cell * 0.5), n, vel);
            }
            else if (p == "rasgueado")
            {
                for (int beat = 0; beat < 4; ++beat)
                    for (size_t ix = 0; ix < notes.size(); ++ix)
                        put (chordTrack, bt + beat * TPQ + (int) ix * 12, 40, notes[ix], 90);
            }
        }
    }

    /* ---- Bass ---- */
    Track bassTrack = newTrack ("Bass", 2, RoleBass, g.gmBass);
    {
        const std::string p = g.bass;
        for (int b = 0; b < bars; ++b)
        {
            const int deg = chords[(size_t) b];
            const int bt = b * BAR_TICKS;
            const int rootN  = clampv (scalePitch (rootM, intervals, deg, -2), 26, 60);
            const int fifthN = clampv (scalePitch (rootM, intervals, deg + 4, -2), 26, 60);
            if (p == "walking")
            {
                for (int beat = 0; beat < 4; ++beat)
                    put (bassTrack, bt + beat * TPQ, TPQ - 8, clampv (scalePitch (rootM, intervals, deg + beat, -2), 26, 60), 92);
            }
            else if (p == "eighths")
            {
                for (int i = 0; i < 8; ++i)
                {
                    const int n = (i % 2 == 0) ? rootN : (rng.next() < 0.3 ? fifthN : rootN);
                    put (bassTrack, bt + i * (TPQ / 2), rnd (TPQ * 0.45), n, 88);
                }
            }
            else if (p == "four" || p == "pop")
            {
                for (int beat = 0; beat < 4; ++beat)
                    put (bassTrack, bt + beat * TPQ, rnd (TPQ * 0.8), (beat % 2 == 0) ? rootN : fifthN, 90);
            }
            else if (p == "albertiLow")
            {
                const int seq[8] = { rootN, fifthN, rootN + 12, fifthN, rootN, fifthN, rootN + 12, fifthN };
                const int cell = BAR_TICKS / 8;
                for (int i = 0; i < 8; ++i) put (bassTrack, bt + i * cell, cell - 6, seq[i], 80);
            }
            else if (p == "funk" || p == "city" || p == "bossa")
            {
                std::vector<int> pat;
                if (p == "funk") pat = { 0, 3, 6, 8, 11, 14 };
                else if (p == "bossa") pat = { 0, 2, 4, 6 };
                else pat = { 0, 3, 4, 7, 8, 11, 12, 14 };
                const int cell = BAR_TICKS / 16;
                for (int slot : pat)
                {
                    const int n = (slot % 4 == 0) ? rootN : (rng.next() < 0.4 ? rootN : fifthN);
                    put (bassTrack, bt + slot * cell, rnd (cell * 0.7), n, 86);
                }
            }
            else if (p == "trap" || p == "dem")
            {
                const int sub = clampv (rootN - 12, 24, 48);
                const std::vector<double> hits = (p == "dem") ? std::vector<double> { 0, 1.5, 2 } : std::vector<double> { 0, 2.5 };
                for (double hb : hits) put (bassTrack, bt + rnd (hb * TPQ), rnd (TPQ * 0.9), sub, 96);
            }
            else if (p == "montuno" || p == "rasgueado")
            {
                for (int beat = 0; beat < 4; ++beat)
                    put (bassTrack, bt + beat * TPQ, rnd (TPQ * 0.5), (beat % 2 == 0) ? rootN : fifthN, 90);
            }
            else if (p == "pedal")
            {
                if (b % 4 == 0) put (bassTrack, bt, BAR_TICKS * 4 - 6, rootN, 70);
            }
        }
    }

    /* ---- Pad ---- */
    Track padTrack = newTrack ("Pad", 3, RolePad, g.gmPad);
    if (g.hasPad)
    {
        for (int b = 0; b < bars; b += 2)
        {
            const std::vector<int> notes = chordNotes (chords[(size_t) b], 0);
            for (int n : notes) put (padTrack, b * BAR_TICKS, BAR_TICKS * 2 - 10, n, 58);
        }
    }

    /* ---- Drums ---- */
    Track drums = newTrack ("Drums", 9, RoleDrums, -1);
    if (g.drum && *g.drum)
        generateDrums (drums, g.drum, bars, swing, barType, barEnergy, energyMul, rng);

    /* ---- finalize ---- */
    song->tracks.push_back (std::move (mel));
    song->tracks.push_back (std::move (chordTrack));
    song->tracks.push_back (std::move (bassTrack));
    if (g.hasPad) song->tracks.push_back (std::move (padTrack));
    if (g.drum && *g.drum) song->tracks.push_back (std::move (drums));

    for (auto& t : song->tracks)
    {
        for (auto& e : t.events)
            e.tick = e.on ? std::min (e.tick, total - 1) : std::min (e.tick, total - 1);
        std::stable_sort (t.events.begin(), t.events.end(), eventLess);
        song->merged.insert (song->merged.end(), t.events.begin(), t.events.end());
    }
    std::stable_sort (song->merged.begin(), song->merged.end(), eventLess);

    song->totalTicks = total;
    song->swing = swing;
    song->opts = opts;
    return song;
}

/* ---------------- MIDI file writer ---------------- */

namespace
{
void vlq (std::vector<uint8_t>& out, uint32_t v)
{
    uint8_t buf[5]; int n = 0;
    buf[n++] = (uint8_t) (v & 0x7f); v >>= 7;
    while (v) { buf[n++] = (uint8_t) (0x80 | (v & 0x7f)); v >>= 7; }
    while (n) out.push_back (buf[--n]);
}
void w32 (std::vector<uint8_t>& o, uint32_t v) { o.push_back ((uint8_t) (v >> 24)); o.push_back ((uint8_t) (v >> 16)); o.push_back ((uint8_t) (v >> 8)); o.push_back ((uint8_t) v); }
void w16 (std::vector<uint8_t>& o, uint32_t v) { o.push_back ((uint8_t) (v >> 8)); o.push_back ((uint8_t) v); }
void chunk (std::vector<uint8_t>& out, const std::vector<uint8_t>& data)
{
    out.push_back ('M'); out.push_back ('T'); out.push_back ('r'); out.push_back ('k');
    w32 (out, (uint32_t) data.size());
    out.insert (out.end(), data.begin(), data.end());
}
void nameMeta (std::vector<uint8_t>& d, const std::string& name)
{
    vlq (d, 0); d.push_back (0xFF); d.push_back (0x03); d.push_back ((uint8_t) name.size());
    for (char c : name) d.push_back ((uint8_t) (c & 0x7f));
}
}

std::vector<uint8_t> buildMidi (const Song& s, unsigned roleMask)
{
    std::vector<const Track*> list;
    for (auto& t : s.tracks) if (roleMask & (1u << t.role)) list.push_back (&t);

    std::vector<uint8_t> out;
    out.push_back ('M'); out.push_back ('T'); out.push_back ('h'); out.push_back ('d');
    w32 (out, 6); w16 (out, 1); w16 (out, (uint32_t) list.size() + 1); w16 (out, TPQ);

    { // control track
        std::vector<uint8_t> d;
        nameMeta (d, "Control");
        const uint32_t us = (uint32_t) std::lround (60000000.0 / s.opts.bpm);
        vlq (d, 0); d.push_back (0xFF); d.push_back (0x51); d.push_back (0x03);
        d.push_back ((uint8_t) (us >> 16)); d.push_back ((uint8_t) (us >> 8)); d.push_back ((uint8_t) us);
        vlq (d, 0); d.push_back (0xFF); d.push_back (0x58); d.push_back (0x04);
        d.push_back (4); d.push_back (2); d.push_back (24); d.push_back (8);
        vlq (d, (uint32_t) s.totalTicks); d.push_back (0xFF); d.push_back (0x2F); d.push_back (0x00);
        chunk (out, d);
    }
    for (const Track* t : list)
    {
        std::vector<uint8_t> d;
        nameMeta (d, t->name);
        if (t->program >= 0) { vlq (d, 0); d.push_back ((uint8_t) (0xC0 | t->channel)); d.push_back ((uint8_t) t->program); }
        int last = 0;
        for (auto& e : t->events)
        {
            vlq (d, (uint32_t) (e.tick - last)); last = e.tick;
            d.push_back ((uint8_t) ((e.on ? 0x90 : 0x80) | e.channel));
            d.push_back (e.note);
            d.push_back (e.on ? e.vel : 0);
        }
        vlq (d, (uint32_t) std::max (0, s.totalTicks - last));
        d.push_back (0xFF); d.push_back (0x2F); d.push_back (0x00);
        chunk (out, d);
    }
    return out;
}

} // namespace mozart
