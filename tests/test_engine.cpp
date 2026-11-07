#include "TestUtil.h"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <set>

using test::makeEngine;
using test::render;

namespace {

void setExtreme(madcap::Engine& e, int variant)
{
    e.setTarget(P_MASTER, 18.0f);
    e.setTarget(P_GRAIN_LVL, 1.5f);
    e.setTarget(P_ARP_LEVEL, 1.5f);
    e.setTarget(P_IMG_LEVEL, 1.5f);
    e.setTarget(P_IMG_DRY, 1.5f);
    e.setTarget(P_IMG_SWEEP, 1.0f);
    e.setTarget(P_DENSITY, 60.0f);
    e.setTarget(P_SIZE, variant & 1 ? 1000.0f : 10.0f);
    e.setTarget(P_SPRAY, 1.0f);
    e.setTarget(P_SCAN, variant & 2 ? -1.0f : 1.0f);
    e.setTarget(P_REVERSE, 0.5f);
    e.setTarget(P_OCTAVES, 1.0f);
    e.setTarget(P_DETUNE, 50.0f);
    e.setTarget(P_TONE, variant & 4 ? -1.0f : 1.0f);
    e.setTarget(P_CHORD, 9.0f);       // 9th
    e.setTarget(P_VOICING, 1.0f);
    e.setTarget(P_OCTAVE, 1.0f);
    e.setTarget(P_DRV_ON, 1.0f);
    e.setTarget(P_DRV_AMT, 1.0f);
    e.setTarget(P_DRV_TYPE, (float)(variant % 3));
    e.setTarget(P_DRV_MIX, 1.0f);
    e.setTarget(P_CMP_ON, 1.0f);
    e.setTarget(P_CMP_MAKEUP, 24.0f);
    e.setTarget(P_CMP_THR, -40.0f);
    e.setTarget(P_CMP_RATIO, 20.0f);
    e.setTarget(P_DLY_ON, 1.0f);
    e.setTarget(P_DLY_FB, 0.98f);
    e.setTarget(P_DLY_MIX, 1.0f);
    e.setTarget(P_DLY_DIV, 0.0f);
    e.setTarget(P_DLY_TIME, variant & 1 ? 10.0f : 600.0f);
    e.setTarget(P_REV_ON, 1.0f);
    e.setTarget(P_REV_SIZE, 1.0f);    // infinite
    e.setTarget(P_REV_MIX, 1.0f);
    e.setTarget(P_REV_WIDTH, 1.5f);
    e.setTarget(P_REV_MOD, 1.0f);
    e.setTarget(P_FX_ORDER, (float)((variant * 7) % 24));
    for (int d = 0; d < 7; ++d) e.setTarget(P_DEG1 + d, 1.0f);   // full cluster
}

} // namespace

TEST_CASE("No NaN/Inf and peak <= 0 dBFS across all modes and extreme settings", "[engine][safety]")
{
    const int input = GENERATE(0, 1);
    const int arp = GENERATE(0, 1);
    for (int mode = 0; mode < GS_NUM_MODES; ++mode) {
        auto e = makeEngine(48000.0);
        setExtreme(*e, mode + input * 3);
        e->setTarget(P_MODE, (float)mode);
        e->setTarget(P_SRC, (float)input);
        e->setTarget(P_ARP_ON, (float)arp);
        e->setTarget(P_ARP_RATE, 5.0f);
        e->setTarget(P_ARP_OCT, 4.0f);
        e->pitchBend(16383);
        e->modWheel(1.0f);
        auto st = render(*e, 1.5, 512);
        INFO("mode " << mode << " input " << input << " arp " << arp);
        CHECK(st.finite);
        CHECK(st.peak <= 1.0f);
        CHECK(st.rms > 1e-4);
    }
}

