#include "madcap/MidiChord.h"
#include "madcap/Resample.h"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

extern "C" {
#include "params.h"
#include "theory.h"
}

TEST_CASE("Diatonic chord types are built from scale steps", "[theory]")
{
    float v[GS_MAX_VOICES];
    // C major, degree I
    REQUIRE(gs_build_chord(0, 1, 0, 0.0f, v) == 1);            // unison
    CHECK(v[0] == 0);
    REQUIRE(gs_build_chord(0, 1, 1, 0.0f, v) == 2);            // octave
    CHECK(v[1] == 12);
    REQUIRE(gs_build_chord(0, 1, 4, 0.0f, v) == 2);            // 5th
    CHECK(v[1] == 7);
    REQUIRE(gs_build_chord(0, 1, 5, 0.0f, v) == 3);            // triad C E G
    CHECK((v[0] == 0 && v[1] == 4 && v[2] == 7));
    REQUIRE(gs_build_chord(0, 1, 6, 0.0f, v) == 3);            // sus2 C D G
    CHECK(v[1] == 2);
    REQUIRE(gs_build_chord(0, 1, 7, 0.0f, v) == 3);            // sus4 C F G
    CHECK(v[1] == 5);
    REQUIRE(gs_build_chord(0, 1, 8, 0.0f, v) == 4);            // 7th
    CHECK(v[3] == 11);
    REQUIRE(gs_build_chord(0, 1, 9, 0.0f, v) == 5);            // 9th
    CHECK(v[4] == 14);
    // degree II in C major = D minor triad D F A
    REQUIRE(gs_build_chord(0, 1 << 1, 5, 0.0f, v) == 3);
    CHECK((v[0] == 2 && v[1] == 5 && v[2] == 9));
    // cluster I + V
    CHECK(gs_build_chord(0, 1 | (1 << 4), 5, 0.0f, v) == 5);   // C E G + G B D, G shared
    // open voicing lifts tones / adds a sub root
    CHECK(gs_build_chord(0, 1, 5, 1.0f, v) > 3);
}

TEST_CASE("Chord pad names", "[theory]")
{
    char buf[32];
    gs_degree_name(0, 1, 0, buf, sizeof buf);   // C minor I
    CHECK(std::string(buf) == "C min");
    gs_degree_name(0, 1, 1, buf, sizeof buf);   // II = D dim
    CHECK(std::string(buf) == "D dim");
    gs_degree_name(0, 0, 4, buf, sizeof buf);   // C major V = G maj
    CHECK(std::string(buf) == "G maj");
}

TEST_CASE("MIDI notes map to the nearest scale degree", "[midi]")
{
    // C major
    CHECK(gs_note_to_degree(0, 0, 60) == 0);
    CHECK(gs_note_to_degree(0, 0, 62) == 1);
    CHECK(gs_note_to_degree(0, 0, 71) == 6);
    CHECK(gs_note_to_degree(0, 0, 61) == 0);   // C# -> tie, lower degree
    CHECK(gs_note_to_degree(0, 0, 72 + 7) == 4);
    // A minor pentatonic (scale 8): A C D E G
    CHECK(gs_note_to_degree(9, 8, 69) == 0);
    CHECK(gs_note_to_degree(9, 8, 72) == 1);
    CHECK(gs_note_to_degree(9, 8, 67) == 4);
    // octaves
    CHECK(gs_note_to_octave(60) == 0);
    CHECK(gs_note_to_octave(59) == -1);
    CHECK(gs_note_to_octave(36) == -2);
    CHECK(gs_note_to_octave(12) == -2);
    CHECK(gs_note_to_octave(72) == 1);
    CHECK(gs_note_to_octave(120) == 1);
}

TEST_CASE("MidiChordState: cluster from several notes", "[midi]")
{
    madcap::MidiChordState m;
    CHECK_FALSE(m.anyHeld());
    m.noteOn(60, 0.5f);
    m.noteOn(64, 0.7f);
    m.noteOn(67, 0.9f);
    auto c = m.compute(0, 0, madcap::MidiChordState::Degree);
    CHECK(c.degreeMask == (1 | (1 << 2) | (1 << 4)));
    CHECK(c.octave == 0);
    CHECK(c.velocity == 0.9f);
    m.noteOn(64, 0.0f);   // velocity 0 = note off
    CHECK(m.heldCount() == 2);
    m.noteOff(60);
    m.noteOff(67);
    CHECK_FALSE(m.anyHeld());
    m.noteOn(50, 1.0f);   // D3, ROOT mode
    auto r = m.compute(0, 0, madcap::MidiChordState::Root);
    CHECK(r.rootKey == 2);
    CHECK(r.degreeMask == 1);
    CHECK(r.octave == -1);
}

TEST_CASE("Resampler keeps length ratio and pitch", "[resample]")
{
    const double src = 44100, dst = 96000;
    std::vector<float> s(44100);
    for (size_t i = 0; i < s.size(); ++i) s[i] = std::sin(2.0 * 3.14159265358979 * 440.0 * (double)i / src);
    auto out = madcap::resampleMono(s.data(), s.size(), src, dst);
    CHECK(std::abs((double)out.size() - 96000.0) <= 1.0);
    // count zero crossings in the middle second-ish: 440 Hz -> ~880 per second
    int zc = 0;
    for (size_t i = 1000; i + 1 < out.size() - 1000; ++i)
        if ((out[i] < 0) != (out[i + 1] < 0)) ++zc;
    double secs = (double)(out.size() - 2001) / dst;
    CHECK(std::abs(zc / secs - 880.0) < 6.0);
    float pk = 0;
    for (size_t i = 1000; i < out.size() - 1000; ++i) pk = std::max(pk, std::abs(out[i]));
    CHECK(pk > 0.97f);
    CHECK(pk < 1.03f);

    auto down = madcap::resampleMono(s.data(), s.size(), src, 22050);
    CHECK(std::abs((double)down.size() - 22050.0) <= 1.0);
}

TEST_CASE("Demo voice and posters exist", "[assets]")
{
    auto v = madcap::demoVoice(48000.0, 2.0f);
    CHECK(v.size() == 96000);
    float pk = 0;
    for (float x : v) pk = std::max(pk, std::abs(x));
    CHECK(pk > 0.5f);
    CHECK(pk <= 0.9f);
    CHECK(madcap::posterCount() == 4);
    for (int i = 0; i < 4; ++i) {
        auto px = madcap::renderPoster(i, 128, 64);
        auto g = madcap::lumaGridFromABGR(px.data(), 128, 64, 128);
        float mn = 1, mx = 0;
        for (float x : g) { mn = std::min(mn, x); mx = std::max(mx, x); }
        CHECK(mx - mn > 0.3f);   // there is a picture
    }
}
