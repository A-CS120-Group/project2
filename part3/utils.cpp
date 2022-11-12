#include "utils.h"

unsigned int crc32(const char *src, size_t srcSize) {
    boost::crc_32_type crc;
    crc.process_bytes(src, srcSize);
    return crc.checksum();
}

int judgeBit(float signal1, float signal2) {
    if (signal1 > 0 && signal2 < 0 && signal1 - signal2 > PREAMBLE_THRESHOLD)
        return 1;
    else if (signal2 > 0 && signal1 < 0 && signal2 - signal1 > PREAMBLE_THRESHOLD)
        return 0;
    else return -1;
};