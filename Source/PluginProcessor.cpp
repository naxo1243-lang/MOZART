#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

MozartProcessor::MozartProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    for (auto& r : roleEnabled) r = true;
    applyGenreDefaults (0);
    regenerate();
}

void MozartProcessor::applyGenreDefaults (int genre)
{
    const auto d = mozart::genreDefaults (genre);
    settings.genre = genre;
    settings.scale = d.scale;
    settings.root = d.root;
    settings.octave = d.octave;
    settings.bpm = d.bpm;
    settings.bars = d.bars;
    settings.density = d.density;
    settings.swing = 0;
    bpmAtomic = settings.bpm;
}

void MozartProcessor::regenerate (const juce::String& seedOverride)
{
    mozart::Options o;
    o.genre = settings.genre;   o.scale = settings.scale;     o.root = settings.root;
    o.octave = settings.octave; o.bpm = settings.bpm;         o.bars = settings.bars;
    o.swing = settings.swing;   o.density = settings.density; o.energy = settings.energy;
    o.humanize = settings.humanize;
    o.seed = (seedOverride.isNotEmpty() ? seedOverride : settings.seed).toStdString();

    std::shared_ptr<const mozart::Song> fresh = mozart::generate (o);
    std::shared_ptr<const mozart::Song> old;
    {
        const juce::SpinLock::ScopedLockType sl (songLock);
        old = song;
        song = fresh;
    }
    resolvedSeed = juce::String (fresh->seedText);
    bpmAtomic = fresh->opts.bpm;
    flushRequested = true;
}

void MozartProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
    pending.clear();
    pending.reserve (8192);
    flushRequested = true;
}

bool MozartProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void MozartProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    midi.clear();                                   // this plugin is a generator: input MIDI is ignored

    auto flushAll = [&] (int offs)
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int nt = 0; nt < 128; ++nt)
                if (held[ch][nt]) { midi.addEvent (juce::MidiMessage::noteOff (ch + 1, nt), offs); held[ch][nt] = false; }
        synth.allOff();
    };

    const juce::SpinLock::ScopedTryLockType lock (songLock);
    if (! lock.isLocked() || song == nullptr)
        return;
    const mozart::Song& s = *song;

    if (flushRequested.exchange (false))
        flushAll (0);

    // ---- transport: host playhead has priority, otherwise internal preview clock ----
    int mode = 0;                                   // 0 stopped, 1 host, 2 preview
    double startTick = 0.0, tps = 0.0;              // tps = ticks per sample
    const double sr = getSampleRate();

    if (auto* ph = getPlayHead())
    {
        auto pos = ph->getPosition();
        if (pos.hasValue() && pos->getIsPlaying())
        {
            auto ppq = pos->getPpqPosition();
            auto bpm = pos->getBpm();
            if (ppq.hasValue())
            {
                const double b = bpm.hasValue() ? *bpm : (double) bpmAtomic.load();
                startTick = *ppq * mozart::TPQ;
                tps = b / 60.0 * mozart::TPQ / sr;
                mode = 1;
            }
        }
    }
    if (mode == 0 && previewPlaying.load())
    {
        if (previewReset.exchange (false)) { previewTick = 0.0; flushAll (0); }
        startTick = previewTick;
        tps = (double) bpmAtomic.load() / 60.0 * mozart::TPQ / sr;
        previewTick += n * tps;
        mode = 2;
    }

    if (mode != lastMode)                           // entering/leaving a transport mode
        flushAll (0);
    else if (mode == 1 && std::abs (startTick - expectedTick) > 8.0)   // host locate / loop jump
        flushAll (0);
    lastMode = mode;
    transportRunning = (mode != 0);

    if (mode == 0 || s.totalTicks <= 0 || tps <= 0.0)
    {
        // let any synth tail ring out
        synth.render (buffer.getWritePointer (0), buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr, n);
        return;
    }
    expectedTick = startTick + n * tps;

    // ---- collect events for this block (pattern loops every song.totalTicks) ----
    const double total = (double) s.totalTicks;
    double p = std::fmod (startTick, total);
    if (p < 0.0) p += total;
    const double endP = p + n * tps;
    currentBar = (int) (p / (double) mozart::BAR_TICKS);

    const bool sound = internalSound.load() || mode == 2;
    pending.clear();
    const auto& ev = s.merged;

    auto emit = [&] (double fromTick, double toTick, double sampleBase)
    {
        auto it = std::lower_bound (ev.begin(), ev.end(), fromTick,
                                    [] (const mozart::Event& e, double t) { return (double) e.tick < t; });
        for (; it != ev.end() && (double) it->tick < toTick; ++it)
        {
            const int offs = juce::jlimit (0, n - 1, (int) std::floor (sampleBase + ((double) it->tick - fromTick) / tps));
            const int ch = it->channel, nt = it->note;
            if (it->on)
            {
                if (! roleEnabled[it->role].load()) continue;
                if (held[ch][nt]) { midi.addEvent (juce::MidiMessage::noteOff (ch + 1, nt), offs); }   // re-trigger safety
                midi.addEvent (juce::MidiMessage::noteOn (ch + 1, nt, (juce::uint8) it->vel), offs);
                held[ch][nt] = true;
                if (sound && pending.size() < pending.capacity()) pending.push_back ({ offs, true, ch, nt, (int) it->vel });
            }
            else
            {
                if (held[ch][nt]) { midi.addEvent (juce::MidiMessage::noteOff (ch + 1, nt), offs); held[ch][nt] = false; }
                if (sound && pending.size() < pending.capacity()) pending.push_back ({ offs, false, ch, nt, 0 });
            }
        }
    };

    if (endP <= total) emit (p, endP, 0.0);
    else { emit (p, total, 0.0); emit (0.0, endP - total, (total - p) / tps); }

    // ---- render the built-in synth in segments between events ----
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    int pos = 0;
    for (const auto& pe : pending)
    {
        if (pe.offs > pos) { synth.render (L + pos, R ? R + pos : nullptr, pe.offs - pos); pos = pe.offs; }
        if (pe.on) synth.noteOn (pe.ch, pe.note, pe.vel); else synth.noteOff (pe.ch, pe.note);
    }
    if (pos < n) synth.render (L + pos, R ? R + pos : nullptr, n - pos);
}

