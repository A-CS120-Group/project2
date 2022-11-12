#pragma once

#include <algorithm>
#include <boost/crc.hpp>
#include <chrono>
#include <iostream>
#include <vector>

#define LENGTH_OF_ONE_BIT 4
#define MTU 64
#define LENGTH_PREAMBLE 3
#define LENGTH_SEQ 2
#define LENGTH_LEN 2
#define LENGTH_CRC 4
#define MAX_LENGTH_BODY (MTU - LENGTH_PREAMBLE - LENGTH_SEQ - LENGTH_LEN - LENGTH_CRC)

#define SHIFT_LEN LENGTH_PREAMBLE
#define SHIFT_SEQ (SHIFT_LEN + LENGTH_LEN)
#define SHIFT_BODY (SHIFT_SEQ + LENGTH_SEQ)
#define SHIFT_CRC (SHIFT_BODY + len())
#define SHIFT_END (SHIFT_CRC + LENGTH_CRC)

#define SLIDING_WINDOW_SIZE 1
#define SLIDING_WINDOW_TIMEOUT 1
#define PREAMBLE_THRESHOLD 0.5f

/* Structure of a frame
 * PREAMBLE
 * LEN      the length of BODY; Len = 0: ACK
 * SEQ      +x: Node1 counter , -x: Node2 counter, 0: end signal;
 * BODY
 * CRC
 */

constexpr char preamble[LENGTH_PREAMBLE]{0x77, 0x77, 0x7f};

unsigned int crc32(const char *src, size_t srcSize);

int judgeBit(float signal1, float signal2);

class FrameType {
public:
    char data[MTU]{0x77, 0x77, 0x7f};
    FrameType() = delete;

    FrameType(unsigned short numLen, short numSeq, const char *bodySrc) {
        len() = numLen;
        seq() = numSeq;
        memcpy_s(body(), len(), bodySrc, len());
        crc() = crc32(data, LENGTH_PREAMBLE + LENGTH_LEN + LENGTH_SEQ + len());
    }

    [[nodiscard]] unsigned short &len() const { return *(unsigned short *) (data + SHIFT_LEN); }
    [[nodiscard]] short &seq() const { return *(short *) (data + SHIFT_SEQ); }
    [[nodiscard]] char *body() const { return (char *) (data + SHIFT_BODY); }
    [[nodiscard]] unsigned int &crc() const { return *(unsigned int *) (data + SHIFT_CRC); }
    [[nodiscard]] unsigned short size() const { return SHIFT_END; }
    [[nodiscard]] bool crcCheck() const {
        return crc() == crc32(data, SHIFT_CRC);
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