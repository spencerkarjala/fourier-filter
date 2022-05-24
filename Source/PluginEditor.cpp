/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <numeric>

//==============================================================================
PluginProcessorEditor::PluginProcessorEditor (PluginProcessor& p, juce::AudioProcessorValueTreeState& vts)
    :   AudioProcessorEditor(&p),
        pluginProcessor(p),
        m_audioBuffers(NUM_CHANNELS),
        m_spectra(NUM_CHANNELS),
        m_valueTreeState(vts)
{
    for (auto& audioBuffer : m_audioBuffers) {
        audioBuffer.clear();
        audioBuffer.resize(1024);
    }

    for (auto& spectrum : m_spectra) {
        spectrum.resize(FFT_SIZE);

        for (int i_bin = 0; i_bin < spectrum.size(); ++i_bin) {
            spectrum[i_bin].amplitude = 0.f;
            spectrum[i_bin].phase = 0.f;
        }
    }

    setSize(400, 300);

    m_dialBands.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    m_dialBands.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 0);
    m_dialBandsAttachment.reset(new SliderAttachment(m_valueTreeState, "bands", m_dialBands));
    this->addAndMakeVisible(m_dialBands);

    m_dialPosition.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    m_dialPosition.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 0);
    m_dialPositionAttachment.reset(new SliderAttachment(m_valueTreeState, "position", m_dialPosition));
    this->addAndMakeVisible(m_dialPosition);

    m_dialWidth.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    m_dialWidth.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 0);
    m_dialWidthAttachment.reset(new SliderAttachment(m_valueTreeState, "width", m_dialWidth));
    this->addAndMakeVisible(m_dialWidth);

    m_dialOffset.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    m_dialOffset.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 0);
    m_dialOffsetAttachment.reset(new SliderAttachment(m_valueTreeState, "offset", m_dialOffset));
    this->addAndMakeVisible(m_dialOffset);

    m_dialBias.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    m_dialBias.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 0);
    m_dialBiasAttachment.reset(new SliderAttachment(m_valueTreeState, "bias", m_dialBias));
    this->addAndMakeVisible(m_dialBias);

    m_dialMakeup.setSliderStyle(juce::Slider::SliderStyle::RotaryVerticalDrag);
    m_dialMakeup.setTextBoxStyle(juce::Slider::TextBoxBelow, true, 100, 0);
    m_dialMakeupAttachment.reset(new SliderAttachment(m_valueTreeState, "makeup", m_dialMakeup));
    this->addAndMakeVisible(m_dialMakeup);

    m_labelBands.setText("Bands", juce::dontSendNotification);
    m_labelBands.attachToComponent(&m_dialBands, false);
    m_labelBands.setColour(juce::Label::textColourId, juce::Colours::black);
    m_labelBands.setJustificationType(juce::Justification::centredBottom);
    this->addAndMakeVisible(m_labelBands);

    m_labelPosition.setText("Position", juce::dontSendNotification);
    m_labelPosition.attachToComponent(&m_dialPosition, false);
    m_labelPosition.setColour(juce::Label::textColourId, juce::Colours::black);
    m_labelPosition.setJustificationType(juce::Justification::centredBottom);
    this->addAndMakeVisible(m_labelPosition);

    m_labelWidth.setText("Width", juce::dontSendNotification);
    m_labelWidth.attachToComponent(&m_dialWidth, false);
    m_labelWidth.setColour(juce::Label::textColourId, juce::Colours::black);
    m_labelWidth.setJustificationType(juce::Justification::centredBottom);
    this->addAndMakeVisible(m_labelWidth);

    m_labelOffset.setText("Offset", juce::dontSendNotification);
    m_labelOffset.attachToComponent(&m_dialOffset, false);
    m_labelOffset.setColour(juce::Label::textColourId, juce::Colours::black);
    m_labelOffset.setJustificationType(juce::Justification::centredBottom);
    this->addAndMakeVisible(m_labelOffset);

    m_labelBias.setText("Bias", juce::dontSendNotification);
    m_labelBias.attachToComponent(&m_dialBias, false);
    m_labelBias.setColour(juce::Label::textColourId, juce::Colours::black);
    m_labelBias.setJustificationType(juce::Justification::centredBottom);
    this->addAndMakeVisible(m_labelBias);

    m_labelMakeup.setText("Makeup", juce::dontSendNotification);
    m_labelMakeup.attachToComponent(&m_dialMakeup, false);
    m_labelMakeup.setColour(juce::Label::textColourId, juce::Colours::black);
    m_labelMakeup.setJustificationType(juce::Justification::centredBottom);
    this->addAndMakeVisible(m_labelMakeup);

    this->startTimerHz(60);
}

