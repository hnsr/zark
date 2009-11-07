#define UNICODE
#include <windows.h>
#include <shlobj.h>
#include <io.h>
#include <conio.h>
#include <crtdbg.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <GL/glew.h>
#include <GL/wglew.h>

#include "common.h"

// OS-specific code. Handles windowing, input, timing.

static HINSTANCE app_instance;
static HWND window_handle;
static int cmd_show;
static HDC device_context;
static HGLRC render_context;
static int window_active;
static int mouse_active;
static int console_enabled;
static ATOM wndclass_atom;

// Window rectangle, this is updated when the window is resized, moved, and opened. It is used by
// only the mouse input code (so it knows where to warp the mouse pointer).
static RECT window_rect;



// Get the modifier mask. I need to call GetKeyState for this, not sure how reliable this is
// considering I am processing key events from a queue (I guess it keeps track of key states for the
// last event I peeked?)
static unsigned int zGetWinModMask(void)
{
    unsigned int mask = 0;

    // I'm only interested in the high-order bit here, which signifies wether or not the key was
    // down. The low-order bit is for the 'toggle' state. I'll just shift one to the left to get rid
    // of the low-order byte..
    if ( GetKeyState(VK_SHIFT)   >> 1 ) mask |= Z_KEY_MOD_SHIFT;
    if ( GetKeyState(VK_CONTROL) >> 1 ) mask |= Z_KEY_MOD_CTRL;
    if ( GetKeyState(VK_LMENU)   >> 1 ) mask |= Z_KEY_MOD_LALT;
    if ( GetKeyState(VK_RMENU)   >> 1 ) mask |= Z_KEY_MOD_RALT;
    if ( GetKeyState(VK_LWIN)    >> 1 ) mask |= Z_KEY_MOD_SUPER;
    if ( GetKeyState(VK_RWIN)    >> 1 ) mask |= Z_KEY_MOD_SUPER;

    return mask;
}



// Mouse event handling moved to seperate function, sets zkev and returns TRUE if handled, FALSE
// otherwise.
static int zHandleMouseEvents(UINT message, WPARAM wparam, LPARAM lparam, ZKeyEvent *zkev)
{
    zkev->key = Z_KEY_UNKNOWN;

    switch (message) {

    // Mouse 1
    case WM_LBUTTONDOWN:
        zkev->key = Z_KEY_MOUSE1;
        zkev->keystate = Z_KEY_STATE_PRESS;
        break;
    case WM_LBUTTONUP:
        zkev->key = Z_KEY_MOUSE1;
        zkev->keystate = Z_KEY_STATE_RELEASE;
        break;

    // Mouse 2
    case WM_MBUTTONDOWN:
        zkev->key = Z_KEY_MOUSE2;
        zkev->keystate = Z_KEY_STATE_PRESS;
        break;
    case WM_MBUTTONUP:
        zkev->key = Z_KEY_MOUSE2;
        zkev->keystate = Z_KEY_STATE_RELEASE;
        break;

    // Mouse 3
    case WM_RBUTTONDOWN:
        zkev->key = Z_KEY_MOUSE3;
        zkev->keystate = Z_KEY_STATE_PRESS;
        break;
    case WM_RBUTTONUP:
        zkev->key = Z_KEY_MOUSE3;
        zkev->keystate = Z_KEY_STATE_RELEASE;
        break;

    // Mouse 8/9
    case WM_XBUTTONDOWN:
        if (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) zkev->key = Z_KEY_MOUSE8;
        else                                        zkev->key = Z_KEY_MOUSE9;
        zkev->keystate = Z_KEY_STATE_PRESS;
        break;
    case WM_XBUTTONUP:
        if (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) zkev->key = Z_KEY_MOUSE8;
        else                                        zkev->key = Z_KEY_MOUSE9;
        zkev->keystate = Z_KEY_STATE_RELEASE;
        break;

    // Mouse 4/5
    case WM_MOUSEWHEEL:
        zkev->keystate = Z_KEY_STATE_PRESS;
        if ( GET_WHEEL_DELTA_WPARAM(wparam) < 0 ) zkev->key = Z_KEY_MOUSE5;
        else                                      zkev->key = Z_KEY_MOUSE4;
        break;

    // Mouse 6/7
    case WM_MOUSEHWHEEL:
        zkev->keystate = Z_KEY_STATE_PRESS;
        if ( GET_WHEEL_DELTA_WPARAM(wparam) < 0 ) zkev->key = Z_KEY_MOUSE7;
        else                                      zkev->key = Z_KEY_MOUSE6;
        break;

    default:
        zWarning("Unhandled mouse event");
        return FALSE;
    }

    zkev->modmask = zGetWinModMask();
    return TRUE;
    //zDebug("%s (%s)", zKeyName(zkev.key), zkev.keystate == Z_KEY_STATE_PRESS ? "press" : "release");
}



