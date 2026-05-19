#ifdef MIYOO_USE_SDL2
#include <SDL2/SDL.h>
#else
#include <SDL/SDL.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

static const int SCREEN_W = 320;
static const int SCREEN_H = 240;

static Uint32 rgb(SDL_Surface* s, Uint8 r, Uint8 g, Uint8 b) {
    return SDL_MapRGB(s->format, r, g, b);
}

static void fill(SDL_Surface* s, int x, int y, int w, int h, Uint8 r, Uint8 g, Uint8 b) {
    SDL_Rect rc;
    rc.x = static_cast<Sint16>(x);
    rc.y = static_cast<Sint16>(y);
    rc.w = static_cast<Uint16>(std::max(0, w));
    rc.h = static_cast<Uint16>(std::max(0, h));
    SDL_FillRect(s, &rc, rgb(s, r, g, b));
}

static const char** glyph(char c) {
    static const char* blank[] = {"     ","     ","     ","     ","     ","     ","     "};
    static const char* box[]   = {"#####","#   #","# # #","#   #","# # #","#   #","#####"};
    static const char* dot[]   = {"     ","     ","     ","     ","     "," ##  "," ##  "};
    static const char* colon[] = {"     "," ##  "," ##  ","     "," ##  "," ##  ","     "};
    static const char* dash[]  = {"     ","     ","     ","#####","     ","     ","     "};
    static const char* slash[] = {"    #","   # ","   # ","  #  "," #   "," #   ","#    "};
    static const char* under[] = {"     ","     ","     ","     ","     ","     ","#####"};
    static const char* plus[]  = {"     ","  #  ","  #  ","#####","  #  ","  #  ","     "};
    static const char* zero[]  = {" ### ","#   #","#  ##","# # #","##  #","#   #"," ### "};
    static const char* one[]   = {"  #  "," ##  ","# #  ","  #  ","  #  ","  #  ","#####"};
    static const char* two[]   = {" ### ","#   #","    #","   # ","  #  "," #   ","#####"};
    static const char* three[] = {"#### ","    #","    #"," ### ","    #","    #","#### "};
    static const char* four[]  = {"#   #","#   #","#   #","#####","    #","    #","    #"};
    static const char* five[]  = {"#####","#    ","#    ","#### ","    #","    #","#### "};
    static const char* six[]   = {" ### ","#    ","#    ","#### ","#   #","#   #"," ### "};
    static const char* seven[] = {"#####","    #","   # ","  #  "," #   "," #   "," #   "};
    static const char* eight[] = {" ### ","#   #","#   #"," ### ","#   #","#   #"," ### "};
    static const char* nine[]  = {" ### ","#   #","#   #"," ####","    #","    #"," ### "};
    static const char* A[]={" ### ","#   #","#   #","#####","#   #","#   #","#   #"};
    static const char* B[]={"#### ","#   #","#   #","#### ","#   #","#   #","#### "};
    static const char* C[]={" ### ","#   #","#    ","#    ","#    ","#   #"," ### "};
    static const char* D[]={"#### ","#   #","#   #","#   #","#   #","#   #","#### "};
    static const char* E[]={"#####","#    ","#    ","#### ","#    ","#    ","#####"};
    static const char* F[]={"#####","#    ","#    ","#### ","#    ","#    ","#    "};
    static const char* G[]={" ### ","#   #","#    ","# ###","#   #","#   #"," ### "};
    static const char* H[]={"#   #","#   #","#   #","#####","#   #","#   #","#   #"};
    static const char* I[]={"#####","  #  ","  #  ","  #  ","  #  ","  #  ","#####"};
    static const char* J[]={"#####","   # ","   # ","   # ","   # ","#  # "," ##  "};
    static const char* K[]={"#   #","#  # ","# #  ","##   ","# #  ","#  # ","#   #"};
    static const char* L[]={"#    ","#    ","#    ","#    ","#    ","#    ","#####"};
    static const char* M[]={"#   #","## ##","# # #","#   #","#   #","#   #","#   #"};
    static const char* N[]={"#   #","##  #","# # #","#  ##","#   #","#   #","#   #"};
    static const char* O[]={" ### ","#   #","#   #","#   #","#   #","#   #"," ### "};
    static const char* P[]={"#### ","#   #","#   #","#### ","#    ","#    ","#    "};
    static const char* Q[]={" ### ","#   #","#   #","#   #","# # #","#  # "," ## #"};
    static const char* R[]={"#### ","#   #","#   #","#### ","# #  ","#  # ","#   #"};
    static const char* S[]={" ####","#    ","#    "," ### ","    #","    #","#### "};
    static const char* T[]={"#####","  #  ","  #  ","  #  ","  #  ","  #  ","  #  "};
    static const char* U[]={"#   #","#   #","#   #","#   #","#   #","#   #"," ### "};
    static const char* V[]={"#   #","#   #","#   #","#   #","#   #"," # # ","  #  "};
    static const char* W[]={"#   #","#   #","#   #","# # #","# # #","## ##","#   #"};
    static const char* X[]={"#   #","#   #"," # # ","  #  "," # # ","#   #","#   #"};
    static const char* Y[]={"#   #","#   #"," # # ","  #  ","  #  ","  #  ","  #  "};
    static const char* Z[]={"#####","    #","   # ","  #  "," #   ","#    ","#####"};
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    switch (c) {
    case ' ': return blank; case '.': return dot; case ':': return colon; case '-': return dash;
    case '/': return slash; case '_': return under; case '+': return plus;
    case '0': return zero; case '1': return one; case '2': return two; case '3': return three; case '4': return four;
    case '5': return five; case '6': return six; case '7': return seven; case '8': return eight; case '9': return nine;
    case 'A': return A; case 'B': return B; case 'C': return C; case 'D': return D; case 'E': return E; case 'F': return F;
    case 'G': return G; case 'H': return H; case 'I': return I; case 'J': return J; case 'K': return K; case 'L': return L;
    case 'M': return M; case 'N': return N; case 'O': return O; case 'P': return P; case 'Q': return Q; case 'R': return R;
    case 'S': return S; case 'T': return T; case 'U': return U; case 'V': return V; case 'W': return W; case 'X': return X;
    case 'Y': return Y; case 'Z': return Z;
    default: return box;
    }
}

