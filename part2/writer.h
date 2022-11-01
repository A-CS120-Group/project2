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


    explicit Writer(std::queue<float> *bufferOut, CriticalSection *lockOutput) : Thread("Writer"), output(bufferOut), protectOutput(lockOutput) {
        auto sampleRate = 48000;
        std::vector<float> t;
        t.reserve((size_t) sampleRate);
        for (int i = 0; i <= sampleRate; ++i) { t.push_back((float) i / (float) sampleRate); }

        auto f = linspace(2000, 10000, 120);
        auto f_temp = linspace(10000, 2000, 120);
        f.reserve(f.size() + f_temp.size());
        f.insert(std::end(f), std::begin(f_temp), std::end(f_temp));

        std::vector<float> x(t.begin(), t.begin() + 240);
        preamble = cumtrapz(x, f);
        for (float &i: preamble) { i = sin(2.0f * PI * i); }

        sync = std::deque<float>(240, 0);
    }

    ~Writer() override { this->signalThreadShouldExit(); }

    void send(const std::vector<bool>& MacFrame) {
        // This function should be fast enough.
        assert(MacFrame.size() % 8 == 0);
        protectOutput->enter();
        auto writeToFloat = [this](bool bit) {
            for (int i = 0; i < LENGTH_OF_ONE_BIT; ++i) { this->output->push(bit ? 0.75f : 0); }
        };
        // Preamble
        for (auto i: preamble) { output->push(i); }
        // Protection with overhead
        for (int i = 0; i < LENGTH_OF_ONE_BIT * 8; ++i) { output->push(0.45f); }
        // Put length after preamble
        auto length = static_cast<int16_t>(MacFrame.size() / 8);
        for (int i = 15; i >= 0; --i) { writeToFloat(length >> i & 1); }
        // Write the content
        for (auto bit: MacFrame) { writeToFloat(bit); }
        // CRC-32
        auto crcResult = crc(MacFrame);
        for (int i = 15; i >= 0; --i) { writeToFloat(crcResult >> i & 1); }
        protectOutput->exit();
    }

    void run() override {}

private:
    std::queue<float> *output{nullptr};
    CriticalSection *protectOutput;

    std::vector<float> preamble;
    std::deque<float> sync;
    std::vector<float> decode;
};

#endif//WRITER_H
