#ifdef _WIN32

typedef int WINBOOL;
typedef void* HWND;
typedef void* HDC;
typedef void* HBITMAP;
typedef void* HGDIOBJ;

typedef struct tagRECT {
    long left;
    long top;
    long right;
    long bottom;
} RECT;

typedef struct tagBITMAP {
    long bmType;
    long bmWidth;
    long bmHeight;
    long bmWidthBytes;
    unsigned short bmPlanes;
    unsigned short bmBitsPixel;
    void* bmBits;
} BITMAP;

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

// win32 api function declarations
extern "C" {
    HDC GetDC(HWND hWnd);
    int ReleaseDC(HWND hWnd, HDC hDC);
    HDC CreateCompatibleDC(HDC hdc);
    WINBOOL DeleteDC(HDC hdc);
    HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy);
    HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h);
    WINBOOL DeleteObject(HGDIOBJ ho);
    WINBOOL BitBlt(HDC hdcDest, int xDest, int yDest, int w, int h, HDC hdcSrc, int xSrc, int ySrc, unsigned long rop);
    int GetDIBits(HDC hdc, HBITMAP hbm, unsigned int start, unsigned int cLines, void* lpvBits, BITMAPINFO* lpbmi, unsigned int usage);
    WINBOOL GetObjectA(HGDIOBJ hgdiobj, int cbBuffer, void *lpvObject);
    int GetSystemMetrics(int nIndex);
    HWND FindWindowA(const char* lpClassName, const char* lpWindowName);
    WINBOOL GetClientRect(HWND hWnd, RECT* lpRect);
    WINBOOL IsWindow(HWND hWnd);
}

#define NULL                0L
#define SRCCOPY             0x00CC0020
#define BI_RGB              0
#define DIB_RGB_COLORS      0
#define SM_CXSCREEN         0
#define SM_CYSCREEN         1

#endif // WIN32

#include <raylib.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <string>
#include <cctype>
#include <algorithm>

#include "./resources/zain_black.h"
#include "./resources/zain_regular.h"

using namespace std;

// forward declarations
class ScreenCapture;
struct WindowsScreenCapture;

// function declarations
Image MatToRaylibImage(const cv::Mat& mat);
Color HexToColor(const string& hex);
float Clamp(float value, float min, float max);

// class declaration
class ScreenCapture {
private:
    void* impl; // pimpl pattern to hide platform-specific implementation
public:
    ScreenCapture();
    ~ScreenCapture();
    bool Initialize();
    cv::Mat CaptureScreen();
};

