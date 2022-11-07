#include "utils.h"

FrameType::FrameType(size_t sizeOfFrame, size_t numSEQ) : seq(numSEQ) {
    assert(sizeOfFrame != 0);
    frame.resize(sizeOfFrame);
}

unsigned int FrameType::crc() const {
    return ::crc(frame);
}

size_t FrameType::size() const {
    return frame.size();
}

unsigned int crc(const std::vector<bool> &source) {
    static unsigned char sourceString[10000];
    int stringLength = ((int) source.size() - 1) / 8 + 1;
    for (int i = 0; i < stringLength; ++i) {
        unsigned q = 0;
        for (int j = 0; j < 8 && i * 8 + j < source.size(); ++j) q |= (int) source[i * 8 + j] << j;
        sourceString[i] = q;
    }
    boost::crc_32_type crc;
    crc.process_bytes(sourceString, stringLength);
    return crc.checksum();
}

bool crcCheck(std::vector<bool> source) {
    unsigned int check = 0;
    for (int i = 0; i < LENGTH_CRC; ++i) {
        check |= *source.rbegin() << i;
        source.pop_back();
    }
    return check == crc(source);
}
