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
            while (!threadShouldExit()) {
                protectInput->enter();
                if (input->empty()) protectInput->exit();
                else {
                    float nextValue = input->front();
                    // TODO:
                }
            }
            return false;
        };
        auto readInt = [readBool](int len) {
            unsigned int ret = 0;
            for (int i = 0; i < len; ++i) { ret |= readBool() << i; }
            return ret;
        };
        while (!threadShouldExit()) {
            // wait for PREAMBLE
            sync = std::deque<float>(LENGTH_PREAMBLE * LENGTH_OF_ONE_BIT, 0);
            while (!threadShouldExit()) {
                protectInput->enter();
                if (input->empty()) { protectInput->exit(); }
                else {
                    float nextValue = input->front();
                    input->pop();
                    protectInput->exit();
                    sync.pop_front();
                    sync.push_back(nextValue);
                    /*TODO: preamble detecting*/
                    if (/*TODO: preamble detected*/) {
                        std::cout << "Header found" << std::endl;
                        break;
                    }
                }
            }
            // read SEQ, LEN
            short numSEQ = readInt(LENGTH_SEQ);
            short numLEN = readInt(LENGTH_LEN);
            if (numLEN > MTU) {
                // TODO: Too long! There must be some errors.
            }
            // read BODY
            FrameType frame(numLEN);
            for (int i = 0; i < numLEN; ++i)
                frame[i] = readBool();
            // read CRC
            unsigned int numCRC = readInt(LENGTH_CRC);
            if (crc(frame) != numCRC) {
                // TODO: CRC error!
            }
            protectOutput->enter();
            output->push(frame);
            protectOutput->exit();
        }
    }

private:
    std::queue<float> *input{nullptr};
    std::queue<FrameType> *output{nullptr};
    CriticalSection *protectInput;
    CriticalSection *protectOutput;

    int count = 0;
    std::deque<float> sync;
};

#endif//READER_H
