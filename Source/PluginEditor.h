#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

typedef juce::AudioProcessorValueTreeState::SliderAttachment SliderAttachment;

class PluginProcessorEditor
  : public juce::AudioProcessorEditor
  , private juce::Timer
{
  public:
    PluginProcessorEditor(PluginProcessor&, juce::AudioProcessorValueTreeState&);
    ~PluginProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void timerCallback() override;

  private:
    PluginProcessor& pluginProcessor;

    std::vector<std::vector<float>> m_audioBuffers;
    std::vector<std::vector<Polar>> m_spectra;

    juce::Slider m_dialBands;
    juce::Slider m_dialPosition;
    juce::Slider m_dialWidth;
    juce::Slider m_dialOffset;
    juce::Slider m_dialBias;
    juce::Slider m_dialMakeup;

    juce::Label m_labelBands;
    juce::Label m_labelPosition;
    juce::Label m_labelWidth;
    juce::Label m_labelOffset;
    juce::Label m_labelBias;
    juce::Label m_labelMakeup;

    std::unique_ptr<SliderAttachment> m_dialBandsAttachment;
    std::unique_ptr<SliderAttachment> m_dialPositionAttachment;
    std::unique_ptr<SliderAttachment> m_dialWidthAttachment;
    std::unique_ptr<SliderAttachment> m_dialOffsetAttachment;
    std::unique_ptr<SliderAttachment> m_dialBiasAttachment;
    std::unique_ptr<SliderAttachment> m_dialMakeupAttachment;

    juce::AudioProcessorValueTreeState& m_valueTreeState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessorEditor)
};
