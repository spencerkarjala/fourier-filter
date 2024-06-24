#include <cmath>

#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginProcessor::PluginProcessor()
  : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true))
  , m_circularAudioBuffers(NUM_CHANNELS)
  , m_fftBuffer(NUM_CHANNELS,
                2 * FFT_SIZE,
                FFT_ORDER,
                WINDOW_SIZE,
                2,
                [this](std::complex<float>* fftData, unsigned int channel) { this->processFFT(fftData, channel); })
  , m_prevAudioBuffer(NUM_CHANNELS)
  , m_prevSpectrum(NUM_CHANNELS)
  , m_isSpectrumReady(false)
  , m_params(*this,
             nullptr,
             juce::Identifier("fourier-filter"),
             { std::make_unique<juce::AudioParameterFloat>("bands", "Bands", 0, 1.f, 0.5f),
               std::make_unique<juce::AudioParameterFloat>("position", "Position", 0.f, 1.f, 0.5f),
               std::make_unique<juce::AudioParameterFloat>("width", "Width", 0.f, 1.f, 0.5f),
               std::make_unique<juce::AudioParameterFloat>("offset", "Offset", 0.f, 1.f, 0.f),
               std::make_unique<juce::AudioParameterFloat>("bias", "Bias", -1.f, 1.f, 0.f),
               std::make_unique<juce::AudioParameterFloat>("makeup", "Makeup", 0.f, 1.f, 0.f) })
{
    this->setLatencySamples(FFT_SIZE);

    p_bands = m_params.getRawParameterValue("bands");
    p_position = m_params.getRawParameterValue("position");
    p_width = m_params.getRawParameterValue("width");
    p_offset = m_params.getRawParameterValue("offset");
    p_bias = m_params.getRawParameterValue("bias");
    p_makeup = m_params.getRawParameterValue("makeup");

    for (auto& circularAudioBuffer : m_circularAudioBuffers) {
        circularAudioBuffer.resize(1024);
    }

    for (auto& channelAudioBuffer : m_prevAudioBuffer) {
        channelAudioBuffer.resize(FFT_SIZE);
    }

    for (auto& channelSpectrum : m_prevSpectrum) {
        channelSpectrum.resize(FFT_SIZE);
    }
}

PluginProcessor::~PluginProcessor() {}

const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PluginProcessor::acceptsMidi() const
{
    return false;
}

bool PluginProcessor::producesMidi() const
{
    return false;
}

bool PluginProcessor::isMidiEffect() const
{
    return false;
}

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1;
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram(int index) {}

const juce::String PluginProcessor::getProgramName(int index)
{
    return {};
}

void PluginProcessor::changeProgramName(int index, const juce::String& newName) {}

void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {}

// called when playback stops
void PluginProcessor::releaseResources()
{
    m_fftBuffer.clear();
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) {
        return false;
    }

    return true;
}

const uint32_t PARAM_MAX = 1024;

void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessage)
{
    juce::ScopedNoDenormals noDenormals;
    const auto totalNumInputChannels = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();
    const auto numSamples = buffer.getNumSamples();

    // clear input channels that have no corresponding output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, numSamples);
    }

    for (int channel = 0; channel < 2; ++channel) {
        auto* channelData = buffer.getWritePointer(channel);

        for (int sampleIndex = 0; sampleIndex < numSamples; ++sampleIndex) {
            float outputValue = m_fftBuffer.readResult(channel);
            m_fftBuffer.write(channel, channelData[sampleIndex]);

            channelData[sampleIndex] = outputValue;
            m_circularAudioBuffers[channel].write(outputValue);
        }

        m_readWriteAudioBufferLock.lock();
        memcpy(&m_prevAudioBuffer[channel][0], channelData, m_prevAudioBuffer[channel].size() * sizeof(float));
        m_readWriteAudioBufferLock.unlock();
    }
}

#define PI 3.1415926535
#define TWO_PI 2.0 * PI

void PluginProcessor::processFFT(std::complex<float>* fftData, unsigned int channel)
{
    const double bands = static_cast<double>((*p_bands).load());
    const double position = static_cast<double>((*p_position).load());
    const double width = static_cast<double>((*p_width).load());
    const double offset = static_cast<double>((*p_offset).load());
    const double bias = static_cast<double>((*p_bias).load());
    const double makeup = static_cast<double>((*p_makeup).load());

    // scale from [0.0, 1.0] to some custom values
    // all of these numbers were just set to what sounds good
    double bandsScaled = bands * 3.0;
    double positionScaled = -position;
    double widthScaled = pow(width * 5.0, 3.0);

    // used to have separate phases for left and right channel
    double phaseOffset = (2.0 * TWO_PI * channel - TWO_PI) * offset;

    // used as a log-scaled makeup gain for individual bands
    double makeupScaled = (std::pow(100.0, makeup) - 1) / (100.0 - 1);

    // used to bias frequencies low or high; conditional to avoid division by zero
    double biasScaled = bias == 0.0 ? 1.001 : std::pow(10, bias);

    for (int i = 0; i <= WINDOW_SIZE / 2; ++i) {
        double indexScaled = static_cast<double>(i) / (static_cast<double>(WINDOW_SIZE) / 2.0);
        double indexBias = (std::pow(biasScaled, 5.0 * indexScaled) - 1.0) / (std::pow(biasScaled, 5.0) - 1.0);
        double indexLog = log(static_cast<float>(i) * 12.0 * indexBias + 1.0) * bandsScaled + positionScaled;

        double binScaleModulated = 0.5 * cos(TWO_PI * indexLog + phaseOffset) + 0.5;

        double binScaleScaled = pow(binScaleModulated, widthScaled) * (1.0 + makeupScaled);
        double binScaleClipped = juce::jlimit(0.0, 1.0, binScaleScaled * (1.0 + makeupScaled));

        float scale = static_cast<float>(binScaleClipped) * (1.f + 3.f * *p_width);

        std::complex<float>& fftBin = fftData[i];
        std::complex<float>& fftBinMirrored = fftData[static_cast<int>(WINDOW_SIZE) - i];

        fftBin *= scale;
        fftBinMirrored *= scale;

        m_readWriteSpectrumLock.lock();
        m_prevSpectrum[channel][i] = { std::abs(fftBin), std::arg(fftBin) };
        m_readWriteSpectrumLock.unlock();
    }

    m_isSpectrumReady.store(true);
}

bool PluginProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginProcessorEditor(*this, m_params);
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = m_params.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(this->getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr && xmlState->hasTagName(m_params.state.getType())) {
        m_params.replaceState(juce::ValueTree::fromXml(*xmlState));
    }
}

bool PluginProcessor::isAudioBufferReady(uint32_t channel)
{
    return m_circularAudioBuffers[channel].isFilled();
}

void PluginProcessor::copyAudioBuffer(std::vector<float>& destination, uint32_t channel)
{
    m_circularAudioBuffers[channel].copyTo(destination);
}

bool PluginProcessor::isSpectrumReady()
{
    return m_isSpectrumReady.load();
}

void PluginProcessor::copySpectrum(std::vector<std::vector<Polar>>& destination)
{
    jassert(destination.size() == m_prevSpectrum.size());
    jassert(destination[0].size() == m_prevSpectrum[0].size());

    for (int channel = 0; channel < NUM_CHANNELS; ++channel) {
        m_readWriteSpectrumLock.lock();
        memcpy(&destination[channel][0], &m_prevSpectrum[channel][0], m_prevSpectrum[channel].size() * sizeof(Polar));
        m_readWriteSpectrumLock.unlock();
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
