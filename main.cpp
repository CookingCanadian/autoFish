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

#include "./resources/zain_black.h"
#include "./resources/zain_bold.h"
#include "./resources/zain_regular.h"

class ScreenCapture {
private:
    void* impl;
public:
    ScreenCapture();
    ~ScreenCapture();
    bool Initialize();
    cv::Mat CaptureScreen();
};

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
            std::cerr << "faild to get screen DC" << std::endl;
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
        bi.biHeight = -bmpScreen.bmHeight;
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

int main() {
    std::cout << "OpenCV version: " << CV_VERSION << std::endl;

    ScreenCapture screenCap;
    if (!screenCap.Initialize()) {
        std::cerr << "failed to initialize screen capture" << std::endl;
        return -1;
    }

    InitWindow(1024, 600, "screen capture testing");
    SetTargetFPS(30);

    Texture2D screenTexture = {};
    bool textureLoaded = false;

    while (!WindowShouldClose()) {
        cv::Mat screenMat = screenCap.CaptureScreen();

        if (!screenMat.empty()) {
            cv::Mat resizedMat;
            cv::resize(screenMat, resizedMat, cv::Size(GetScreenWidth(), GetScreenHeight()));

            Image screenImg = MatToRaylibImage(resizedMat);

            if (screenImg.data) {
                if (textureLoaded) {
                    UnloadTexture(screenTexture);
                }
                screenTexture = LoadTextureFromImage(screenImg);
                textureLoaded = true;
                UnloadImage(screenImg);
            }
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (textureLoaded) {
            DrawTexture(screenTexture, 0, 0, WHITE);    
            DrawText("Press ESC to exit", 10, 40, 16, RED);
        } else {
            DrawText("Capturing screen...", 100, 100, 20, BLACK);
        }
        EndDrawing();
    }

    if (textureLoaded) {
        UnloadTexture(screenTexture);
    }
    CloseWindow();
    return 0;
}