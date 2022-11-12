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

    explicit Reader(std::queue<float> *bufferIn, CriticalSection *lockInput, std::queue<FrameType> *bufferOut,
                    CriticalSection *lockOutput)
            : Thread("Reader"), input(bufferIn), output(bufferOut), protectInput(lockInput),
              protectOutput(lockOutput) {
        fprintf(stderr, "    Reader Thread Start\n");
    }

    ~Reader() override { this->signalThreadShouldExit(); }

    void run() override {
        assert(input != nullptr);
        assert(output != nullptr);
        assert(protectInput != nullptr);
        assert(protectOutput != nullptr);
        auto readBool = [this]() {
            float sum = 0.0f;
            protectInput->enter();
            for (int i = 0; i < LENGTH_OF_ONE_BIT && !threadShouldExit(); ++i) {
                if (input->empty()) {
                    protectInput->exit();
                    continue;
                }
                float nextValue = input->front();
                input->pop();
                if (i != 0 && i != LENGTH_OF_ONE_BIT - 1)
                    sum += 2 * nextValue;
                else sum += nextValue;
            }
            protectInput->exit();
            return sum > 0.0f;
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
                    if ((preamble[i] && sync[i * LENGTH_OF_ONE_BIT] < PREAMBLE_THRESHOLD) ||
                        (!preamble[i] && sync[i * LENGTH_OF_ONE_BIT] > -PREAMBLE_THRESHOLD)) {
                        isPreamble = false;
                        break;
                    }
                if (isPreamble) return;
            }
        };
        while (!threadShouldExit()) {
            // wait for PREAMBLE
            waitForPreamble();
            if (threadShouldExit()) break;
            // read LEN, SEQ
            int numLEN = (unsigned short) readShort();
            int numSEQ = readShort();
            if (numLEN > MAX_LENGTH_BODY) {
                // Too long! There must be some errors.
                fprintf(stderr, "    Discarded due to wrong length. len = %d, seq = %d\n", numLEN, numSEQ);
                continue;
            }
            // read BODY
            FrameType frame(numLEN, numSEQ);
            for (int i = 0; i < numLEN; ++i) frame.frame[i] = readBool();
            // read CRC
            unsigned int numCRC = readInt();
            if (frame.crc() != numCRC) {
                fprintf(stderr, "    Discarded due to failing CRC check. len = %d, seq = %d\n", numLEN, numSEQ);
                continue;
            }
            protectOutput->enter();
            output->push(frame);
            protectOutput->exit();
            fprintf(stderr, "    SUCCEED! len = %d, seq = %d\n", numLEN, numSEQ);
        }
    }

private:
    std::queue<float> *input{nullptr};
    std::queue<FrameType> *output{nullptr};
    CriticalSection *protectInput;
    CriticalSection *protectOutput;
};

#endif//READER_H
