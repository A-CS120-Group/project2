#pragma once

#include <boost/crc.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <functional>
#include <cmath>

#define PI acosf(-1)
#define LENGTH_OF_ONE_BIT 6// Must be a number in 1/2/3/4/5/6/8/10

std::vector<float> linspace(float min, float max, int n);

std::vector<float> cumtrapz(std::vector<float> t, std::vector<float> f);

unsigned int crc(const std::vector<bool> &source);

bool crcCheck(std::vector<bool> source);


std::vector<float> smooth(const std::vector<float> &y, size_t span);