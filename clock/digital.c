#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#if !(_MSC_VER && !__INTEL_COMPILER)
#define SDL_MAIN_HANDLED //gcc need this line smhw, it basically undef main macro defined by SDL
#endif

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_syswm.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 350

typedef unsigned char u8;
typedef float f32;
typedef uint32_t u32;

#define FPS 24.0
#define DELTA_TIME 1 / FPS

//https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html

//TODO:: Add an .ini file to save CClock pos, scale, shadowEffect
//MAYBE:: Make it compatible with Linux/MacoOS ????
//TODO:: Maybe when we change the scale of the CClock, we may use different fontSizes instead of full scaling for ex: if default is 256px and scale == .5, we can use 128px

enum CClockMode {
    CCLOCK_CLOCK,
    CCLOCK_TIMER,
    CCLOCK_CHRONO,
};

enum CClockContextMenuId {
    CLOCK_MODE_ID = 1,
    CHRONO_MODE_10s_ID,
    CHRONO_MODE_10M_ID,
    CHRONO_MODE_15M_ID,
    CHRONO_MODE_30M_ID,
    CHRONO_MODE_1H_ID,
    CHRONO_MODE_2H_ID,
    CHRONO_MODE_3H_ID,
    CHRONO_MODE_4H_ID,
    CHRONO_MODE_5H_ID,
    SHADOW_ID,
    EXIT_ID,
};

struct tm get_tm() {
    time_t currentTime;
    time(&currentTime);
    // Convert the current time to a local time structure
    struct tm localTime = { 0 };
    localtime_s(&localTime, &currentTime);

    return localTime;
}

struct tm get_tm_chrono(unsigned long offsetBySeconds) {
    struct tm tm = get_tm();
    tm.tm_sec += offsetBySeconds;
    time_t futureTime = mktime(&tm);

    // Convert the current time to a local time structure
    struct tm localTime = { 0 };
    localtime_s(&localTime, &futureTime);

    return localTime;
}

// return time difference in seconds
double get_tm_diff(struct tm* lhs, struct tm* rhs) {

    time_t t1 = mktime(lhs);
    time_t t2 = mktime(rhs);

    double diff = difftime(t2, t1);

    return diff;
}

HWND get_hwnd(SDL_Window* window) {
    // Get window handle (https://stackoverflow.com/a/24118145/3357935)
    SDL_SysWMinfo wmInfo;
    // Initialize wmInfo
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hWnd = wmInfo.info.win.window;

    return hWnd;
}

// Makes a window transparent by setting a transparency color.
bool MakeWindowTransparent(SDL_Window* window, COLORREF colorKey) {

    HWND hWnd = get_hwnd(window);
    // Change window type to layered (https://stackoverflow.com/a/3970218/3357935)
    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

    // Set transparency color
    return SetLayeredWindowAttributes(hWnd, colorKey, 0, LWA_COLORKEY);
}

SDL_HitTestResult MyHitTestCallback(SDL_Window* win, const SDL_Point* point, void* data) {
    // We'll consider the horz upper part of the window draggable and the bottom part normal
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    SDL_Rect rect = { .x = 0, .y = 0, .w = w, .h = h * .5 };
    if (SDL_PointInRect(point, &rect)) {
        SDL_Log("HIT-TEST: DRAGGABLE\n");
        return SDL_HITTEST_DRAGGABLE;
    }
    SDL_Log("HIT-TEST: NORMAL\n");
    return SDL_HITTEST_NORMAL;
}

