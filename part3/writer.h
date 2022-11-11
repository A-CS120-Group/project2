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

    void writeBool(bool bit) {
        for (int i = 0; i < LENGTH_OF_ONE_BIT; ++i) { output->push(bit ? 1.0f : 0.0f); }
    };

    void writeShort(short x) {
        for (int i = 0; i < 16; ++i) { writeBool((bool) (x >> i & 1)); }
    };

    void writeInt(int x) {
        for (int i = 0; i < 32; ++i) { writeBool((bool) (x >> i & 1)); }
    };

    void send(const FrameType &frame) {
        MyTimer testNoisyTime;
        while (!quiet->get()); // listen before transmit
        fprintf(stderr, "defer %lfs because of noisy\n", testNoisyTime.duration());
        protectOutput->enter();
//        while (!output->empty()) {
//            protectOutput->exit();
//            protectOutput->enter();
//        }
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
        protectOutput->exit();
    }

private:
    std::queue<float> *output{nullptr};
    CriticalSection *protectOutput;
    Atomic<bool> *quiet;
};

#endif//WRITER_H
