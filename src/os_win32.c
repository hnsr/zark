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
// the mouse input code.
static RECT window_rect;


//#define WINDOW_STYLE (WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU)
#define WINDOW_STYLE (WS_OVERLAPPEDWINDOW | WS_SYSMENU)


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
    zkev->key = KEY_UNKNOWN;

    switch (message) {

    // Mouse 1
    case WM_LBUTTONDOWN:
        zkev->key = KEY_MOUSE1;
        zkev->keystate = Z_KEY_STATE_PRESS;
        break;
    case WM_LBUTTONUP:
        zkev->key = KEY_MOUSE1;
        zkev->keystate = Z_KEY_STATE_RELEASE;
        break;

    // Mouse 2
    case WM_MBUTTONDOWN:
        zkev->key = KEY_MOUSE2;
        zkev->keystate = Z_KEY_STATE_PRESS;
        break;
    case WM_MBUTTONUP:
        zkev->key = KEY_MOUSE2;
        zkev->keystate = Z_KEY_STATE_RELEASE;
        break;

    // Mouse 3
    case WM_RBUTTONDOWN:
        zkev->key = KEY_MOUSE3;
        zkev->keystate = Z_KEY_STATE_PRESS;
        break;
    case WM_RBUTTONUP:
        zkev->key = KEY_MOUSE3;
        zkev->keystate = Z_KEY_STATE_RELEASE;
        break;

    // Mouse 8/9
    case WM_XBUTTONDOWN:
        if (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) zkev->key = KEY_MOUSE8;
        else                                        zkev->key = KEY_MOUSE9;
        zkev->keystate = Z_KEY_STATE_PRESS;
        break;
    case WM_XBUTTONUP:
        if (GET_XBUTTON_WPARAM(wparam) == XBUTTON1) zkev->key = KEY_MOUSE8;
        else                                        zkev->key = KEY_MOUSE9;
        zkev->keystate = Z_KEY_STATE_RELEASE;
        break;

    // Mouse 4/5
    case WM_MOUSEWHEEL:
        zkev->keystate = Z_KEY_STATE_PRESS;
        if ( GET_WHEEL_DELTA_WPARAM(wparam) < 0 ) zkev->key = KEY_MOUSE5;
        else                                      zkev->key = KEY_MOUSE4;
        break;

    // Mouse 6/7
    case WM_MOUSEHWHEEL:
        zkev->keystate = Z_KEY_STATE_PRESS;
        if ( GET_WHEEL_DELTA_WPARAM(wparam) < 0 ) zkev->key = KEY_MOUSE7;
        else                                      zkev->key = KEY_MOUSE6;
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
    if      (wparam >= 'A' && wparam <= 'Z') return KEY_A + (wparam - 'A');
    else if (wparam >= '0' && wparam <= '9') return KEY_0 + (wparam - '0');

    // Do the rest by hand.
    switch (wparam) {

        case VK_ESCAPE:     return KEY_ESCAPE;
        case VK_F1:         return KEY_F1;
        case VK_F2:         return KEY_F2;
        case VK_F3:         return KEY_F3;
        case VK_F4:         return KEY_F4;
        case VK_F5:         return KEY_F5;
        case VK_F6:         return KEY_F6;
        case VK_F7:         return KEY_F7;
        case VK_F8:         return KEY_F8;
        case VK_F9:         return KEY_F9;
        case VK_F10:        return KEY_F10;
        case VK_F11:        return KEY_F11;
        case VK_F12:        return KEY_F12;

        case VK_OEM_3:      return KEY_GRAVE;
        case VK_OEM_MINUS:  return KEY_MINUS;
        case VK_OEM_PLUS:   return KEY_EQUALS;
        case VK_BACK:       return KEY_BACKSPACE;
        case VK_TAB:        return KEY_TAB;
        case VK_CAPITAL:    return KEY_CAPSLOCK;

        // Windows doesn't seem to give a way to differentiate left/right for SHIFT.
        case VK_SHIFT:      return KEY_LSHIFT;

        case VK_LWIN:       return KEY_LSUPER;
        case VK_SPACE:      return KEY_SPACEBAR;

        case VK_RWIN:       return KEY_RSUPER;
        case VK_APPS:       return KEY_MENU;

        case VK_OEM_5:      return KEY_BACKSLASH;
        case VK_OEM_4:      return KEY_LSQBRACKET;
        case VK_OEM_6:      return KEY_RSQBRACKET;
        case VK_OEM_1:      return KEY_SEMICOLON;
        case VK_OEM_7:      return KEY_APOSTROPHE;
        case VK_OEM_COMMA:  return KEY_COMMA;
        case VK_OEM_PERIOD: return KEY_PERIOD;
        case VK_OEM_2:      return KEY_SLASH;

        case VK_SNAPSHOT:   return KEY_PRTSCN; // Doesn't work for some reason.
        case VK_SCROLL:     return KEY_SCRLK;
        case VK_PAUSE:      return KEY_PAUSE;

        case VK_NUMLOCK:    return KEY_NUMLOCK;
        case VK_DIVIDE:     return KEY_KP_DIVIDE;
        case VK_MULTIPLY:   return KEY_KP_MULTIPLY;
        case VK_SUBTRACT:   return KEY_KP_SUBTRACT;
        case VK_ADD:        return KEY_KP_ADD;
        case VK_DECIMAL:    return KEY_KP_DELETE;
        case VK_NUMPAD0:    return KEY_KP_0;
        case VK_NUMPAD1:    return KEY_KP_1;
        case VK_NUMPAD2:    return KEY_KP_2;
        case VK_NUMPAD3:    return KEY_KP_3;
        case VK_NUMPAD4:    return KEY_KP_4;
        case VK_NUMPAD5:
        case VK_CLEAR:      return KEY_KP_5;
        case VK_NUMPAD6:    return KEY_KP_6;
        case VK_NUMPAD7:    return KEY_KP_7;
        case VK_NUMPAD8:    return KEY_KP_8;
        case VK_NUMPAD9:    return KEY_KP_9;
    }

    // Some keys need to be mapped differently depending on the extended flag
    if ( is_extended ) {
        switch(wparam) {
            case VK_INSERT:  return KEY_INSERT;
            case VK_HOME:    return KEY_HOME;
            case VK_END:     return KEY_END;
            case VK_PRIOR:   return KEY_PGUP;
            case VK_NEXT:    return KEY_PGDN;

            case VK_MENU:    return KEY_RALT;
            case VK_CONTROL: return KEY_RCTRL;
            case VK_DELETE:  return KEY_DELETE;
            case VK_RETURN:  return KEY_KP_ENTER;

            case VK_UP:      return KEY_UP;
            case VK_DOWN:    return KEY_DOWN;
            case VK_LEFT:    return KEY_LEFT;
            case VK_RIGHT:   return KEY_RIGHT;
        }
    } else {
        switch(wparam) {
            case VK_INSERT:  return KEY_KP_0;
            case VK_HOME:    return KEY_KP_7;
            case VK_END:     return KEY_KP_1;
            case VK_PRIOR:   return KEY_KP_9;
            case VK_NEXT:    return KEY_KP_3;

            case VK_MENU:    return KEY_LALT;
            case VK_CONTROL: return KEY_LCTRL;
            case VK_DELETE:  return KEY_KP_DELETE;
            case VK_RETURN:  return KEY_ENTER;

            case VK_UP:      return KEY_KP_8;
            case VK_DOWN:    return KEY_KP_2;
            case VK_LEFT:    return KEY_KP_4;
            case VK_RIGHT:   return KEY_KP_6;
        }
    }
    
    return KEY_UNKNOWN;
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
    timeBeginPeriod(1);

    // TODO: Autoexec not loaded at this point so I can't use something like a 'showconsole'
    // variable. For now I'll only open one bydefault in DEBUG builds. For release builds I should
    // maybe check a command line flag (-console) or something.
#ifdef DEBUG
    zInitConsole();
#else
    // TODO: Check for -console flag
    zInitConsole();
#endif

    app_instance = hInstance;
    cmd_show = nCmdShow;

    return zMain(0, NULL);
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
        return 0;
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



// Open an OpenGL-enabled window.
void zOpenWindow(int width, int height)
{
    GLenum glew_err;
    WNDCLASSEX windowClass;
    PIXELFORMATDESCRIPTOR pfd;
    RECT rect;
    int format;
    int win_width, win_height;

    assert(!window_active);

    windowClass.cbSize        = sizeof(WNDCLASSEX);
    windowClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc   = WndProc;
    windowClass.cbClsExtra    = 0;
    windowClass.cbWndExtra    = 0;
    windowClass.hInstance     = app_instance;
    windowClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    windowClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    windowClass.lpszMenuName  = NULL;
    windowClass.lpszClassName = TEXT("MainWindow");
    windowClass.hIconSm       = LoadIcon(NULL, IDI_WINLOGO);

    if ( !(wndclass_atom = RegisterClassEx(&windowClass)) ) {

        zError("Failed to register window class.");
        goto zOpenWindow_0;
    }

    // Calculate the required window size (which includes window border) for the client-area I want.
    rect.left = 0;
    rect.top = 0;
    rect.right = width;
    rect.bottom = height;

    AdjustWindowRect(&rect, WINDOW_STYLE, FALSE);

    win_width  = rect.right  - rect.left;
    win_height = rect.bottom - rect.top;

    window_handle = CreateWindowEx(
        0,                            // Extended style
        TEXT("MainWindow"),           // Class name
        TEXT(PACKAGE_STRING),         // Window name
        WINDOW_STYLE,                 // Window style
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

    if (r_stencilbuffer)
        pfd.cStencilBits = 24;

    if (r_depthbuffer)
        pfd.cDepthBits = 24;

    if (r_doublebuffer)
        pfd.dwFlags |= PFD_DOUBLEBUFFER;

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
#define USERDIR_MAX 1024
char *zGetUserDir(void)
{
    static int initialized;
    static char userdir[USERDIR_MAX];

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
            if ( WideCharToMultiByte(CP_UTF8, 0, path, -1, userdir, USERDIR_MAX, NULL, NULL) == 0 ) {

                zWarning("Failed to convert home directory string to UTF-8, using empty string");
                userdir[0] = '\0';

            } else {
                //zDebug("Using home directory \"%s\".", userdir);

                // Append app name.
                if ( (strlen(userdir) + strlen(Z_DIR_USERDATA) + 1) < USERDIR_MAX ) {
                    strcat(userdir, Z_DIR_SEPARATOR);
                    strcat(userdir, Z_DIR_USERDATA);
                } else {
                    zWarning("Documents + user data directory exceeded buffer dize, using empty"
                        " string.");
                    userdir[0] = '\0';
                }

                //zDebug("Using user data directory \"%s\".", userdir);
            }

        } else {

            zWarning("Failed to look up home directory, using empty string.");
        }

        initialized = 1;
    }

    return userdir;
}



int zFileExists(char *path)
{
    WCHAR pathwide[MAX_PATH];
    DWORD res;

    // Transform path to wide characters and try to read its attributes.
    if ( MultiByteToWideChar(CP_UTF8, 0, path, -1, pathwide, MAX_PATH) == 0 ) {
        
        zError("%s: Failed to check file existance because character set conversion for path \"%s\""
            " failed", __func__, path);
        return 0;

    } else {

        //zDebug("%s: Converted path \"%s\" to wide chars", __func__, path);

        if ( (res = GetFileAttributesW(pathwide)) != INVALID_FILE_ATTRIBUTES ) {

            if ( res != FILE_ATTRIBUTE_DIRECTORY ) {

                //zDebug("%s: \"%s\" is not a directory, so returning 1", __func__, path);
                return 1;

            } else {

                //zDebug("%s: \"%s\" is a directory, returning 0", __func__, path);
                return 0;
            }

        } else {

            //zDebug("%s: Failed to read file attributes for \"%s\"", __func__, path);
            return 0;
        }
    }

    return 0;
}

