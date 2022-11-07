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

    explicit Writer(std::queue<FrameType> *bufferIn, CriticalSection *lockInput,
                    std::queue<float> *bufferOut, CriticalSection *lockOutput)
            : Thread("Writer"), input(bufferIn), output(bufferOut), protectInput(lockInput),
              protectOutput(lockOutput) {}

    ~Writer() override { this->signalThreadShouldExit(); }

    void run() override {
        assert(input != nullptr);
        assert(output != nullptr);
        assert(protectInput != nullptr);
        assert(protectOutput != nullptr);
        auto writeBool = [this](bool bit) {
            for (int i = 0; i < LENGTH_OF_ONE_BIT; ++i) { this->output->push(bit ? 0.75f : 0); }
        };
        auto writeInt = [writeBool](unsigned int x, int len) {
            for (int i = 0; i < len; ++i) { writeBool((bool) (x >> i & 1)); }
        };
        while (!threadShouldExit()) {
            protectInput->enter();
            if (!input->empty()) {
                FrameType frame = std::move(input->front());
                input->pop();
                protectInput->exit();

                protectOutput->enter();
                // PREAMBLE
                for (auto b: preamble) { writeBool(b); }
                // SEQ
                ++count;
                writeInt(count, LENGTH_SEQ);
                // LEN
                writeInt(frame.size(), LENGTH_LEN);
                // BODY
                for (auto b: frame) { writeBool(b); };
                // CRC
                writeInt(crc(frame), LENGTH_CRC);
                protectOutput->exit();
            } else
                protectInput->exit();
        }
    }

private:
    std::queue<FrameType> *input{nullptr};
    std::queue<float> *output{nullptr};
    CriticalSection *protectInput;
    CriticalSection *protectOutput;

    int count = 0;
    static constexpr bool preamble[LENGTH_PREAMBLE]
            {1, 0, 1, 0, 1, 0, 1, 0,
             1, 0, 1, 0, 1, 0, 1, 0,
             1, 0, 1, 0, 1, 0, 1, 0,
             1, 0, 1, 0, 1, 0, 1, 0,
             1, 0, 1, 0, 1, 0, 1, 0,
             1, 0, 1, 0, 1, 0, 1, 1,
            };
};

#endif//WRITER_H