static void text(SDL_Surface* s, int x, int y, const std::string& str, Uint8 r, Uint8 g, Uint8 b, int scale = 1) {
    Uint32 c = rgb(s, r, g, b);
    int px = x;
    for (char ch : str) {
        const char** gp = glyph(ch);
        for (int yy = 0; yy < 7; ++yy) {
            for (int xx = 0; xx < 5; ++xx) {
                if (gp[yy][xx] != ' ') {
                    SDL_Rect rc;
                    rc.x = static_cast<Sint16>(px + xx * scale);
                    rc.y = static_cast<Sint16>(y + yy * scale);
                    rc.w = static_cast<Uint16>(scale);
                    rc.h = static_cast<Uint16>(scale);
                    SDL_FillRect(s, &rc, c);
                }
            }
        }
        px += 6 * scale;
        if (px > SCREEN_W - 4) break;
    }
}

static void flip(
#ifdef MIYOO_USE_SDL2
    SDL_Window* window,
#endif
    SDL_Surface* screen) {
#ifdef MIYOO_USE_SDL2
    (void)screen;
    SDL_UpdateWindowSurface(window);
#else
    SDL_Flip(screen);
#endif
}

#ifndef MIYOO_USE_SDL2
static std::string sdl_key_name(SDLKey key) {
    const char* n = SDL_GetKeyName(key);
    return n ? n : "unknown";
}
#endif

int main() {
    setenv("SDL_NOMOUSE", "1", 1);
    setenv("SDL_MOUSEDRV", "dummy", 1);
#ifndef MIYOO_USE_SDL2
    setenv("SDL_VIDEODRIVER", "fbcon", 0);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_JOYSTICK) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

#ifdef MIYOO_USE_SDL2
    SDL_Window* window = SDL_CreateWindow("miyoo-keytest", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_Surface* screen = SDL_GetWindowSurface(window);
#else
    SDL_ShowCursor(SDL_DISABLE);
    SDL_Surface* screen = SDL_SetVideoMode(SCREEN_W, SCREEN_H, 16, SDL_SWSURFACE);
#endif
    if (!screen) {
        std::fprintf(stderr, "SDL video setup failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    std::deque<std::string> lines;
    lines.push_front("PRESS BUTTONS");
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            }
            if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                bool down = e.type == SDL_KEYDOWN;
#ifdef MIYOO_USE_SDL2
                SDL_Keycode sym = e.key.keysym.sym;
                SDL_Scancode scancode = e.key.keysym.scancode;
                int mod = static_cast<int>(e.key.keysym.mod);
                std::string name = SDL_GetKeyName(sym);
                char buf[96];
                std::snprintf(buf, sizeof(buf), "%s KEY %d SCAN %d MOD %d",
                              down ? "DOWN" : "UP", static_cast<int>(sym),
                              static_cast<int>(scancode), mod);
                std::fprintf(stderr, "%s NAME %s\n", buf, name.c_str());
#else
                SDLKey sym = e.key.keysym.sym;
                int scancode = static_cast<int>(e.key.keysym.scancode);
                int mod = static_cast<int>(e.key.keysym.mod);
                std::string name = sdl_key_name(sym);
                char buf[96];
                std::snprintf(buf, sizeof(buf), "%s KEY %d SCAN %d MOD %d",
                              down ? "DOWN" : "UP", static_cast<int>(sym), scancode, mod);
                std::fprintf(stderr, "%s NAME %s\n", buf, name.c_str());
#endif
                lines.push_front(std::string(buf));
                lines.push_front("NAME " + name);
                while (lines.size() > 12) lines.pop_back();

                if (down && (sym == SDLK_RCTRL || sym == SDLK_q)) {
                    running = false;
                }
            }
        }

        SDL_FillRect(screen, nullptr, rgb(screen, 7, 9, 12));
        fill(screen, 0, 0, SCREEN_W, 24, 24, 42, 50);
        text(screen, 6, 8, "MIYOO KEYTEST", 232, 240, 235);
        text(screen, 190, 8, "RESET EXIT", 184, 202, 202);
        text(screen, 8, 34, "WRITE DOWN KEY NUMBERS", 210, 220, 170);

        int y = 56;
        for (const std::string& line : lines) {
            text(screen, 8, y, line, 220, 220, 220);
            y += 15;
            if (y > 224) break;
        }

#ifdef MIYOO_USE_SDL2
        flip(window, screen);
#else
        flip(screen);
#endif
        SDL_Delay(16);
    }

#ifdef MIYOO_USE_SDL2
    SDL_DestroyWindow(window);
#endif
    SDL_Quit();
    return 0;
}