// main
int main() {
    const int NATIVE_WIDTH = 2400;
    const int NATIVE_HEIGHT = 1200;
    const int WINDOW_WIDTH = 600;
    const int WINDOW_HEIGHT = 300;
    const int MAX_FPS = 60;
    const int CLIENT_REFRESH_FRAME_WAIT = 300;
    const string BACKGROUND_HEX = "#1B1E24";
    const string PRIMARY_HEX = "#272A33";
    const int HANDLE_SIZE = 20;
    const float MIN_WIDTH = 300.0f;
    const float MAX_WIDTH = 1920.0f;
    const float TOP_BAR_HEIGHT = 24.0f;
    const float SLIDER_MIN_SCALE = 0.5f; 
    const float SLIDER_MAX_SCALE = 3.0f;
    const float SLIDER_TRACK_LENGTH = 800.0f;
    const float SLIDER_HANDLE_WIDTH = 10.0f;
    
    bool TOGGLE_PRESSED = false;

    static bool timerRunning = false;
    static bool wasTogglePressed = false;
    static float startTime = 0.0f;
    static float stopTime = 0.0f;


    // initialize screen capture
    ScreenCapture screenCap;
    if (!screenCap.Initialize()) {
        // continue instead of quitting
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "autoFish");
    SetTraceLogLevel(LOG_NONE);
    SetTargetFPS(MAX_FPS);

    // load embedded fonts
    Font zainBlack = LoadFontFromMemory(".ttf", zain_black_data, zain_black_data_size, 100, nullptr, 0);
    Font zainRegular = LoadFontFromMemory(".ttf", zain_regular_data, zain_regular_data_size, 100, nullptr, 0);
    if (zainBlack.texture.id == 0 || zainRegular.texture.id == 0) {
        CloseWindow();
        return 1;
    }
    SetTextureFilter(zainBlack.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(zainRegular.texture, TEXTURE_FILTER_BILINEAR);
    
    // screen capture texture
    Texture2D screenTexture = {};
    bool textureLoaded = false;

    RenderTexture2D windowTexture = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
    SetTextureFilter(windowTexture.texture, TEXTURE_FILTER_BILINEAR);
    Vector2 windowSize = {(float)WINDOW_WIDTH, (float)WINDOW_HEIGHT};
    bool draggingResize = false;
    bool draggingWindow = false;
    Vector2 dragOffsetToWindow = {0, 0};

    // slider variables
    float videoOffsetY = 0.0f;   
    float videoScale = 1.0f;    
    bool draggingLeftSlider = false;
    bool draggingRightSlider = false;

    int frameCount = 0;

    while (!WindowShouldClose()) {
        float structureScale = windowSize.x / (float)NATIVE_WIDTH;

        Vector2 mousePositionInWindow = GetMousePosition();
        Vector2 currentWindowPos = GetWindowPosition();
        Vector2 absoluteMousePosition = {
            currentWindowPos.x + mousePositionInWindow.x,
            currentWindowPos.y + mousePositionInWindow.y
        };  
             
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            float scale = windowSize.x / (float)WINDOW_WIDTH;
            Rectangle handle = {windowSize.x - (float)HANDLE_SIZE, windowSize.y - (float)HANDLE_SIZE, (float)HANDLE_SIZE, (float)HANDLE_SIZE};
            Rectangle topBar = {0, 0, windowSize.x, TOP_BAR_HEIGHT * scale};
            
            // left slider (vertical position control)
            float leftSliderTrackStart = 40 * structureScale;
            float movableLeftTrackLength = SLIDER_TRACK_LENGTH * structureScale - SLIDER_HANDLE_WIDTH * structureScale;
            float leftSliderProgress = (videoOffsetY + 1.0f) * 0.5f;
            float leftSliderVisualX = leftSliderTrackStart + leftSliderProgress * movableLeftTrackLength;
            
            Rectangle leftSliderHitbox = {
                leftSliderVisualX - 40,
                950 * structureScale - (40 * structureScale / 2),
                SLIDER_HANDLE_WIDTH * structureScale + 80,
                40 * structureScale
            };
            
            // right slider (scale control)
            float rightSliderTrackStart = 1560 * structureScale;
            float movableRightTrackLength = SLIDER_TRACK_LENGTH * structureScale - SLIDER_HANDLE_WIDTH * structureScale;
            float rightSliderProgress = (videoScale - SLIDER_MIN_SCALE) / (SLIDER_MAX_SCALE - SLIDER_MIN_SCALE);
            rightSliderProgress = Clamp(rightSliderProgress, 0.0f, 1.0f);
            float rightSliderVisualX = rightSliderTrackStart + rightSliderProgress * movableRightTrackLength;
            
            Rectangle rightSliderHitbox = {
                rightSliderVisualX - 40,
                950 * structureScale - (40 * structureScale / 2),
                SLIDER_HANDLE_WIDTH * structureScale + 80,
                40 * structureScale
            };
            
            if (CheckCollisionPointRec(mousePositionInWindow, leftSliderHitbox)) {
                draggingLeftSlider = true;
            } else if (CheckCollisionPointRec(mousePositionInWindow, rightSliderHitbox)) {
                draggingRightSlider = true;
            } else if (CheckCollisionPointRec(mousePositionInWindow, handle)) {
                draggingResize = true;
            } else if (CheckCollisionPointRec(mousePositionInWindow, topBar)) {
                draggingWindow = true;
                dragOffsetToWindow.x = absoluteMousePosition.x - currentWindowPos.x;
                dragOffsetToWindow.y = absoluteMousePosition.y - currentWindowPos.y;
            }

            if (CheckCollisionPointCircle(mousePositionInWindow, (Vector2){1200 * structureScale, 950 * structureScale}, 160.0f * structureScale)) {
                TOGGLE_PRESSED = !TOGGLE_PRESSED;
                
                if (TOGGLE_PRESSED != wasTogglePressed) {
                    if (TOGGLE_PRESSED) {
                      
                        timerRunning = true;
                        startTime = GetTime();
                    } else {                  
                        timerRunning = false;
                        stopTime = GetTime();
                    }
                    wasTogglePressed = TOGGLE_PRESSED;
                }
            }
        }
        
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            draggingResize = false;
            draggingWindow = false;
            draggingLeftSlider = false;
            draggingRightSlider = false;
        }
        
        // handle slider dragging
        if (draggingLeftSlider && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float leftTrackStart = 40 * structureScale;
            float movableTrackLength = SLIDER_TRACK_LENGTH * structureScale - SLIDER_HANDLE_WIDTH * structureScale;
            
            float progress = Clamp((mousePositionInWindow.x - leftTrackStart) / movableTrackLength, 0.0f, 1.0f);
            videoOffsetY = (progress * 2.0f) - 1.0f;
        }
        
        if (draggingRightSlider && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float rightTrackStart = 1560 * structureScale;
            float movableTrackLength = SLIDER_TRACK_LENGTH * structureScale - SLIDER_HANDLE_WIDTH * structureScale;

            float progress = Clamp((mousePositionInWindow.x - rightTrackStart) / movableTrackLength, 0.0f, 1.0f);
            videoScale = SLIDER_MIN_SCALE + (progress * (SLIDER_MAX_SCALE - SLIDER_MIN_SCALE));
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
            SetWindowPosition((int)(absoluteMousePosition.x - dragOffsetToWindow.x), (int)(absoluteMousePosition.y - dragOffsetToWindow.y));
        }

        // capture screen
        cv::Mat screenMat = screenCap.CaptureScreen();
        if (!screenMat.empty()) {
            Image screenImg = MatToRaylibImage(screenMat);
            if (screenImg.data) {
                if (textureLoaded) {
                    UnloadTexture(screenTexture);
                }
                screenTexture = LoadTextureFromImage(screenImg);
                textureLoaded = true;
                UnloadImage(screenImg);
            }
        } else {
            textureLoaded = false;
        }

        float scale = windowSize.x / (float)WINDOW_WIDTH;
        structureScale = windowSize.x / (float)NATIVE_WIDTH;

        BeginTextureMode(windowTexture);
            ClearBackground(BLACK);
            
            // draw background
            DrawRectanglePro((Rectangle){0, 0, windowSize.x, windowSize.y}, (Vector2){0, 0}, 0.0f, HexToColor(BACKGROUND_HEX));
            
            // draw top bar
            DrawRectanglePro((Rectangle){0, 0, windowSize.x, 96 * structureScale}, (Vector2){0, 0}, 0.0f, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){864 * structureScale, 96 * structureScale}, 40 * structureScale, 45, 180, 32, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){1536 * structureScale, 96 * structureScale}, 40 * structureScale, 0, 135, 32, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){832 * structureScale, 176 * structureScale}, 80 * structureScale, 180, 360, 32, HexToColor(BACKGROUND_HEX));
            DrawCircleSector((Vector2){1568 * structureScale, 176 * structureScale}, 80 * structureScale, 180, 360, 32, HexToColor(BACKGROUND_HEX));
            
            // draw captured video or error message
            float videoX = 0;
            float videoY = 114.0f * structureScale;
            float videoWidth = NATIVE_WIDTH * structureScale;
            float videoHeight = 600.0f * structureScale;
            
            BeginScissorMode((int)videoX, (int)videoY, (int)videoWidth, (int)videoHeight);  
            if (textureLoaded) {                
                float baseScaleX = videoWidth / screenTexture.width;
                float baseScaleY = videoHeight / screenTexture.height;
                float baseMinScale = fmaxf(baseScaleX, baseScaleY);
                float minEffectiveScale = baseMinScale;
                float maxEffectiveScale = SLIDER_MAX_SCALE * structureScale;
                float sliderProgress = (videoScale - SLIDER_MIN_SCALE) / (SLIDER_MAX_SCALE - SLIDER_MIN_SCALE);
                float actualScale = minEffectiveScale + sliderProgress * (maxEffectiveScale - minEffectiveScale);
                
                float scaledWidth = screenTexture.width * actualScale;
                float scaledHeight = screenTexture.height * actualScale;
                float centerX = videoX + (videoWidth - scaledWidth) * 0.5f;                
                float maxOffsetY = (scaledHeight - videoHeight) * 0.5f;                
                float clampedOffsetY = Clamp(videoOffsetY, -1.0f, 1.0f);

                if (maxOffsetY <= 0) {
                    clampedOffsetY = 0;
                }
                
                float actualOffsetY = clampedOffsetY * maxOffsetY;
                float centerY = videoY + (videoHeight - scaledHeight) * 0.5f - actualOffsetY;
                
                Rectangle videoSrc = {0.0f, 0.0f, (float)screenTexture.width, (float)screenTexture.height};
                Rectangle videoDst = {centerX, centerY, scaledWidth, scaledHeight};
                DrawTexturePro(screenTexture, videoSrc, videoDst, (Vector2){0, 0}, 0.0f, WHITE);
                DrawRectangleGradientV(0, 112 * structureScale, windowSize.x, 100 * structureScale, HexToColor(BACKGROUND_HEX), HexToColor("#1B1E2400"));
            } else {            
                const char* errorText = "roblox player not detected";
                float fontSize = 200 * structureScale;
                Vector2 textSize = MeasureTextEx(zainBlack, errorText, fontSize, 1.0f);
                float textX = videoX + (videoWidth - textSize.x) * 0.5f;
                float textY = videoY + (videoHeight - textSize.y) * 0.5f;
                DrawTextEx(zainBlack, errorText, (Vector2){textX, textY}, fontSize, 1.0f, HexToColor("#111417"));
                
                if (frameCount >= CLIENT_REFRESH_FRAME_WAIT) {
                    screenCap.Initialize();
                    frameCount = 0;
                }
                frameCount++;
            }
            EndScissorMode();
            
            // draw ui overlays
            
            DrawRectanglePro((Rectangle){960 * structureScale, 96 * structureScale, 480 * structureScale, 80 * structureScale}, (Vector2){0, 0}, 0.0f, HexToColor(PRIMARY_HEX));
            DrawRectanglePro((Rectangle){0, 714 * structureScale, windowSize.x, 10 * structureScale}, (Vector2){0, 0}, 0.0f, HexToColor(PRIMARY_HEX));
            DrawRectanglePro((Rectangle){960 * structureScale, 136 * structureScale, 96 * structureScale, 80 * structureScale}, (Vector2){96 * structureScale, 40 * structureScale}, 45, HexToColor(PRIMARY_HEX));
            DrawRectanglePro((Rectangle){1440 * structureScale, 136 * structureScale, 96 * structureScale, 80 * structureScale}, (Vector2){96 * structureScale, 40 * structureScale}, 135, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){1440 * structureScale, 136 * structureScale}, 40 * structureScale, 45, 90, 32, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){960 * structureScale, 136 * structureScale}, 40 * structureScale, 90, 135, 32, HexToColor(PRIMARY_HEX));
            DrawTextEx(zainBlack, "autoFish", (Vector2){40 * structureScale, 0}, 100 * structureScale, 1.0f, WHITE);
           
            // draw resize handle
            DrawRectangle((int)(windowSize.x - HANDLE_SIZE), (int)(windowSize.y - HANDLE_SIZE), HANDLE_SIZE, HANDLE_SIZE, HexToColor("#14171B"));
            
            // left slider track
            float leftTrackStart = 40 * structureScale;
            DrawRectanglePro((Rectangle){leftTrackStart, 950 * structureScale, SLIDER_TRACK_LENGTH * structureScale, 12 * structureScale}, (Vector2){0, 6 * structureScale}, 0.0f, HexToColor("#111417"));
            DrawTextEx(zainBlack, "move", (Vector2){leftTrackStart, 820 * structureScale}, 100 * structureScale, 1.0f, HexToColor("#C6D2E0"));
            DrawTextEx(zainBlack, "up", (Vector2){leftTrackStart, 960 * structureScale}, 80 * structureScale, 1.0f, HexToColor("#0A0C0F"));
            DrawTextEx(zainBlack, "down", (Vector2){leftTrackStart + SLIDER_TRACK_LENGTH * structureScale - MeasureTextEx(zainBlack, "down", 80 * structureScale, 1.0f).x, 960 * structureScale}, 80 * structureScale, 1.0f, HexToColor("#0A0C0F"));

            // left slider handle
            float movableLeftTrackLength = SLIDER_TRACK_LENGTH * structureScale - SLIDER_HANDLE_WIDTH * structureScale;
            float leftSliderProgress = (videoOffsetY + 1.0f) * 0.5f;
            float leftSliderX = leftTrackStart + leftSliderProgress * movableLeftTrackLength;
            float leftSliderY = 950 * structureScale;
            DrawRectanglePro((Rectangle){leftSliderX, leftSliderY, SLIDER_HANDLE_WIDTH * structureScale, 40 * structureScale}, (Vector2){0, 20 * structureScale}, 0.0f, HexToColor("#DFE8F5"));
            
            // right slider track
            float rightTrackStart = 1560 * structureScale;
            DrawRectanglePro((Rectangle){rightTrackStart, 950 * structureScale, SLIDER_TRACK_LENGTH * structureScale, 12 * structureScale}, (Vector2){0, 6 * structureScale}, 0.0f, HexToColor("#111417"));
            DrawTextEx(zainBlack, "zoom", (Vector2){rightTrackStart + SLIDER_TRACK_LENGTH * structureScale - MeasureTextEx(zainBlack, "zoom", 100 * structureScale, 1.0f).x, 820 * structureScale}, 100 * structureScale, 1.0f, HexToColor("#C6D2E0"));
            DrawTextEx(zainBlack, "out", (Vector2){rightTrackStart, 960 * structureScale}, 80 * structureScale, 1.0f, HexToColor("#0A0C0F"));
            DrawTextEx(zainBlack, "in", (Vector2){rightTrackStart + SLIDER_TRACK_LENGTH * structureScale - MeasureTextEx(zainBlack, "in", 80 * structureScale, 1.0f).x, 960 * structureScale}, 80 * structureScale, 1.0f, HexToColor("#0A0C0F"));

            // right slider handle
            float movableRightTrackLength = SLIDER_TRACK_LENGTH * structureScale - SLIDER_HANDLE_WIDTH * structureScale;
            float rightSliderProgress = (videoScale - SLIDER_MIN_SCALE) / (SLIDER_MAX_SCALE - SLIDER_MIN_SCALE);
            rightSliderProgress = Clamp(rightSliderProgress, 0.0f, 1.0f);
            float rightSliderX = rightTrackStart + rightSliderProgress * movableRightTrackLength;
            float rightSliderY = 950 * structureScale;
            DrawRectanglePro((Rectangle){rightSliderX, rightSliderY, SLIDER_HANDLE_WIDTH * structureScale, 40 * structureScale}, (Vector2){0, 20 * structureScale}, 0.0f, HexToColor("#DFE8F5"));
            
            // toggle button
            DrawCircleSector((Vector2){1200 * structureScale, 950 * structureScale}, 160.0f * structureScale, 0.0f, 360.0f, 100, HexToColor("#3C4151"));
            DrawRing((Vector2){1200 * structureScale, 950 * structureScale}, 136.0f * structureScale, 142.0f * structureScale, 0.0f, 360.0f, 100, HexToColor("#7380A7"));
            DrawRing((Vector2){1200 * structureScale, 950 * structureScale}, 100.0f * structureScale, 120.0f * structureScale, 45.0f, 135.0f, 40, HexToColor(TOGGLE_PRESSED ? "#6B8FFA" : "#2C3347"));

            const char* buttonText = TOGGLE_PRESSED ? "STOP" : "START";
            float fontSize = 100 * structureScale; 
            Vector2 textSize = MeasureTextEx(zainBlack, buttonText, fontSize, 1.0f);
            float textX = (1200 * structureScale) - (textSize.x * 0.5f); 
            float textY = (950 * structureScale) - (textSize.y * 0.5f); 
            DrawTextEx(zainBlack, buttonText, (Vector2){textX, textY}, fontSize, 1.0f, WHITE);
            
            // counter text
            float currentTime = 0.0f;
            if (timerRunning) {
                currentTime = GetTime() - startTime;
            } else if (stopTime > 0) {
          
                currentTime = stopTime - startTime;
            }       

            int totalSeconds = (int)currentTime;
            int minutes = totalSeconds / 60;
            int seconds = totalSeconds % 60;

            const char* timerText = TextFormat("%d:%02d", minutes, seconds);
            float timerFontSize = 120 * structureScale;
            Vector2 timerTextSize = MeasureTextEx(zainRegular, timerText, timerFontSize, 1.0f);
            float timerTextX = (windowSize.x - timerTextSize.x) * 0.5f;
            float timerTextY = 30 * structureScale;

            DrawTextEx(zainBlack, timerText, (Vector2){timerTextX, timerTextY}, timerFontSize, 1.0f, WHITE);

        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);           
            DrawTexturePro(windowTexture.texture, (Rectangle){0, 0, (float)windowTexture.texture.width, -(float)windowTexture.texture.height}, (Rectangle){0, 0, windowSize.x, windowSize.y}, (Vector2){0, 0}, 0.0f, WHITE);
            DrawTextEx(zainRegular, TextFormat("FPS: %i", GetFPS()), (Vector2){10, 50}, 20 * scale, 1.0f, GREEN);
        EndDrawing();
    }

    // cleanup resources
    if (textureLoaded) {
        UnloadTexture(screenTexture);
    }
    UnloadFont(zainBlack);
    UnloadFont(zainRegular);
    UnloadRenderTexture(windowTexture);
    CloseWindow();
    return 0;
}