// Translate windows keydown events to XKey. The meaning of some of the VK_ constants differs per
// layout.. not sure how do deal with that properly, for now I'll just assume a US keyboard layout.
// Also see http://msdn.microsoft.com/en-us/library/dd375731(VS.85).aspx
static ZKey zTranslateWinKey(WPARAM wparam, LPARAM lparam)
{
    int is_extended = lparam & 0x1000000;

    // Handle A-Z/0-9 keys the easy way
    if      (wparam >= 'A' && wparam <= 'Z') return Z_KEY_A + (wparam - 'A');
    else if (wparam >= '0' && wparam <= '9') return Z_KEY_0 + (wparam - '0');

    // Do the rest by hand.
    switch (wparam) {

        case VK_ESCAPE:     return Z_KEY_ESCAPE;
        case VK_F1:         return Z_KEY_F1;
        case VK_F2:         return Z_KEY_F2;
        case VK_F3:         return Z_KEY_F3;
        case VK_F4:         return Z_KEY_F4;
        case VK_F5:         return Z_KEY_F5;
        case VK_F6:         return Z_KEY_F6;
        case VK_F7:         return Z_KEY_F7;
        case VK_F8:         return Z_KEY_F8;
        case VK_F9:         return Z_KEY_F9;
        case VK_F10:        return Z_KEY_F10;
        case VK_F11:        return Z_KEY_F11;
        case VK_F12:        return Z_KEY_F12;

        case VK_OEM_3:      return Z_KEY_GRAVE;
        case VK_OEM_MINUS:  return Z_KEY_MINUS;
        case VK_OEM_PLUS:   return Z_KEY_EQUALS;
        case VK_BACK:       return Z_KEY_BACKSPACE;
        case VK_TAB:        return Z_KEY_TAB;
        case VK_CAPITAL:    return Z_KEY_CAPSLOCK;

        // Windows doesn't seem to give a way to differentiate left/right for SHIFT.
        case VK_SHIFT:      return Z_KEY_LSHIFT;

        case VK_LWIN:       return Z_KEY_LSUPER;
        case VK_SPACE:      return Z_KEY_SPACEBAR;

        case VK_RWIN:       return Z_KEY_RSUPER;
        case VK_APPS:       return Z_KEY_MENU;

        case VK_OEM_5:      return Z_KEY_BACKSLASH;
        case VK_OEM_4:      return Z_KEY_LSQBRACKET;
        case VK_OEM_6:      return Z_KEY_RSQBRACKET;
        case VK_OEM_1:      return Z_KEY_SEMICOLON;
        case VK_OEM_7:      return Z_KEY_APOSTROPHE;
        case VK_OEM_COMMA:  return Z_KEY_COMMA;
        case VK_OEM_PERIOD: return Z_KEY_PERIOD;
        case VK_OEM_2:      return Z_KEY_SLASH;

        case VK_SNAPSHOT:   return Z_KEY_PRTSCN; // Doesn't work for some reason.
        case VK_SCROLL:     return Z_KEY_SCRLK;
        case VK_PAUSE:      return Z_KEY_PAUSE;

        case VK_NUMLOCK:    return Z_KEY_NUMLOCK;
        case VK_DIVIDE:     return Z_KEY_KP_DIVIDE;
        case VK_MULTIPLY:   return Z_KEY_KP_MULTIPLY;
        case VK_SUBTRACT:   return Z_KEY_KP_SUBTRACT;
        case VK_ADD:        return Z_KEY_KP_ADD;
        case VK_DECIMAL:    return Z_KEY_KP_DELETE;
        case VK_NUMPAD0:    return Z_KEY_KP_0;
        case VK_NUMPAD1:    return Z_KEY_KP_1;
        case VK_NUMPAD2:    return Z_KEY_KP_2;
        case VK_NUMPAD3:    return Z_KEY_KP_3;
        case VK_NUMPAD4:    return Z_KEY_KP_4;
        case VK_NUMPAD5:
        case VK_CLEAR:      return Z_KEY_KP_5;
        case VK_NUMPAD6:    return Z_KEY_KP_6;
        case VK_NUMPAD7:    return Z_KEY_KP_7;
        case VK_NUMPAD8:    return Z_KEY_KP_8;
        case VK_NUMPAD9:    return Z_KEY_KP_9;
    }

    // Some keys need to be mapped differently depending on the extended flag
    if ( is_extended ) {
        switch(wparam) {
            case VK_INSERT:  return Z_KEY_INSERT;
            case VK_HOME:    return Z_KEY_HOME;
            case VK_END:     return Z_KEY_END;
            case VK_PRIOR:   return Z_KEY_PGUP;
            case VK_NEXT:    return Z_KEY_PGDN;

            case VK_MENU:    return Z_KEY_RALT;
            case VK_CONTROL: return Z_KEY_RCTRL;
            case VK_DELETE:  return Z_KEY_DELETE;
            case VK_RETURN:  return Z_KEY_KP_ENTER;

            case VK_UP:      return Z_KEY_UP;
            case VK_DOWN:    return Z_KEY_DOWN;
            case VK_LEFT:    return Z_KEY_LEFT;
            case VK_RIGHT:   return Z_KEY_RIGHT;
        }
    } else {
        switch(wparam) {
            case VK_INSERT:  return Z_KEY_KP_0;
            case VK_HOME:    return Z_KEY_KP_7;
            case VK_END:     return Z_KEY_KP_1;
            case VK_PRIOR:   return Z_KEY_KP_9;
            case VK_NEXT:    return Z_KEY_KP_3;

            case VK_MENU:    return Z_KEY_LALT;
            case VK_CONTROL: return Z_KEY_LCTRL;
            case VK_DELETE:  return Z_KEY_KP_DELETE;
            case VK_RETURN:  return Z_KEY_ENTER;

            case VK_UP:      return Z_KEY_KP_8;
            case VK_DOWN:    return Z_KEY_KP_2;
            case VK_LEFT:    return Z_KEY_KP_4;
            case VK_RIGHT:   return Z_KEY_KP_6;
        }
    }

    return Z_KEY_UNKNOWN;
}



