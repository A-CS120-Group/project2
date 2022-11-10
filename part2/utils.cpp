#include "utils.h"

FrameType::FrameType(size_t sizeOfFrame, int numSEQ) : seq(numSEQ) {
    frame.resize(sizeOfFrame);
}

unsigned int FrameType::crc() const {
    std::vector<bool> wholeFrame(LENGTH_SEQ + frame.size());
    for (int i = 0; i < LENGTH_SEQ; ++i) { wholeFrame[i] = seq >> i & 1; }
    for (int i = 0; i < frame.size(); ++i) { wholeFrame[i + LENGTH_SEQ] = frame[i]; }
    return ::crc(wholeFrame);
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
