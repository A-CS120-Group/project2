#ifndef WRITER_H
#define WRITER_H

#include "utils.h"
#include <JuceHeader.h>
#include <cassert>
#include <ostream>
#include <queue>

class Writer : public Thread {
public:
    Writer() = delete;

    Writer(const Writer &) = delete;

    Writer(const Writer &&) = delete;

    explicit Writer(std::queue<FrameType> *bufferIn, CriticalSection *lockInput, std::queue<float> *bufferOut, CriticalSection *lockOutput)
        : Thread("Writer"), input(bufferIn), output(bufferOut), protectInput(lockInput), protectOutput(lockOutput) {}

    ~Writer() override { this->signalThreadShouldExit(); }

    void run() override {
        assert(input != nullptr);
        assert(output != nullptr);
        assert(protectInput != nullptr);
        assert(protectOutput != nullptr);
        auto writeBool = [this](bool bit) { // TODO: what about 1.0f : -1.0f ?
            for (int i = 0; i < LENGTH_OF_ONE_BIT; ++i) { this->output->push(bit ? 1.0f : 0); }
        };
        auto writeShort = [writeBool](short x) {
            for (int i = 0; i < 16; ++i) { writeBool((bool) (x >> i & 1)); }
        };
        auto writeInt = [writeBool](int x) {
            for (int i = 0; i < 32; ++i) { writeBool((bool) (x >> i & 1)); }
        };
        while (!threadShouldExit()) {
            protectInput->enter();
            if (input->empty()) {
                protectInput->exit();
                continue;
            }
            FrameType frame = std::move(input->front());
            input->pop();
            protectInput->exit();

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
            std::string logString;
            std::ostringstream logOut(logString);
            logOut << "Send frame " << frame.seq << std::endl;
            std::cout << logString;
            protectOutput->exit();
        }
    }

private:
    std::queue<FrameType> *input{nullptr};
    std::queue<float> *output{nullptr};
    CriticalSection *protectInput;
    CriticalSection *protectOutput;
};

#endif//WRITER_H
