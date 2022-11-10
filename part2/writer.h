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

    explicit Writer(std::queue<float> *bufferOut, CriticalSection *lockOutput) : output(bufferOut),
                                                                                 protectOutput(lockOutput) {}

    void writeBool(bool bit) {// TODO: what about 1.0f : -1.0f ?
        for (int i = 0; i < LENGTH_OF_ONE_BIT; ++i) { output->push(bit ? 1.0f : -1.0f); }
    };

    void writeShort(short x) {
        for (int i = 0; i < 16; ++i) { writeBool((bool) (x >> i & 1)); }
    };

    void writeInt(int x) {
        for (int i = 0; i < 32; ++i) { writeBool((bool) (x >> i & 1)); }
    };

    void send(const FrameType &frame) {
        assert(output != nullptr);
        assert(protectOutput != nullptr);

        protectOutput->enter();
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
//        std::ostringstream logOut;
//        logOut << "    Send frame " << frame.seq << std::endl;
//        std::cout << logOut.str();
        protectOutput->exit();
    }

private:
    std::queue<float> *output{nullptr};
    CriticalSection *protectOutput;
};

#endif//WRITER_H
