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
    CycleOrder,
    Quit
};

Action poll_action();