TEST_CASE("Random parameter fuzz stays finite and below 0 dBFS", "[engine][safety]")
{
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> u(0.0f, 1.0f);
    auto e = makeEngine(44100.0);
    for (int round = 0; round < 40; ++round) {
        for (int id = 0; id < P_COUNT; ++id) {
            if (id == P_PLAY) continue;
            e->setTarget(id, gs_param_from_norm(id, u(rng)));
        }
        e->pitchBend((int)(u(rng) * 16383));
        e->modWheel(u(rng));
        if (round % 3 == 0) e->noteOn(36 + (int)(u(rng) * 60), u(rng));
        if (round % 5 == 0) e->allNotesOff();
        auto st = render(*e, 0.25, -1, [](int) {}, (unsigned)round);
        INFO("round " << round);
        REQUIRE(st.finite);
        REQUIRE(st.peak <= 1.0f);
    }
}

TEST_CASE("Every sample rate and odd block sizes", "[engine][rates]")
{
    const double sr = GENERATE(44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0);
    const int block = GENERATE(1, 17, 333, 4096, -1);
    for (int input = 0; input < 2; ++input) {
        auto e = makeEngine(sr);
        e->setTarget(P_SRC, (float)input);
        e->setTarget(P_IMG_SWEEP, 1.0f);      // cover the whole poster in the short render
        e->setTarget(P_ARP_ON, 1.0f);
        e->setTarget(P_DRV_ON, 1.0f);
        auto st = render(*e, block == 1 ? 0.35 : 0.8, block);
        INFO("sr " << sr << " block " << block << " input " << input);
        CHECK(st.finite);
        CHECK(st.peak <= 1.0f);
        CHECK(st.rms > 1e-4);
        CHECK(e->latencySamples() > 0);
        CHECK(e->latencySamples() == (int)(0.0015 * sr + 0.5));
    }
}

TEST_CASE("Block size does not change the sound (control rate is time based)", "[engine][rates]")
{
    // Same seed, same settings: rendering with 1-sample blocks vs 512-sample blocks must match
    // closely (the wrapper runs its control slices at a fixed 32-sample rate).
    auto a = makeEngine(48000.0);
    auto b = makeEngine(48000.0);
    for (auto* e : {a.get(), b.get()}) {
        e->setTarget(P_DLY_ON, 0.0f);
        e->setTarget(P_REV_ON, 0.0f);
    }
    std::vector<float> la(24000), ra(24000), lb(24000), rb(24000);
    for (int i = 0; i < 24000; i += 512) a->process(la.data() + i, ra.data() + i, std::min(512, 24000 - i));
    for (int i = 0; i < 24000; i += 32) b->process(lb.data() + i, rb.data() + i, std::min(32, 24000 - i));
    double diff = 0, ref = 0;
    for (size_t i = 0; i < la.size(); ++i) { diff += std::abs(la[i] - lb[i]); ref += std::abs(la[i]); }
    CHECK(ref > 1.0);
    CHECK(diff / ref < 1e-3);
}

TEST_CASE("Switching input or loading a shorter sample mid-play stays in bounds", "[engine][bounds]")
{
    const double sr = GENERATE(44100.0, 96000.0);
    auto e = makeEngine(sr);
    e->setTarget(P_TRIM_IN, 0.8f);
    e->setTarget(P_TRIM_OUT, 1.0f);
    e->setTarget(P_SLICES, 4.0f);   // 16 slices
    e->setTarget(P_SLICE_MODE, 1.0f);
    e->setTarget(P_SLICE_SEL, 15.0f);
    e->setTarget(P_ARP_ON, 1.0f);
    e->setTarget(P_ARP_START, 1.0f);
    e->setTarget(P_ARP_WALK, 0.5f);
    e->setTarget(P_MODE, (float)MODE_TUNNEL);
    e->setTarget(P_SIZE, 1000.0f);
    e->setPreview(true);
    e->setTarget(P_PREVIEW_LOOP, 1.0f);

    // a long sample first, so grains / arp voices sit near its end
    std::vector<float> longS(size_t(sr * 8.0));
    for (size_t i = 0; i < longS.size(); ++i) longS[i] = 0.5f * std::sin((float)i * 0.01f);
    e->setSourceNow(test::makeSource(longS, sr));

    int step = 0;
    auto st = render(*e, 3.0, 257, [&](int done) {
        int t = done / (int)(sr * 0.1);   // every 100 ms do something nasty
        if (t == step) return;
        step = t;
        switch (step % 5) {
        case 0: e->postSource(test::makeSource(std::vector<float>(size_t(sr * 0.1), 0.3f), sr)); break;
        case 1: e->setTarget(P_SRC, 1.0f); break;
        case 2: e->setTarget(P_SRC, 0.0f); break;
        case 3: e->postSource(test::makeSource(std::vector<float>(300, 0.3f), sr)); break;   // tiny
        case 4: e->postSource(test::makeSource(longS, sr)); e->postImage(test::posterGrid(step)); break;
        }
        e->collectGarbage();
    });
    CHECK(st.finite);
    CHECK(st.peak <= 1.0f);

    // an empty source must be harmless too
    e->postSource(test::makeSource({}, sr));
    auto st2 = render(*e, 0.3, 64);
    CHECK(st2.finite);
}

