// Mozart engine: pure C++ port of the generative MIDI engine (no JUCE dependency).
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mozart
{
constexpr int TPQ = 480;
constexpr int BAR_TICKS = TPQ * 4;

enum Role { RoleControl = 0, RoleMelody, RoleChord, RoleBass, RolePad, RoleDrums, NumRoles };

struct Event
{
    int tick;
    bool on;
    uint8_t channel, note, vel, role;
};

struct Track
{
    std::string name;
    int channel = 0;
    Role role = RoleControl;
    int program = -1;
    std::vector<Event> events;
    int noteCount() const;
};

struct Section { std::string type; int bars; double energy; };

struct Options
{
    int genre = 0, scale = 0, root = 0, octave = 3, bpm = 120, bars = 32;
    int swing = 0;              // 0 = auto (genre), 1 = on, 2 = off
    int density = 60, energy = 50;
    bool humanize = true;
    std::string seed;           // "" or non-numeric = random
};

struct Song
{
    Options opts;               // sanitized options actually used
    std::string seedText;       // resolved seed (feed back into Options::seed to reproduce)
    bool swing = false;
    int totalTicks = 0;
    std::vector<Track> tracks;  // melody, harmony, bass, pad, drums (no control track)
    std::vector<Event> merged;  // all tracks, sorted for realtime playback
    std::vector<Section> sections;
    const Track* find (Role r) const;
};

int genreCount();            const char* genreName (int i);
int scaleCount();            const char* scaleName (int i);
int rootCount();             const char* rootName (int i);

struct GenreDefaults { int scale, root, octave, bpm, bars, density; };
GenreDefaults genreDefaults (int genre);

std::shared_ptr<Song> generate (const Options& options);

// Standard MIDI file (format 1). roleMask: bit (1 << role) selects tracks; a control track is always included.
std::vector<uint8_t> buildMidi (const Song& song, unsigned roleMask = 0xFFFFFFFFu);
}
