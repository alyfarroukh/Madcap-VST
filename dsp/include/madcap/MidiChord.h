// MIDI -> harmony state. Framework-free so it can be unit-tested.
#pragma once
#include <array>
#include <cstddef>

namespace madcap {

class MidiChordState {
public:
    enum Mode { Degree = 0, Root = 1 };

    struct Chord {
        int degreeMask = 1;   // bit d = scale degree d (I..VII)
        int octave = 0;       // OCTAVE parameter override (-2..+1)
        int rootKey = -1;     // ROOT mode: pitch class that becomes the KEY, -1 otherwise
        float velocity = 1.0f;
    };

    void noteOn(int note, float velocity01) noexcept;
    void noteOff(int note) noexcept;
    void reset() noexcept;
    bool anyHeld() const noexcept { return count_ > 0; }
    int heldCount() const noexcept { return count_; }
    bool isHeld(int note) const noexcept { return note >= 0 && note < 128 && vel_[(std::size_t)note] > 0.0f; }

    // Chord for the currently held notes. key 0..11, scale index, mode Degree/Root.
    Chord compute(int key, int scale, int mode) const noexcept;

private:
    std::array<float, 128> vel_{};
    int count_ = 0;
    float lastVelocity_ = 1.0f;
};

} // namespace madcap
