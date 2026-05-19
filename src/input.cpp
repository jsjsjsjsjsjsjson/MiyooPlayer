#include "input.hpp"

Action poll_action() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            return Action::Quit;
        }
        if (e.type != SDL_KEYDOWN) {
            continue;
        }
#ifdef MIYOO_USE_SDL2
        SDL_Keycode k = e.key.keysym.sym;
#else
        SDLKey k = e.key.keysym.sym;
#endif
        switch (k) {
        case SDLK_UP: return Action::Up;
        case SDLK_DOWN: return Action::Down;
        case SDLK_LEFT: return Action::Left;
        case SDLK_RIGHT: return Action::Right;

        // Miyoo mapping reported by the target device:
        // SELECT=Escape, START=Return, RESET=Right Ctrl,
        // A=Left Alt, B=Left Ctrl, X=Left Shift, Y=Space,
        // L1=Tab, L2=PageUp, R1=Backspace, R2=PageDown.
        case SDLK_RETURN:
            return Action::Start;
        case SDLK_LALT:
        case SDLK_SPACE: return Action::Confirm;

        case SDLK_LCTRL:
            return Action::Back;
        case SDLK_BACKSPACE:
            return Action::R1;

        case SDLK_ESCAPE:
        case SDLK_m:
            return Action::Menu;
        case SDLK_TAB:
            return Action::L1;

        case SDLK_RCTRL:
        case SDLK_q:
            return Action::Quit;

        case SDLK_LSHIFT:
            return Action::CycleOrder;
        case SDLK_p: return Action::Pause;

        case SDLK_a:
            return Action::SeekBack;
        case SDLK_PAGEUP:
            return Action::L2;
        case SDLK_PAGEDOWN:
        case SDLK_d:
            return Action::SeekForward;

        case SDLK_MINUS:
        case SDLK_s: return Action::VolDown;
        case SDLK_EQUALS:
        case SDLK_w: return Action::VolUp;
        default: break;
        }
    }
    return Action::None;
}
