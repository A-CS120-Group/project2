#pragma once

#include <boost/crc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>

#define PI acosf(-1)

std::vector<float> linspace(float min, float max, int n);

std::vector<float> cumtrapz(std::vector<float> t, std::vector<float> f);

unsigned int crc(const std::vector<bool> &source);

bool crcCheck(std::vector<bool> source);


std::vector<float> smooth(const std::vector<float> &y, size_t span);