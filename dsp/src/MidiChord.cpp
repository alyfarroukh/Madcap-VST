#include "madcap/MidiChord.h"
#include "theory.h"

namespace madcap {

void MidiChordState::noteOn(int note, float velocity01) noexcept
{
    if (note < 0 || note > 127) return;
    if (velocity01 <= 0.0f) { noteOff(note); return; }
    if (vel_[(size_t)note] <= 0.0f) ++count_;
    vel_[(size_t)note] = velocity01 > 1.0f ? 1.0f : velocity01;
    lastVelocity_ = vel_[(size_t)note];
}

void MidiChordState::noteOff(int note) noexcept
{
    if (note < 0 || note > 127) return;
    if (vel_[(size_t)note] > 0.0f) {
        vel_[(size_t)note] = 0.0f;
        if (count_ > 0) --count_;
    }
}

void MidiChordState::reset() noexcept
{
    vel_.fill(0.0f);
    count_ = 0;
}

MidiChordState::Chord MidiChordState::compute(int key, int scale, int mode) const noexcept
{
    Chord c;
    c.velocity = lastVelocity_;
    if (count_ == 0) return c;
    int lowest = -1;
    int mask = 0;
    for (int n = 0; n < 128; ++n) {
        if (vel_[(size_t)n] <= 0.0f) continue;
        if (lowest < 0) lowest = n;
        if (mode == Degree) mask |= 1 << gs_note_to_degree(key, scale, n);
    }
    if (lowest < 0) return c;
    c.octave = gs_note_to_octave(lowest);
    if (mode == Root) {
        c.rootKey = lowest % 12;
        c.degreeMask = 1;
    } else {
        c.degreeMask = mask ? mask : 1;
    }
    return c;
}

} // namespace madcap
