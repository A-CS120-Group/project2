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
        static auto MacLayer = [this](bool isNode1) {
            // Transmission Initialization
            std::ifstream fIn("INPUT.bin", std::ios::binary | std::ios::in);
            if (fIn.is_open()) {
                fprintf(stderr, "successfully open INPUT.bin!\n");
            } else {
                fprintf(stderr, "failed to open INPUT.bin!\n");
                return;
            }
            std::string data;
            for (char c; fIn.get(c);) { data.push_back(c); }
            size_t dataLength = data.size();
            // frameList[0] is used to store the number of frames
            std::vector<FrameType> frameListSent(1);
            std::map<unsigned, FrameType>frameListRec;
            std::vector<FrameWaitingInfo> info;
            for (unsigned i = 0; i * MAX_LENGTH_BODY < dataLength; ++i) {
                auto len = (LENType) std::min(MAX_LENGTH_BODY, dataLength - i * MAX_LENGTH_BODY);
                auto seq = (SEQType) ((signed)(i + 2) * (isNode1 ? 1 : -1));
                frameListSent.emplace_back(FrameType(len, seq, data.c_str() + i));
            }
            auto frameNumSent = (SEQType) frameListSent.size();
            frameListSent[0] = FrameType((LENType) LENGTH_SEQ, (SEQType) (isNode1 ? 1 : -1), &frameNumSent);
            // Node2 waits for Node1 to tell it start
            if (!isNode1) {
                while (true) {
                    binaryInputLock.enter();
                    if (!binaryInput.empty()) break;
                    binaryInputLock.exit();
                }
                binaryInputLock.exit();
            }
            MyTimer testTotalTime;
            unsigned LAR = 0, LFS = 0, LFR = 0;
            bool ACKedAll = false, receiveAll = false;
            while (!ACKedAll || !receiveAll) {
                // try to receive a frame or an ACK
                while (true) {
                    binaryInputLock.enter();
                    if (binaryInput.empty()) {
                        binaryInputLock.exit();
                        break;
                    }
                    FrameType frame = binaryInput.front();
                    binaryInput.pop();
                    binaryInputLock.exit();
                    auto seqNum = (unsigned) abs(frame.seq);
                    // It's a frame
                    if (frame.len != 0) {
                        // ignore self sent
                        if (isNode1 ? frame.seq > 0 : frame.seq < 0)
                            continue;
                        fprintf(stderr, "frame received, seq = %d\n", frame.seq);
                        // Accept this frame and update LFR
                        frameListRec[seqNum] = frame;
                        while (frameListRec.find(LFR+1)!=frameListRec.end()) ++LFR;
                        // send ACK
                        writer->send(FrameType(0, frame.seq, nullptr));
                        fprintf(stderr, "ACK sent, seq = %d\n", frame.seq);
                        // every frame from the other Node is received
                        if (!receiveAll && LFR == (unsigned) *(SEQType *) &frameListRec[1].body) {
                            receiveAll = true;
                            fprintf(stderr, "------- All frames received in %lfs --------\n", testTotalTime.duration());
                            std::ofstream fOut("OUTPUT.bin", std::ios::binary | std::ios::out);
                            for (auto iter: frameListRec) {
                                if (iter.first == 1) continue;
                                for (unsigned i = 0; i < iter.second.len; ++i)
                                    fOut.put(iter.second.body[i]);
                            }
                        }
                    } else { // It's an ACK
                        if (LAR < seqNum && seqNum <= LFS) {
                            info[LFS - seqNum].receiveACK = true;
                            fprintf(stderr, "ACK %d received after %lfs, resendTimes left %d\n", frame.seq,
                                    info[LFS - seqNum].timer.duration(), info[LFS - seqNum].resendTimes);
                        }
                    }
                }
                // update LAR
                while (LAR < LFS && info.rbegin()->receiveACK) {
                    ++LAR;
                    info.pop_back();
                    // every frame to the other Node is ACKed
                    if (!ACKedAll && LAR == (unsigned) frameNumSent)
                        ACKedAll = true;
                }
                // resend timeout frames
                for (unsigned seq = LAR+1; seq <= LFS; ++seq) {
                    if (info[LFS - seq].receiveACK ||
                        info[LFS - seq].timer.duration() < (isNode1 ? SLIDING_WINDOW_TIMEOUT_NODE1
                                                                    : SLIDING_WINDOW_TIMEOUT_NODE2))
                        continue;
                    if (info[LFS - seq].resendTimes == 0) {
                        fprintf(stderr, "Link error detected! frame seq = %d resend too many times...\n", seq);
                        return;
                    }
                    writer->send(frameListSent[seq - 1]);
                    fprintf(stderr, "Oh No Frame Resent!, seq = %d\n", frameListSent[seq - 1].seq);
                    info[LFS - seq].timer.restart();
                    info[LFS - seq].resendTimes--;
                }
                // try to update LFS and send a frame
                if (LFS - LAR < SLIDING_WINDOW_SIZE && LFS < (unsigned) frameNumSent) {
                    ++LFS;
                    writer->send(frameListSent[LFS - 1]);
                    info.insert(info.begin(), FrameWaitingInfo());
                    fprintf(stderr, "Frame sent, seq = %d\n", LFS);
                }
            }
        };

        titleLabel.setText("Part3", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(160, 40);
        titleLabel.setFont(juce::Font(36, juce::Font::FontStyleFlags::bold));
        titleLabel.setJustificationType(juce::Justification(juce::Justification::Flags::centred));
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        Node1Button.setButtonText("Node1");
        Node1Button.setSize(80, 40);
        Node1Button.setCentrePosition(150, 140);
        Node1Button.onClick = [] { return MacLayer(true); };
        addAndMakeVisible(Node1Button);

        Node2Button.setButtonText("Node2");
        Node2Button.setSize(80, 40);
        Node2Button.setCentrePosition(450, 140);
        Node2Button.onClick = [] { return MacLayer(false); };
        addAndMakeVisible(Node2Button);

        setSize(600, 300);
        setAudioChannels(1, 1);
    }

    ~MainContentComponent() override { shutdownAudio(); }

