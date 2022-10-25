#include "utils.h"
#include <JuceHeader.h>
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

#pragma once

using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;

class MainContentComponent : public juce::AudioAppComponent {
public:
    MainContentComponent() {
        titleLabel.setText("Part3", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(160, 40);
        titleLabel.setFont(juce::Font(36, juce::Font::FontStyleFlags::bold));
        titleLabel.setJustificationType(juce::Justification(juce::Justification::Flags::centred));
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        recordButton.setButtonText("Send");
        recordButton.setSize(80, 40);
        recordButton.setCentrePosition(150, 140);
        recordButton.onClick = [this] {
            track.clear();
            std::ifstream f("INPUT.bin", std::ios::binary | std::ios::in);
            char c;
            while (f.get(c)) {
                for (int i = 7; i >= 0; i--) { track.push_back(static_cast<bool>((c >> i) & 1)); }
            }
            status = 1;
        };
        addAndMakeVisible(recordButton);

        playbackButton.setButtonText("Receive");
        playbackButton.setSize(80, 40);
        playbackButton.setCentrePosition(450, 140);
        playbackButton.onClick = nullptr;
        addAndMakeVisible(playbackButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void prepareToPlay([[maybe_unused]] int samplesPerBlockExpected, double sampleRate) override {
        std::vector<float> t;
        t.reserve((int) sampleRate);
        std::cout << sampleRate << std::endl;
        for (int i = 0; i <= sampleRate; ++i) { t.push_back((float) i / (float) sampleRate); }

        auto f = linspace(2000, 10000, 240);
        auto f_temp = linspace(10000, 2000, 240);
        f.reserve(f.size() + f_temp.size());
        f.insert(std::end(f), std::begin(f_temp), std::end(f_temp));

        std::vector<float> x(t.begin(), t.begin() + 480);
        preamble = cumtrapz(x, f);
        for (float &i: preamble) { i = sin(2.0f * PI * i); }
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) override {
        auto *device = deviceManager.getCurrentAudioDevice();
        auto activeInputChannels = device->getActiveInputChannels();
        auto activeOutputChannels = device->getActiveOutputChannels();
        auto maxInputChannels = activeInputChannels.getHighestBit() + 1;
        auto maxOutputChannels = activeOutputChannels.getHighestBit() + 1;
        auto buffer = bufferToFill.buffer;
        auto bufferSize = buffer->getNumSamples();

        for (auto channel = 0; channel < maxOutputChannels; ++channel) {
            if ((!activeInputChannels[channel] || !activeOutputChannels[channel]) || maxInputChannels == 0) {
                bufferToFill.buffer->clear(channel, bufferToFill.startSample, bufferToFill.numSamples);
            } else {
                if (status == 0) {
                    const float *data = buffer->getReadPointer(channel);
                    for (int i = 0; i < bufferSize; ++i) {
                        //                        std::cout << (int) ((int) (data[i] / 10e-4) > 350) << " ";
                        if (data[i] / 10e-4 > 100) {
                            std::cout << "1";
                        } else if (data[i] / 10e-4 < -100) {
                            std::cout << "0";
                        } else {
                            std::cout << "_";
                        }
                    }
                    buffer->clear();
                } else if (status == 1) {
                    float *writePosition = buffer->getWritePointer(channel);
                    for (int i = 0; i < bufferSize; ++i) {
                        if (track.at(readPosition)) {
                            writePosition[i] = 0.75f;
                        } else {
                            writePosition[i] = 0.0f;
                        }
                        if (i % LENGTH_OF_ONE_BIT == LENGTH_OF_ONE_BIT - 1) {
                            ++readPosition;
                            if (readPosition == track.size()) { status = 0; }
                        }
                    }
                }
            }
        }

        const MessageManagerLock mmLock;
        switch (status) {
            case 0:
                titleLabel.setText("Part1", juce::NotificationType::dontSendNotification);
                break;
            case 1:
                titleLabel.setText("Sending", juce::NotificationType::dontSendNotification);
                break;
            case 2:
                titleLabel.setText("Listening", juce::NotificationType::dontSendNotification);
                break;
            case 3:
                titleLabel.setText("Processing", juce::NotificationType::dontSendNotification);
                break;
        }
    }

    void releaseResources() override {}

private:
    std::vector<bool> track;
    std::vector<float> preamble;

    juce::Label titleLabel;
    juce::TextButton recordButton;
    juce::TextButton playbackButton;

    int status{0};
    int readPosition{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
