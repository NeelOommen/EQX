/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
EQXAudioProcessor::EQXAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
}

EQXAudioProcessor::~EQXAudioProcessor()
{
}

//==============================================================================
const juce::String EQXAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EQXAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EQXAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EQXAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EQXAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EQXAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EQXAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EQXAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String EQXAudioProcessor::getProgramName (int index)
{
    return {};
}

void EQXAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void EQXAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..

    juce::dsp::ProcessSpec spec;

    spec.maximumBlockSize = samplesPerBlock;

    spec.numChannels = 1;

    spec.sampleRate = sampleRate;

    leftChain.prepare(spec);
    rightChain.prepare(spec);

    auto chainSettings = getChainSettings(apvts);

    auto peakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate, 
        chainSettings.peakFreq, 
        chainSettings.peakQuality, 
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels)
    );

    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoeff;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoeff;
}

void EQXAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EQXAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void EQXAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());


    //update params
    auto chainSettings = getChainSettings(apvts);

    auto peakCoeff = juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        getSampleRate(),
        chainSettings.peakFreq,
        chainSettings.peakQuality,
        juce::Decibels::decibelsToGain(chainSettings.peakGainInDecibels)
    );

    *leftChain.get<ChainPositions::Peak>().coefficients = *peakCoeff;
    *rightChain.get<ChainPositions::Peak>().coefficients = *peakCoeff;

    juce::dsp::AudioBlock<float> block(buffer);

    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = block.getSingleChannelBlock(1);

    juce::dsp::ProcessContextReplacing<float> leftContext(leftBlock);
    juce::dsp::ProcessContextReplacing<float> rightContext(rightBlock);

    leftChain.process(leftContext);
    rightChain.process(rightContext);
}

//==============================================================================
bool EQXAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EQXAudioProcessor::createEditor()
{
    //return new EQXAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor(*this);
}

//==============================================================================
void EQXAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void EQXAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

ChainSettings getChainSettings(juce::AudioProcessorValueTreeState& apvts) {
    ChainSettings settings;
        
    settings.lowCutFreq = apvts.getRawParameterValue("Low Cut Frequency")->load();
    settings.highCutFreq = apvts.getRawParameterValue("High Cut Frequency")->load();
    settings.peakFreq = apvts.getRawParameterValue("Peak Cut Frequency")->load();
    settings.peakGainInDecibels = apvts.getRawParameterValue("Peak Gain")->load();
    settings.peakQuality = apvts.getRawParameterValue("Peak Quality")->load();
    settings.lowCutSlope = apvts.getRawParameterValue("Low Cut Slope")->load();
    settings.highCutSlope = apvts.getRawParameterValue("High Cut Slope")->load();

    return settings;
}


juce::AudioProcessorValueTreeState::ParameterLayout EQXAudioProcessor::createLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Low Cut Frequency",
        "Low Cut Frequency",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f),
        20.f
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "High Cut Frequency",
        "High Cut Frequency",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f),
        20000.f
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Cut Frequency",
        "Peak Cut Frequency",
        juce::NormalisableRange<float>(20.f, 20000.f, 1.f, 1.f),
        750.f
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Gain",
        "Peak Gain",
        juce::NormalisableRange<float>(-24.f, 24.f, 0.5f, 1.f),
        0.0f
        )
    );

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Peak Quality",
        "Peak Quality",
        juce::NormalisableRange<float>(0.1f, 10.f, 0.05f, 1.f),
        1.f
        )
    );

    juce::StringArray options;

    for (int i = 0; i < 4; i++) {
        juce::String str;
        str << (12 + (i*12));
        str << " dB/Oct";
        options.add(str);
    }

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "Low Cut Slope",
        "Low Cut Slope",
        options,
        0
        )
    );

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "High Cut Slope",
        "High Cut Slope",
        options,
        0
        )
    );

    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQXAudioProcessor();
}
