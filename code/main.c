#include "base.h"
#include <windows.h>

#define internal_fn static
#define local_persist static
#define global_var static

typedef struct offscreen_buffer
{
    // NOTE: pixels are always 4 bytes (32 bits) wide,
    // memory order : BB GG RR XX
    // Little Endian: XX RR GG BB
    BITMAPINFO BitmapInfo;
    void      *memory;
    int        width;
    int        height;
    int        pitch;
} offscreen_buffer;

typedef struct win32_window_dimensions
{
    int width;
    int height;
} win32_window_dimensions;

// TODO: This is a global for now.
global_var bool                    GlobalRunning = true;
global_var struct offscreen_buffer GlobalBackBuffer;

internal_fn win32_window_dimensions
getWindowDimensions(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    win32_window_dimensions ret = {
        clientRect.right - clientRect.left,
        clientRect.bottom - clientRect.top,
    };
    return ret;
}

internal_fn void
renderWeirdGradient(int xOffset, int yOffset)
{
    u8 *row = (u8 *)GlobalBackBuffer.memory;
    for (int y = 0; y < GlobalBackBuffer.height; ++y)
    {
        u32 *pixel = (u32 *)row;
        for (int x = 0; x < GlobalBackBuffer.width; ++x)
        {
            //                   1  2  3  4
            // pixel in memory: BB GG RR xx  (bc MSFT wanted to see RGB in register (see register)
            //     in register: xx RR GG BB  (bc it's little endian)
            u8 bb    = (u8)(x + xOffset);
            u8 gg    = (u8)(y + yOffset);
            *pixel++ = (gg << 8 | bb);
        }

        row += GlobalBackBuffer.pitch;
    }
}

void
Win32ResizeDIBSection(int width, int height)
{
    // TODO: bulletproof this.

    if (GlobalBackBuffer.memory)
    {
        VirtualFree(GlobalBackBuffer.memory, 0, MEM_RELEASE);
    }

    GlobalBackBuffer.width  = width;
    GlobalBackBuffer.height = height;
    int bytesPerPixel       = 4;
    GlobalBackBuffer.pitch  = width * bytesPerPixel;

    // NOTE: When the biHeight field is negative, this is the clue to Windows to treat this bitmap as top down, not
    // bottom up, meaning that the first 3 bytes of the image are the color for the top left pixel in the bitmap,
    // not the bottom left
    GlobalBackBuffer.BitmapInfo.bmiHeader.biSize        = sizeof(GlobalBackBuffer.BitmapInfo.bmiHeader);
    GlobalBackBuffer.BitmapInfo.bmiHeader.biWidth       = width;
    GlobalBackBuffer.BitmapInfo.bmiHeader.biHeight      = -height;
    GlobalBackBuffer.BitmapInfo.bmiHeader.biPlanes      = 1;
    GlobalBackBuffer.BitmapInfo.bmiHeader.biBitCount    = 32;
    GlobalBackBuffer.BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize    = (GlobalBackBuffer.width * GlobalBackBuffer.height) * bytesPerPixel;
    GlobalBackBuffer.memory = VirtualAlloc(NULL, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal_fn void
Win32DisplayBufferInWindow(HDC deviceContext, int destWidth, int destHeight)
{
    // TODO: aspect ratio correction
    StretchDIBits(deviceContext, 0, 0, destWidth, destHeight, 0, 0, GlobalBackBuffer.width, GlobalBackBuffer.height,
                  GlobalBackBuffer.memory, &GlobalBackBuffer.BitmapInfo, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(HWND window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT result = 0;

    switch (Message)
    {
    case WM_CREATE: {
        OutputDebugStringA("WM_CREATE\n");
    }
    break;

    case WM_SIZE: {
        OutputDebugStringA("WM_SIZE\n");
    }
    break;

    case WM_DESTROY: {
        GlobalRunning = false;
        OutputDebugStringA("WM_DESTROY\n");
    }
    break;

    case WM_CLOSE: {
        GlobalRunning = false;
        OutputDebugStringA("WM_CLOSE\n");
    }
    break;

    case WM_ACTIVATEAPP: {
        OutputDebugStringA("WM_ACTIVATEAPP\n");
    }
    break;

    case WM_PAINT: {
        PAINTSTRUCT             paint;
        HDC                     deviceContext = BeginPaint(window, &paint);
        win32_window_dimensions dims          = getWindowDimensions(window);

        Win32DisplayBufferInWindow(deviceContext, dims.width, dims.height);
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

int
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, INT ShowCode)
{
    WNDCLASS WindowClass = {};

    Win32ResizeDIBSection(1280, 720);

    // TODO: check if these matter
    WindowClass.style       = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance   = Instance;
    // WindowClass.hIcon;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (!RegisterClass(&WindowClass))
    {
        // TODO: logging
        return 1;
    };

    HWND WindowHandle = CreateWindowExA(0, WindowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, Instance, 0);
    if (!WindowHandle)
    {
        // TODO: Error handling/logging
        return 1;
    }

    // NOTE: since we specified CS_OWNDC, we can just get one device context and use it
    // forever because we are not sharing it with anyone
    HDC deviceContext = GetDC(WindowHandle);

    int xOffset = 0;
    int yOffset = 0;

    MSG message;
    while (GlobalRunning)
    {
        if (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                GlobalRunning = false;
            }

            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        renderWeirdGradient(xOffset, yOffset);

        win32_window_dimensions dims = getWindowDimensions(WindowHandle);

        Win32DisplayBufferInWindow(deviceContext, dims.width, dims.height);

        ++xOffset;
        yOffset += 2;
    }

    return 0;
}