// Create a console and hook up stdout/stdin/stderr.
static void zInitConsole(void)
{
    int crt;

    // Create a console.
    AllocConsole();

    // Get C run-time file descriptors associated with our console output and hook them up to
    // stdout/err/in.
    crt = _open_osfhandle((intptr_t) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
    *stdout = *_fdopen(crt, "w");     // Get file descriptor and hook it up to stdout
    setvbuf(stdout, NULL, _IONBF, 0); // Set non-buffered

    // Same for stderr.
    crt = _open_osfhandle((intptr_t) GetStdHandle(STD_ERROR_HANDLE), _O_TEXT);
    *stderr = *_fdopen(crt, "w");
    setvbuf(stderr, NULL, _IONBF, 0);

    // Same for stdin.
    crt = _open_osfhandle((intptr_t) GetStdHandle(STD_INPUT_HANDLE), _O_TEXT);
    *stdin = *_fdopen(crt, "r");
    setvbuf(stdin, NULL, _IONBF, 0);

    console_enabled = 1;
}



// Application entry-point.
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    int res;

    timeBeginPeriod(1);

    // Always open a console for now. I should probably only open one by default for DEBUG builds,
    // but make it overrideable by some commandline option.
    zInitConsole();

    app_instance = hInstance;
    cmd_show = nCmdShow;

    res = zMain(0, NULL);

    // Silly hack so I get a chance to read console output on exit..
    getchar();

    return res;
}



void zEnableTextInput(ZTextInputCallback cb)
{
    assert(window_active);

    if (text_input) {
        // This should ideally not happen, but it should be safe to just re-assign the callback
        // pointer.. I think.
        zWarning("Overriding text input handler.");
        text_input_cb = cb;
        return;
    }

    assert(text_input_cb == NULL);
    text_input = 1;
    text_input_cb = cb;
}



void zDisableTextInput(void)
{
    assert(window_active);

    if (!text_input) {
        zDebug("Text input already disabled.");
    }

    text_input = 0;
    text_input_cb = NULL;
}



static int m_old_x, m_old_y;
static int mouse_stack; // See os.c

