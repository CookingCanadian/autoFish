#include <raylib.h>
#include <iostream>
#include <string>
#include <cctype>
#include <algorithm>

#include "./resources/zain_black.h"
#include "./resources/zain_regular.h"

using namespace std;

Color HexToColor(const string& hex) {
    if (hex.length() != 7 && hex.length() != 9 || hex[0] != '#') return BLACK;
    const char* h = hex.c_str() + 1;
    auto hexVal = [](char c) -> int { return isdigit(c) ? c - '0' : toupper(c) - 'A' + 10; };
    Color color;
    color.r = static_cast<unsigned char>((hexVal(h[0]) << 4) | hexVal(h[1]));
    color.g = static_cast<unsigned char>((hexVal(h[2]) << 4) | hexVal(h[3]));
    color.b = static_cast<unsigned char>((hexVal(h[4]) << 4) | hexVal(h[5]));
    color.a = (hex.length() == 9) ? static_cast<unsigned char>((hexVal(h[6]) << 4) | hexVal(h[7])) : 255;
    return color;
}

float Clamp(float value, float min, float max) {
    return (value < min) ? min : (value > max) ? max : value;
}

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
    const float VIDEO_HEIGHT = 600.0f; 

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "autoFish");
    SetTargetFPS(60);

    Font zainBlack = LoadFontFromMemory(".ttf", zain_black_data, zain_black_data_size, 100, nullptr, 0);
    Font zainRegular = LoadFontFromMemory(".ttf", zain_regular_data, zain_regular_data_size, 100, nullptr, 0);
    if (zainBlack.texture.id == 0 || zainRegular.texture.id == 0) {
        std::cerr << "ERROR: font loading incomplete" << std::endl;
        CloseWindow();
        return 1;
    }
    SetTextureFilter(zainBlack.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(zainRegular.texture, TEXTURE_FILTER_BILINEAR);

    RenderTexture2D structureTexture = LoadRenderTexture(NATIVE_WIDTH, NATIVE_HEIGHT);
    SetTextureFilter(structureTexture.texture, TEXTURE_FILTER_BILINEAR);
    BeginTextureMode(structureTexture);
        ClearBackground(HexToColor(BACKGROUND_HEX));      
        DrawRectangle(0, 0, 2400, 96, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){864, 96}, 40, 45, 180, 32, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){1536, 96}, 40, 0, 135, 32, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){832, 176}, 80, 180, 360, 32, HexToColor(BACKGROUND_HEX));
        DrawCircleSector((Vector2){1568, 176}, 80, 180, 360, 32, HexToColor(BACKGROUND_HEX));

        // Draw video placeholder
        DrawRectangle(0, 114, 2400, (int)VIDEO_HEIGHT, DARKGRAY);

        DrawRectangleGradientV(0, 96, 2400, 100, HexToColor(BACKGROUND_HEX), HexToColor("#1B1E2400"));
        DrawRectangle(960, 96, 480, 80, HexToColor(PRIMARY_HEX));
        DrawRectanglePro((Rectangle){960, 136, 96, 80}, (Vector2){96, 40}, 45, HexToColor(PRIMARY_HEX));
        DrawRectanglePro((Rectangle){1440, 136, 96, 80}, (Vector2){96, 40}, 135, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){1440, 136}, 40, 45, 90, 32, HexToColor(PRIMARY_HEX));
        DrawCircleSector((Vector2){960, 136}, 40, 90, 135, 32, HexToColor(PRIMARY_HEX));

        DrawTextEx(zainBlack, "autoFish", (Vector2){40, 0}, 100, 1.0f, WHITE);
    EndTextureMode();

    RenderTexture2D windowTexture = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
    SetTextureFilter(windowTexture.texture, TEXTURE_FILTER_BILINEAR);
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

        // Window interaction logic
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            float scale = windowSize.x / (float)WINDOW_WIDTH;
            Rectangle handle = {windowSize.x - (float)HANDLE_SIZE, windowSize.y - (float)HANDLE_SIZE, (float)HANDLE_SIZE, (float)HANDLE_SIZE};
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
        
        // Window resizing logic
        if (draggingResize && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float targetWidth = Clamp(mousePositionInWindow.x, MIN_WIDTH, MAX_WIDTH);
            float targetHeight = targetWidth * ((float)WINDOW_HEIGHT / WINDOW_WIDTH);
            windowSize = {targetWidth, targetHeight};
            SetWindowSize((int)windowSize.x, (int)windowSize.y);
            UnloadRenderTexture(windowTexture);
            windowTexture = LoadRenderTexture((int)windowSize.x, (int)windowSize.y);
            SetTextureFilter(windowTexture.texture, TEXTURE_FILTER_BILINEAR);
        }
        
        // Window dragging logic
        if (draggingWindow && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            SetWindowPosition((int)(absoluteMousePosition.x - dragOffsetToWindow.x), (int)(absoluteMousePosition.y - dragOffsetToWindow.y));
        }

        // Rendering
        float scale = windowSize.x / (float)WINDOW_WIDTH;
        float topBarHeight = TOP_BAR_HEIGHT * scale;
        float videoHeight = VIDEO_HEIGHT * scale;

        BeginTextureMode(windowTexture);
            ClearBackground(BLACK);            
    
            Rectangle srcUI = {0.0f, 0.0f, (float)structureTexture.texture.width, -(float)structureTexture.texture.height};
            Rectangle dstUI = {0, 0, windowSize.x, windowSize.y};
            DrawTexturePro(structureTexture.texture, srcUI, dstUI, (Vector2){0, 0}, 0.0f, WHITE);            
         
            DrawRectangle((int)(windowSize.x - HANDLE_SIZE), (int)(windowSize.y - HANDLE_SIZE), HANDLE_SIZE, HANDLE_SIZE, GRAY);
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            DrawRectangleRounded((Rectangle){0, 0, windowSize.x, windowSize.y}, CORNER_RADIUS * scale / 2.0f, 16, BLACK);
            DrawTexturePro(windowTexture.texture, (Rectangle){0, 0, (float)windowTexture.texture.width, -(float)windowTexture.texture.height}, (Rectangle){0, 0, windowSize.x, windowSize.y}, (Vector2){0, 0}, 0.0f, WHITE);
            DrawTextEx(zainRegular, TextFormat("FPS: %i", GetFPS()), (Vector2){10, 50}, 20 * scale, 1.0f, GREEN);
        EndDrawing();
    }

    UnloadFont(zainBlack);
    UnloadFont(zainRegular);
    UnloadRenderTexture(structureTexture);
    UnloadRenderTexture(windowTexture);
    CloseWindow();
    return 0;
}