private:
    void initThreads() {
        reader = new Reader(&directInput, &directInputLock, &binaryInput, &binaryInputLock);
        reader->startThread();
        writer = new Writer(&directOutput, &directOutputLock, &quiet);
    }

    void prepareToPlay([[maybe_unused]] int samplesPerBlockExpected, [[maybe_unused]] double sampleRate) override {
        initThreads();
        AudioDeviceManager::AudioDeviceSetup currentAudioSetup;
        deviceManager.getAudioDeviceSetup(currentAudioSetup);
        currentAudioSetup.bufferSize = 144; // 144 160 192
        // String ret = deviceManager.setAudioDeviceSetup(currentAudioSetup, true);
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
                for (int i = 0; i < bufferSize; ++i) { directInput.push(data[i]); }
                directInputLock.exit();
                // listen if the channel is quiet
                bool nowQuiet = true;
                for (int i = bufferSize - LENGTH_PREAMBLE * LENGTH_OF_ONE_BIT; i < bufferSize; ++i)
                    if (fabs(data[i]) > NOISY_THRESHOLD) {
                        nowQuiet = false;
//                        fprintf(stderr, "\t\tNoisy Now!!!!\n");
//                        std::ofstream logOut("log.out", std::ios::app);
//                        for (int j = 0; j < bufferSize; ++j)logOut << (int) (data[j] * 100) << ' ';
//                        logOut << "\n";
//                        logOut.close();
                        break;
                    }
                quiet.set(nowQuiet);
                buffer->clear();
                // Write if PHY layer wants
                float *writePosition = buffer->getWritePointer(channel);
                for (int i = 0; i < bufferSize; ++i)
                    writePosition[i] = 0.0f;
//                constexpr int W = 2;
//                constexpr int bits[16] = {1,1,1,1,0,1,1,1,
//                                          1,1,1,1,0,1,1,1};
//                for (int i = 0; i < 16; ++i) {
//                    writePosition[4 * i + 0] = writePosition[4 * i + 1] = bits[i] ? 1.0f : -1.0f;
//                    writePosition[4 * i + 2] = writePosition[4 * i + 3] = bits[i] ? -1.0f : 1.0f;
//                }
                directOutputLock.enter();
                for (int i = 0; i < bufferSize; ++i) {
                    if (directOutput.empty()) {
                        directOutputLock.exit();
                        directOutputLock.enter();
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
    Atomic<bool> quiet = false;

    // GUI related
    juce::Label titleLabel;
    juce::TextButton Node1Button;
    juce::TextButton Node2Button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainContentComponent)
};
