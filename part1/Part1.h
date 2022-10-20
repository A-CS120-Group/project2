#include <JuceHeader.h>
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

#pragma once

#define LENGTH_OF_ONE_BIT 6   // Must be a number in 1/2/3/4/5/6/8/10

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
    void prepareToPlay([[maybe_unused]] int samplesPerBlockExpected, [[maybe_unused]] double sampleRate) override {}

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
                    for (int i = 0; i < bufferSize; ++i) { std::cout << (int) ((int) (data[i] / 10e-4) > 350) << " "; }
                    std::cout << std::endl;
                    buffer->clear();
                    break;
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
                            if (readPosition == track.size()) {
                                status = 0;
                            }
                        }
                    }
                    break;
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

    void processInput() {
#ifdef WIN32
        juce::File writeTo(R"(C:\Users\caoster\Desktop\CS120\project1\)" + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#else
        juce::File writeTo(juce::File::getCurrentWorkingDirectory().getFullPathName() + juce::Time::getCurrentTime().toISO8601(false) + ".out");
#endif
        track.clear();
        for (auto b: track) {
            std::cout << b;
            writeTo.appendText(b ? "1" : "0");
        }
        status = 0;
    }

    void releaseResources() override {}

private:
    std::vector<bool> track;

    juce::Label titleLabel;
    juce::TextButton recordButton;
    juce::TextButton playbackButton;

    int status{0};
    int readPosition{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
