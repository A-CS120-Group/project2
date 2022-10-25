//
// Created by caoster on 2022/10/24.
//

#ifndef PROJECT2_PART1_READER_H
#define PROJECT2_PART1_READER_H

#include <JuceHeader.h>
#include <cassert>
#include <queue>
#include "utils.h"

class Reader : Thread {
public:
    explicit Reader(std::queue<float> *writeTo) : Thread("Reader"), target(writeTo) {
        auto sampleRate = 48000;
        std::vector<float> t;
        t.reserve((size_t) sampleRate);
        for (int i = 0; i <= sampleRate; ++i) { t.push_back((float) i / (float) sampleRate); }

        auto f = linspace(2000, 10000, 240);
        auto f_temp = linspace(10000, 2000, 240);
        f.reserve(f.size() + f_temp.size());
        f.insert(std::end(f), std::begin(f_temp), std::end(f_temp));

        std::vector<float> x(t.begin(), t.begin() + 480);
        preamble = cumtrapz(x, f);
        for (float &i: preamble) { i = sin(2.0f * PI * i); }

        sync = std::deque<float>(480, 0);
    }

    void run() override {
        assert(target != nullptr);
        while (threadShouldExit()) {
            if (!target->empty()) {// Not fully utilized yet

                float nextValue = target->front();
                target->pop();

                power = power * (63.0f / 64.0f) + nextValue * nextValue / 64.0f;
                if (state == 0) {
                    sync.pop_front();
                    sync.push_back(nextValue);
                    auto syncPower = static_cast<float>(
                            std::inner_product(sync.begin(), sync.end(), preamble.begin(), 0.0f) / 200.0f);
                    if (syncPower > power * 2 && syncPower > syncPower_localMax && syncPower > 0.05f) {
                        syncPower_localMax = syncPower;
                        start_index = count;
                    } else if (count - start_index > 220 && start_index != -1) {
                        syncPower_localMax = 0;
                        state = 1;
//                            decode = std::vector<float>(inputBuffer.begin() + start_index + 1, inputBuffer.begin() + i + 1); // copy the last elements of sync
                        decode = std::vector<float>(sync.end() - 220 - 1, sync.end());
                        std::cout << "Header found" << std::endl;
                        sync = std::deque<float>(480, 0);
                    }
                } else {
                    decode.push_back(nextValue);
                    if (decode.size() == LENGTH_OF_ONE_BIT * 400) {
                        // TODO: 400 bits here
                    }
                }
                ++count;
            }
        }
    }

private:
    std::queue<float> *target{nullptr};
    std::vector<float> preamble;
    int count{0};

    float power = 0;
    int start_index = -1;

    std::deque<float> sync;
    std::vector<float> decode;
    float syncPower_localMax = 0;
    int state = 0;// 0 sync; 1 decode
};

#endif//PROJECT2_PART1_READER_H