void zEnableMouse(void)
{
    POINT pt;

    assert(window_active);

    if (mouse_stack < 1) {

        ShowCursor(FALSE);

        // Save old cursor position.
        GetCursorPos(&pt);
        m_old_x = pt.x;
        m_old_y = pt.y;

        // Move cursor to middle of window.
        SetCursorPos((window_rect.left + window_rect.right) /2,
                     (window_rect.top  + window_rect.bottom)/2);
        mouse_active = 1;
    }

    mouse_stack++;
}



void zDisableMouse(void)
{
    assert(window_active);

    mouse_stack--;

    if (mouse_stack < 1) {
        // Restore mouse position.
        SetCursorPos(m_old_x, m_old_y);
        ShowCursor(TRUE);
        mouse_active = 0;
    }
}



// Window procedure.
LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    int is_keydown = 0;
    ZKeyEvent zkev;

    // Moved mouse event handling to a seperate function. Apparently HWHEEL falls outside
    // FIRST/LAST..
    if ( (message > WM_MOUSEFIRST && message < WM_MOUSELAST) || message == WM_MOUSEHWHEEL ) {

        if ( zHandleMouseEvents(message, wparam, lparam, &zkev) ) {
            //zDebug("Got mouse event, mapped to %s (%s, modmask %#x)",
            //    zKeyName(zkev.key), zkev.keystate == Z_KEY_STATE_PRESS ? "keydown" : "keyup",
            //    zkev.modmask);
            zDispatchKeyEvent(&zkev);
            return 0; // Event was handled so return.
        }
    }

    switch (message) {

    case WM_ACTIVATEAPP:
        // Release pressed keys if window is deactivated.
        if (!wparam) zReleaseKeys(0);
        break;

    case WM_SIZE:

        // Only call reshape if we are done setting up the window for OpenGL.
        if (window_active) {
            viewport_width = LOWORD(lparam);
            viewport_height = HIWORD(lparam);
            zReshapeViewport();
        }

        // Update window rectangle for mouse input code.
        GetWindowRect(window_handle, &window_rect);
        break;

    case WM_MOVE:
        // Same reason as WM_SIZE.
        GetWindowRect(window_handle, &window_rect);
        break;

    case WM_CLOSE:
        // User wants to close window, so just quit, window will be closed after we break out of the
        // main loop.
        running = 0;
        break;

    case WM_DESTROY:
        // WM_DESTROY should only have been triggered by DestroyWindow called by either CloseWindow,
        // or by CreateWindow when any of the additional steps to enable OpenGL fail. Either of
        // these functions should have done any neccesary clean up so nothing to do here.
        assert(!window_active);
        break;

#if 0
    case WM_DEADCHAR:
        //zDebug("WM_DEADCHAR");
        break;
    case WM_SYSDEADCHAR:
        //zDebug("WM_SYSDEADCHAR");
        break;
     case WM_SYSCHAR:
        //zDebug("WM_SYSCHAR");
        break;
#endif


    case WM_CHAR:
        // The preceeding key symbol call might have already caused text input to have been
        // disabled, so must check for this while handling WM_CHAR as well.
        if (text_input) {

            // Turn the incoming wide (UTF-16) character into UTF-8. This code assumes UNICODE is
            // defined and will probably break in some way if it isn't.
            WCHAR buf_in[2];
            char buf_out[10];
            int len;

            buf_in[0] = (WCHAR) wparam;
            buf_in[1] = 0;

            len = WideCharToMultiByte(CP_UTF8, 0, buf_in, -1, buf_out, 10, NULL, NULL);

            if (in_debug) {
                zDebug("%s: WM_CHAR: wparam = %#x", __func__, (unsigned int) wparam);
                zDebug("%s: WM_CHAR: buf_out = %s", __func__, buf_out);
                zDebug("%s: WM_CHAR: buf_out[0] = %#x", __func__, (unsigned int) buf_out[0]);
                zDebug("%s: WM_CHAR: buf_out[1] = %#x", __func__, (unsigned int) buf_out[1]);
                zDebug("%s: WM_CHAR: buf_out[2] = %#x", __func__, (unsigned int) buf_out[2]);
                zDebug("%s: WM_CHAR: buf_out[3] = %#x", __func__, (unsigned int) buf_out[3]);
            }

            assert(text_input_cb);

            if (len) text_input_cb(NULL, buf_out);
            else zWarning("%s: WM_CHAR: Error while converting to UTF-8.", __func__);
        }
        break;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        is_keydown = 1;

    case WM_KEYUP:
    case WM_SYSKEYUP:

        zkev.key = zTranslateWinKey(wparam, lparam);
        zkev.modmask = zGetWinModMask();
        zkev.keystate = is_keydown ? Z_KEY_STATE_PRESS : Z_KEY_STATE_RELEASE;

        if (text_input && is_keydown ) {

            // Call text input callback for with key symbol.
            assert(text_input_cb);
            text_input_cb(&zkev, NULL);

        } else {

            // Ignore repeating keydown events.
            if ( is_keydown && lparam & 0x40000000 ) break;

            zDispatchKeyEvent(&zkev);

            if (in_debug) {

                TCHAR keyname[200];
                GetKeyNameText(lparam, keyname, 200);

                zDebug("%s: %#x (%s), mapped to %s (modmask %#x)",
                    is_keydown ? "keydown" : "keyup", wparam,
                    keyname, zKeyName(zkev.key), zkev.modmask);
            }
        }
        break;

    default:
        return DefWindowProc(hwnd, message, wparam, lparam);
    }

    return 0;
}



