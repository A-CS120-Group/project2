#pragma once

#include <boost/crc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>

#define PI acosf(-1)
#define LENGTH_OF_ONE_BIT 6// Must be a number in 1/2/3/4/5/6/8/10
#define MTU 512
#define LENGTH_PREAMBLE 48
#define LENGTH_SEQ 16
#define LENGTH_LEN 16
#define LENGTH_CRC 32
#define MAX_LENGTH_BODY (MTU - LENGTH_PREAMBLE - LENGTH_SEQ - LENGTH_LEN - LENGTH_CRC)

/* Structure of a frame
 * PREAMBLE
 * SEQ      frame counter if > 0, else ACK counter < 0, else end signal;
 * LEN      the length of BODY;
 * BODY
 * CRC
 */

constexpr bool preamble[LENGTH_PREAMBLE]
        {1, 0, 1, 0, 1, 0, 1, 0,
         1, 0, 1, 0, 1, 0, 1, 0,
         1, 0, 1, 0, 1, 0, 1, 0,
         1, 0, 1, 0, 1, 0, 1, 0,
         1, 0, 1, 0, 1, 0, 1, 0,
         1, 0, 1, 0, 1, 0, 1, 1,
        };

class FrameType {
public:
    FrameType() = delete;

    FrameType(size_t sizeOfFrame, size_t numSEQ);

    FrameType(std::vector<bool> &value, size_t numSEQ) : frame(value), seq(numSEQ) {}

    [[nodiscard]] unsigned int crc() const;

    [[nodiscard]] size_t size() const;

    std::vector<bool> frame;
    size_t seq;
};

unsigned int crc(const std::vector<bool> &source);

bool crcCheck(std::vector<bool> source);