PluginProcessorEditor::~PluginProcessorEditor() {
    this->stopTimer();
}

constexpr float EPSILON = std::numeric_limits<float>::epsilon();

float getAmplitudeInDbScaled(float ampl, float min, float max) {
    float resultInDb = 10.f * std::log(ampl + EPSILON);
    float resultInDbScaled = juce::jlimit(0.f, max - min, resultInDb - min) / (max - min);

    return resultInDbScaled;
}

typedef std::vector<std::pair<float, float>> PairVector;

//==============================================================================
void PluginProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(0xfff6efe4));

    uint32_t width = this->getWidth();
    uint32_t height = this->getHeight();

    uint32_t spectrumPadding = 10;
    uint32_t spectrumHeight = height / 2;
    uint32_t spectrumWidth = width;

    juce::Path spectrumPath;

    auto& spectrum = m_spectra[0];

    const float DB_20 = (1/10.f) * 20.f;
    const float SAMPLE_RATE = static_cast<float>(pluginProcessor.getSampleRate());
    const float FREQ_PER_BIN = SAMPLE_RATE / (static_cast<float>(FFT_SIZE) / 2.f);

    float pathBaseVertical = static_cast<float>(height - spectrumPadding);
    float pathBaseHorizontal = static_cast<float>(spectrumPadding);
    float pathHeight = static_cast<float>(spectrumHeight - 2.0 * spectrumPadding);
    float binWidth = 2.f * (static_cast<float>(spectrumWidth) - 2.f * pathBaseHorizontal) / static_cast<float>(spectrum.size());

    spectrumWidth = this->getWidth() - 2 * spectrumPadding;

    PairVector amplLeft(spectrum.size());
    PairVector amplRight(spectrum.size());
    PairVector amplCombined(spectrum.size());

    float incr = SAMPLE_RATE / (2.f * static_cast<float>(spectrum.size()));
    float minFreq = 20.f;
    
    for (uint32_t bin = 1; bin < spectrum.size(); ++bin) {
        float binFreq = bin * incr;
        float logBinFreq = std::log10f(binFreq - minFreq);
        float logNyquist = std::log10f(SAMPLE_RATE / 2.f - minFreq);

        float logIndex = logBinFreq / logNyquist;
        float logIndexScaled = logIndex * (spectrum.size() - 1);

        uint32_t logScaleIndex = std::lround(logIndexScaled);

        float lvalue = getAmplitudeInDbScaled(m_spectra[0][bin].amplitude, 0.f, 80.f);
        float rvalue = getAmplitudeInDbScaled(m_spectra[1][bin].amplitude, 0.f, 80.f);

        float horizontalPosition = static_cast<float>(spectrumPadding) + logIndex * static_cast<float>(spectrumWidth);

        if (lvalue > rvalue) {
            amplCombined[bin] = { horizontalPosition, rvalue };
        }
        else {
            amplCombined[bin] = { horizontalPosition, lvalue };
        }

        amplLeft[bin] = { horizontalPosition, lvalue };
        amplRight[bin] = { horizontalPosition, rvalue };
    }

    const std::vector<std::pair<PairVector&, juce::Colour>> amplitudeColorPairs = {
        { amplLeft, juce::Colour(0xff444372) },
        { amplRight, juce::Colour(0xff744342) },
        { amplCombined, juce::Colour(0xaa000000) }
    };

    juce::Point<float> prevPoint, currPoint, nextPoint;
    
    for (auto& amplitudeColorPair : amplitudeColorPairs) {
        auto& ampl = amplitudeColorPair.first;
        const auto& color = amplitudeColorPair.second;

        PairVector amplAveraged(ampl.size());

        const uint32_t averageWidth = 9;
        const uint32_t halfAvgWidth = (averageWidth - 1) / 2;

        //for (uint32_t index = 0; index < halfAvgWidth; ++index) {
            //amplAveraged[index] = ampl[index];
            //amplAveraged[amplAveraged.size() - 1 - index] = ampl[ampl.size() - 1 - index];
        //}

        for (uint32_t index = halfAvgWidth; index < amplAveraged.size() - halfAvgWidth; ++index) {
            float total = 0.f;

            for (uint32_t avgIndex = 0; avgIndex < averageWidth; ++avgIndex) {
                total += ampl[index - halfAvgWidth + avgIndex].second;
            }

            amplAveraged[index] = { ampl[index].first, total / static_cast<float>(averageWidth) };
        }

        for (int i = 1; i < ampl.size(); ++i) {
            //amplAveraged[i] = ampl[i];
            //amplAveraged[i] = amplAveraged[i - 1] + 0.6 * (ampl[i] - amplAveraged[i - 1]);
        }

        float prevAmpl = amplAveraged[0].second;
        float currAmpl = amplAveraged[1].second;

        prevPoint = { pathBaseHorizontal - 1, pathBaseVertical };
        currPoint = { pathBaseHorizontal, pathBaseVertical - pathHeight * prevAmpl };

        //prevPoint = { pathBaseHorizontal, pathBaseVertical - pathHeight * prevAmpl };
        //currPoint = { pathBaseHorizontal + binWidth, pathBaseVertical - pathHeight * currAmpl };

        spectrumPath.clear();
        spectrumPath.startNewSubPath(prevPoint);

        for (uint32_t index = 1; index < ampl.size(); ++index) {
            float horizontalPosition = amplAveraged[index].first;
            float nextAmpl = amplAveraged[index].second;

            nextPoint = { horizontalPosition, pathBaseVertical - pathHeight * nextAmpl };

            juce::Point<float> prevPointCtrl = currPoint + (prevPoint - currPoint) * 0.5f;
            juce::Point<float> nextPointCtrl = currPoint + (nextPoint - currPoint) * 0.5f;

            spectrumPath.cubicTo(prevPointCtrl, currPoint, nextPointCtrl);

            prevPoint = currPoint;
            currPoint = nextPoint;
        }

        float xBottomRight = static_cast<float>(spectrumWidth) + static_cast<float>(spectrumPadding);
        float xBottomLeft = static_cast<float>(spectrumPadding);
        float yBottom = static_cast<float>(this->getHeight()) - static_cast<float>(spectrumPadding);

        spectrumPath.lineTo(xBottomRight, yBottom);
        spectrumPath.lineTo(xBottomLeft, yBottom);

        g.setColour(color);
        g.fillPath(spectrumPath);

        // draws over the sides of the spectrum component to clip its boundaries
        // todo: find a better solution for this
        g.setColour(juce::Colour(0xfff6efe4));
        g.fillRect(juce::Rectangle<int>(0, this->getHeight() - spectrumPadding - spectrumHeight, spectrumPadding, spectrumHeight));
        g.fillRect(juce::Rectangle<int>(0, this->getHeight() - spectrumPadding, static_cast<uint32_t>(this->getWidth()), spectrumPadding));
        g.fillRect(juce::Rectangle<int>(static_cast<uint32_t>(this->getWidth()) - spectrumPadding, static_cast<uint32_t>(this->getHeight()) - spectrumPadding - spectrumHeight, spectrumPadding, spectrumHeight));
    }
}