void zSleep(float ms)
{
    // Don't bother sleeping < 1.0f
    if (ms < 1.0f) return;

    Sleep((unsigned int)ms);
}



// Get timestamp in ms.
float zGetTimeMS(void)
{
    static int base = 0;

    if (!base) {
        base = timeGetTime();
        return 0.0f;
    }

    return (float) (timeGetTime() - base);
}



// Process windowing events.
void zProcessEvents(void)
{
    MSG message;
    POINT pt;
    int win_middle_screen_x, win_middle_screen_y;

    assert(window_active);

    while ( PeekMessage(&message, window_handle, 0, 0, PM_REMOVE) ) {

        if (text_input)
            TranslateMessage(&message);

        DispatchMessage(&message);
    }

    if (mouse_active) {

        // Get middle of window in screen-coords.
        win_middle_screen_x = (window_rect.left + window_rect.right)/2;
        win_middle_screen_y = (window_rect.top + window_rect.bottom)/2;

        // Get cursor position.
        GetCursorPos(&pt);

        // Calculate motion vector.
        controller.yaw_delta = m_sensitivity * (pt.x - win_middle_screen_x);
        controller.pitch_delta = m_sensitivity * (pt.y - win_middle_screen_y);

        // Move cursor back to middle of window.
        SetCursorPos(win_middle_screen_x, win_middle_screen_y);

    } else {

        controller.yaw_delta = 0;
        controller.pitch_delta = 0;
    }
}



// Attempts to set the configured display mode and returns the dimensions of resulting display mode
// (so a window of the right size can be created). Returns TRUE on success, else FALSE. If the
// configured mode can't be set it will try to fallback a minimal mode.
static int zSetDisplayMode(int *screen_width, int *screen_height)
{
    DEVMODE dm;

    // Try to directly set configured mode first.
    dm.dmSize        = sizeof(DEVMODE);
    dm.dmDriverExtra = 0;
    dm.dmPelsWidth   = r_screenwidth;
    dm.dmPelsHeight  = r_screenheight;
    dm.dmFields      = DM_PELSWIDTH | DM_PELSHEIGHT;

    if (r_screenrefresh > 0) {
        dm.dmDisplayFrequency = r_screenrefresh;
        dm.dmFields |= DM_DISPLAYFREQUENCY;
    }

    if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL) {
        *screen_width  = r_screenwidth;
        *screen_height = r_screenheight;
        return TRUE;
    } else {

        int i = 0;

        // Setting the configured mode failed, fall back to minimal display mode
        zWarning("Failed to set the configured display mode, falling back to minimal mode.");
        memset(&dm, 0, sizeof(DEVMODE));
        dm.dmSize = sizeof(DEVMODE);
        dm.dmDriverExtra = 0;

#define MIN_WIDTH   800
#define MIN_HEIGHT  600
#define MIN_REFRESH 60
#define MIN_BPP     32 // 16 bpp modes make it impossible to alt-tab to desktop..

        // Find a reasonable mode that satisfies some basic constraints.
        while (EnumDisplaySettings(NULL, i++, &dm)) {

            if (r_windebug) {
                zDebug("devmode %d has width %d, height %d, refresh %d, bpp %d, flags %x", i,
                    dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayFrequency, dm.dmBitsPerPel,
                    dm.dmDisplayFlags);
            }

            if (dm.dmPelsWidth        >= MIN_WIDTH   && dm.dmPelsHeight >= MIN_HEIGHT &&
                dm.dmDisplayFrequency >= MIN_REFRESH && dm.dmBitsPerPel >= MIN_BPP) {

                if (ChangeDisplaySettings(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL) {

                    *screen_width  = dm.dmPelsWidth;
                    *screen_height = dm.dmPelsHeight;

                    // Also update the configuration I guess..
                    r_screenwidth   = dm.dmPelsWidth;
                    r_screenheight  = dm.dmPelsHeight;
                    r_screenrefresh = dm.dmDisplayFrequency;

                    if (r_windebug) zDebug("Fell back to mode %d", i);

                    return TRUE;
                }
            }
        }
        zError("Failed to fall back to minimal display mode.");
    }

    return FALSE;
}



