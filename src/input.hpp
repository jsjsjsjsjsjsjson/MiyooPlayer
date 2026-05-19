#pragma once

#include "sdl_platform.hpp"

enum class Action {
    None,
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Start,
    Back,
    Pause,
    SeekBack,
    SeekForward,
    VolDown,
    VolUp,
    Menu,
    L1,
    L2,
    R1,
    CycleOrder,
    Quit
};

Action poll_action();
