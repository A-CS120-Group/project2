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


    explicit Reader(std::queue<float> *bufferIn, CriticalSection *lockInput,
                    std::queue<FrameType> *bufferOut, CriticalSection *lockOutput)
            : Thread("Reader"), input(bufferIn), output(bufferOut), protectInput(lockInput),
              protectOutput(lockOutput) {}

    ~Reader() override {
        this->signalThreadShouldExit();
    }

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
                if (input->empty()) protectInput->exit();
                else {
                    float nextValue = input->front();
                    input->pop();
                    protectInput->exit();
                    // Divide here - in case we have a complex float-to-bool method
                    avg += nextValue / LENGTH_OF_ONE_BIT;
                    ++temp;
                    if (temp == LENGTH_OF_ONE_BIT) {
                        return (avg > 0.0f);
                    }
                }
            }
            return false;
        };
        auto readInt = [readBool](int len) {
            unsigned int ret = 0;
            for (int i = 0; i < len; ++i) { ret |= static_cast<unsigned int>(readBool() << i); }
            return ret;
        };
        auto detectPreamble = [](const std::deque<float> &compare) {
            for (auto i = 0; i < LENGTH_PREAMBLE; ++i) {
                auto temp = std::accumulate(compare.begin() + i * LENGTH_OF_ONE_BIT,
                                            compare.begin() + (i + 1) * LENGTH_OF_ONE_BIT, 0.0f);
                if (preamble[i] != (temp > 0)) return false;
            }
            return true;
        };
        while (!threadShouldExit()) {
            // wait for PREAMBLE
            auto sync = std::deque<float>(LENGTH_PREAMBLE * LENGTH_OF_ONE_BIT, 0);
            while (!threadShouldExit()) {
                protectInput->enter();
                if (input->empty()) { protectInput->exit(); }
                else {
                    float nextValue = input->front();
                    input->pop();
                    protectInput->exit();
                    sync.pop_front();
                    sync.push_back(nextValue);
                    if (detectPreamble(sync)) {
                        std::cout << "Header found ";
                        protectInput->enter();
                        input->pop();
                        input->pop();
                        protectInput->exit();
                        break;
                    }
                }
            }
            // read LEN, SEQ
            unsigned int numLEN = readInt(LENGTH_LEN);
            unsigned int numSEQ = readInt(LENGTH_SEQ);
            if (numLEN > MAX_LENGTH_BODY) {
                // Too long! There must be some errors.
                std::cout << "and discarded due to wrong length. (" << numLEN << "), seq: (" << numSEQ << ")"
                          << std::endl;
                continue;
            }
            // read BODY
            FrameType frame(numLEN, numSEQ);
            for (auto i = 0u; i < numLEN; ++i)
                frame.frame[i] = readBool();
            // read CRC
            unsigned int numCRC = readInt(LENGTH_CRC);
            if (frame.crc() != numCRC) {
                std::cout << "and discarded due to failing CRC check. (sequence " << numSEQ << ")" << std::endl;
                continue;
            }
            protectOutput->enter();
            output->push(frame);
            protectOutput->exit();
            std::cout << "and SUCCEED! (sequence " << numSEQ << ")" << std::endl;
        }
    }

private:
    std::queue<float> *input{nullptr};
    std::queue<FrameType> *output{nullptr};
    CriticalSection *protectInput;
    CriticalSection *protectOutput;
};

#endif//READER_H
