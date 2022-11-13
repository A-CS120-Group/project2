#include "reader.h"
#include "utils.h"
#include "writer.h"
#include <JuceHeader.h>
#include <fstream>
#include <map>
#include <queue>
#include <thread>
#include <vector>

#pragma once

class MainContentComponent : public juce::AudioAppComponent {
public:
    MainContentComponent() {
        titleLabel.setText("Part5", juce::NotificationType::dontSendNotification);
        titleLabel.setSize(160, 40);
        titleLabel.setFont(juce::Font(36, juce::Font::FontStyleFlags::bold));
        titleLabel.setJustificationType(juce::Justification(juce::Justification::Flags::centred));
        titleLabel.setCentrePosition(300, 40);
        addAndMakeVisible(titleLabel);

        Node1Button.setButtonText("Node1");
        Node1Button.setSize(80, 40);
        Node1Button.setCentrePosition(150, 140);
        Node1Button.onClick = [this] {// ping
            // Transmission Initialization
            std::string data;

            // Fill random bytes for MacPerf
            juce::Random e;
            for (int i = 0; i < PERF_NUMBER_PACKETS; ++i) {
                for (int j = 0; j < MAX_LENGTH_BODY; ++j) {
                    data.push_back(static_cast<char>(e.nextInt(juce::Range<int>(0, 128))));
                }
            }

            size_t dataLength = data.size();
            // frameList[0] is used to store the number of frames
            std::vector<FrameType> frameListSent(1), frameListRec;
            for (unsigned i = 0; i * MAX_LENGTH_BODY < dataLength; ++i) {
                auto len = (LENType) std::min(MAX_LENGTH_BODY, dataLength - i * MAX_LENGTH_BODY);
                auto seq = (SEQType) ((signed) (i + 2) * 1);
                frameListSent.emplace_back(FrameType(len, seq, data.c_str() + i));
            }
            auto frameNumSent = (SEQType) frameListSent.size();
            frameListSent[0] = FrameType((LENType) LENGTH_SEQ, (SEQType) 1, &frameNumSent);
            // Node2 waits for Node1 to tell it start
            MyTimer testTotalTime;
            MyTimer pingTimeRec, pingTimeSent;
            unsigned LFR = 0;
            bool receiveAll = false;
            while (!receiveAll) {
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
                    auto seqNum = (unsigned) abs(frame.seq), index = seqNum - 1;
                    // It's a frame
                    if (frame.len != 0) {
                        // ignore self sent
                        if (frame.seq > 0) continue;
                        fprintf(stderr, "Perf frame received, seq = %d\n", frame.seq);
                        // Accept this frame and update LFR
                        while (frameListRec.size() <= index) frameListRec.emplace_back(FrameType());
                        frameListRec[index] = frame;
                        while (LFR < frameListRec.size() && frameListRec[LFR].len != 0) ++LFR;
                        // send ACK
                        writer->send(FrameType(0, frame.seq, nullptr));
                        // every frame from the other Node is received
                        if (!receiveAll && LFR == (unsigned) *(SEQType *) &frameListRec[0].body) {
                            receiveAll = true;
                            fprintf(stderr, "Test Finish with average throughput: %dbps",
                                    static_cast<int>((PERF_NUMBER_PACKETS / testTotalTime.duration()) *
                                                     MAX_LENGTH_BODY * 8));
                            // We don't want to keep those random packets
                        }
                    } else {// It's an ACK
                        fprintf(stderr, "Ping succeed with RTT %lfs.\n", pingTimeRec.duration());
                        pingTimeRec.restart();
                        pingTimeSent.restart();
                    }
                }
                // repeat sending ping frame
                if (pingTimeSent.duration() > 0.3) {
                    writer->send(frameListSent[0]);
                    fprintf(stderr, "PING sent!, seq = %d\n", frameListSent[0].seq);
                    pingTimeSent.restart();
                }
                if (pingTimeRec.duration() > MACPING_REPLY) {
                    fprintf(stderr, "PING TIMEOUT!!!\n");
                    break;
                }
            }
        };
        addAndMakeVisible(Node1Button);

