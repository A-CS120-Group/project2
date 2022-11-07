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

        sendButton.setButtonText("Send");
        sendButton.setSize(80, 40);
        sendButton.setCentrePosition(150, 140);
        sendButton.onClick = [this] {
            std::ifstream fin("INPUT.bin", std::ios::binary | std::ios::in);
            assert(fin.is_open());
            std::vector<bool> data; // reserved for length
            for (char c; fin.get(c);) {
                for (int i = 7; i >= 0; i--)
                    data.push_back(static_cast<bool>((c >> i) & 1));
            }
            // Read file is very fast, so we reduce the overhead of critical sections.
            binaryOutputLock.enter();
            for (size_t i = 0; i < data.size(); i += MAX_LENGTH_BODY) {
                FrameType frame;
                size_t jEnd = std::min(i + MAX_LENGTH_BODY, data.size());
                for (size_t j = i; j < jEnd; ++j)
                    frame.push_back(data[j]);
                binaryOutput.push(frame);
            }
            binaryOutputLock.exit();
        };
        addAndMakeVisible(sendButton);

        saveButton.setButtonText("Save");
        saveButton.setSize(80, 40);
        saveButton.setCentrePosition(450, 140);
        saveButton.onClick = [this] {
            std::ofstream fout("OUTPUT.bin", std::ios::binary | std::ios::out);
            int ACKCount = 0;
            for (int i = 0; i < 10; ++i) {//TODO: only test several frames
                binaryInputLock.enter();
                while (!binaryInput.empty()) {
                    FrameType frame = binaryInput.front();
                    for (auto b: frame)fout << b;
                }
                binaryInputLock.exit();
            }
        };
        addAndMakeVisible(saveButton);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void initThreads() {
        reader = new Reader(&directInput, &directInputLock, &binaryInput, &binaryInputLock);
        reader->startThread();
        writer = new Writer(&binaryOutput, &binaryOutputLock, &directOutput, &directOutputLock);
        writer->startThread();
    }

    void prepareToPlay([[maybe_unused]]int samplesPerBlockExpected, [[maybe_unused]]double sampleRate) override {
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

private:
    // Process Input
    Reader *reader{nullptr};
    std::queue<float> directInput;
    CriticalSection directInputLock;
    std::queue<FrameType> binaryInput;
    CriticalSection binaryInputLock;

    // Process Output
    Writer *writer{nullptr};
    std::queue<float> directOutput;
    CriticalSection directOutputLock;
    std::queue<FrameType> binaryOutput;
    CriticalSection binaryOutputLock;

    // GUI related
    juce::Label titleLabel;
    juce::TextButton sendButton;
    juce::TextButton saveButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