// function implementations
Image MatToRaylibImage(const cv::Mat& mat) {
    Image img = {};
    if (mat.empty()) return img;

    img.width = mat.cols;
    img.height = mat.rows;
    img.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    img.mipmaps = 1;

    int dataSize = img.width * img.height * 3;
    img.data = malloc(dataSize);

    if (img.data) {
        memcpy(img.data, mat.data, dataSize);
    }
    return img;
}

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

// platform-specific implementations
#ifdef _WIN32
struct WindowsScreenCapture {
    HWND hwndRoblox;
    HDC hdcScreen;
    HDC hdcMemDC;
    HBITMAP hbmScreen;
    BITMAP bmpScreen;
    int captureWidth;
    int captureHeight;
    cv::Mat screenMat;

    WindowsScreenCapture() : hwndRoblox(NULL), hdcScreen(NULL), hdcMemDC(NULL),
                             hbmScreen(NULL), captureWidth(0), captureHeight(0) {}

    bool Initialize() {
        hwndRoblox = FindWindowA(NULL, "Roblox");
        if (!hwndRoblox) {
            return false;
        }

        RECT clientRect;
        if (!GetClientRect(hwndRoblox, &clientRect)) {
            return false;
        }
        captureWidth = clientRect.right - clientRect.left;
        captureHeight = clientRect.bottom - clientRect.top;

        if (captureWidth <= 0 || captureHeight <= 0) {
            return false;
        }

        hdcScreen = GetDC(hwndRoblox);
        if (!hdcScreen) {
            return false;
        }

        hdcMemDC = CreateCompatibleDC(hdcScreen);
        if (!hdcMemDC) {
            ReleaseDC(hwndRoblox, hdcScreen);
            return false;
        }
        hbmScreen = CreateCompatibleBitmap(hdcScreen, captureWidth, captureHeight);
        if (!hbmScreen) {
            DeleteDC(hdcMemDC);
            ReleaseDC(hwndRoblox, hdcScreen);
            return false;
        }
        SelectObject(hdcMemDC, hbmScreen);
        GetObjectA(hbmScreen, sizeof(BITMAP), &bmpScreen);
        screenMat = cv::Mat::zeros(captureHeight, captureWidth, CV_8UC4);

        return true;
    }

