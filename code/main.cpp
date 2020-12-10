#include <windows.h>
#include <stdint.h>

#define internal_fn static
#define local_persist static
#define global_variable static

// TODO: This is a global for now.
global_variable bool Running = true;

typedef struct offscreen_buffer {
    BITMAPINFO BitmapInfo;
    void* memory;
    int width;
    int height;
    int bytesPerPixel;
    int pitch;
} offscreen_buffer;

typedef struct win32_window_dimensions {
    int width;
    int height;
} win32_window_dimensions;

global_variable struct offscreen_buffer GlobalBackBuffer;

internal_fn win32_window_dimensions getWindowDimensions(HWND window)
{
    RECT clientRect;
    GetClientRect(window, &clientRect);
    return win32_window_dimensions{
            clientRect.right - clientRect.left,
            clientRect.bottom - clientRect.top,
    };
}

internal_fn void renderWeirdGradient(offscreen_buffer buffer, int xOffset, int yOffset)
{
    uint8_t* row = (uint8_t*) buffer.memory;
    for (int y = 0; y < buffer.height; ++y) {
        uint32_t* pixel = (uint32_t*) row;
        for (int x = 0; x < buffer.width; ++x) {
            //                   1  2  3  4
            // pixel in memory: BB GG RR xx  (bc MSFT wanted to see RGB in register (see register)
            //     in register: xx RR GG BB  (bc it's little endian)
            uint8_t bb = (uint8_t) (x + xOffset);
            uint8_t gg = (uint8_t) (y + yOffset);
            *pixel++ = (gg << 8 | bb);
        }

        row += buffer.pitch;
    }
}

void Win32ResizeDIBSection(offscreen_buffer* buffer, int width, int height)
{
    // TODO: bulletproof this.

    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;
    buffer->bytesPerPixel = 4;
    buffer->pitch = width * 4;

    buffer->BitmapInfo.bmiHeader.biSize = sizeof(buffer->BitmapInfo.bmiHeader);
    buffer->BitmapInfo.bmiHeader.biWidth = width;
    buffer->BitmapInfo.bmiHeader.biHeight = -height;
    buffer->BitmapInfo.bmiHeader.biPlanes = 1;
    buffer->BitmapInfo.bmiHeader.biBitCount = 32;
    buffer->BitmapInfo.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = (buffer->width * buffer->height) * buffer->bytesPerPixel;
    buffer->memory = VirtualAlloc(nullptr, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
}

internal_fn void
Win32DisplayBufferInWindow(HDC deviceContext, int dcWidth, int dcHeight, offscreen_buffer buffer, int x, int y)
{
    StretchDIBits(
            deviceContext,
            0, 0, dcWidth, dcHeight,
            x, y, buffer.width, buffer.height,
            buffer.memory,
            &buffer.BitmapInfo,
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

            win32_window_dimensions dims = getWindowDimensions(window);

            Win32DisplayBufferInWindow(deviceContext, dims.width, dims.height, GlobalBackBuffer, x, y);
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
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

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

        renderWeirdGradient(GlobalBackBuffer, xOffset, yOffset);

        HDC deviceContext = GetDC(WindowHandle);
        win32_window_dimensions dims = getWindowDimensions(WindowHandle);
        ReleaseDC(WindowHandle, deviceContext);

        Win32DisplayBufferInWindow(deviceContext, dims.width, dims.height, GlobalBackBuffer, 0, 0);

        ++xOffset;
        yOffset += 2;
    }

    return 0;
}