TEST_CASE("Image notes respond to intervals and the chord degree shifts the melody", "[engine][image]")
{
    auto e = makeEngine(48000.0);
    // a single bright horizontal band on black
    auto g = std::make_unique<madcap::ImageGrid>();
    g->cols = 300;   // a non-power-of-two picture size
    g->rows = 128;
    g->luma.assign((size_t)g->cols * (size_t)g->rows, 0.0f);
    for (int r = 60; r < 66; ++r)
        for (int c = 0; c < g->cols; ++c) g->luma[(size_t)r * (size_t)g->cols + (size_t)c] = 1.0f;
    e->setImageNow(std::move(g));
    e->setTarget(P_SRC, 1.0f);
    e->setTarget(P_IMG_READ, 0.0f);   // listen to light
    e->setTarget(P_IMG_MODE, 0.0f);   // spectral
    e->setTarget(P_IMG_GRID, 0.0f);   // smooth: held tones
    e->setTarget(P_IMG_VOICES, 1.0f);
    e->setTarget(P_GRAIN_ON, 0.0f);
    e->setTarget(P_VOICING, 0.0f);

    auto notesFor = [&](int chord, int degreeBit) {
        e->setTarget(P_CHORD, (float)chord);
        for (int d = 0; d < 7; ++d) e->setTarget(P_DEG1 + d, d == degreeBit ? 1.0f : 0.0f);
        render(*e, 0.1, 128);
        int out[IMG_MAX_NOTES];
        int n = gs_img_active_notes(&e->raw()->img, out, IMG_MAX_NOTES);
        std::vector<int> v(out, out + n);
        std::sort(v.begin(), v.end());
        return v;
    };

    auto unison = notesFor(0, 0);
    auto fifth = notesFor(4, 0);
    auto triad = notesFor(5, 0);
    CHECK(unison.size() == 1);
    CHECK(fifth.size() == 2);
    CHECK(triad.size() == 3);
    REQUIRE(!unison.empty());
    CHECK(fifth.front() == unison.front());
    CHECK(triad.front() == unison.front());

    // degree II moves the drawn note one step up the image scale
    auto shifted = notesFor(0, 1);
    REQUIRE(shifted.size() == 1);
    const auto& notes = e->raw()->img.notes;
    int idx = -1;
    for (int i = 0; i < e->raw()->img.nnext; ++i)
        if (notes[i] == unison.front()) idx = i;
    REQUIRE(idx >= 0);
    CHECK(shifted.front() == notes[idx + 1]);

    // and degree III two steps
    auto shifted2 = notesFor(0, 2);
    REQUIRE(shifted2.size() == 1);
    CHECK(shifted2.front() == notes[idx + 2]);
}

TEST_CASE("GRAINS off + ARP off + SAMPLE input = silence", "[engine]")
{
    auto e = makeEngine(48000.0);
    e->setTarget(P_GRAIN_ON, 0.0f);
    e->setTarget(P_ARP_ON, 0.0f);
    e->setTarget(P_SRC, 0.0f);
    e->setTarget(P_MASTER, 18.0f);
    auto st = render(*e, 1.0, 256);
    CHECK(st.finite);
    CHECK(st.peak < 1e-6f);
}