static void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* dateStr, int x, int y, float scale, SDL_Color color) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, dateStr, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    int textWidth, textHeight;
    SDL_QueryTexture(texture, NULL, NULL, &textWidth, &textHeight);

    const SDL_Rect textDestRect = {
        .x = x,
        .y = y,
        .w = textWidth * scale,
        .h = textHeight * scale,
    };

    SDL_RenderCopy(renderer, texture, NULL, &textDestRect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void render_digit_str(SDL_Renderer* renderer, TTF_Font* font, const char* text, int* x, int* y) {

    SDL_Surface* surface = TTF_RenderText_Solid(font, text, (SDL_Color) { 255, 255, 255, 255 });
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);


    int textWidth, textHeight;
    SDL_QueryTexture(texture, NULL, NULL, &textWidth, &textHeight);
    SDL_Rect textDestRect = {
        .x = *x,
        .y = *y,
        .w = textWidth,
        .h = textHeight,
    };


    int widthDigit;
    TTF_SizeText(font, "0", &widthDigit, NULL);

    //if its a 1 we place the digit on the right side to align it with other digits
    if (text[0] == '1') {
        textDestRect.x = *x + widthDigit - textWidth;
    }
    if (isdigit(text[0])) {
        *x += widthDigit;
    }
    else {
        *x += textWidth;
    }

    SDL_RenderCopy(renderer, texture, NULL, &textDestRect);

    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void render_digit(SDL_Renderer* renderer, TTF_Font* font, int digit, int* x, int* y) {
    char text[2];
    if (digit >= 0 && digit <= 9) {
        sprintf_s(text, 2, "%d", digit);
    }
    else {
        sprintf_s(text, 2, ":");
    }
    render_digit_str(renderer, font, text, x, y);
}

int show_context_menu(SDL_Window* window, int x, int y, bool isShadowEnabled) {
    //Create the popup MENU
    HMENU hpopupMenu = CreatePopupMenu();
    HMENU hSubMenu = CreatePopupMenu();
    //Insert wanted options here
    //we can use AppendMenuA or InsertMenuA
    AppendMenuA(hpopupMenu, MF_STRING, CLOCK_MODE_ID, "Clock Mode");
    AppendMenuA(hpopupMenu, MF_POPUP, (UINT_PTR)hSubMenu, "Chrono Mode");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_10s_ID, "10s");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_10M_ID, "10min");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_15M_ID, "15min");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_30M_ID, "30min");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_1H_ID, "1h");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_2H_ID, "2h");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_3H_ID, "3h");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_4H_ID, "4h");
    AppendMenuA(hSubMenu, MF_STRING, CHRONO_MODE_5H_ID, "5h");

    AppendMenuA(hpopupMenu, isShadowEnabled ? MF_CHECKED: MF_UNCHECKED, SHADOW_ID, "Shadow");
    AppendMenuA(hpopupMenu, MF_STRING, EXIT_ID, "Exit");

    //Get the window HWND from SDL_Window
    HWND hwnd = get_hwnd(window);

    SetForegroundWindow(hwnd);

    POINT point = { .x = x, .y = y };
    ClientToScreen(hwnd, &point);
    const int itemSelected = TrackPopupMenu(hpopupMenu,
        TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        point.x, point.y, 0, hwnd, NULL);

    // Clean up
    DestroyMenu(hSubMenu);
    DestroyMenu(hpopupMenu);

    return itemSelected;
}

static SDL_Rect get_clock_position(SDL_Window* window, int textWidth, int textHeight) {
    
    int winW, winH;
    SDL_GetWindowSize(window, &winW, &winH);
    SDL_Rect ttfDestRect = {
        (winW - textWidth) / 2,
        (winH - textHeight) / 2,
        textWidth,
        textHeight
    };
    printf("win size: %dx%d\n", winW, winH);
    printf("fnt size: %dx%d\n", textWidth, textHeight);

    return ttfDestRect;
}

static void get_text_size(TTF_Font* font, const char* text, float scale, int* textWidth, int* textHeight) {
    if (TTF_SizeText(font, text, textWidth, textHeight) < 0) {
        fprintf(stderr, "Could not retrieve text size\n");
        exit(EXIT_FAILURE);
    }
    *textWidth *= scale;
    *textHeight *= scale;
}

static void get_clock_text_size(TTF_Font* font, float scale, int* textWidth, int* textHeight) {
    const char* placeholder = "00:00:00";
    get_text_size(font, placeholder, scale, textWidth, textHeight);
}

typedef struct {
    int winX;
    int winY;
    f32 clockScale;
    int shadowEffect;
} CClockConfig;

static int exists(const char* fname) {
    FILE* file;
    if (fopen_s(&file, fname, "r") == 0) //aka if it succeeded
    {
        fclose(file);
        return 1;
    }
    return 0;
}

void write_ini(const char* iniFileName, const CClockConfig* conf) {
    FILE* f = NULL;
    fopen_s(&f, iniFileName, "w");
    if (f != NULL) {
        fprintf(f, "x=%d\n", conf->winX);
        fprintf(f, "y=%d\n", conf->winY);
        fprintf(f, "clockScale=%f\n", conf->clockScale);
        fprintf(f, "shadow=%d\n", conf->shadowEffect);
        fclose(f);
    }
}

#define MAX_LINE_LENGTH 100

void read_ini(const char* iniFileName, CClockConfig* conf) {
    FILE* f = NULL;
    fopen_s(&f, iniFileName, "r");

    if (f != NULL) {
        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "x=") != NULL) sscanf_s(line, "x=%d", &conf->winX);
            else if (strstr(line, "y=") != NULL)            sscanf_s(line, "y=%d", &conf->winY);
            else if (strstr(line, "clockScale=") != NULL)   sscanf_s(line, "clockScale=%f", &conf->clockScale);
            else if (strstr(line, "shadow=") != NULL)       sscanf_s(line, "shadow=%d", &conf->shadowEffect);
        }

        fclose(f);
    }
}