void PluginProcessorEditor::resized() {
    const int width = this->getWidth();

    m_dialBands.setBounds(width / 7 - 35, 20, 70, 70);
    m_dialPosition.setBounds(2*width / 7 - 35, 20, 70, 70);
    m_dialWidth.setBounds(3*width / 7 - 35, 20, 70, 70);
    m_dialOffset.setBounds(4 * width / 7 - 35, 20, 70, 70);
    m_dialBias.setBounds(5 * width / 7 - 35, 20, 70, 70);
    m_dialMakeup.setBounds(6 * width / 7 - 35, 20, 70, 70);
}

std::complex<float> convertToComplex(const Polar& rhs) {
    return {
        rhs.amplitude * std::cosf(rhs.phase),
        rhs.amplitude * std::sinf(rhs.phase)
    };
}

void PluginProcessorEditor::timerCallback() {
    bool shouldRepaint = false;

    std::vector<std::vector<Polar>> tmpSpectra(m_spectra.size());

    for (auto& tmpSpectrum : tmpSpectra) {
        tmpSpectrum.resize(m_spectra[0].size());
    }

    if (pluginProcessor.isSpectrumReady()) {
        pluginProcessor.copySpectrum(m_spectra);
        shouldRepaint = true;
    }

    if (shouldRepaint) {
        this->repaint();
    }
}
