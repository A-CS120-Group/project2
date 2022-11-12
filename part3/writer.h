#ifndef WRITER_H
#define WRITER_H

#include "utils.h"
#include <JuceHeader.h>
#include <cassert>
#include <ostream>
#include <queue>

class Writer {
public:
    Writer() = delete;

    Writer(const Writer &) = delete;

    Writer(const Writer &&) = delete;

    explicit Writer(std::queue<float> *bufferOut, CriticalSection *lockOutput, Atomic<bool> *quietPtr) :
            output(bufferOut), protectOutput(lockOutput), quiet(quietPtr) {}

    void send(const FrameType &frame) {
        // listen before transmit
        MyTimer testNoisyTime;
        while (!quiet->get());
        fprintf(stderr, "defer %lfs because of noisy\n", testNoisyTime.duration());
        // transmit
        protectOutput->enter();
        for (auto byte: preamble + frame.wholeString() + inString(frame.crc()))
            for (int bitPos = 0; bitPos < 8; ++bitPos) {
                if (byte >> bitPos & 1) {
                    output->push(1.0f);
                    output->push(1.0f);
                    output->push(-1.0f);
                    output->push(-1.0f);
                } else {
                    output->push(-1.0f);
                    output->push(-1.0f);
                    output->push(1.0f);
                    output->push(1.0f);
                }
            }
        // wait until the transmission finished
        while (!output->empty()) {
            protectOutput->exit();
            protectOutput->enter();
        }
        protectOutput->exit();
    }

private:
    std::queue<float> *output{nullptr};
    CriticalSection *protectOutput;
    Atomic<bool> *quiet;
};

#endif//WRITER_H