int main(int argc, char** argv) {

    const char* iniFileName = "CClock.ini";
    //before creating the window we check if the .ini file exist and or create it
    CClockConfig config = {
        .winX = SDL_WINDOWPOS_CENTERED,
        .winY = SDL_WINDOWPOS_CENTERED,
        .clockScale = 1.f,
        .shadowEffect = true
    };

    if (exists(iniFileName)) {
        read_ini(iniFileName, &config);
    }
    else {
        write_ini(iniFileName, &config);
    }

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        fprintf(stderr, "SDL failed to initialise: %s\n", SDL_GetError());
        return 1;
    }
    /* Creates a SDL window */
    SDL_Window* window = SDL_CreateWindow("CClock", /* Title of the SDL window */
        config.winX, /* Position x of the window */
        config.winY, /* Position y of the window */
        WINDOW_WIDTH, /* Width of the window in pixels */
        WINDOW_HEIGHT, /* Height of the window in pixels */
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS); /* Additional flag(s) */

    if (!window) {
        printf("SDL window failed to initialise: %s\n", SDL_GetError());
        return 1;
    }
    // Set the hit-test callback
    if (SDL_SetWindowHitTest(window, MyHitTestCallback, NULL) < 0) {
        printf("Error with SDL_SetWindowHitTest: %s", SDL_GetError());
    }

    bool isRunning = true;
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    if (!renderer) {
        printf("SDL renderer failed to initialise: %s\n", SDL_GetError());
        return 1;
    }

    // Add window transparency (Black will be see-through)
    MakeWindowTransparent(window, RGB(0, 0, 0));

#ifdef FEATURE_HOTKEY_SUPPORT
#define HOTKEY_LCTRLT 1
    if (RegisterHotKey(get_hwnd(window), HOTKEY_LCTRLT, MOD_CONTROL | MOD_NOREPEAT, (UINT)'T') < 0) {
        exit(-1);
    }
