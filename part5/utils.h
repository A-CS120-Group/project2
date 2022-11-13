#pragma once

#include <algorithm>
#include <boost/crc.hpp>
#include <chrono>
#include <iostream>
#include <vector>
#include <random>

using LENType = unsigned char;
using SEQType = char;

#define LENGTH_OF_ONE_BIT 4
#define MTU 60
#define LENGTH_PREAMBLE 3
#define LENGTH_LEN sizeof(LENType)
#define LENGTH_SEQ sizeof(SEQType)
#define LENGTH_CRC sizeof(unsigned int)
#define MAX_LENGTH_BODY (MTU - LENGTH_PREAMBLE - LENGTH_SEQ - LENGTH_LEN - LENGTH_CRC)

#define SLIDING_WINDOW_SIZE 3
#define SLIDING_WINDOW_TIMEOUT 0.5
#define PREAMBLE_THRESHOLD 0.3f
#define NOISY_THRESHOLD 0.01f

#define PERF_NUMBER_PACKETS 10000

unsigned int crc32(const char *src, size_t srcSize);

int judgeBit(float signal1, float signal2);

template<class T>
[[nodiscard]] std::string inString(T object) {
    return {(const char *) &object, sizeof(T)};
}

/* Structure of a frame
 * PREAMBLE
 * LEN      the length of BODY; Len = 0: ACK
 * SEQ      +x: Node1 frame, -x: Node2 frame;
 * BODY
 * CRC
 */
constexpr char preamble[LENGTH_PREAMBLE]{0x55, 0x55, 0x54};

class FrameType {
public:
    LENType len = 0;
    SEQType seq = 0;
    char body[MAX_LENGTH_BODY]{};

    FrameType() = default;

    FrameType(LENType numLen, SEQType numSeq, const char *bodySrc) :
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
    int resendTimes = 10;
};