#include "Parameters.h"

namespace madcap::params {

juce::String format(int p, float value)
{
    char buf[64];
    gs_param_format(p, value, buf, (int)sizeof buf);
    return juce::String(buf);
}

namespace {

bool isOnOff(const GsParamDesc& d)
{
    return d.fmt == FMT_ENUM && d.labels != nullptr && d.min == 0.0f && d.max == 1.0f
        && juce::String(d.labels[0]) == "Off" && juce::String(d.labels[1]) == "On";
}

std::unique_ptr<juce::RangedAudioParameter> makeParameter(int p)
{
    const GsParamDesc& d = GS_PARAMS[p];
    const juce::ParameterID pid { d.id, 1 };
    const juce::String name (d.name);
    auto toText = [p](float v, int) { return format(p, v); };
    auto fromText = [p](const juce::String& t) { return gs_param_parse(p, t.toRawUTF8()); };

    if (isOnOff(d)) {
        return std::make_unique<juce::AudioParameterBool>(pid, name, d.def > 0.5f,
            juce::AudioParameterBoolAttributes().withCategory(juce::AudioProcessorParameter::genericParameter));
    }
    if (d.fmt == FMT_ENUM) {
        juce::StringArray labels;
        const int n = gs_param_num_steps(p);
        for (int i = 0; i < n; ++i) labels.add(d.labels[i]);
        return std::make_unique<juce::AudioParameterChoice>(pid, name, labels, juce::roundToInt(d.def - d.min));
    }
    if (d.discrete) {
        return std::make_unique<juce::AudioParameterInt>(pid, name, (int)d.min, (int)d.max, juce::roundToInt(d.def),
            juce::AudioParameterIntAttributes()
                .withStringFromValueFunction([p](int v, int) { return format(p, (float)v); })
                .withValueFromStringFunction([p](const juce::String& t) { return juce::roundToInt(gs_param_parse(p, t.toRawUTF8())); }));
    }
    // continuous: value = min + (max - min) * norm^curve  ==  JUCE skew 1 / curve
    juce::NormalisableRange<float> range (d.min, d.max, 0.0f, 1.0f / d.curve);
    juce::String unit;
    switch (d.fmt) {
        case FMT_PCT: unit = "%"; break;
        case FMT_MS: unit = "ms"; break;
        case FMT_DB: unit = "dB"; break;
        case FMT_HZ: unit = "Hz"; break;
        case FMT_CENTS: unit = "ct"; break;
        case FMT_SEC: unit = "s"; break;
        default: break;
    }
    return std::make_unique<juce::AudioParameterFloat>(pid, name, range, d.def,
        juce::AudioParameterFloatAttributes()
            .withLabel(unit)
            .withStringFromValueFunction(toText)
            .withValueFromStringFunction(fromText));
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    std::array<std::unique_ptr<juce::AudioProcessorParameterGroup>, PAGE_COUNT> groups;
    for (int g = 0; g < PAGE_COUNT; ++g)
        groups[(size_t)g] = std::make_unique<juce::AudioProcessorParameterGroup>(
            juce::String(pageName(g)).toLowerCase(), pageName(g), " | ");
    for (int p = 0; p < P_COUNT; ++p)
        groups[(size_t)GS_PARAMS[p].page]->addChild(makeParameter(p));
    for (auto& g : groups)
        layout.add(std::move(g));
    return layout;
}

} // namespace madcap::params