juce::AudioProcessorEditor* MozartProcessor::createEditor() { return new MozartEditor (*this); }

void MozartProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    juce::XmlElement xml ("MozartState");
    xml.setAttribute ("genre", settings.genre);
    xml.setAttribute ("scale", settings.scale);
    xml.setAttribute ("root", settings.root);
    xml.setAttribute ("octave", settings.octave);
    xml.setAttribute ("bpm", settings.bpm);
    xml.setAttribute ("bars", settings.bars);
    xml.setAttribute ("swing", settings.swing);
    xml.setAttribute ("density", settings.density);
    xml.setAttribute ("energy", settings.energy);
    xml.setAttribute ("humanize", (int) settings.humanize);
    xml.setAttribute ("seed", settings.seed);
    xml.setAttribute ("resolvedSeed", resolvedSeed);
    xml.setAttribute ("internalSound", (int) internalSound.load());
    for (int r = mozart::RoleMelody; r < mozart::NumRoles; ++r)
        xml.setAttribute ("role" + juce::String (r), (int) roleEnabled[r].load());
    copyXmlToBinary (xml, dest);
}

void MozartProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName ("MozartState"))
        return;

    settings.genre    = xml->getIntAttribute ("genre", 0);
    settings.scale    = xml->getIntAttribute ("scale", 0);
    settings.root     = xml->getIntAttribute ("root", 0);
    settings.octave   = xml->getIntAttribute ("octave", 3);
    settings.bpm      = xml->getIntAttribute ("bpm", 120);
    settings.bars     = xml->getIntAttribute ("bars", 32);
    settings.swing    = xml->getIntAttribute ("swing", 0);
    settings.density  = xml->getIntAttribute ("density", 60);
    settings.energy   = xml->getIntAttribute ("energy", 50);
    settings.humanize = xml->getIntAttribute ("humanize", 1) != 0;
    settings.seed     = xml->getStringAttribute ("seed");
    internalSound     = xml->getIntAttribute ("internalSound", 0) != 0;
    for (int r = mozart::RoleMelody; r < mozart::NumRoles; ++r)
        roleEnabled[r] = xml->getIntAttribute ("role" + juce::String (r), 1) != 0;

    regenerate (xml->getStringAttribute ("resolvedSeed"));   // same seed => identical piece after reload
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MozartProcessor(); }