    cv::Mat CaptureScreen() {
        if (!hwndRoblox || !hdcScreen || !hdcMemDC || !hbmScreen) {
            return cv::Mat();
        }

        if (!IsWindow(hwndRoblox)) {
            return cv::Mat();
        }

        RECT clientRect;
        if (!GetClientRect(hwndRoblox, &clientRect)) {
            return cv::Mat();
        }
        int width = clientRect.right - clientRect.left;
        int height = clientRect.bottom - clientRect.top;

        if (width != captureWidth || height != captureHeight) {
            captureWidth = width;
            captureHeight = height;
            DeleteObject(hbmScreen);
            DeleteDC(hdcMemDC);
            hbmScreen = CreateCompatibleBitmap(hdcScreen, captureWidth, captureHeight);
            if (!hbmScreen) {
                return cv::Mat();
            }
            hdcMemDC = CreateCompatibleDC(hdcScreen);
            if (!hdcMemDC) {
                DeleteObject(hbmScreen);
                return cv::Mat();
            }
            SelectObject(hdcMemDC, hbmScreen);
            GetObjectA(hbmScreen, sizeof(BITMAP), &bmpScreen);
            screenMat = cv::Mat::zeros(captureHeight, captureWidth, CV_8UC4);
        }

        WINBOOL bltResult = BitBlt(hdcMemDC, 0, 0, captureWidth, captureHeight,
                                   hdcScreen, 0, 0, SRCCOPY);
        if (!bltResult) {
            return cv::Mat();
        }

        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmpScreen.bmWidth;
        bi.biHeight = -bmpScreen.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        int scanlinesCopied = GetDIBits(hdcMemDC, hbmScreen, 0, (unsigned int)captureHeight,
                                        screenMat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        if (scanlinesCopied == 0) {
            return cv::Mat();
        }

        cv::Mat rgbMat;
        cv::cvtColor(screenMat, rgbMat, cv::COLOR_BGRA2RGB);
        return rgbMat;
    }

    ~WindowsScreenCapture() {
        if (hdcMemDC) DeleteDC(hdcMemDC);
        if (hbmScreen) DeleteObject(hbmScreen);
        if (hdcScreen && hwndRoblox) ReleaseDC(hwndRoblox, hdcScreen);
    }
};

ScreenCapture::ScreenCapture() {
    impl = new WindowsScreenCapture();
}

ScreenCapture::~ScreenCapture() {
    delete static_cast<WindowsScreenCapture*>(impl);
}

bool ScreenCapture::Initialize() {
    return static_cast<WindowsScreenCapture*>(impl)->Initialize();
}

cv::Mat ScreenCapture::CaptureScreen() {
    return static_cast<WindowsScreenCapture*>(impl)->CaptureScreen();
}

#else
ScreenCapture::ScreenCapture() : impl(nullptr) {}
ScreenCapture::~ScreenCapture() {}
bool ScreenCapture::Initialize() {
    return false;
}

cv::Mat ScreenCapture::CaptureScreen() {
    return cv::Mat();
}
#endif