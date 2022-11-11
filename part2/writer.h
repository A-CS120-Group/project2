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

    explicit Writer(std::queue<float> *bufferOut, CriticalSection *lockOutput) :
            output(bufferOut), protectOutput(lockOutput) {}

    void writeBool(bool bit) {
        for (int i = 0; i < LENGTH_OF_ONE_BIT; ++i) { output->push(bit ? 1.0f : 0.0f); }
    };

    void writeShort(short x) {
        for (int i = 0; i < 16; ++i) { writeBool((bool) (x >> i & 1)); }
    };

    void writeInt(int x) {
        for (int i = 0; i < 32; ++i) { writeBool((bool) (x >> i & 1)); }
    };

    // Send a frame, return estimated waiting time
    double send(const FrameType &frame) {
        protectOutput->enter();
        while (!output->empty()) {
            protectOutput->exit();
            protectOutput->enter();
        }
        // PREAMBLE
        for (auto b: preamble) { writeBool(b); }
        // LEN
        writeShort((short) frame.size());
        // SEQ
        writeShort((short) frame.seq);
        // BODY
        for (auto b: frame.frame) { writeBool(b); }
        // CRC
        writeInt((int) frame.crc());
        double waitingTime = (double) output->size() / 48000.0;
        protectOutput->exit();
        return waitingTime;
    }

private:
    std::queue<float> *output{nullptr};
    CriticalSection *protectOutput;
};

#endif//WRITER_H
