#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>
#include <vector>

constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;
constexpr int UI_BAR_H = 24;

enum class PlaybackOrder {
    Sequential,
    ListLoop,
    RepeatOne,
    Shuffle
};

inline int clamp_int(int v, int lo, int hi) {
    return std::max(lo, std::min(v, hi));
}

float now_seconds();
