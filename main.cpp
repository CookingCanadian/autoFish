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
#include <iostream>
#include <string>
#include <cctype>
#include <algorithm>

#include "./resources/zain_black.h"
#include "./resources/zain_regular.h"

using namespace std;

// Forward declarations
class ScreenCapture;
struct WindowsScreenCapture;

// Function declarations
Image MatToRaylibImage(const cv::Mat& mat);
Color HexToColor(const string& hex);
float Clamp(float value, float min, float max);

// Class declaration
class ScreenCapture {
private:
    void* impl; // pimpl pattern to hide platform-specific implementation
public:
    ScreenCapture();
    ~ScreenCapture();
    bool Initialize();
    cv::Mat CaptureScreen();
};

int main() {
    const int NATIVE_WIDTH = 2400;
    const int NATIVE_HEIGHT = 1200;
    const int WINDOW_WIDTH = 600;
    const int WINDOW_HEIGHT = 300;
    const int MAX_FPS = 60;
    const string BACKGROUND_HEX = "#1B1E24";
    const string PRIMARY_HEX = "#272A33";
    const int HANDLE_SIZE = 20;
    const float MIN_WIDTH = 300.0f;
    const float MAX_WIDTH = 1920.0f;
    const float TOP_BAR_HEIGHT = 24.0f;
    const float CORNER_RADIUS = 10.0f;
    const float VIDEO_HEIGHT = 600.0f; 

    ScreenCapture screenCap;
    if (!screenCap.Initialize()) {
        std::cerr << "failed to initialize screen capture" << std::endl;
        return -1;
    }

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE | FLAG_WINDOW_UNDECORATED);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "autoFish");
    SetTargetFPS(MAX_FPS);

    // load embedded font data into raylib fonts
    Font zainBlack = LoadFontFromMemory(".ttf", zain_black_data, zain_black_data_size, 100, nullptr, 0);
    Font zainRegular = LoadFontFromMemory(".ttf", zain_regular_data, zain_regular_data_size, 100, nullptr, 0);
    if (zainBlack.texture.id == 0 || zainRegular.texture.id == 0) {
        std::cerr << "ERROR: font loading incomplete" << std::endl;
        CloseWindow();
        return 1;
    }
    SetTextureFilter(zainBlack.texture, TEXTURE_FILTER_BILINEAR);
    SetTextureFilter(zainRegular.texture, TEXTURE_FILTER_BILINEAR);
    
    // screen capture texture - gets replaced each frame when we capture
    Texture2D screenTexture = {};
    bool textureLoaded = false;

    RenderTexture2D windowTexture = LoadRenderTexture(WINDOW_WIDTH, WINDOW_HEIGHT);
    SetTextureFilter(windowTexture.texture, TEXTURE_FILTER_BILINEAR);
    Vector2 windowSize = {(float)WINDOW_WIDTH, (float)WINDOW_HEIGHT};
    bool draggingResize = false;
    bool draggingWindow = false;
    Vector2 dragOffsetToWindow = {0, 0};

    int frameCount = 0;
    const int SCREEN_CAPTURE_INTERVAL = 2; // for testing

    while (!WindowShouldClose()) {
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
            SetWindowPosition((int)(absoluteMousePosition.x - dragOffsetToWindow.x), (int)(absoluteMousePosition.y - dragOffsetToWindow.y));
        }

        // screen capture only every few frames for performance
        frameCount++;
        if (frameCount % SCREEN_CAPTURE_INTERVAL == 0) {
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
            }
        }

        float scale = windowSize.x / (float)WINDOW_WIDTH;

        BeginTextureMode(windowTexture);
            ClearBackground(BLACK);
            
            // scale everything based on window size vs native resolution
            float structureScale = windowSize.x / (float)NATIVE_WIDTH;
            
            // background
            DrawRectanglePro((Rectangle){0, 0, windowSize.x, windowSize.y}, (Vector2){0, 0}, 0.0f, HexToColor(BACKGROUND_HEX));
            
            // top bar
            DrawRectanglePro((Rectangle){0, 0, windowSize.x, 96 * structureScale}, (Vector2){0, 0}, 0.0f, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){864 * structureScale, 96 * structureScale}, 40 * structureScale, 45, 180, 32, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){1536 * structureScale, 96 * structureScale}, 40 * structureScale, 0, 135, 32, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){832 * structureScale, 176 * structureScale}, 80 * structureScale, 180, 360, 32, HexToColor(BACKGROUND_HEX));
            DrawCircleSector((Vector2){1568 * structureScale, 176 * structureScale}, 80 * structureScale, 180, 360, 32, HexToColor(BACKGROUND_HEX));
            
            // screen capture video in the middle section
            if (textureLoaded) {
                float videoX = 0;
                float videoY = 114.0f * structureScale;
                float videoWidth = NATIVE_WIDTH * structureScale;
                float videoHeight = VIDEO_HEIGHT * structureScale;
                
                // scissor test to keep video in bounds
                BeginScissorMode((int)videoX, (int)videoY, (int)videoWidth, (int)videoHeight);
                
                // scale video to fill area while maintaining aspect ratio
                float scaleX = videoWidth / screenTexture.width;
                float scaleY = videoHeight / screenTexture.height;
                float videoScale = fmaxf(scaleX, scaleY);
                
                float scaledWidth = screenTexture.width * videoScale;
                float scaledHeight = screenTexture.height * videoScale;
                float centerX = videoX + (videoWidth - scaledWidth) * 0.5f;
                float centerY = videoY + (videoHeight - scaledHeight) * 0.5f;
                
                Rectangle videoSrc = {0.0f, 0.0f, (float)screenTexture.width, (float)screenTexture.height};
                Rectangle videoDst = {centerX, centerY, scaledWidth, scaledHeight};
                DrawTexturePro(screenTexture, videoSrc, videoDst, (Vector2){0, 0}, 0.0f, WHITE);
                
                EndScissorMode();
            }
            
            // ui elements that overlay on top of video
            DrawRectangleGradientV(0, 112 * structureScale, windowSize.x, 100 * structureScale, HexToColor(BACKGROUND_HEX), HexToColor("#1B1E2400"));
            DrawRectanglePro((Rectangle){960 * structureScale, 96 * structureScale, 480 * structureScale, 80 * structureScale}, (Vector2){0, 0}, 0.0f, HexToColor(PRIMARY_HEX));
            DrawRectanglePro((Rectangle){960 * structureScale, 136 * structureScale, 96 * structureScale, 80 * structureScale}, (Vector2){96 * structureScale, 40 * structureScale}, 45, HexToColor(PRIMARY_HEX));
            DrawRectanglePro((Rectangle){1440 * structureScale, 136 * structureScale, 96 * structureScale, 80 * structureScale}, (Vector2){96 * structureScale, 40 * structureScale}, 135, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){1440 * structureScale, 136 * structureScale}, 40 * structureScale, 45, 90, 32, HexToColor(PRIMARY_HEX));
            DrawCircleSector((Vector2){960 * structureScale, 136 * structureScale}, 40 * structureScale, 90, 135, 32, HexToColor(PRIMARY_HEX));
            DrawTextEx(zainBlack, "autoFish", (Vector2){40 * structureScale, 0}, 100 * structureScale, 1.0f, WHITE);
         
            // resize handle
            DrawRectangle((int)(windowSize.x - HANDLE_SIZE), (int)(windowSize.y - HANDLE_SIZE), HANDLE_SIZE, HANDLE_SIZE, GRAY);
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            DrawRectangleRounded((Rectangle){0, 0, windowSize.x, windowSize.y}, CORNER_RADIUS * scale / 2.0f, 16, BLACK);
            // flip y coordinate because render texture is upside down
            DrawTexturePro(windowTexture.texture, (Rectangle){0, 0, (float)windowTexture.texture.width, -(float)windowTexture.texture.height}, (Rectangle){0, 0, windowSize.x, windowSize.y}, (Vector2){0, 0}, 0.0f, WHITE);
            DrawTextEx(zainRegular, TextFormat("FPS: %i", GetFPS()), (Vector2){10, 50}, 20 * scale, 1.0f, GREEN);
        EndDrawing();
    }

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
    HDC hdcScreen;
    HDC hdcMemDC;
    HBITMAP hbmScreen;
    BITMAP bmpScreen;
    int screenWidth;
    int screenHeight;
    cv::Mat screenMat;

    WindowsScreenCapture() : hdcScreen(NULL), hdcMemDC(NULL),
                             hbmScreen(NULL), screenWidth(0), screenHeight(0) {}

    bool Initialize() {
        screenWidth = GetSystemMetrics(SM_CXSCREEN);
        screenHeight = GetSystemMetrics(SM_CYSCREEN);

        hdcScreen = GetDC(NULL);
        if (!hdcScreen) {
            std::cerr << "failed to get screen DC" << std::endl;
            return false;
        }
        hdcMemDC = CreateCompatibleDC(hdcScreen);
        if (!hdcMemDC) {
            std::cerr << "failed to create screen DC" << std::endl;
            ReleaseDC(NULL, hdcScreen);
            return false;
        }
        hbmScreen = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
        if (!hbmScreen) {
            std::cerr << "failed to create bitmap" << std::endl;
            DeleteDC(hdcMemDC);
            ReleaseDC(NULL, hdcScreen);
            return false;
        }
        SelectObject(hdcMemDC, hbmScreen);
        GetObjectA(hbmScreen, sizeof(BITMAP), &bmpScreen);
        screenMat = cv::Mat::zeros(screenHeight, screenWidth, CV_8UC4);

        std::cout << "screen started: " << screenWidth << "x" << screenHeight << std::endl;
        return true;
    }

    cv::Mat CaptureScreen() {
        if (!hdcScreen || !hdcMemDC || !hbmScreen) {
            std::cerr << "screen capture not initialized." << std::endl;
            return cv::Mat();
        }
        WINBOOL bltResult = BitBlt(hdcMemDC, 0, 0, screenWidth, screenHeight,
                                hdcScreen, 0, 0, SRCCOPY);
        if (!bltResult) {
            std::cerr << "bitblt failed" << std::endl;
            return cv::Mat();
        }

        BITMAPINFOHEADER bi = {};
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = bmpScreen.bmWidth;
        bi.biHeight = -bmpScreen.bmHeight; // negative for top-down bitmap
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;

        int scanlinesCopied = GetDIBits(hdcMemDC, hbmScreen, 0, (unsigned int)screenHeight,
                                screenMat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        if (scanlinesCopied == 0) {
            std::cerr << "GetDIBits failed" << std::endl;
            return cv::Mat();
        }
        cv::Mat rgbMat;
        cv::cvtColor(screenMat, rgbMat, cv::COLOR_BGRA2RGB);
        return rgbMat;
    }

    ~WindowsScreenCapture() {
        if (hdcMemDC) DeleteDC(hdcMemDC);
        if (hbmScreen) DeleteObject(hbmScreen);
        if (hdcScreen) ReleaseDC(NULL, hdcScreen);
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
    std::cout << "screen capture not implemented for this platform" << std::endl;
    return false;
}

cv::Mat ScreenCapture::CaptureScreen() {
    return cv::Mat();
}
#endif