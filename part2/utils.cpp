#include "utils.h"

std::vector<float> linspace(float min, float max, int n) {
    std::vector<float> result;
    result.reserve(n);
    int it = 0;
    for (auto interval = (max - min) / (float) (n - 1); it < n - 1; it++) { result.push_back(min + (float) it * interval); }
    result.push_back(max);
    return result;
}

std::vector<float> cumtrapz(std::vector<float> t, std::vector<float> f) {
    //	assert(t.size() == f.size());
    size_t size = t.size();
    std::vector<float> result;
    result.reserve(size);
    float total = 0.0;
    float last = *t.begin();
    result.push_back(0);
    for (int k = 0; k < size - 1; ++k) {
        float d;
        float s_tmp;
        d = t[k + 1] - t[k];
        t[k] = d;
        s_tmp = f[k + 1];
        total += d * ((last + s_tmp) / 2.0f);
        last = s_tmp;
        result.push_back(total);
    }
    return result;
}

unsigned int crc(const std::vector<bool> &source) {
    static unsigned char sourceString[10000];
    int stringLength = ((int) source.size() - 1) / 8 + 1;
    for (int i = 0; i < stringLength; ++i) {
        unsigned q = 0;
        for (int j = 0; j < 8 && i * 8 + j < source.size(); ++j) q |= (int) source[i * 8 + j] << j;
        sourceString[i] = q;
    }
    boost::crc_16_type crc;
    crc.process_bytes(sourceString, stringLength);
    return crc.checksum();
}

bool crcCheck(std::vector<bool> source) {
    unsigned int check = 0;
    for (int i = 0; i < 16; ++i) {
        check |= *source.rbegin() << i;
        source.pop_back();
    }
    return check == crc(source);
}

std::vector<float> smooth(const std::vector<float> &y, size_t span) {
    if (span == 0) { return y; }
    auto size = y.size();
    auto smoothSize = ((span - 1) | 1);
    auto result = std::vector<float>{};
    result.reserve(size);
    for (size_t pos = 0; pos < size; ++pos) {
        auto maxDistToEdge = std::min({smoothSize / 2, pos, size - pos - 1});
        result.push_back(y[pos]);
        for (auto i = 1; i <= maxDistToEdge; ++i) {
            result[pos] += y[pos - i];
            result[pos] += y[pos + i];
        }
        result[pos] /= (float) (maxDistToEdge * 2 + 1);
    }
    return result;
}