TEST_CASE("Latch off: silent until a note is played", "[engine][midi]")
{
    auto e = makeEngine(48000.0);
    e->setTarget(P_LATCH, 0.0f);
    e->setTarget(P_DLY_ON, 0.0f);
    e->setTarget(P_REV_ON, 0.0f);
    CHECK(render(*e, 0.5, 256).peak < 1e-6f);
    e->noteOn(62, 1.0f);          // D: degree II in C minor
    auto on = render(*e, 0.5, 256);
    CHECK(on.rms > 1e-3);
    CHECK(e->ui().midiMask.load() == 0b10);
    CHECK(e->ui().degreeMask.load() == 0b10);
    e->noteOff(62);
    render(*e, 2.0, 256);         // let grains and arp notes finish
    CHECK(render(*e, 0.5, 256).peak < 1e-6f);
}

TEST_CASE("Latch on keeps the last MIDI chord; pads take over again", "[engine][midi]")
{
    auto e = makeEngine(48000.0);
    e->noteOn(64, 0.8f);          // E -> degree III in C minor (Eb nearest -> III)
    e->noteOn(67, 0.8f);          // G -> degree V
    render(*e, 0.05, 64);
    CHECK(e->ui().degreeMask.load() == ((1 << 2) | (1 << 4)));
    e->allNotesOff();
    render(*e, 0.05, 64);
    CHECK(e->ui().degreeMask.load() == ((1 << 2) | (1 << 4)));   // latched
    CHECK(e->ui().gate.load());
    e->setTarget(P_DEG2, 1.0f);                                   // pad press
    render(*e, 0.05, 64);
    CHECK(e->ui().degreeMask.load() == 0b11);
}

TEST_CASE("MIDI ROOT mode makes the note the key, its octave sets OCTAVE", "[engine][midi]")
{
    auto e = makeEngine(48000.0);
    e->setTarget(P_MIDI_MODE, 1.0f);
    e->noteOn(45, 1.0f);   // A2
    render(*e, 0.05, 64);
    CHECK(e->ui().key.load() == 9);
    CHECK(e->ui().octave.load() == -2);
    CHECK(e->ui().degreeMask.load() == 1);
}

TEST_CASE("Transport STOP silences everything including FX tails", "[engine][transport]")
{
    auto e = makeEngine(48000.0);
    e->setTarget(P_REV_SIZE, 1.0f);
    e->setTarget(P_REV_MIX, 1.0f);
    CHECK(render(*e, 1.0, 512).rms > 1e-3);
    e->setPlaying(false);
    render(*e, 0.1, 512);   // fade
    CHECK(render(*e, 1.0, 512).peak < 1e-6f);
    e->setPlaying(true);
    CHECK(render(*e, 1.0, 512).rms > 1e-3);
}

TEST_CASE("Audition plays the cut dry while the transport is stopped", "[engine][preview]")
{
    auto e = makeEngine(48000.0);
    e->setPlaying(false);
    render(*e, 0.2, 512);
    e->setPreview(true);
    auto st = render(*e, 0.5, 512);
    CHECK(st.rms > 1e-3);
    CHECK(st.peak <= 1.0f);
    render(*e, 3.0, 512);     // one-shot reaches OUT and stops by itself
    CHECK_FALSE(e->isPreviewing());
}

TEST_CASE("Host sync locks the arpeggiator to the bar", "[engine][sync]")
{
    auto e = makeEngine(48000.0);
    e->setTarget(P_BPM, 120.0f);
    e->setTarget(P_ARP_ON, 1.0f);
    e->setTarget(P_ARP_RATE, 3.0f);  // 1/16
    e->setTarget(P_GRAIN_ON, 0.0f);
    const double spb = 48000.0 * 60.0 / 120.0;
    double ppq = 3.3;                // start mid-bar
    std::vector<float> l(64), r(64);
    std::set<int> stepsAtBar;
    int lastTrig = e->ui().arpTrig.load();
    for (int b = 0; b < 3000; ++b) {
        madcap::HostInfo h;
        h.playing = true;
        h.ppq = ppq;
        h.barStartPpq = std::floor(ppq / 4.0) * 4.0;
        e->setHost(h);
        e->process(l.data(), r.data(), 64);
        int trig = e->ui().arpTrig.load();
        if (trig != lastTrig) {
            // a step fired inside this block: its lane index must match the host position
            double inBar = ppq + 64.0 / spb - h.barStartPpq;
            int expected = (int)std::floor(inBar / 0.25 + 1e-6) % 16;
            int got = e->ui().arpStep.load();
            int d = std::abs(got - expected);
            d = std::min(d, 16 - d);
            CHECK(d <= 1);
            lastTrig = trig;
        }
        ppq += 64.0 / spb;
    }
}

