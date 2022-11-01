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


    explicit Writer(std::queue<float> *bufferOut, CriticalSection *lockOutput)
        : Thread("Writer"), output(bufferOut), protectOutput(lockOutput) {
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

    ~Writer() override {
        this->signalThreadShouldExit();
    }

    void send(std::vector<bool> MacFrame) {
        // This function should be fast enough.
    }

    void run() override {

    }

private:
    std::queue<float> *output{nullptr};
    CriticalSection *protectOutput;

    std::vector<float> preamble;
    std::deque<float> sync;
    std::vector<float> decode;
};

#endif//WRITER_H
