#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <math.h>

#define SDL_MAIN_HANDLED
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

enum CClockMode {
    CCLOCK_CLOCK,
    CCLOCK_TIMER,
    CCLOCK_CHRONO,
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

enum {
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
    EXIT_ID,
};

int show_context_menu(SDL_Window* window, int x, int y) {
    //Create the popup MENU
    HMENU hpopupMenu = CreatePopupMenu();
    HMENU hSubMenu = CreatePopupMenu();
    //Insert wanted options here
    /*
    InsertMenuA(hpopupMenu, 0, MF_BYPOSITION | MF_STRING,
        CHRONO_MODE_ID, "Set");
    InsertMenuA(hpopupMenu, 0, MF_BYPOSITION | MF_STRING,
        EXIT_ID, "Exit");
    */
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

int main(int argc, char** argv) {

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("SDL failed to initialise: %s\n", SDL_GetError());
        return 1;
    }
    /* Creates a SDL window */
    SDL_Window* window = SDL_CreateWindow("CClock", /* Title of the SDL window */
        SDL_WINDOWPOS_CENTERED, /* Position x of the window */
        SDL_WINDOWPOS_CENTERED, /* Position y of the window */
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


    if (TTF_Init() < 0) {
        printf("Could not init SDL_ttf\n");
        return 1;
    }


    //use the appropriate size depending on the window size
    //TTF_Font* font512 = TTF_OpenFont("digital.ttf", 512);
    TTF_Font* font512 = TTF_OpenFont("digital-mono.ttf", 256);
    TTF_Font* font64 = TTF_OpenFont("digital-mono.ttf", 48);
    if (!font512 || !font64) {
        printf("Could not load font\n");
        return 1;
    }

    //TODO:: Refactor this code into a func maybe
    //
    const char* placeholder = "00:00:00";
    SDL_Surface* ttfSurface = TTF_RenderText_Solid(font512,
        placeholder, (SDL_Color) { 13, 13, 13 });

    int textWidth, textHeight;
    if (TTF_SizeText(font512, placeholder, &textWidth, &textHeight) < 0) {
        printf("Could not retrieve text size\n");
        return 1;
    }
    SDL_Texture* placeholderTex = SDL_CreateTextureFromSurface(renderer, ttfSurface);

    float clockScale = 1.f;


    textWidth *= clockScale;
    textHeight *= clockScale;

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
    //

    const char* dayName[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

    const char* monthName[] = { "January", "February", "March", "April", "May", "June", "July",
                     "August", "September", "October", "November", "December" };

    enum CClockMode mode = CCLOCK_CLOCK;
    struct tm chronoTargetTm = get_tm();

    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
    while (isRunning) {

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
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN) {
                if (e.button.button == SDL_BUTTON_RIGHT) {
                    int x, y;
                    SDL_GetMouseState(&x, &y);
                    const bool itemSelected = show_context_menu(window, x, y);
                    if (!itemSelected) {
                        fprintf(stderr, "Context menu failed to show\n");
                    }
                }
            }
            else if (e.type == SDL_MOUSEWHEEL) {
                if (e.wheel.y > 0) // scroll up
                {
                    if ((clockScale += .1f) > 1.5) clockScale = 1.5f;
                    // Put code for handling "scroll up" here!
                }
                else if (e.wheel.y < 0) // scroll down
                {
                    if ((clockScale -= .1f) < 0.5f) clockScale = 0.5f;
                    // Put code for handling "scroll down" here!
                }

                //update vars tht depends on clockScale
                if (TTF_SizeText(font512, placeholder, &textWidth, &textHeight) < 0) {
                    printf("Could not retrieve text size\n");
                    return 1;
                }
                textWidth *= clockScale;
                textHeight *= clockScale;

                int winW, winH;
                SDL_GetWindowSize(window, &winW, &winH);
                ttfDestRect = (SDL_Rect){
                    (winW - textWidth) / 2,
                    (winH - textHeight) / 2,
                    textWidth,
                    textHeight
                };
            }
            else if (e.type == SDL_SYSWMEVENT) {
                if (e.syswm.msg->msg.win.msg == WM_COMMAND) {
                    switch (LOWORD(e.syswm.msg->msg.win.wParam)) {
                    case EXIT_ID:
                        isRunning = false;
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

        char title[80];
        char timeStr[80];

        if (mode == CCLOCK_CLOCK) {
            const struct tm tm = get_tm();
            const int hour = tm.tm_hour;
            const int min = tm.tm_min;
            const int sec = tm.tm_sec;
            SDL_Color clockColor = (SDL_Color){ 255, 255, 255, 255 };

            sprintf_s(title, 80, "%d%d:%d%d:%d%d - CClock", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);
            SDL_SetWindowTitle(window, title);

            sprintf_s(timeStr, 80, "%d%d:%d%d:%d%d", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);

            char dateStr[80];
            sprintf_s(dateStr, 80, "%s %d %s %d", dayName[tm.tm_wday], tm.tm_mday, monthName[tm.tm_mon], 1900 + tm.tm_year);

            render_text(renderer, font64, dateStr, ttfDestRect.x + 15, ttfDestRect.y - 40, clockScale, clockColor);
            render_text(renderer, font512, timeStr, ttfDestRect.x, ttfDestRect.y, clockScale, clockColor);

        }
        else if (mode == CCLOCK_CHRONO) {
            struct tm currentTm = get_tm();
            double diff = get_tm_diff(&currentTm, &chronoTargetTm);
            SDL_Color chronoColor = (SDL_Color){ 255, 255, 255, 255 };
            if (diff <= 0) {
                chronoColor = (SDL_Color){ 255, 87, 51, 255 };
                diff = 0;
            }
            const int hour = (int)diff / 3600;
            const int min = ((int)diff % 3600) / 60;
            const int sec = (int)diff % 60;

            sprintf_s(title, 80, "%d%d:%d%d:%d%d - CClock (Timer Mode)", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);
            SDL_SetWindowTitle(window, title);

            sprintf_s(timeStr, 80, "%d%d:%d%d:%d%d", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);
            render_text(renderer, font512, timeStr, ttfDestRect.x, ttfDestRect.y, clockScale, chronoColor);

            const char* dateStr = "Timer Mode: ";
            render_text(renderer, font64, dateStr, ttfDestRect.x + 15, ttfDestRect.y - 40, clockScale, chronoColor);

        }


        // Add window transparency (Magenta will be see-through)
        MakeWindowTransparent(window, RGB(0, 0, 0));

        // Update the screen
        SDL_RenderPresent(renderer);

        SDL_Delay((u32)floor(DELTA_TIME * 1000.0));
    }

    SDL_DestroyTexture(placeholderTex);
    SDL_FreeSurface(ttfSurface);

    TTF_CloseFont(font512);
    TTF_CloseFont(font64);

    TTF_Quit();

    /* Frees memory */
    SDL_DestroyWindow(window);

    /* Shuts down all SDL subsystems */
    SDL_Quit();
    return 0;
}
