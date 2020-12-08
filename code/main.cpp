#include <windows.h>
#include <stdint.h>

#define internal_fn static
#define local_persist static
#define global_variable static

// TODO: This is a global for now.
global_variable bool Running = true;

global_variable BITMAPINFO BitmapInfo;
global_variable void* bitmapMemory;
global_variable int bitmapWidth;
global_variable int bitmapHeight;

const int bytesPerPixel = 4;

internal_fn void renderWeirdGradient(int xOffset, int yOffset)
{
    int pitch = bitmapWidth * bytesPerPixel;
    uint8_t* row = (uint8_t*) bitmapMemory;
    for (int y = 0; y < bitmapHeight; ++y) {
        uint32_t* pixel = (uint32_t*) row;
        for (int x = 0; x < bitmapWidth; ++x) {
            //                   1  2  3  4
            // pixel in memory: BB GG RR xx  (bc MSFT wanted to see RGB in register (see register)
            //     in register: xx RR GG BB  (bc it's little endian)
            uint8_t bb = (uint8_t) (x + xOffset);
            uint8_t gg = (uint8_t) (y + yOffset);
            *pixel++ = (gg << 8 | bb);
        }

        row += pitch;
    }
}

void Win32ResizeDIBSection(int width, int height)
{
    // TODO: bulletproof this.

    if (bitmapMemory) {
        VirtualFree(bitmapMemory, 0, MEM_RELEASE);
    }

    bitmapWidth = width;
    bitmapHeight = height;

    BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
    BitmapInfo.bmiHeader.biWidth = width;
    BitmapInfo.bmiHeader.biHeight = -height;
    BitmapInfo.bmiHeader.biPlanes = 1;
    BitmapInfo.bmiHeader.biBitCount = 32;
    BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = (bitmapWidth * bitmapHeight) * bytesPerPixel;
    bitmapMemory = VirtualAlloc(nullptr, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal_fn void Win32UpdateWindow(HDC deviceContext, RECT* clientRect, int x, int y, int width, int height)
{
    int windowWidth = clientRect->right - clientRect->left;
    int windowHeight = clientRect->bottom - clientRect->top;

    StretchDIBits(
            deviceContext,
            0, 0, bitmapWidth, bitmapHeight,
            x, y, windowWidth, windowHeight,
            bitmapMemory,
            &BitmapInfo,
            DIB_RGB_COLORS,
            SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND window,
                        UINT Message,
                        WPARAM WParam,
                        LPARAM LParam)
{
    LRESULT result = 0;

    switch (Message) {
        case WM_CREATE: {
            OutputDebugStringA("WM_CREATE\n");

        }
            break;

        case WM_SIZE: {
            RECT clientRect;
            GetClientRect(window, &clientRect);
            int width = clientRect.right - clientRect.left;
            int height = clientRect.bottom - clientRect.top;
            Win32ResizeDIBSection(width, height);
            OutputDebugStringA("WM_SIZE\n");
        }
            break;

        case WM_DESTROY: {
            Running = false;
            OutputDebugStringA("WM_DESTROY\n");
        }
            break;

        case WM_CLOSE: {
            Running = false;
            OutputDebugStringA("WM_CLOSE\n");
        }
            break;

        case WM_ACTIVATEAPP: {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        }
            break;

        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC deviceContext = BeginPaint(window, &paint);
            int width = paint.rcPaint.right - paint.rcPaint.left;
            int height = paint.rcPaint.bottom - paint.rcPaint.top;
            LONG x = paint.rcPaint.left;
            LONG y = paint.rcPaint.top;

            RECT clientRect;
            GetClientRect(window, &clientRect);

            Win32UpdateWindow(deviceContext, &clientRect, x, y, width, height);
            EndPaint(window, &paint);
        }
            break;

        default: {
//            OutputDebugStringA("default\n")
            result = DefWindowProc(window, Message, WParam, LParam);
        }
            break;
    }

    return (result);
};

int WinMain(HINSTANCE Instance,
            HINSTANCE PrevInstance,
            PSTR CommandLine,
            INT ShowCode)
{
    WNDCLASS WindowClass = {};

    // TODO: check if these matter
    WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (!RegisterClass(&WindowClass)) {
        // TODO: logging
        return 1;
    };

    HWND WindowHandle = CreateWindowEx(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
    if (!WindowHandle) {
        // TODO: Error handling/logging
        return 1;
    }

    MSG message;
    int xOffset = 0;
    int yOffset = 0;
    while (Running) {
        if (PeekMessageA(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                Running = false;
            }

            TranslateMessage(&message);
            DispatchMessageA(&message);
        }
        
        renderWeirdGradient(xOffset, yOffset);

        HDC deviceContext = GetDC(WindowHandle);
        RECT clientRect;
        GetClientRect(WindowHandle, &clientRect);
        int windowWidth = clientRect.right - clientRect.left;
        int windowHeight = clientRect.bottom - clientRect.top;
        ReleaseDC(WindowHandle, deviceContext);
        
        Win32UpdateWindow(deviceContext, &clientRect, 0, 0, windowWidth, windowHeight);

        ++xOffset;
        ++yOffset;
    }

    return 0;
}