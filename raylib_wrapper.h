#ifndef RAYLIB_WRAPPER_H
#define RAYLIB_WRAPPER_H

#if defined(_WIN32)
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor
    #undef DrawText
    #undef DrawTextEx
    #undef LoadImage

    #define Rectangle RaylibRectangle
    #define CloseWindow RaylibCloseWindow
    #define ShowCursor RaylibShowCursor
#endif

#include <raylib.h>

#if defined(_WIN32)
    #undef Rectangle
    #undef CloseWindow
    #undef ShowCursor
#endif

#endif 