#define WINDOW_STYLE       (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX)
#define WINDOW_STYLE_FS    (WS_POPUP|WS_VISIBLE|WS_SYSMENU)
#define EX_WINDOW_STYLE    (0)
#define EX_WINDOW_STYLE_FS (0)

// Open an OpenGL-enabled window.
void zOpenWindow(void)
{
    GLenum glew_err;
    WNDCLASSEX windowClass;
    PIXELFORMATDESCRIPTOR pfd;
    RECT rect;
    DWORD style, ex_style;
    int format;
    int width, height; // client-area dimensions
    int win_width, win_height; // adjusted window dimensions (includes window frame)

    assert(!window_active);

    windowClass.cbSize        = sizeof(WNDCLASSEX);
    windowClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc   = WndProc;
    windowClass.cbClsExtra    = 0;
    windowClass.cbWndExtra    = 0;
    windowClass.hInstance     = app_instance;
    windowClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    windowClass.lpszMenuName  = NULL;
    windowClass.lpszClassName = TEXT("MainWindow");
    windowClass.hIconSm       = LoadIcon(NULL, IDI_WINLOGO);

    if ( !(wndclass_atom = RegisterClassEx(&windowClass)) ) {
        zError("Failed to register window class.");
        goto zOpenWindow_0;
    }

    if (r_fullscreen) {
        style = WINDOW_STYLE_FS;
        ex_style = EX_WINDOW_STYLE_FS;

        if (!zSetDisplayMode(&width, &height)) {
            zError("Failed to set display mode.");
            goto zOpenWindow_0;
        }
    } else {
        style = WINDOW_STYLE;
        ex_style = EX_WINDOW_STYLE;
        width = r_winwidth;
        height = r_winheight;
    }

    // Calculate the required window size (which includes window border) for the client-area I want.
    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;

    AdjustWindowRect(&rect, style, FALSE);

    win_width  = rect.right  - rect.left;
    win_height = rect.bottom - rect.top;

    window_handle = CreateWindowEx(
        ex_style,                     // Extended style
        TEXT("MainWindow"),           // Class name
        TEXT(PACKAGE_STRING),         // Window name
        style,                        // Window style
        CW_USEDEFAULT, CW_USEDEFAULT, // Window coords
        win_width, win_height,        // Dimensions
        NULL,                         // Parent handle
        NULL,                         // Menu handle
        app_instance,                 // Application instance
        NULL);                        // Etra parameters

    if ( !window_handle ) {
        zError("Failed to create window.");
        goto zOpenWindow_1;
    }

    if ( NULL == (device_context = GetDC(window_handle)) ) {
        zError("Failed to get DC from window.");
        goto zOpenWindow_1;
    }

    ZeroMemory(&pfd, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    if (r_stencilbuffer) pfd.cStencilBits = 24;
    if (r_depthbuffer)   pfd.cDepthBits   = 24;
    if (r_doublebuffer)  pfd.dwFlags     |= PFD_DOUBLEBUFFER;

    format = ChoosePixelFormat(device_context, &pfd);

    // Attempt to set the pixel format.
    if ( !format || FALSE == SetPixelFormat(device_context, format, &pfd) ) {
        zError("Failed to choose or set pixel format.");
        goto zOpenWindow_2;
    }

    // Create rendering context.
    if ( NULL == (render_context = wglCreateContext(device_context)) ) {
        zError("Failed to create rendering context.");
        goto zOpenWindow_2;
    }

    if ( !wglMakeCurrent(device_context, render_context) ) {
        zError("Failed to make rendering context current.");
        goto zOpenWindow_3;
    }

    viewport_width = width;
    viewport_height = height;


    // Init glew, check for required OpenGL support.
    if ( GLEW_OK != (glew_err = glewInit()) ) {
        zError("Failed to initialize glew. %s", glewGetErrorString(glew_err));
        goto zOpenWindow_3;
    }

    // Doesn't seem to work right on Vista, unless the intel driver really doesn't support OpenGL
    // 1.4..
    //if (!zCheckOpenGLSupport())
    //    goto zOpenWindow_3;

    ShowWindow(window_handle, cmd_show);
    UpdateWindow(window_handle);
    GetWindowRect(window_handle, &window_rect);

    window_active = 1;

    zRendererInit();
    return;

zOpenWindow_3:
    wglDeleteContext(render_context);
zOpenWindow_2:
    ReleaseDC(window_handle, device_context);
zOpenWindow_1:
    DestroyWindow(window_handle);
zOpenWindow_0:
    zFatal("Failed to open window.");
    zShutdown();
    exit(EXIT_FAILURE);
}



void zCloseWindow()
{
    assert(window_active);

    zRendererDeinit();

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(render_context);
    ReleaseDC(window_handle, device_context);

    window_active = 0;

    DestroyWindow(window_handle);

    UnregisterClass((LPCTSTR)wndclass_atom, app_instance);

    // Restore display mode.
    ChangeDisplaySettings(NULL, 0);
}



void zSetFullscreen(int state)
{
    // There are some issues with changing window to/from fullscreen on the fly where I can't get it
    // to update the window borders correctly (even after calling SetWindowPos)..
    zError("Changing fullscreen on-the-fly is not supported. Use r_fullscreen and restartvideo()"
        " instead.");
}



void zSwapInterval(int interval)
{
    assert(window_active);

    if (WGLEW_EXT_swap_control)
        wglSwapIntervalEXT(interval);
    else
        zError("Swap control not supported, unable to set interval.");
}



void zSwapBuffers()
{
    assert(window_active);

    SwapBuffers(device_context);
}



void zShutdown(void)
{
    timeEndPeriod(1);
}



// Since storing stuff directly under the user directory on Windows isn't really appropriate, I will
// use My Documents (CSIDL_PERSONAL) instead.
// More info: http://msdn.microsoft.com/en-us/library/bb762181(VS.85).aspx
char *zGetUserDir(void)
{
    static int initialized;
    static char userdir[Z_PATH_SIZE];

    if (!initialized) {

        WCHAR path[MAX_PATH];

        // On Vista+ SHGetFolderPath has been deprecated and turned into a wrapper around the new
        // Known Folders system, see http://msdn.microsoft.com/en-us/library/bb776911(VS.85).aspx, I
        // might want to add support for that at some point (though for now Vista/Win7 are backward
        // compatible..)

        // Using the Unicode variant explicitly since I need to know what type of character I am
        // working with for conversion to UTF-8.
        if ( SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, path) == S_OK ) {

            // Convert to UTF8 encoded string.
            if ( WideCharToMultiByte(CP_UTF8, 0, path, -1, userdir, Z_PATH_SIZE, NULL, NULL) == 0 )
            {
                zWarning("Failed to convert home directory string to UTF-8, using empty string");
                userdir[0] = '\0';
            } else {

                // Append app name.
                if ( (strlen(userdir) + strlen(Z_DIR_USERDATA) + 1) < Z_PATH_SIZE ) {

                    strcat(userdir, Z_DIR_SEPARATOR);
                    strcat(userdir, Z_DIR_USERDATA);

                    // Make sure the directory exists (create if not) and that it is a directory.
                    if (!zPathExists(userdir)) {

                        WCHAR userdir_w[MAX_PATH];

                        if ( !MultiByteToWideChar(CP_UTF8, 0, userdir, -1, userdir_w, MAX_PATH) ) {
                            zError("%s: Character set conversion for path \"%s\" failed", __func__,
                                path);
                            userdir[0] = '\0';
                        } else {
                            zPrint("Creating user data directory at \"%s\".\n", userdir);
                            if (!CreateDirectoryW(userdir_w, NULL)) {
                                zWarning("Failed to create user directory.");
                                userdir[0] = '\0';
                            }
                        }
                    } else if (zPathExists(userdir) != Z_EXISTS_DIR) {
                        zWarning("User directory \"%s\" exists but is not a directory.", userdir);
                        userdir[0] = '\0';
                    }

                } else {
                    zWarning("Exceeded buffer size while building userdir, using empty string.");
                    userdir[0] = '\0';
                }
            }
        } else {
            zWarning("Failed to look up home directory, using empty string.");
        }

        initialized = 1;
    }

    // Only return if it's not an empty string.
    if (*userdir) return userdir;

    return NULL;
}



