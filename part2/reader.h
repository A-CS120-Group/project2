#ifndef READER_H
#define READER_H

#include "utils.h"
#include <JuceHeader.h>
#include <cassert>
#include <ostream>
#include <queue>

class Reader : public Thread {
public:
    Reader() = delete;

    Reader(const Reader &) = delete;

    Reader(const Reader &&) = delete;


    explicit Reader(std::queue<float> *bufferIn, CriticalSection *lockInput, std::queue<FrameType> *bufferOut, CriticalSection *lockOutput)
        : Thread("Reader"), input(bufferIn), output(bufferOut), protectInput(lockInput), protectOutput(lockOutput) {}

    ~Reader() override { this->signalThreadShouldExit(); }

    void run() override {
        assert(input != nullptr);
        assert(output != nullptr);
        assert(protectInput != nullptr);
        assert(protectOutput != nullptr);
        auto readBool = [this]() {
            auto temp = 0;
            auto avg = 0.0f;
            while (!threadShouldExit()) {
                protectInput->enter();
                if (input->empty()) {
                    protectInput->exit();
                    continue;
                }
                float nextValue = input->front();
                input->pop();
                protectInput->exit();
                // Divide here - in case we have a complex float-to-bool method
                avg += nextValue / LENGTH_OF_ONE_BIT;
                ++temp;
                if (temp == LENGTH_OF_ONE_BIT) { return (avg > 0.0f); }
            }
            return false;
        };
        auto readShort = [readBool]() {
            short ret = 0;
            for (int i = 0; i < 16; ++i) { ret = (short) (ret | (readBool() << i)); }
            return ret;
        };
        auto readInt = [readBool]() {
            int ret = 0;
            for (int i = 0; i < 32; ++i) { ret = ret | (readBool() << i); }
            return ret;
        };
        auto waitForPreamble = [this]() {// sync[i] = Σ signal[i : i + LENGTH_OF_ONE_BIT]
            auto sync = std::deque<float>(LENGTH_PREAMBLE * LENGTH_OF_ONE_BIT, 0);
            while (!threadShouldExit()) {
                protectInput->enter();
                if (input->empty()) {
                    protectInput->exit();
                    continue;
                }
                float nextValue = input->front();
                input->pop();
                protectInput->exit();
                sync.pop_front();
                sync.push_back(nextValue);
                for (int i = 1; i < LENGTH_OF_ONE_BIT; ++i) *(sync.rbegin() + i) += nextValue;
                bool isPreamble = true;
                for (int i = 0; i < LENGTH_PREAMBLE; ++i)
                    if (preamble[i] != (sync[i * LENGTH_OF_ONE_BIT] > 0)) {
                        isPreamble = false;
                        break;
                    }
                if (isPreamble) return;
            }
        };
        std::string logString;
        std::ostringstream logOut(logString);
        while (!threadShouldExit()) {
            std::cout << logString;
            // wait for PREAMBLE
            waitForPreamble();
            if (threadShouldExit()) break;
            std::cout << "Header found ";
            protectInput->enter();
            if (!input->empty()) input->pop();
            protectInput->exit();
            // read LEN, SEQ
            int numLEN = readShort();
            int numSEQ = readShort();
            if (numLEN > MAX_LENGTH_BODY) {
                // Too long! There must be some errors.
                logOut << "Header found and discarded due to wrong length. (" << numLEN << "), seq: (" << numSEQ << ")" << std::endl;
                continue;
            }
            // read BODY
            FrameType frame(numLEN, numSEQ);
            for (int i = 0; i < numLEN; ++i) frame.frame[i] = readBool();
            // read CRC
            unsigned int numCRC = readInt();
            if (frame.crc() != numCRC) {
                logOut << "Header found and discarded due to failing CRC check. (sequence " << numSEQ << ")" << std::endl;
                continue;
            }
            protectOutput->enter();
            output->push(frame);
            protectOutput->exit();
            logOut << "Header found and SUCCEED! (sequence " << numSEQ << ")" << std::endl;
        }
    }

private:
    std::queue<float> *input{nullptr};
    std::queue<FrameType> *output{nullptr};
    CriticalSection *protectInput;
    CriticalSection *protectOutput;
};

#endif//READER_H
