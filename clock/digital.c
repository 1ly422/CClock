#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_syswm.h>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 350


typedef unsigned char u8;
typedef float f32;
typedef uint32_t u32;

#define FPS 24.0
#define DELTA_TIME 1 / FPS

struct tm get_tm() {
    time_t currentTime;
    time(&currentTime);
    // Convert the current time to a local time structure
    struct tm localTime = {0};
    localtime_s(&localTime, &currentTime);

    return localTime;
}


// Makes a window transparent by setting a transparency color.
bool MakeWindowTransparent(SDL_Window* window, COLORREF colorKey) {
    // Get window handle (https://stackoverflow.com/a/24118145/3357935)
    SDL_SysWMinfo wmInfo;
	// Initialize wmInfo
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hWnd = wmInfo.info.win.window;

    // Change window type to layered (https://stackoverflow.com/a/3970218/3357935)
    SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);

    // Set transparency color
    return SetLayeredWindowAttributes(hWnd, colorKey, 0, LWA_COLORKEY);
}

SDL_HitTestResult MyHitTestCallback(SDL_Window* win, const SDL_Point* area, void* data) {
    // For simplicity, we'll consider the whole window draggable
    return SDL_HITTEST_DRAGGABLE;
}

void render_text(SDL_Renderer* renderer, TTF_Font* font, const char* dateStr, int x, int y) {
    SDL_Surface* surface = TTF_RenderText_Solid(font, dateStr, (SDL_Color) { 255, 255, 255, 255 });
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    int textWidth, textHeight;
    SDL_QueryTexture(texture, NULL, NULL, &textWidth, &textHeight);

    const SDL_Rect textDestRect = {
        .x = x,
        .y = y,
        .w = textWidth,
        .h = textHeight,
    };

    SDL_RenderCopy(renderer, texture, NULL, &textDestRect);

    SDL_DestroyTexture(texture);
	SDL_FreeSurface(surface);
}

void render_digit_str(SDL_Renderer* renderer, TTF_Font* font, const char* text, int* x, int* y) {

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

#define SET_ID 1
#define EXIT_ID 2

int show_context_menu(SDL_Window* window, int x, int y) {
    //Create the popup MENU
	HMENU hpopupMenu = CreatePopupMenu();
    //Insert wanted options here
    /*
	InsertMenuA(hpopupMenu, 0, MF_BYPOSITION | MF_STRING,
        SET_ID, "Set");
    InsertMenuA(hpopupMenu, 0, MF_BYPOSITION | MF_STRING,
        EXIT_ID, "Exit");
    */
    AppendMenuA(hpopupMenu, MF_STRING, SET_ID, "Set Time");
    AppendMenuA(hpopupMenu, MF_STRING, EXIT_ID, "Exit");

    //Get the window HWND from SDL_Window
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_GetWindowWMInfo(window, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;

	SetForegroundWindow(hwnd);

	POINT point = { .x = x, .y = y };
    ClientToScreen(hwnd, &point);
    const int itemSelected = TrackPopupMenu(hpopupMenu,
        TPM_BOTTOMALIGN | TPM_LEFTALIGN,
        point.x, point.y, 0, hwnd, NULL);

    // Clean up
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
    //SDL_SetWindowHitTest(window, MyHitTestCallback, NULL);

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
    

    const char* placeholder = "00:00:00";
    SDL_Surface* ttfSurface = TTF_RenderText_Solid(font512,
        placeholder, (SDL_Color){ 13, 13, 13 });

    int textWidth, textHeight;
    if (TTF_SizeText(font512, placeholder, &textWidth, &textHeight) < 0) {
        printf("Could not retrieve text size\n");
        return 1;
    }

    SDL_Texture* placeholderTex = SDL_CreateTextureFromSurface(renderer, ttfSurface);

    int texW = 0;
    int texH = 0;
    SDL_QueryTexture(placeholderTex, NULL, NULL, &texW, &texH);

    int winW, winH;
    SDL_GetWindowSize(window, &winW, &winH);
    SDL_Rect ttfDestRect = {
    	(winW - textWidth) / 2,
        (winH - textHeight) / 2,
    	texW,
    	texH };

    const char* dayName[] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

    const char* monthName[] = { "January", "February", "March", "April", "May", "June", "July",
                     "August", "September", "October", "November", "December" };

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
            else if (e.type == SDL_SYSWMEVENT) {
	            if (e.syswm.msg->msg.win.msg == WM_COMMAND) {
	            	if (LOWORD(e.syswm.msg->msg.win.wParam) == EXIT_ID) {
                        printf("Exit\n");
	            		isRunning = false;
	            	}
                    else if (LOWORD(e.syswm.msg->msg.win.wParam) == SET_ID) {
                        printf("Set time:: TODO\n");
                    }
	            }
            }
            else if (e.type == SDL_QUIT) {
                isRunning = false;
            }
        }
        // Set the draw color to red
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);        // Create a rectangle for the square
       
        const struct tm tm = get_tm();
        const int hour = tm.tm_hour;
        const int min = tm.tm_min;
        const int sec = tm.tm_sec;
        

        char title[80];
        sprintf_s(title, 80,"%d%d:%d%d:%d%d - CClock", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);
    	SDL_SetWindowTitle(window, title);
        
        // Clear the screen
        SDL_RenderClear(renderer);
        //SDL_RenderCopy(renderer, placeholderTex, NULL, &ttfDestRect);
        
        char timeStr[80];
        sprintf_s(timeStr, 80,"%d%d:%d%d:%d%d", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);

#if 0
        int xPen = ttfDestRect.x;
        int yPen = ttfDestRect.y;
        render_digit(renderer, font512, hour/10, &xPen, &yPen);
        render_digit(renderer, font512, hour % 10, &xPen, &yPen);
        render_digit_str(renderer, font512, " ", &xPen, &yPen);
        render_digit(renderer, font512, -1, &xPen, &yPen);
        render_digit_str(renderer, font512, " ", &xPen, &yPen);
        render_digit(renderer, font512, min / 10, &xPen, &yPen);
        render_digit(renderer, font512, min % 10, &xPen, &yPen);
        render_digit_str(renderer, font512, " ", &xPen, &yPen);
        render_digit(renderer, font512, -1, &xPen, &yPen);
        render_digit_str(renderer, font512, " ", &xPen, &yPen);
        render_digit(renderer, font512, sec / 10, &xPen, &yPen);
        render_digit(renderer, font512, sec % 10, &xPen, &yPen);
#else
    	render_text(renderer, font512, timeStr, ttfDestRect.x, ttfDestRect.y);
#endif    


        char dateStr[80];
        sprintf_s(dateStr, 80, "%s %d %s %d", dayName[tm.tm_wday], tm.tm_mday, monthName[tm.tm_mon], 1900 + tm.tm_year);
        render_text(renderer, font64, dateStr, ttfDestRect.x + 15, ttfDestRect.y - 40);
        
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