#endif

    if (TTF_Init() < 0) {
        printf("Could not init SDL_ttf\n");
        return 1;
    }


    //use the appropriate size depending on the window size
    //TTF_Font* font512 = TTF_OpenFont("digital.ttf", 512);
    TTF_Font* font256 = TTF_OpenFont("digital-mono.ttf", 256);
    TTF_Font* font64 = TTF_OpenFont("digital-mono.ttf", 48);
    if (!font256 || !font64) {
        printf("Could not load font\n");
        return 1;
    }
    
    int textWidth, textHeight;
    
    get_clock_text_size(font256, config.clockScale, &textWidth, &textHeight);
    
    SDL_Rect ttfDestRect = get_clock_position(window, textWidth, textHeight);

    const char* dayName[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

    const char* monthName[] = { "January", "February", "March", "April", "May", "June", "July",
                     "August", "September", "October", "November", "December" };

    enum CClockMode mode = CCLOCK_CLOCK;
    struct tm chronoTargetTm = get_tm();

    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    while (isRunning) {

        int xx, yy;
        SDL_GetWindowPosition(window, &xx, &yy);
        printf("(%d,%d)\n", xx, yy);

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    isRunning = false;
                }
            }
            else if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    ttfDestRect.x = (e.window.data1 - textWidth) / 2;
                    ttfDestRect.y = (e.window.data2 - textHeight) / 2;
                }
                else if (e.window.event == SDL_WINDOWEVENT_MOVED) {
                    config.winX = e.window.data1;
                    config.winY = e.window.data2;
                }
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_RIGHT) {
                    int x, y;
                    SDL_GetMouseState(&x, &y);
                    const bool itemSelected = show_context_menu(window, x, y, config.shadowEffect);
                    if (!itemSelected) {
                        fprintf(stderr, "Context menu failed to show\n");
                    }
                }
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                if (e.wheel.y > 0) // scroll up
                {
                    if ((config.clockScale += .1f) > 1.5) config.clockScale = 1.5f;
                    // Put code for handling "scroll up" here!
                }
                else if (e.wheel.y < 0) // scroll down
                {
                    if ((config.clockScale -= .1f) < 0.5f) config.clockScale = 0.5f;
                    // Put code for handling "scroll down" here!
                }

                get_clock_text_size(font256, config.clockScale, &textWidth, &textHeight);
                ttfDestRect = get_clock_position(window, textWidth, textHeight);
            }
            else if (e.type == SDL_SYSWMEVENT) {
#ifdef FEATURE_HOTKEY_SUPPORT
                if (e.syswm.msg->msg.win.msg == WM_HOTKEY) {
                    if (e.syswm.msg->msg.win.wParam == HOTKEY_LCTRLT) {
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(10);
                    }
                }
                else 
#endif
                if (e.syswm.msg->msg.win.msg == WM_COMMAND) {
                    switch (LOWORD(e.syswm.msg->msg.win.wParam)) {
                    case EXIT_ID:
                        isRunning = false;
                        break;
                    case SHADOW_ID:
                        config.shadowEffect = !config.shadowEffect;
                        break;
                    case CHRONO_MODE_10s_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(10);
                        break;
                    case CHRONO_MODE_10M_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(60 * 10);
                        break;
                    case CHRONO_MODE_15M_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(60 * 15);
                        break;
                    case CHRONO_MODE_30M_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(60 * 30);
                        break;
                    case CHRONO_MODE_1H_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(3600);
                        break;
                    case CHRONO_MODE_2H_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(2 * 3600);
                        break;
                    case CHRONO_MODE_3H_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(3 * 3600);
                        break;
                    case CHRONO_MODE_4H_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(4 * 3600);
                        break;
                    case CHRONO_MODE_5H_ID:
                        mode = CCLOCK_CHRONO;
                        chronoTargetTm = get_tm_chrono(5 * 3600);
                        break;
                    case CLOCK_MODE_ID:
                        mode = CCLOCK_CLOCK;
                        break;
                    }
                }
            }
            else if (e.type == SDL_QUIT) {
                isRunning = false;
            }
        }

        // Set the draw color to red
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);        // Create a rectangle for the square
        // Clear the screen
        SDL_RenderClear(renderer);
        //SDL_RenderCopy(renderer, placeholderTex, NULL, &ttfDestRect);

        char windowTitle[80];
        char timeStr[80];
        char dateStr[80];

        const f32 shadowOffset = 4.f;
        const f32 shadowDateOffset = shadowOffset * .5f;
        const SDL_Color shadowColor = (SDL_Color){ 1, 1, 1, 255 };
        SDL_Color clockColor = (SDL_Color){ 245, 245, 245, 255 };

        if (mode == CCLOCK_CLOCK) {
            const struct tm tm = get_tm();
            const int hour = tm.tm_hour;
            const int min = tm.tm_min;
            const int sec = tm.tm_sec;

            clockColor = (SDL_Color){ 245, 245, 245, 255 };
            sprintf_s(windowTitle, 80, "%d%d:%d%d:%d%d - CClock", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);
            SDL_SetWindowTitle(window, windowTitle);

            sprintf_s(timeStr, 80, "%d%d:%d%d:%d%d", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);
            sprintf_s(dateStr, 80, "%s %d %s %d", dayName[tm.tm_wday], tm.tm_mday, monthName[tm.tm_mon], 1900 + tm.tm_year);

        }
        else if (mode == CCLOCK_CHRONO) {
            struct tm currentTm = get_tm();
            double diff = get_tm_diff(&currentTm, &chronoTargetTm);
            if (diff <= 0) {
                clockColor = (SDL_Color){ 255, 87, 51, 255 };
                diff = 0;
            }
            const int hour = (int)diff / 3600;
            const int min = ((int)diff % 3600) / 60;
            const int sec = (int)diff % 60;

            sprintf_s(windowTitle, 80, "%d%d:%d%d:%d%d - CClock (Timer Mode)", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);
            SDL_SetWindowTitle(window, windowTitle);

            strcpy_s(dateStr, 13, "Timer Mode: ");
            sprintf_s(timeStr, 80, "%d%d:%d%d:%d%d", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);

        }

        if (config.shadowEffect) {
            render_text(renderer, font64, dateStr, ttfDestRect.x + 15 + shadowDateOffset, ttfDestRect.y - 40 + shadowDateOffset, config.clockScale, shadowColor);
            render_text(renderer, font256, timeStr, ttfDestRect.x + shadowOffset, ttfDestRect.y + shadowOffset, config.clockScale, shadowColor);
        }

        render_text(renderer, font64, dateStr, ttfDestRect.x + 15, ttfDestRect.y - 40, config.clockScale, clockColor);
        render_text(renderer, font256, timeStr, ttfDestRect.x, ttfDestRect.y, config.clockScale, clockColor);

        // Update the screen
        SDL_RenderPresent(renderer);

        SDL_Delay((u32)floor(DELTA_TIME * 1000.0));
    }

    write_ini(iniFileName, &config);

    TTF_CloseFont(font256);
    TTF_CloseFont(font64);

    TTF_Quit();

    /* Frees memory */
    SDL_DestroyWindow(window);

    /* Shuts down all SDL subsystems */
    SDL_Quit();
    return 0;
}