int zPathExists(const char *path)
{
    WCHAR pathwide[MAX_PATH];
    DWORD attrib;

    if ( !MultiByteToWideChar(CP_UTF8, 0, path, -1, pathwide, MAX_PATH) ) {
        zError("%s: Character set conversion for path \"%s\" failed", __func__, path);
        return 0;
    }

    if ( (attrib = GetFileAttributesW(pathwide)) != INVALID_FILE_ATTRIBUTES ) {

        if ( attrib & FILE_ATTRIBUTE_DIRECTORY ) {
            return Z_EXISTS_DIR;
        } else {
            // Just checking for the NORMAL attribute doesn't work, for some reason.. So I'll just
            // assume it is a regular file when DIRECTORY is not set.
            return Z_EXISTS_REGULAR;
        }
    } else {

        DWORD err = GetLastError();

        // I suppose I should warn if the error wasn't simply about the file not existing..
        if ( err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND )
            zError("%s: Failed to read file attributes for \"%s\". err = %d", __func__, path, err);
    }

    return 0;
}



char *zGetFileFromDir(const char *path)
{
    int len;
    static int start = 1;
    static char dir_path[MAX_PATH];  // Full path to the directory; sysdir + path
    static char file_path[MAX_PATH];  // Temp storage for full_path+filename
    static HANDLE handle = INVALID_HANDLE_VALUE;
    WIN32_FIND_DATAW data;

    if (start) {

        WCHAR search_path[MAX_PATH];

        // Construct directory search pattern.
        if ( (strlen(path) + strlen(Z_DIR_SYSDATA) + 3) > (MAX_PATH-1) ) {
            zWarning("Failed to open directory \"%s\", path length exceeded MAX_PATH.", path);
            return NULL;
        }
        dir_path[0] = '\0';
        strcat(dir_path, Z_DIR_SYSDATA);
        strcat(dir_path, Z_DIR_SEPARATOR);
        strcat(dir_path, path);
        strcat(dir_path, Z_DIR_SEPARATOR);
        strcat(dir_path, "*");

        // Only really needed for 'path', but since I need to work on a copy of it I'll just do it
        // this way.
        zRewriteDirsep(dir_path);

        // Convert search path to wide characters.
        if ( !MultiByteToWideChar(CP_UTF8, 0, dir_path, -1, search_path, MAX_PATH) ) {
            zError("%s: Character set conversion for path \"%s\" failed", __func__, path);
            return NULL;
        }

        handle = FindFirstFileW(search_path, &data);

        // At this point I remove the trailing "*" from dir_path, so I can reuse to append the
        // filename to later on.
        len = strlen(dir_path);
        assert(len > 0);
        dir_path[len-1] = '\0';

        if (handle == INVALID_HANDLE_VALUE) {
            zWarning("Failed to open directory \"%s\". last_error = %d", path, GetLastError());
            return NULL;
        }
    }

    // Iterate over dir entries, return once a regular file is found. Prevent calling FindNextFile
    // on first call, since data will have been filled in by FindFirstFile.
    while (start || FindNextFileW(handle, &data)) {

        char filename[Z_PATH_SIZE];

        start = 0;

        // Convert data.cFileName to UTF8.
        if ( !WideCharToMultiByte(CP_UTF8, 0, data.cFileName, -1, filename, Z_PATH_SIZE, NULL, NULL)
                ) {
            zError("%s: Character set conversion failed while reading directory \"%s\".", __func__,
                dir_path);
            break;
        }
        // Append filename to dir_path.
        if ( (strlen(dir_path) + strlen(filename)) > (MAX_PATH-1) ) {
            zError("%s: file_path would exceed MAX_PATH while reading directory \"%s\".", __func__,
                dir_path);
            break;
        }

        file_path[0] = '\0';
        strcat(file_path, dir_path);
        strcat(file_path, filename);

        if (zPathExists(file_path) == Z_EXISTS_REGULAR)
            return file_path;
    }

    // FindNextFile returned false (= end of dir?) or some error occured, time to close..
    FindClose(handle);
    start = 1;

    return NULL;
}


