//
// Created by caoster on 2022/10/24.
//

#ifndef PROJECT2_PART1_READER_H
#define PROJECT2_PART1_READER_H

#include <JuceHeader.h>
#include <cassert>
#include <queue>

class Reader : Thread {
public:
    explicit Reader(std::vector<bool> *writeTo) : Thread("Reader"), target(writeTo) {
        writeTo->reserve(48000);// One second should be enough
    }

    void run() override {
        assert(target != nullptr);
        while (threadShouldExit()) {
            if (!target->empty()) {// Not fully utilized yet

                float power = 0;
                int start_index = -1;
                std::deque<float> sync(480, 0);
                std::vector<float> decode;
                float syncPower_localMax = 0;
                int state = 0;// 0 sync; 1 decode

                for (int i = 0; i < inputBuffer.size(); ++i) {
                    float cur = inputBuffer[i];
                    power = power * (63.0 / 64) + cur * cur / 64;
                    if (state == 0) {
                        sync.pop_front();
                        sync.push_back(cur);
                        float syncPower = std::inner_product(sync.begin(), sync.end(), preamble.begin(), 0.0) / 200.0;
                        if (syncPower > power * 2 && syncPower > syncPower_localMax && syncPower > 0.05) {
                            syncPower_localMax = syncPower;
                            start_index = i;
                        } else if (i - start_index > 200 && start_index != -1) {
                            syncPower_localMax = 0;
                            sync = std::deque<float>(480, 0);
                            state = 1;
                            decode = std::vector<float>(inputBuffer.begin() + start_index + 1, inputBuffer.begin() + i + 1);
                            std::cout << "Header found" << std::endl;
                        }
                    } else {
                        decode.push_back(cur);
                        if (decode.size() == 48 * 400) {
                            // TODO: 400 bits here
                        }
                    }
                }

            }
        }
    }

private:
    std::vector<bool> *target{nullptr};
};

#endif//PROJECT2_PART1_READER_H
