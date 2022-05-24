/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "CircularBuffer.h"
#include "FFTBuffer.h"

struct Polar {
    float amplitude;
    float phase;
};

enum {
    FFT_ORDER = 12,
    FFT_SIZE = 1 << FFT_ORDER,
    WINDOW_SIZE = FFT_SIZE,
    NUM_CHANNELS = 2,
};

//==============================================================================
/**
*/
class PluginProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    PluginProcessor();
    ~PluginProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
   #endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool isAudioBufferReady(uint32_t channel);
    void copyAudioBuffer(std::vector<float>& destination, uint32_t channel);

    bool isSpectrumReady();
    void copySpectrum(std::vector<std::vector<Polar>>& destination);

private:
    std::vector<CircularBuffer<float>> m_circularAudioBuffers;
    FFTBuffer m_fftBuffer;

    std::atomic<float>* p_bands = nullptr;
    std::atomic<float>* p_position = nullptr;
    std::atomic<float>* p_width = nullptr;
    std::atomic<float>* p_offset = nullptr;
    std::atomic<float>* p_bias = nullptr;
    std::atomic<float>* p_makeup = nullptr;

    std::atomic<bool> m_isSpectrumReady = false;
    std::vector<std::vector<float>> m_prevAudioBuffer;
    std::vector<std::vector<Polar>> m_prevSpectrum;

    std::mutex m_readWriteAudioBufferLock;
    std::mutex m_readWriteSpectrumLock;

    void processFFT(std::complex<float>* fftData, unsigned int channel);

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)

    juce::AudioProcessorValueTreeState m_params;
};
