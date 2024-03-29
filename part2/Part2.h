#include "reader.h"
#include "utils.h"
#include "writer.h"
#include <JuceHeader.h>
#include <fstream>
#include <queue>
#include <thread>
#include <vector>
#include <map>

#pragma once

class MainContentComponent : public juce::AudioAppComponent {
public:
    MainContentComponent() {
        titleLabel.setText("Part2", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(160, 40);
        titleLabel.setFont(juce::Font(36, juce::Font::FontStyleFlags::bold));
        titleLabel.setJustificationType(juce::Justification(juce::Justification::Flags::centred));
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        sendButton.setButtonText("Send");
        sendButton.setSize(80, 40);
        sendButton.setCentrePosition(150, 140);
        sendButton.onClick = [this] {
//            writer->send(FrameType(0, 0));
//            MyTimer testRingTime;
//            while (true) {
//                binaryInputLock.enter();
//                if (binaryInput.empty()) {
//                    binaryInputLock.exit();
//                    continue;
//                }
//                fprintf(stderr, "%lf\n", testRingTime.duration());
//                binaryInput.pop();
//                binaryInputLock.exit();
//                break;
//            }
            MyTimer testTotalTime;
            std::ifstream fIn("INPUT.bin", std::ios::binary | std::ios::in);
            assert(fIn.is_open());
            std::vector<bool> data;
            for (char c; fIn.get(c);) {
                for (int i = 0; i < 8; ++i)
                    data.push_back((bool) ((c >> i) & 1));
            }
            int dataLength = (int) data.size();
            std::vector<FrameType> frameList(1, {0, 0}); // the first one is dummy
            for (int i = 0; i * MAX_LENGTH_BODY < dataLength; ++i) {
                int len = std::min(MAX_LENGTH_BODY, dataLength - i * MAX_LENGTH_BODY);
                FrameType frame(len, i + 1);
                for (int j = 0; j < len; ++j)
                    frame.frame[j] = data[i * MAX_LENGTH_BODY + j];
                frameList.emplace_back(std::move(frame));
            }
            int LAR = 0, LFS = 0;
            std::vector<FrameWaitingInfo> info;
            while (LAR < frameList.rbegin()->seq) {
                // try to receive ACK
                binaryInputLock.enter();
                if (!binaryInput.empty()) {
                    FrameType ACKFrame = std::move(binaryInput.front());
                    binaryInput.pop();
                    int seq = -ACKFrame.seq;
                    if (LAR < seq && seq <= LFS) {
                        info[LFS - seq].receiveACK = true;
                        fprintf(stderr, "ACK %d detected after waiting for %lfs\n", seq,
                                info[LFS - seq].timer.duration() - info[LFS - seq].waitingTime);
                    }
                }
                binaryInputLock.exit();
                // update LAR
                while (LAR < LFS && info.rbegin()->receiveACK) {
                    ++LAR;
                    info.pop_back();
                }
                // resend timeout frames
                for (int seq = LFS; seq > LAR; --seq) {
                    if (info[LFS - seq].receiveACK ||
                        info[LFS - seq].timer.duration() - info[LFS - seq].waitingTime < SLIDING_WINDOW_TIMEOUT)
                        continue;
                    if (info[LFS - seq].resendTimes == 0) {
                        fprintf(stderr, "Link error detected! seq = %d\n", seq);
                        return;
                    }
                    info[LFS - seq].waitingTime = writer->send(frameList[seq]);
                    info[LFS - seq].timer.restart();
                    info[LFS - seq].resendTimes--;
                    fprintf(stderr, "Frame resent, seq = %d\n", seq);
                }
                // try to update LFS and send a frame
                if (LFS - LAR < SLIDING_WINDOW_SIZE && LFS < frameList.rbegin()->seq) {
                    ++LFS;
                    info.insert(info.begin(), FrameWaitingInfo());
                    info.begin()->waitingTime = writer->send(frameList[LFS]);
                    fprintf(stderr, "Frame sent, seq = %d\n", LFS);
                }
            }
            // all ACKs detected, tell the receiver client to terminate
            writer->send(frameList[0]);
            writer->send(frameList[0]);
            fprintf(stderr, "Transmission finished in %lfs\n", testTotalTime.duration());
        };
        addAndMakeVisible(sendButton);

        saveButton.setButtonText("Save");
        saveButton.setSize(80, 40);
        saveButton.setCentrePosition(450, 140);
        saveButton.onClick = [this] {
            int LFR = 0;
            std::map<int, FrameType> frameList;
            while (true) {
                binaryInputLock.enter();
                if (binaryInput.empty()) {
                    binaryInputLock.exit();
                    continue;
                }
                FrameType frame = std::move(binaryInput.front());
                binaryInput.pop();
                binaryInputLock.exit();
                fprintf(stderr, "frame received, seq = %d\n", frame.seq);
                // End of transmission
                if (frame.seq == 0) break;
                // Discard it because it's ACK sent by itself
                if (frame.seq < 0) continue;
                // Accept this frame and update LFR
                frameList.insert(std::make_pair(frame.seq, frame));
                while (frameList.find(LFR + 1) != frameList.end()) ++LFR;
                // send ACK
                writer->send({0, -frame.seq});
                fprintf(stderr, "ACK sent, seq = %d\n", -frame.seq);
            }
            std::vector<bool> data;
            for (auto const &iter: frameList)
                for (auto b: iter.second.frame) data.push_back(b);
            auto dataLength = data.size();
            assert(dataLength % 8 == 0);
            std::ofstream fOut("OUTPUT.bin", std::ios::binary | std::ios::out);
            for (int i = 0; i < dataLength; i += 8) {
                char c = 0;
                for (int j = 0; j < 8; ++j)
                    c = (char) (c | (data[i + j] << j));
                fOut.put(c);
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
        writer = new Writer(&directOutput, &directOutputLock);
    }

    void prepareToPlay([[maybe_unused]] int samplesPerBlockExpected, [[maybe_unused]] double sampleRate) override {
        initThreads();
        fprintf(stderr, "Main Thread Start\n");
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
                static int channelEmptyPeriods = 0;
                if (directOutput.empty()) {
                    ++channelEmptyPeriods;
                } else {
                    if (channelEmptyPeriods)
                        fprintf(stderr, "        Channel Empty for %d periods!!!!\n", channelEmptyPeriods);
                    channelEmptyPeriods = 0;
                }
                for (int i = 0; i < bufferSize; ++i) {
                    if (directOutput.empty()) {
                        writePosition[i] = 0.45f;
                        continue;
                    }
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

    // GUI related
    juce::Label titleLabel;
    juce::TextButton sendButton;
    juce::TextButton saveButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