        Node2Button.setButtonText("Node2");
        Node2Button.setSize(80, 40);
        Node2Button.setCentrePosition(450, 140);
        Node2Button.onClick = [this] {// perf
            // Transmission Initialization
            constexpr bool isNode1 = false;
            std::string data;

            // Fill random bytes for MacPerf
            juce::Random e;
            for (int i = 0; i < PERF_NUMBER_PACKETS; ++i) {
                for (int j = 0; j < MAX_LENGTH_BODY; ++j) {
                    data.push_back(static_cast<char>(e.nextInt(juce::Range<int>(0, 128))));
                }
            }

            size_t dataLength = data.size();
            // frameList[0] is used to store the number of frames
            std::vector<FrameType> frameListSent(1), frameListRec;
            std::vector<FrameWaitingInfo> info;
            for (unsigned i = 0; i * MAX_LENGTH_BODY < dataLength; ++i) {
                auto len = (LENType) std::min(MAX_LENGTH_BODY, dataLength - i * MAX_LENGTH_BODY);
                auto seq = (SEQType) ((signed) (i + 2) * -1);
                frameListSent.emplace_back(FrameType(len, seq, data.c_str() + i));
            }
            auto frameNumSent = (SEQType) frameListSent.size();
            frameListSent[0] = FrameType((LENType) LENGTH_SEQ, (SEQType) -1, &frameNumSent);
            // Node2 waits for Node1 to tell it start
            while (true) {
                binaryInputLock.enter();
                if (!binaryInput.empty()) break;
                binaryInputLock.exit();
            }
            binaryInputLock.exit();
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
                    auto seqNum = (unsigned) abs(frame.seq), index = seqNum - 1;
                    // It's a frame
                    if (frame.len != 0) {
                        // ignore self sent
                        if (frame.seq < 0) continue;
                        fprintf(stderr, "Perf frame received, seq = %d\n", frame.seq);
                        // Accept this frame and update LFR
                        while (frameListRec.size() <= index) frameListRec.emplace_back(FrameType());
                        frameListRec[index] = frame;
                        while (LFR < frameListRec.size() && frameListRec[LFR].len != 0) ++LFR;
                        // send ACK
                        writer->send(FrameType(0, frame.seq, nullptr));
                        // every frame from the other Node is received
                        if (!receiveAll && LFR == (unsigned) *(SEQType *) &frameListRec[0].body) {
                            receiveAll = true;
                            fprintf(stderr, "Test Finish with average throughput: %dbps",
                                    static_cast<int>((PERF_NUMBER_PACKETS / testTotalTime.duration()) *
                                                     MAX_LENGTH_BODY * 8));
                            // We don't want to keep those random packets
                        }
                    } else {// It's an ACK
                        if (LAR < seqNum && seqNum <= LFS) {
                            info[LFS - seqNum].receiveACK = true;
                            fprintf(stderr, "Average throughput: %dbps\n",
                                    static_cast<int>((static_cast<double>(seqNum) / testTotalTime.duration()) *
                                                     MAX_LENGTH_BODY * 8));
                        }
                    }
                }
                // update LAR
                while (LAR < LFS && info.rbegin()->receiveACK) {
                    ++LAR;
                    info.pop_back();
                    // every frame to the other Node is ACKed
                    if (!ACKedAll && LAR == (unsigned) frameNumSent) ACKedAll = true;
                }
                // resend timeout frames
                for (unsigned seq = LAR + 1; seq <= LFS; ++seq) {
                    if (info[LFS - seq].receiveACK ||
                        info[LFS - seq].timer.duration() < (isNode1 ? SLIDING_WINDOW_TIMEOUT_NODE1
                                                                    : SLIDING_WINDOW_TIMEOUT_NODE2))
                        continue;
                    if (info[LFS - seq].resendTimes == 0) {
                        fprintf(stderr, "Link error detected!\n");
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
        currentAudioSetup.bufferSize = 144;// 144 160 192
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
                for (int i = 0; i < bufferSize; ++i) writePosition[i] = 0.0f;
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
