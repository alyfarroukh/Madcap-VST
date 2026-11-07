#include <catch2/catch_test_macros.hpp>
#include <cstddef>

#include <cmath>
#include <set>
#include <string>

extern "C" {
#include "fx.h"
#include "params.h"
}

TEST_CASE("Every parameter has a unique stable ID, a name and a sane range", "[params]")
{
    std::set<std::string> ids;
    for (int i = 0; i < P_COUNT; ++i) {
        const GsParamDesc& d = GS_PARAMS[i];
        INFO("param " << i);
        REQUIRE(d.id != nullptr);
        REQUIRE(d.name != nullptr);
        CHECK(ids.insert(d.id).second);
        CHECK(d.max > d.min);
        CHECK(d.def >= d.min);
        CHECK(d.def <= d.max);
        CHECK(d.curve > 0.0f);
        CHECK(GS_PARAM_SHORT[i] != nullptr);
        if (d.fmt == FMT_ENUM) CHECK(d.labels != nullptr);
        CHECK(gs_param_find(d.id) == i);
    }
}

TEST_CASE("Normalised mapping round-trips", "[params]")
{
    for (int i = 0; i < P_COUNT; ++i) {
        const GsParamDesc& d = GS_PARAMS[i];
        for (int k = 0; k <= 20; ++k) {
            float n = k / 20.0f;
            float v = gs_param_from_norm(i, n);
            CHECK(v >= d.min);
            CHECK(v <= d.max);
            if (!d.discrete) CHECK(std::abs(gs_param_to_norm(i, v) - n) < 1e-3f);
        }
        CHECK(gs_param_from_norm(i, NAN) == gs_param_from_norm(i, 0.0f));
    }
}

TEST_CASE("Text formatting parses back", "[params]")
{
    char buf[64];
    for (int i = 0; i < P_COUNT; ++i) {
        const GsParamDesc& d = GS_PARAMS[i];
        if (i == P_ARP_PATTERN) continue;
        for (int k = 0; k <= 10; ++k) {
            float v = gs_param_from_norm(i, k / 10.0f);
            gs_param_format(i, v, buf, sizeof buf);
            float back = gs_param_parse(i, buf);
            INFO(d.id << " value " << v << " text '" << buf << "' -> " << back);
            float tol = d.discrete ? 0.0f : (d.max - d.min) * 0.02f + 1e-3f;
            if (i == P_REV_SIZE) {
                if (v >= GS_REV_INFINITE) CHECK(back >= GS_REV_INFINITE);
                else CHECK(std::abs(gs_reverb_rt60(back) - gs_reverb_rt60(v)) < 0.06f * gs_reverb_rt60(v) + 0.05f);
            } else {
                CHECK(std::abs(back - v) <= tol);
            }
        }
    }
}

TEST_CASE("FX order permutations", "[params]")
{
    std::set<std::string> seen;
    for (int i = 0; i < 24; ++i) {
        CHECK(gs_fx_order_index(GS_FX_ORDERS[i]) == i);
        seen.insert(GS_FX_ORDER_LABELS[i]);
    }
    CHECK(seen.size() == 24);
}
