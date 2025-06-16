#include <iostream>
#include <string>
#include <cctype>
#include <algorithm>

// Forward declare Windows types we need
typedef void* HWND;
typedef void* HDC;
typedef void* HBITMAP;
typedef struct tagRECT {
    long left;
    long top;
    long right;
    long bottom;
} RECT;
typedef struct tagBITMAPINFOHEADER {
    unsigned long biSize;
    long biWidth;
    long biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned long biCompression;
    unsigned long biSizeImage;
    long biXPelsPerMeter;
    long biYPelsPerMeter;
    unsigned long biClrUsed;
    unsigned long biClrImportant;
} BITMAPINFOHEADER;
typedef struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    unsigned long bmiColors[1];
} BITMAPINFO;

// Windows API function declarations
extern "C" {
    HWND FindWindowA(const char* lpClassName, const char* lpWindowName);
    int GetWindowRect(HWND hWnd, RECT* lpRect);
    HDC GetDC(HWND hWnd);
    int ReleaseDC(HWND hWnd, HDC hDC);
    HDC CreateCompatibleDC(HDC hdc);
    int DeleteDC(HDC hdc);
    HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy);
    void* SelectObject(HDC hdc, void* h);
    int DeleteObject(void* ho);
    int PrintWindow(HWND hwnd, HDC hdcBlt, unsigned int nFlags);
    int GetDIBits(HDC hdc, HBITMAP hbm, unsigned int start, unsigned int cLines, void* lpvBits, BITMAPINFO* lpbmi, unsigned int usage);
}

// Windows constants
#define PW_CLIENTONLY 0x00000001
#define BI_RGB 0
#define DIB_RGB_COLORS 0

// Now include raylib
#include <raylib.h>

#include "./resources/zain_black.h"
#include "./resources/zain_bold.h"
#include "./resources/zain_regular.h"

using namespace std;

Color HexToColor(const string& hex);
float Clamp(float value, float min, float max);

