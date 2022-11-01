#include "reader.h"
#include "utils.h"
#include "writer.h"
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
            std::ifstream f("INPUT.bin", std::ios::binary | std::ios::in);
            assert(f.is_open());
            char c;
            binaryOutputLock.enter();
            while (f.get(c)) {
                for (int i = 7; i >= 0; i--) { binaryOutput.push(static_cast<bool>((c >> i) & 1)); }
            }
            binaryOutputLock.exit();
            generateOutput();
        };
        addAndMakeVisible(recordButton);

        playbackButton.setButtonText("Save");
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
        writer = new Writer(&directOutput, &directOutputLock);
    }


    void prepareToPlay([[maybe_unused]] int samplesPerBlockExpected, double sampleRate) override {
        std::vector<float> t;
        t.reserve((size_t) sampleRate);
        std::cout << sampleRate << std::endl;
        for (int i = 0; i <= sampleRate; ++i) { t.push_back((float) i / (float) sampleRate); }

        auto f = linspace(2000, 10000, 120);
        auto f_temp = linspace(10000, 2000, 120);
        f.reserve(f.size() + f_temp.size());
        f.insert(std::end(f), std::begin(f_temp), std::end(f_temp));

        std::vector<float> x(t.begin(), t.begin() + 240);
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
                // Read in PHY layer
                const float *data = buffer->getReadPointer(channel);
                directInputLock.enter();
                for (auto i = 0; i < bufferSize; ++i) { directInput.push(data[i]); }
                directInputLock.exit();
                buffer->clear();
                // Write if PHY layer wants
                float *writePosition = buffer->getWritePointer(channel);
                directOutputLock.enter();
                for (int i = 0; i < bufferSize; ++i) {
                    if (directOutput.empty()) { break; }
                    writePosition[i] = directOutput.front();
                    directOutput.pop();
                }
                directOutputLock.exit();
            }
        }
    }

    void releaseResources() override {
        delete reader;
        delete writer;
    }

    void generateOutput() {
        // TODO: move the function to writer.h
        auto count = 0;
        binaryOutputLock.enter();
        directOutputLock.enter();
        while (!binaryOutput.empty()) {
            if (count % BITS_PER_FRAME == 0) {
                for (int i = 0; i < 10; ++i) { directOutput.push(0); }
                for (auto i: preamble) { directOutput.push(i); }
                for (int i = 0; i < LENGTH_OF_ONE_BIT * 8; ++i) { directOutput.push(0.45f); }
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
        directOutputLock.exit();
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
    Writer *writer{nullptr};
    std::queue<float> directOutput;
    CriticalSection directOutputLock;
    std::queue<bool> binaryOutput;
    CriticalSection binaryOutputLock;

    std::vector<float> preamble;

    // GUI related
    juce::Label titleLabel;
    juce::TextButton recordButton;
    juce::TextButton playbackButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
