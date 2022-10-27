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
        titleLabel.setText("Part1", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(160, 40);
        titleLabel.setFont(juce::Font(36, juce::Font::FontStyleFlags::bold));
        titleLabel.setJustificationType(juce::Justification(juce::Justification::Flags::centred));
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        recordButton.setButtonText("Send");
        recordButton.setSize(80, 40);
        recordButton.setCentrePosition(150, 140);
        recordButton.onClick = [this] {
            status = 2;// TODO: Remember to remove this line
            std::ifstream f("INPUT.bin", std::ios::binary | std::ios::in);
            assert(f.is_open());
            char c;
            binaryOutputLock.enter();
            while (f.get(c)) {
                for (int i = 7; i >= 0; i--) { binaryOutput.push(static_cast<bool>((c >> i) & 1)); }
            }
            binaryOutputLock.exit();
            generateOutput();
            //            status = 1;
            directInputLock.enter();
            while (!directOutput.empty()) {
                directInput.push(directOutput.front());
                directOutput.pop();
            }
            directInputLock.exit();
        };
        addAndMakeVisible(recordButton);

        playbackButton.setButtonText("Receive");
        playbackButton.setSize(80, 40);
        playbackButton.setCentrePosition(450, 140);
        playbackButton.onClick = [this] {
            std::ofstream f("OUTPUT.bin", std::ios::binary | std::ios::out);
            auto count = 0;
            char c = 0;
            binaryInputLock.enter();
            while (!binaryInput.empty()) {
                auto temp = binaryInput.front();
                binaryInput.pop();
                ++count;
                c <<= 1;
                c = temp ? char(c + 1) : char(c);
                if (count % 8 == 0) {
                    f << c;
                    c = 0;
                }
            }
            binaryInputLock.exit();
        };
        addAndMakeVisible(playbackButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void initThreads() {
        reader = new Reader(&directInput, &directInputLock, &binaryInput, &binaryInputLock);
        reader->startThread();
    }


    void prepareToPlay([[maybe_unused]] int samplesPerBlockExpected, double sampleRate) override {
        std::vector<float> t;
        t.reserve((size_t) sampleRate);
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
                    directOutputLock.enter();
                    for (int i = 0; i < bufferSize; ++i) {
                        if (directOutput.empty()) break;
                        auto temp = directOutput.front();
                        writePosition[i] = temp;
                        directOutput.pop();
                    }
                    directOutputLock.exit();
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

    void releaseResources() override { delete reader; }

    void generateOutput() {
        auto count = 0;
        binaryOutputLock.enter();
        while (!binaryOutput.empty()) {
            if (count % 400 == 0) {
                for (int i = 0; i < 10; ++i) { directOutput.push(0); }
                for (auto jjj: preamble) { directOutput.push(jjj); }
            }
            auto temp = binaryOutput.front();
            binaryOutput.pop();
            for (int i = 0; i < LENGTH_OF_ONE_BIT; ++i) {
                if (temp) {
                    directOutput.push(0.75f);
                } else {
                    directOutput.push(0);
                }
            }
            ++count;
        }
        binaryOutputLock.exit();
    }

private:
    // Process Input
    Reader *reader{nullptr};
    std::queue<float> directInput;
    CriticalSection directInputLock;
    std::queue<bool> binaryInput;
    CriticalSection binaryInputLock;

    // Process Output
    std::queue<bool> binaryOutput;
    CriticalSection binaryOutputLock;
    std::queue<float> directOutput;
    CriticalSection directOutputLock;

    std::vector<float> preamble;

    // GUI related
    juce::Label titleLabel;
    juce::TextButton recordButton;
    juce::TextButton playbackButton;

    std::atomic<int> status{0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
