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


    explicit Reader(std::queue<float> *bufferIn, CriticalSection *lockInput, std::queue<bool> *bufferOut, CriticalSection *lockOutput)
        : Thread("Reader"), input(bufferIn), output(bufferOut), protectInput(lockInput), protectOutput(lockOutput) {
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

    ~Reader() override {
        this->signalThreadShouldExit();
    }

    void run() override {
        assert(input != nullptr);
        assert(output != nullptr);
        assert(protectInput != nullptr);
        assert(protectOutput != nullptr);
        while (!threadShouldExit()) {
            protectInput->enter();
            if (!input->empty()) {// Not fully utilized yet

                float nextValue = input->front();
                input->pop();
                protectInput->exit();

                power = power * (63.0f / 64.0f) + nextValue * nextValue / 64.0f;
                if (state == 0) {
                    sync.pop_front();
                    sync.push_back(nextValue);
                    auto syncPower = static_cast<float>(std::inner_product(sync.begin(), sync.end(), preamble.begin(), 0.0f) / 100.0f);
                    if (syncPower > power * 2 && syncPower > syncPower_localMax && syncPower > 0.05f) {
                        syncPower_localMax = syncPower;
                        start_index = count;
                    } else if (count - start_index > 110 && start_index != -1) {
                        syncPower_localMax = 0;
                        state = 1;
                        //decode = std::vector<float>(inputBuffer.begin() + start_index + 1, inputBuffer.begin() + i + 1); // copy the last elements of sync
                        decode = std::vector<float>(sync.end() - 110 - 1, sync.end());
                        std::cout << "Header found" << std::endl;
                        sync = std::deque<float>(240, 0);
                    }
                } else {
                    decode.push_back(nextValue);
                    if (decode.size() == LENGTH_OF_ONE_BIT * (BITS_PER_FRAME + 8)) {
                        protectOutput->enter();
                        for (int q = LENGTH_OF_ONE_BIT * 8; q < LENGTH_OF_ONE_BIT * (BITS_PER_FRAME + 8); q += LENGTH_OF_ONE_BIT) {
                            auto accumulation = std::accumulate(decode.begin() + q, decode.begin() + q + LENGTH_OF_ONE_BIT, 0.0f);
                            if (accumulation > 0) {// Please do not make it short, we may change its logic here.
                                output->push(true);
                            } else {
                                output->push(false);
                            }
                        }
                        protectOutput->exit();
                        start_index = -1;
                        decode.clear();
                        state = 0;
                    }
                }
                ++count;
            } else {
                protectInput->exit();
            }
        }
    }

private:
    std::queue<float> *input{nullptr};
    std::queue<bool> *output{nullptr};
    CriticalSection *protectInput;
    CriticalSection *protectOutput;

    std::vector<float> preamble;
    int count{0};
    float power = 0;
    int start_index = -1;
    std::deque<float> sync;
    std::vector<float> decode;
    float syncPower_localMax = 0;
    int state = 0;// 0 sync; 1 decode
};

#endif//READER_H
