#include <stdio.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>

#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_syswm.h>

#define WINDOW_WIDTH 1600
#define WINDOW_HEIGHT 350


typedef unsigned char u8;
typedef float f32;
typedef uint32_t u32;

#define FPS 60.0
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
    SDL_VERSION(&wmInfo.version);  // Initialize wmInfo
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

int main(int argc, char** argv) {
    
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("SDL failed to initialise: %s\n", SDL_GetError());
        return 1;
    }

    /* Creates a SDL window */
    SDL_Window* window = SDL_CreateWindow("Shift", /* Title of the SDL window */
        SDL_WINDOWPOS_CENTERED, /* Position x of the window */
        100, /* Position y of the window */
        WINDOW_WIDTH, /* Width of the window in pixels */
        WINDOW_HEIGHT, /* Height of the window in pixels */
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS); /* Additional flag(s) */
    
    if (!window) {
        printf("SDL window failed to initialise: %s\n", SDL_GetError());
        return 1;
    }
    // Set the hit-test callback
    SDL_SetWindowHitTest(window, MyHitTestCallback, NULL);

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
    TTF_Font* font512 = TTF_OpenFont("digital.ttf", 256);
    TTF_Font* font128 = TTF_OpenFont("digital.ttf", 128);
    

    const char* placeholder = "00 : 00 : 00";
    SDL_Surface* ttfSurface = TTF_RenderText_Solid(font512,
        placeholder, (SDL_Color){ 13, 213, 13 });

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

    int widthDigit;
    if (TTF_SizeText(font512, "0", NULL, &widthDigit) < 0) {
        printf("Could not retrieve text size\n");
        return 1;
    }
    
    while (isRunning) {
        
       SDL_Event e;
        while (SDL_PollEvent(&e)) {
	        if (e.type == SDL_KEYDOWN) {
		        if (e.key.keysym.sym == SDLK_ESCAPE) {
                    isRunning = false;
		        }
	        }
            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_RESIZED) {
                    ttfDestRect.x = (e.window.data1 - textWidth) / 2;
                    ttfDestRect.y = (e.window.data2 - textHeight) / 2;
                }
            }
            
            if (e.type == SDL_QUIT) {
                isRunning = false;
            }
        }
        // Set the draw color to red
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);        // Create a rectangle for the square
       
        const struct tm tm = get_tm();
        int hour = tm.tm_hour;
        int min = tm.tm_min;
        int sec = tm.tm_sec;
        

        char title[80];
        sprintf_s(title, 80,"%d%d:%d%d:%d%d - Shift", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);

    	SDL_SetWindowTitle(window, title);
        
        char timeStr[80];
        sprintf_s(timeStr, 80,"%d%d : %d%d : %d%d", hour / 10, hour % 10, min / 10, min % 10, sec / 10, sec % 10);
        

        int currentWinWidth;
        int currentWinHeight;
        SDL_GetWindowSize(window, &currentWinWidth, &currentWinHeight);

        // Clear the screen
        SDL_RenderClear(renderer);

        //SDL_RenderCopy(renderer, placeholderTex, NULL, &ttfDestRect);
        
        SDL_Surface* h1Surface = TTF_RenderText_Solid(font512, timeStr, (SDL_Color) { 255,255,255,255 });
        
        SDL_Texture* h1Tex = SDL_CreateTextureFromSurface(renderer, h1Surface);
        
        int timeW, timeH;
    	SDL_QueryTexture(h1Tex, NULL, NULL, &timeW, &timeH);

        SDL_Rect h1Rect = {
            .x = ttfDestRect.x,
            .y = ttfDestRect.y,
            .w = timeW,
            .h = timeH,
        };

    	SDL_RenderCopy(renderer, h1Tex, NULL, &h1Rect);
        

        // Add window transparency (Magenta will be see-through)
        MakeWindowTransparent(window, RGB(0, 0, 0));

        // Update the screen
        SDL_RenderPresent(renderer);

        SDL_Delay((u32)floor(DELTA_TIME * 1000.0));

        SDL_DestroyTexture(h1Tex);
		SDL_FreeSurface(h1Surface);
        
    }

    SDL_DestroyTexture(placeholderTex);
    SDL_FreeSurface(ttfSurface);

    TTF_CloseFont(font512);
    //TTF_CloseFont(font256);
    TTF_CloseFont(font128);

    TTF_Quit();

    /* Frees memory */
    SDL_DestroyWindow(window);

    /* Shuts down all SDL subsystems */
    SDL_Quit();
    return 0;
}