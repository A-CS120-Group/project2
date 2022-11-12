#pragma once

#include <algorithm>
#include <boost/crc.hpp>
#include <chrono>
#include <iostream>
#include <vector>

#define LENGTH_OF_ONE_BIT 4
#define MTU 60
#define LENGTH_PREAMBLE 3
#define LENGTH_SEQ 2
#define LENGTH_LEN 2
#define LENGTH_CRC 4
#define MAX_LENGTH_BODY (MTU - LENGTH_PREAMBLE - LENGTH_SEQ - LENGTH_LEN - LENGTH_CRC)

#define SLIDING_WINDOW_SIZE 8
#define SLIDING_WINDOW_TIMEOUT 0.2
#define PREAMBLE_THRESHOLD 0.3f
#define NOISY_THRESHOLD 0.01f

unsigned int crc32(const char *src, size_t srcSize);

int judgeBit(float signal1, float signal2);

template<class T>
[[nodiscard]] std::string inString(T object) {
    return {(const char *) &object, sizeof(T)};
}

/* Structure of a frame
 * PREAMBLE
 * LEN      the length of BODY; Len = 0: ACK
 * SEQ      +x: Node1 counter , -x: Node2 counter, 0: end signal;
 * BODY
 * CRC
 */
constexpr char preamble[LENGTH_PREAMBLE]{0x77, 0x77, 0x7f};

class FrameType {
public:
    unsigned short len = 0;
    short seq = 0;
    char body[MAX_LENGTH_BODY]{};

    FrameType() = default;

    FrameType(decltype(len) numLen, decltype(seq) numSeq, const char *bodySrc) :
            len(numLen), seq(numSeq) {
        memcpy(body, bodySrc, len);
    }

    [[nodiscard]] std::string wholeString() const {
        std::string ret = inString(len) + inString(seq) + std::string(body, len);
        return ret;
    }

    [[nodiscard]] unsigned int crc() const {
        auto str = wholeString();
        return crc32(str.c_str(), str.size());
    }
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
    int resendTimes = 3;
};