#include "PluginProcessor.h"
#include "PluginEditor.h"

PtV2AProcessor::PtV2AProcessor()
: AudioProcessor (BusesProperties()
                    .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{}

void PtV2AProcessor::prepareToPlay (double /*sampleRate*/, int /*samplesPerBlock*/) {}
void PtV2AProcessor::releaseResources() {}

bool PtV2AProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Require same channel count on in/out and allow mono or stereo
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void PtV2AProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midi*/)
{
    // Pass-through: do not modify audio
    juce::ignoreUnused (buffer);
}

juce::AudioProcessorEditor* PtV2AProcessor::createEditor()
{
    return new PtV2AEditor (*this);
}

void PtV2AProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // minimal: store nothing yet
    juce::MemoryOutputStream (destData, true).writeString ("{}");
}

void PtV2AProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PtV2AProcessor();
}