int main() {
    const int NATIVE_WIDTH = 2400;
    const int NATIVE_HEIGHT = 1200;
    const int WINDOW_WIDTH = 600;
    const int WINDOW_HEIGHT = 300;
    const string BACKGROUND_HEX = "#1B1E24";
    const string PRIMARY_HEX = "#272A33";
    const int HANDLE_SIZE = 20;
    const float MIN_WIDTH = 300.0f;
    const float MAX_WIDTH = 1920.0f;
    const float TOP_BAR_HEIGHT = 24.0f;
    const float CORNER_RADIUS = 10.0f;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "autoFish");
    SetTargetFPS(60);

    Font zainBlack = LoadFontFromMemory(".ttf", zain_black_data, zain_black_data_size, 100, nullptr, 0);
    Font zainBold = LoadFontFromMemory(".ttf", zain_bold_data, zain_bold_data_size, 100, nullptr, 0);
    Font zainRegular = LoadFontFromMemory(".ttf", zain_regular_data, zain_regular_data_size, 100, nullptr, 0);

    if (zainBlack.texture.id == 0 || zainBold.texture.id == 0 || zainRegular.texture.id == 0) {
        std::cerr << "ERROR: font loading incomplete" << std::endl;
        CloseWindow();
        return 1;
    }

    SetTextureFilter(zainBlack.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(zainBold.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(zainRegular.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D structureTexture = LoadRenderTexture(NATIVE_WIDTH, NATIVE_HEIGHT);
    SetTextureFilter(structureTexture.texture, TEXTURE_FILTER_BILINEAR);

    BeginTextureMode(structureTexture);
        ClearBackground(HexToColor(BACKGROUND_HEX));
        DrawRectangle(0, 0, 2400, 96, HexToColor(PRIMARY_HEX));
        DrawRectangle(960, 96, 480, 80, HexToColor(PRIMARY_HEX));
        DrawRectanglePro((Rectangle){960, 136, 96, 80}, (Vector2){96, 40}, 45, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){960, 136}, 40, 90, 135, 32, HexToColor(PRIMARY_HEX));
        DrawRectanglePro((Rectangle){1440, 136, 96, 80}, (Vector2){96, 40}, 135, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){1440, 136}, 40, 45, 90, 32, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){864, 96}, 40, 45, 180, 32, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){832, 176}, 80, 180, 360, 32, HexToColor(BACKGROUND_HEX));
        DrawCircleSector((Vector2){1536, 96}, 40, 0, 135, 32, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){1568, 176}, 80, 180, 360, 32, HexToColor(BACKGROUND_HEX));
        DrawTextEx(zainBlack, "autoFish", (Vector2){40, 0}, 100, 1.0f, WHITE);
    EndTextureMode();

    RenderTexture2D windowTexture = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
    SetTextureFilter(windowTexture.texture, TEXTURE_FILTER_BILINEAR);

    HWND robloxHwnd = FindWindowA(NULL, "Roblox");
    if (!robloxHwnd) {
        std::cerr << "ERROR: Roblox window not found" << std::endl;
        CloseWindow();
        return 1;
    }

    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = NULL;
    unsigned char* pixelBuffer = NULL;
    Texture2D robloxTexture = {0};
    Image robloxImage = {0};
    int lastWidth = 0, lastHeight = 0;

    Vector2 windowSize = {(float)WINDOW_WIDTH, (float)WINDOW_HEIGHT};
    bool draggingResize = false;
    bool draggingWindow = false;
    Vector2 dragOffsetToWindow = {0, 0};

    while (!WindowShouldClose()) {
        Vector2 mousePositionInWindow = GetMousePosition();
        Vector2 currentWindowPos = GetWindowPosition();
        Vector2 absoluteMousePosition = {
            currentWindowPos.x + mousePositionInWindow.x,
            currentWindowPos.y + mousePositionInWindow.y
        };

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            float scale = windowSize.x / (float)WINDOW_WIDTH;
            Rectangle handle = {
                windowSize.x - (float)HANDLE_SIZE,
                windowSize.y - (float)HANDLE_SIZE,
                (float)HANDLE_SIZE,
                (float)HANDLE_SIZE
            };
            Rectangle topBar = {0, 0, windowSize.x, TOP_BAR_HEIGHT * scale};

            if (CheckCollisionPointRec(mousePositionInWindow, handle)) {
                draggingResize = true;
            } else if (CheckCollisionPointRec(mousePositionInWindow, topBar)) {
                draggingWindow = true;
                dragOffsetToWindow.x = absoluteMousePosition.x - currentWindowPos.x;
                dragOffsetToWindow.y = absoluteMousePosition.y - currentWindowPos.y;
            }
        }

        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            draggingResize = false;
            draggingWindow = false;
        }

        if (draggingResize && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float targetWidth = Clamp(mousePositionInWindow.x, MIN_WIDTH, MAX_WIDTH);
            float targetHeight = targetWidth * ((float)WINDOW_HEIGHT / WINDOW_WIDTH);
            windowSize = {targetWidth, targetHeight};
            SetWindowSize((int)windowSize.x, (int)windowSize.y);
            UnloadRenderTexture(windowTexture);
            windowTexture = LoadRenderTexture((int)windowSize.x, (int)windowSize.y);
            SetTextureFilter(windowTexture.texture, TEXTURE_FILTER_BILINEAR);
        }

        if (draggingWindow && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            SetWindowPosition(
                (int)(absoluteMousePosition.x - dragOffsetToWindow.x),
                (int)(absoluteMousePosition.y - dragOffsetToWindow.y)
            );
        }

        RECT robloxRect;
        GetWindowRect(robloxHwnd, &robloxRect);
        int width = robloxRect.right - robloxRect.left;
        int height = robloxRect.bottom - robloxRect.top;

        if (width > 0 && height > 0) {
            if (width != lastWidth || height != lastHeight) {
                if (hBitmap) DeleteObject(hBitmap);
                if (pixelBuffer) delete[] pixelBuffer;
                if (robloxImage.data) UnloadImage(robloxImage);
                if (robloxTexture.id) UnloadTexture(robloxTexture);

                hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);
                pixelBuffer = new unsigned char[width * height * 4];
                robloxImage = GenImageColor(width, height, BLANK);
                robloxImage.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
                robloxImage.width = width;
                robloxImage.height = height;
                robloxTexture = LoadTextureFromImage(robloxImage);

                lastWidth = width;
                lastHeight = height;
            }

            SelectObject(hdcMem, hBitmap);
            PrintWindow(robloxHwnd, hdcMem, PW_CLIENTONLY);

            BITMAPINFO bmi = {0};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = width;
            bmi.bmiHeader.biHeight = -height;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            GetDIBits(hdcMem, hBitmap, 0, height, pixelBuffer, &bmi, DIB_RGB_COLORS);
            memcpy(robloxImage.data, pixelBuffer, width * height * 4);
            UpdateTexture(robloxTexture, robloxImage.data);

            float topBarHeight = TOP_BAR_HEIGHT * (windowSize.x / WINDOW_WIDTH);
            float videoHeight = windowSize.y - topBarHeight;
            float scaleX = windowSize.x / (float)width;
            float scaleY = videoHeight / (float)height;
            float scale = std::min(scaleX, scaleY);
            float scaledWidth = width * scale;
            float scaledHeight = height * scale;
            float offsetX = (windowSize.x - scaledWidth) / 2;

            BeginTextureMode(windowTexture);
                ClearBackground(BLACK);

                Rectangle src = {0, 0, (float)width, (float)height};
                Rectangle dst = {offsetX, topBarHeight, scaledWidth, scaledHeight};
                DrawTexturePro(robloxTexture, src, dst, (Vector2){0, 0}, 0.0f, WHITE);

                Rectangle srcUI = {0.0f, 0.0f, (float)structureTexture.texture.width, -(float)structureTexture.texture.height};
                Rectangle dstUI = {0, 0, windowSize.x, windowSize.y};
                DrawTexturePro(structureTexture.texture, srcUI, dstUI, (Vector2){0, 0}, 0.0f, WHITE);
                DrawRectangle(windowSize.x - (float)HANDLE_SIZE, windowSize.y - (float)HANDLE_SIZE, (float)HANDLE_SIZE, (float)HANDLE_SIZE, GRAY);
            EndTextureMode();
        }

        BeginDrawing();
            ClearBackground(BLACK);
            DrawRectangleRounded((Rectangle){0, 0, windowSize.x, windowSize.y}, CORNER_RADIUS / WINDOW_WIDTH * windowSize.x / 2.0f, 16, BLACK);
            DrawTexturePro(windowTexture.texture, (Rectangle){0, 0, (float)windowTexture.texture.width, -(float)windowTexture.texture.height}, (Rectangle){0, 0, windowSize.x, windowSize.y}, (Vector2){0, 0}, 0.0f, WHITE);
            DrawText(TextFormat("FPS: %i", GetFPS()), 10, 10, 20, GREEN);
        EndDrawing();
    }

    if (pixelBuffer) delete[] pixelBuffer;
    if (hBitmap) DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
    if (robloxImage.data) UnloadImage(robloxImage);
    if (robloxTexture.id) UnloadTexture(robloxTexture);
    UnloadFont(zainBlack);
    UnloadFont(zainBold);
    UnloadFont(zainRegular);
    UnloadRenderTexture(structureTexture);
    UnloadRenderTexture(windowTexture);
    CloseWindow();
    return 0;
}

unsigned char HexByte(const char* hex) {
    auto hexVal = [](char c) -> int {
        return isdigit(c) ? c - '0' : toupper(c) - 'A' + 10;
    };
    return static_cast<unsigned char>((hexVal(hex[0]) << 4) | hexVal(hex[1]));
}

Color HexToColor(const string& hex) {
    if (hex.length() != 7 && hex.length() != 9) return BLACK;
    if (hex[0] != '#') return BLACK;

    const char* h = hex.c_str() + 1;
    Color color;
    color.r = HexByte(h);
    color.g = HexByte(h + 2);
    color.b = HexByte(h + 4);
    color.a = (hex.length() == 9) ? HexByte(h + 6) : 255;
    return color;
}

float Clamp(float value, float min, float max) {
    return (value < min) ? min : (value > max) ? max : value;
}