TEST_CASE("Image grid follows the picture's resolution and aspect, any step count works", "[engine][image]")
{
    int c = 0, r = 0;
    madcap::lumaGridSize(2000, 500, c, r, 512);   // wide panorama: aspect kept, capped
    CHECK(c == 512);
    CHECK(r == 128);
    madcap::lumaGridSize(100, 400, c, r, 512);    // tall picture
    CHECK(c == 64);
    CHECK(r == 256);
    madcap::lumaGridSize(300, 120, c, r, 512);    // small picture: its own resolution
    CHECK(c == 300);
    CHECK(r == 120);
    madcap::lumaGridSize(2048, 1024, c, r, 2048); // GRID RES 2048
    CHECK(c == 2048);
    CHECK(r == 1024);
    madcap::lumaGridSize(2048, 1024, c, r, 64);   // GRID RES 64
    CHECK(c == 64);
    CHECK(r == 32);
    CHECK(madcap::gridResolutionColumns(0) == 64);
    CHECK(madcap::gridResolutionColumns(5) == 2048);
    madcap::lumaGridSize(4, 4, c, r);        // tiny: clamped to the minimum
    CHECK(c == IMG_COLS_MIN);
    CHECK(r == IMG_ROWS_MIN);

    auto e = makeEngine(48000.0);
    auto g = std::make_unique<madcap::ImageGrid>();
    g->cols = IMG_COLS_MIN;
    g->rows = IMG_ROWS_MIN;
    g->luma.assign((size_t)g->cols * (size_t)g->rows, 0.0f);
    for (size_t i = 0; i < g->luma.size(); i += 3) g->luma[i] = 1.0f;
    e->setImageNow(std::move(g));
    e->setTarget(P_SRC, 1.0f);
    e->setTarget(P_IMG_SWEEP, 1.0f);
    for (int steps : {0, 1, 7, 33, 100, 128}) {
        e->setTarget(P_IMG_GRID, (float)steps);
        for (int mode = 0; mode < 3; ++mode) {
            e->setTarget(P_IMG_MODE, (float)mode);
            auto st = render(*e, 0.4, 97);
            INFO("steps " << steps << " mode " << mode);
            CHECK(st.finite);
            CHECK(st.peak <= 1.0f);
        }
    }
}

TEST_CASE("A 2048 x 1024 grid sweeps in real time and stays in bounds", "[engine][image]")
{
    auto e = makeEngine(48000.0);
    auto g = std::make_unique<madcap::ImageGrid>();
    g->cols = IMG_COLS_MAX;
    g->rows = IMG_ROWS_MAX;
    g->luma.resize((size_t)g->cols * (size_t)g->rows);
    for (size_t i = 0; i < g->luma.size(); ++i) g->luma[i] = (float)((i * 2654435761u) % 1000) / 1000.0f;
    e->setImageNow(std::move(g));
    e->setTarget(P_SRC, 1.0f);
    e->setTarget(P_IMG_SWEEP, 1.0f);
    for (int steps : {0, 4, 64}) {
        e->setTarget(P_IMG_GRID, (float)steps);
        for (int mode = 0; mode < 3; ++mode) {
            e->setTarget(P_IMG_MODE, (float)mode);
            const auto t0 = std::chrono::steady_clock::now();
            auto st = render(*e, 1.0, 256);
            const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            INFO("steps " << steps << " mode " << mode << " took " << secs << " s for 1 s of audio");
            CHECK(st.finite);
            CHECK(st.peak <= 1.0f);
            CHECK(secs < 0.5);   // comfortably faster than real time
        }
    }
}
