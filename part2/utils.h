#pragma once

#include <algorithm>
#include <boost/crc.hpp>
#include <chrono>
#include <iostream>
#include <vector>

#define LENGTH_OF_ONE_BIT 4// Must be a number in 1/2/3/4/5/6/8/10
#define MTU 512
#define LENGTH_PREAMBLE 48
#define LENGTH_SEQ 16
#define LENGTH_LEN 16
#define LENGTH_CRC 32
#define MAX_LENGTH_BODY (MTU - LENGTH_PREAMBLE - LENGTH_SEQ - LENGTH_LEN - LENGTH_CRC)
#define SLIDING_WINDOW_SIZE 16
#define SLIDING_WINDOW_TIMEOUT 0.15

/* Structure of a frame
 * PREAMBLE
 * LEN      the length of BODY;
 * SEQ      +x: frame counter , -x: ACK counter, 0: end signal;
 * BODY
 * CRC
 */

constexpr bool preamble[LENGTH_PREAMBLE]{
        1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 0, 1, 0,
        1, 0, 1, 0, 1, 0, 1, 1,
};

class FrameType {
public:
    FrameType() = delete;

    FrameType(size_t sizeOfFrame, int numSEQ);

    FrameType(std::vector<bool> &value, int numSEQ) : frame(value), seq(numSEQ) {}

    [[nodiscard]] unsigned int crc() const;

    [[nodiscard]] size_t size() const;

    std::vector<bool> frame;
    int seq;
};

using std::chrono::steady_clock;

class MyTimer {
public:
    std::chrono::time_point<steady_clock> start;

    MyTimer() : start(steady_clock::now()) {}

    void restart() { start = steady_clock::now(); }

    [[nodiscard]] double duration() const {
        auto now = steady_clock::now();
        return std::chrono::duration<double>(now - start).count();
    }
};

struct FrameWaitingInfo {
    bool receiveACK = false;
    MyTimer timer;
    double waitingTime = 0;
    int resendTimes = 3;
};

unsigned int crc(const std::vector<bool> &source);
