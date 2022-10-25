#include "reader.h"
#include "utils.h"
#include <JuceHeader.h>
#include <chrono>
#include <fstream>
#include <queue>
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
            std::ifstream f("INPUT.bin", std::ios::binary | std::ios::in);
            char c;
            while (f.get(c)) {
                for (int i = 7; i >= 0; i--) { track.push(static_cast<bool>((c >> i) & 1)); }
            }
            generateOutput();
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

    ~MainContentComponent() override {
        delete reader;
        shutdownAudio();
    }

private:
    void initThreads() {
        reader = new Reader(&directInput, &directInputLock, &binaryInput, &binaryInputLock);
        reader->startThread();
    }


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

        initThreads();
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
                    directInputLock.enter();
                    for (auto i = 0; i < bufferSize; ++i) { directInput.push(data[i]); }
                    directInputLock.exit();
                    buffer->clear();
                } else if (status == 1) {
                    float *writePosition = buffer->getWritePointer(channel);
                    //                    outputBinaryLock.enter();
                    //                    for (auto i = 0; i < bufferSize; i += LENGTH_OF_ONE_BIT) {
                    //                        auto temp = track.front();
                    //                        track.pop();
                    //                        for (auto j = 0; j < LENGTH_OF_ONE_BIT; ++j) {
                    //                            if (temp) {// Please do not make it short, we may change its logic here.
                    //                                writePosition[j] = 0.75f;
                    //                            } else {
                    //                                writePosition[j] = 0.0f;
                    //                            }
                    //                        }
                    //                    }
                    //                    outputBinaryLock.exit();
                    for (int i = 0; i < bufferSize; ++i) {
                        if (outputHere.empty()) break;
                        auto temp = outputHere.front();
                        outputHere.pop();
                        writePosition[i] = temp;
                        ++qwer;
                    }
                    std::cout << qwer << "\n";
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

    void generateOutput() {
        auto count = 0;
        while (!track.empty()) {
            if (count % 400 == 0) {
                for (auto jjj: preamble) { outputHere.push(jjj); }
            }
            auto temp = track.front();
            track.pop();
            for (int i = 0; i < LENGTH_OF_ONE_BIT; ++i) {
                if (temp) {
                    outputHere.push(0.75f);
                } else {
                    outputHere.push(0);
                }
            }
            ++count;
        }
    }

private:
    Reader *reader{nullptr};
    std::queue<float> directInput;
    CriticalSection directInputLock;
    std::queue<bool> binaryInput;
    CriticalSection binaryInputLock;

    std::queue<bool> track;
    CriticalSection outputBinaryLock;
    std::vector<float> preamble;

    std::queue<float> outputHere;

    juce::Label titleLabel;
    juce::TextButton recordButton;
    juce::TextButton playbackButton;

    std::atomic<int> status{0};

    size_t qwer{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
