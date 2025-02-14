#define _POSIX_C_SOURCE 199309L // For nanosleep (from time.h)
#define __USE_POSIX             // For signal stuff

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>      // nanosleep
#include <signal.h>
#include <locale.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h> // required by sys/stat.h?
#include <sys/stat.h>  // stat
#include <sys/time.h>  // gettimeofday
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrandr.h>
#include <GL/glew.h>
#include <dirent.h> // opendir etc.

// older versions of glxew.h have a stray 'uint' in them, make sure compiler doesn't error out on it
// (was only an issue for me with -std=c99)
#ifndef uint
#define uint unsigned int
#endif
#include <GL/glxew.h>
#undef uint

// Needed for Xutf8LookupString
#ifndef X_HAVE_UTF8_STRING
#error X(lib) has no UTF-8 support.
#endif

#include "common.h"


/* Some notes on X keyboard handling:
 *
 * A physical key on the keyboard is associated with some keycode, a keycode is associated with a
 * list of keysyms for all the different symbols on the key (which symbols these are I assume
 * depends on the keymap). By some weird algorithm [1] this list of keysyms, together with the state
 * of the modifier keys (numlock, caps lock, etc) and compose key, are used to figure out what
 * printable character (if any) a given key press should produce. I think XLookupString (or maybe
 * its internationalized variants [2]) can be used to do this.
 *
 * However! For non-textual keyboard input, I am only interested in the physical key.. and since I
 * suspect that I can't depend on the keycode for this, I will simply use the first (and primary?)
 * KeySym from the keysym list (which I think I can do with XKeycodeToKeysym) and use that to map to
 * our own key enum.
 *
 * [1] http://tronche.com/gui/x/xlib/input/keyboard-encoding.html
 * [2] http://www.sbin.org/doc/Xlib/chapt_11.html
 */

static Display *dpy;
static int screen;
static Window wnd, root;
static XIM im;
static XIC ic;
static Atom atom_wmdelete;
static GLXContext context;
static int window_active; // Indicates if an OpenGL-enabled windows has been opened.
static int mouse_active;  // Indicates if we're reading mouse motion.
static int fullscreen = 0;

static int restore_mode;
static RRMode mode_old = None;
static RRMode mode_new = None;
static RRCrtc mode_crtc = None;

static int randr_event_base;
static int randr_error_base;

static void zHandleXrandrEvent(XEvent *ev, int type);
static int zXrandrSupported(void);


// Default event mask for the window. Needed in zOpenWindow, and in zSetupIM to augment event mask
// with those the IM requires.
static unsigned int default_event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask |
    ButtonReleaseMask | PointerMotionMask | StructureNotifyMask | FocusChangeMask;


// Attempt to do a clean shutdown when we're being terminated.
static void sighandler(int sig)
{
    running = 0;
}


// Application entry-point.
int main(int argc, char** argv)
{
    struct sigaction sa;
    char *lc;

    // I install a sigint handler so I get chance to exit cleanly when being terminated, this is
    // important for restoring things like X keyboard auto-repeat.
    // This doesn't actually handle being terminated with other singals than SIGINT but for some
    // reason keyboard auto-repeat gets restored to the right setting anyway in that case.. (which
    // wasn't the case whith SIGINT)
    sa.sa_handler = sighandler;
    sa.sa_flags = 0;
    sigemptyset(&(sa.sa_mask));
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGABRT, &sa, NULL);

    // Set user's locale. This is important for i18ned text input handling.
    lc = setlocale(LC_ALL, "");
    if (!lc) {
        zWarning("Failed to set user locale.");
    } else {
        //zDebug("Locale set to %s.", lc);
    }


    return zMain(argc, argv);
}



void zSleep(float ms)
{
    static struct timespec ts = { 0, 0 };
    ts.tv_nsec = (unsigned int) ms*1000000;

    nanosleep(&ts, NULL);
}



float zGetTimeMS(void)
{
    static int initialised = 0;
    static unsigned int base_sec, base_usec;

    static struct timeval tv;

    gettimeofday(&tv, NULL);

    if (!initialised) {

        base_sec = tv.tv_sec;
        base_usec = tv.tv_usec;
        initialised = 1;
        return 0.0f;
    }

    return (float) ((tv.tv_sec - base_sec)*1000  +  (tv.tv_usec-base_usec)/1000);
}



// Translate X modifier mask to ZkeyEvent modmask.
static unsigned int zTranslateXModMask(unsigned int state)
{
    unsigned int zmask = 0;

    if (state & ShiftMask)   zmask |= Z_KEY_MOD_SHIFT;
    if (state & ControlMask) zmask |= Z_KEY_MOD_CTRL;
    if (state & Mod1Mask)    zmask |= Z_KEY_MOD_LALT;
    if (state & Mod4Mask)    zmask |= Z_KEY_MOD_SUPER;
    if (state & Mod5Mask)    zmask |= Z_KEY_MOD_RALT;

    return zmask;
}



// Translate X KeySym to a ZKey
static ZKey zTranslateXKey(KeySym keysym)
{
    switch (keysym) {

        case XK_q: return Z_KEY_Q;
        case XK_w: return Z_KEY_W;
        case XK_e: return Z_KEY_E;
        case XK_r: return Z_KEY_R;
        case XK_t: return Z_KEY_T;
        case XK_y: return Z_KEY_Y;
        case XK_u: return Z_KEY_U;
        case XK_i: return Z_KEY_I;
        case XK_o: return Z_KEY_O;
        case XK_p: return Z_KEY_P;
        case XK_a: return Z_KEY_A;
        case XK_s: return Z_KEY_S;
        case XK_d: return Z_KEY_D;
        case XK_f: return Z_KEY_F;
        case XK_g: return Z_KEY_G;
        case XK_h: return Z_KEY_H;
        case XK_j: return Z_KEY_J;
        case XK_k: return Z_KEY_K;
        case XK_l: return Z_KEY_L;
        case XK_z: return Z_KEY_Z;
        case XK_x: return Z_KEY_X;
        case XK_c: return Z_KEY_C;
        case XK_v: return Z_KEY_V;
        case XK_b: return Z_KEY_B;
        case XK_n: return Z_KEY_N;
        case XK_m: return Z_KEY_M;

        case XK_Escape: return Z_KEY_ESCAPE;
        case XK_F1:     return Z_KEY_F1;
        case XK_F2:     return Z_KEY_F2;
        case XK_F3:     return Z_KEY_F3;
        case XK_F4:     return Z_KEY_F4;
        case XK_F5:     return Z_KEY_F5;
        case XK_F6:     return Z_KEY_F6;
        case XK_F7:     return Z_KEY_F7;
        case XK_F8:     return Z_KEY_F8;
        case XK_F9:     return Z_KEY_F9;
        case XK_F10:    return Z_KEY_F10;
        case XK_F11:    return Z_KEY_F11;
        case XK_F12:    return Z_KEY_F12;

        case XK_grave:      return Z_KEY_GRAVE;
        case XK_1:          return Z_KEY_1;
        case XK_2:          return Z_KEY_2;
        case XK_3:          return Z_KEY_3;
        case XK_4:          return Z_KEY_4;
        case XK_5:          return Z_KEY_5;
        case XK_6:          return Z_KEY_6;
        case XK_7:          return Z_KEY_7;
        case XK_8:          return Z_KEY_8;
        case XK_9:          return Z_KEY_9;
        case XK_0:          return Z_KEY_0;
        case XK_minus:      return Z_KEY_MINUS;
        case XK_equal:      return Z_KEY_EQUALS;
        case XK_BackSpace:  return Z_KEY_BACKSPACE;
        case XK_Tab:        return Z_KEY_TAB;
        case XK_Caps_Lock:  return Z_KEY_CAPSLOCK;
        case XK_Shift_L:    return Z_KEY_LSHIFT;
        case XK_Control_L:  return Z_KEY_LCTRL;
        case XK_Super_L:    return Z_KEY_LSUPER;
        case XK_Alt_L:      return Z_KEY_LALT;
        case XK_space:      return Z_KEY_SPACEBAR;

        case XK_ISO_Level3_Shift: return Z_KEY_RALT;

        case XK_Super_R:        return Z_KEY_RSUPER;
        case XK_Menu:           return Z_KEY_MENU;
        case XK_Control_R:      return Z_KEY_RCTRL;
        case XK_Shift_R:        return Z_KEY_RSHIFT;
        case XK_Return:         return Z_KEY_ENTER;
        case XK_backslash:      return Z_KEY_BACKSLASH;
        case XK_bracketleft:    return Z_KEY_LSQBRACKET;
        case XK_bracketright:   return Z_KEY_RSQBRACKET;
        case XK_semicolon:      return Z_KEY_SEMICOLON;
        case XK_apostrophe:     return Z_KEY_APOSTROPHE;
        case XK_comma:          return Z_KEY_COMMA;
        case XK_period:         return Z_KEY_PERIOD;
        case XK_slash:          return Z_KEY_SLASH;

        // Print should be the right one here, I'll leave SysRq here just in-case..
        case XK_Sys_Req:
        case XK_Print:          return Z_KEY_PRTSCN;

        case XK_Scroll_Lock:    return Z_KEY_SCRLK;
        case XK_Pause:          return Z_KEY_PAUSE;
        case XK_Insert:         return Z_KEY_INSERT;
        case XK_Delete:         return Z_KEY_DELETE;
        case XK_Home:           return Z_KEY_HOME;
        case XK_End:            return Z_KEY_END;
        case XK_Prior:          return Z_KEY_PGUP;
        case XK_Next:           return Z_KEY_PGDN;

        case XK_Up:             return Z_KEY_UP;
        case XK_Down:           return Z_KEY_DOWN;
        case XK_Left:           return Z_KEY_LEFT;
        case XK_Right:          return Z_KEY_RIGHT;

        case XK_Num_Lock:       return Z_KEY_NUMLOCK;
        case XK_KP_Divide:      return Z_KEY_KP_DIVIDE;
        case XK_KP_Multiply:    return Z_KEY_KP_MULTIPLY;
        case XK_KP_Subtract:    return Z_KEY_KP_SUBTRACT;
        case XK_KP_Add:         return Z_KEY_KP_ADD;
        case XK_KP_Enter:       return Z_KEY_KP_ENTER;
        case XK_KP_Delete:      return Z_KEY_KP_DELETE;
        case XK_KP_Insert:      return Z_KEY_KP_0;
        case XK_KP_End:         return Z_KEY_KP_1;
        case XK_KP_Down:        return Z_KEY_KP_2;
        case XK_KP_Next:        return Z_KEY_KP_3;
        case XK_KP_Left:        return Z_KEY_KP_4;
        case XK_KP_Begin:       return Z_KEY_KP_5;
        case XK_KP_Right:       return Z_KEY_KP_6;
        case XK_KP_Home:        return Z_KEY_KP_7;
        case XK_KP_Up:          return Z_KEY_KP_8;
        case XK_KP_Prior:       return Z_KEY_KP_9;
    }

    return Z_KEY_UNKNOWN;
}



static int m_old_x, m_old_y;
static Window m_old_root;

// Create an empty cursor for when we want to hide the mouse when recording motion.
static Cursor zCreateEmptyCursor(void)
{
    Pixmap curmask;
    XGCValues gcvalues;
    GC gc;
    XColor color;
    Cursor cursor;

    // The only way to hide the cursor is to define my own empty cursor
    curmask = XCreatePixmap(dpy, root, 1, 1, 1);
    gcvalues.function = GXclear;
    gc = XCreateGC(dpy, curmask, GCFunction, &gcvalues);
    XFillRectangle(dpy, curmask, gc, 0, 0, 1, 1);
    color.pixel = 0;
    color.red = 0;
    color.flags = DoRed;
    cursor = XCreatePixmapCursor(dpy, curmask, curmask, &color, &color, 0, 0);
    XFreeGC(dpy, gc);
    XFreePixmap(dpy, curmask);

    return cursor;
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
    XSetICFocus(ic);
}



void zDisableTextInput(void)
{
    assert(window_active);

    if (!text_input) {
        zDebug("Text input already disabled.");
    }

    text_input = 0;
    text_input_cb = NULL;
    XUnsetICFocus(ic);
}



// Use a kind of stack mechanism to keep track of how many times the mouse was enabled, only disable
// when it reaches 0 again. This way when for example multiple keybindings activate the mouse,
// releasing one while still holding the other won't cuase the mouse to be disabled prematurely.
static int mouse_stack;

void zEnableMouse(void)
{
    // Dummy vars to store XQueryPointer result
    Window child;
    int win_x, win_y;
    unsigned int mask;

    assert(window_active);

    if (mouse_stack < 1) {

        // Save old cursor position (relative to root window), hide curser, warp it to centre of
        // window.
        XQueryPointer(dpy, wnd, &m_old_root, &child, &m_old_x, &m_old_y, &win_x, &win_y, &mask);
        XDefineCursor(dpy, wnd, zCreateEmptyCursor());
        XWarpPointer(dpy, None, wnd, 0,0,0,0, viewport_width/2, viewport_height/2);
        XSync(dpy, False); // This is important, but I forgot why ..
        mouse_active = 1;
    }

    mouse_stack++;
}



void zDisableMouse(void)
{
    assert(window_active);

    mouse_stack--;

    if (mouse_stack < 1) {

        // Restore mouse position and default cursor.
        XWarpPointer(dpy, None, m_old_root, 0,0,0,0, m_old_x, m_old_y);
        XUndefineCursor(dpy, wnd);
        mouse_active = 0;
    }
}



// Called by XCheckIfEvent to check for a key press that matches a key release (meaning they are
// 'repeated'). This is flawed though since timestamps aren't guaranteed to match. Making sure they
// happened within 2ms of eachothers seems to work for now but I should probably find some other
// way.
static Bool zCheckRepeatedKeyPress(Display *display, XEvent *event, XPointer arg)
{
    XEvent *prev_event = (XEvent *) arg;

    //zDebug("Ignoring repeating key press, serials are: %d -> %d", event->xkey.serial,
    //    prev_event->xkey.serial);

    if (event->type == KeyPress &&
        event->xkey.keycode == prev_event->xkey.keycode &&
        event->xkey.time < prev_event->xkey.time+2 ) {
        //event->xkey.time == prev_event->xkey.time ) {
        // Keycode and timestamp match, should be safe to assume this was a 'repeated' keypress.
        return True;
    }
    return False;
}



// Called when a key is pressed or when going fullscreen, makes sure the pointer grab is released
// when not fullscreen or when the special in_ungrabkey is pressed (and does some additional
// housekeeping).
static void zUpdatePointerGrab(ZKeyEvent *zkev)
{
    static int  ungrab_changed = -1;
    static int  nograb_changed = -1;
    static ZKey ungrab_key     = Z_KEY_UNKNOWN;

    // Lookup ZKey if in_ungrabkey was just changed.
    if (ungrab_changed < zVar(in_ungrabkey).changed) {
        ungrab_key     = zKeyByName(in_ungrabkey);
        ungrab_changed = zVar(in_ungrabkey).changed;
    }

    // Update grab immediately if in_nograb was changed or if switching fullscreen mode.
    if (nograb_changed < zVar(in_nograb).changed || !zkev) {

        if (in_nograb || !fullscreen)
            XUngrabPointer(dpy, CurrentTime);
        else if (fullscreen)
            XGrabPointer(dpy, wnd, True, PointerMotionMask, GrabModeAsync, GrabModeAsync, wnd, None,
                CurrentTime);

        nograb_changed = zVar(in_nograb).changed;
    }

    // Check key press.
    if (zkev && fullscreen && !in_nograb && ungrab_key == zkev->key && ungrab_key != Z_KEY_UNKNOWN) {

        // I only check for !in_nograb on the keyrelease, so it will still be ungrabbed right after
        // user sets in_nograb.
        if (zkev->keystate == Z_KEY_STATE_PRESS)
            XUngrabPointer(dpy, CurrentTime);
        else
            XGrabPointer(dpy, wnd, True, PointerMotionMask, GrabModeAsync, GrabModeAsync, wnd, None,
                CurrentTime);
    }
}



void zProcessEvents(void)
{
    XEvent event;
    ZKeyEvent zkev;
    KeySym keysym;

    int viewport_center_x = viewport_width/2;
    int viewport_center_y = viewport_height/2;
    int m_xpos = viewport_center_x;
    int m_ypos = viewport_center_y;

    assert(window_active);

    // NOTE: XPending might block as it reads from the X connection. Doesn't seem to be a problem
    // for me though. Some games work around this anyway by using select() to see if a non-blocking
    // read is possible, and only then call XPending. (handy to know if it ever becomes a problem).
    while (XPending(dpy)) {

        XNextEvent(dpy, &event);

        // Filter events for input method when doing text input.
        // FIXME: I disabled this line because XFilterEvent was always returning true, breaking
        //  text input entirely. Normally, you need XfilterEvent to avoid issues like
        //  https://github.com/alacritty/alacritty/issues/211
        //if ( text_input && XFilterEvent(&event, wnd) ) return;

        switch (event.type) {

        case ClientMessage:
            // Using the l field of the data union seems to work but it doesn't seem to be properly
            // documented anywhere..
            if ((unsigned long)event.xclient.data.l[0] == atom_wmdelete) {
                // Recieve window delete message fomr window manager, so we quit.
                //zDebug("User closed window. Exiting.");
                running = 0;
            }
            break;

        case MotionNotify:
            m_xpos = event.xmotion.x;
            m_ypos = event.xmotion.y;
            break;

        case ButtonPress:
        case ButtonRelease:
            {
                ZKeyEvent zkev;
                zkev.key = Z_KEY_MOUSE1 + (event.xbutton.button-1);
                zkev.keystate = event.type == ButtonPress ? Z_KEY_STATE_PRESS : Z_KEY_STATE_RELEASE ;
                zkev.modmask = zTranslateXModMask(event.xbutton.state);
                zDispatchKeyEvent(&zkev);

                if (in_debug) {
                    zDebug("%s:\n  From X: button %d, state %d\n  Mapped to: %s, modmask %d",
                        event.type == ButtonPress ? "ButtonPress" : "ButtonRelease",
                        event.xbutton.button, event.xkey.state, zKeyName(zkev.key), zkev.modmask);
                }

            }
            break;

        case MappingNotify:
            XRefreshKeyboardMapping(&event.xmapping);
            break;

        case KeyPress:
        case KeyRelease:

            // Transform X key event into ZKeyEvent
            keysym = XKeycodeToKeysym(dpy, event.xkey.keycode, 0);

            zkev.key = zTranslateXKey(keysym);
            zkev.keystate = event.type == KeyPress ? Z_KEY_STATE_PRESS : Z_KEY_STATE_RELEASE;
            zkev.modmask = zTranslateXModMask(event.xkey.state);

            zUpdatePointerGrab(&zkev);

            if (text_input && event.type == KeyPress) {

                static char buf[60];
                static int buflen = 0;
                static Status im_status;

                assert(text_input_cb);

                // Collect characters in buf, transform keysym, call text input handler.
                buflen = Xutf8LookupString(ic, &event.xkey, buf, sizeof buf, &keysym, &im_status);
                buf[buflen] = '\0';

                // Call text input handler depending on lookupstring results.
                if (im_status == XBufferOverflow) {
                    // Don't bother to retry with larger buffer, improve if it becomes a problem.
                    zWarning("Buffer not large enough to handle looked up string from input method.");
                } else {

                    // Invoke for key event and looked up string seperately.
                    if ( im_status == XLookupKeySym || im_status == XLookupBoth )
                        text_input_cb(&zkev, NULL);

                    if ( im_status == XLookupChars || im_status == XLookupBoth )
                        // Previous call might have disabled text input, so recheck here.
                        if (buflen && text_input) text_input_cb(NULL, buf);
                }

                if (in_debug) {
                    char *keysymstr;
                    keysymstr = XKeysymToString(keysym);
                    zDebug("Got KeyPress in text_input mode. KeySym is %s, buf[0] is %#hhx (len %d)",
                        keysymstr, buf[0], buflen);
                }
                break;

            } else if (!text_input) {

                // Check if we're dealing with a auto-repeat key event. I should see if there is a
                // matching keypress event in the queue and ignore both. Another way to do this is to
                // query the keymap and check the key state. Not sure which is better..
                if ( event.type == KeyRelease && XEventsQueued(dpy, QueuedAlready) ) {
                    XEvent event_return;
                    if ( XCheckIfEvent(dpy, &event_return, zCheckRepeatedKeyPress,
                            (XPointer) &event) ) {
                        // Fake key release found so ignore both. XCheckIfEvent removes mathcing
                        // event from the queue so all I really need to do is break;
                        break;
                    }
                }

                zDispatchKeyEvent(&zkev);

                if (in_debug) {
                    char *keysymstr;
                    keysymstr = XKeysymToString(keysym);
                    zDebug("%s:\n  From X: keycode %d, xkeysym %d (%s), state %d, serial %d, "
                        "send_event %d\n  Mapped to: %s, modmask %d",
                        event.type == KeyPress ? "KeyPress" : "KeyRelease", event.xkey.keycode,
                        keysym, keysymstr, event.xkey.state, event.xkey.serial,
                        event.xkey.send_event, zKeyName(zkev.key), zkev.modmask);
                }
            }
            break;

        case FocusIn:
            XSetICFocus(ic);

            if (fullscreen && !in_nograb)
                XGrabPointer(dpy, wnd, True, PointerMotionMask, GrabModeAsync, GrabModeAsync,
                    wnd, None, CurrentTime);
            break;

        case FocusOut:
            XUnsetICFocus(ic);

            // Release all keys
            zReleaseKeys(0);
            break;

        case ConfigureNotify:
            // Call ReshapeViewport when window is resized.
            if( event.xconfigure.width != viewport_width ||
                event.xconfigure.height != viewport_height) {

                viewport_width = event.xconfigure.width;
                viewport_height = event.xconfigure.height;
                zReshapeViewport();
            }
            break;

        // I don't handle these, but I don't really want 'unknown event' messages for them. I should
        // probably stop rendering or limit the framerate if the window is unmapped however..
        // (and for that matter probably on FocusOut too)
        case MapNotify:
        case UnmapNotify:
        case ReparentNotify:
            break;

        default:

            // Handle some extension events (just Xrandr at the moment).
            if ( zXrandrSupported() ) {
                if (event.type == randr_event_base+RRScreenChangeNotify ||
                    event.type == randr_event_base+RRNotify) {
                    zHandleXrandrEvent(&event, event.type - randr_event_base);
                    break;
                }
            }

            if (r_windebug)
               zDebug("Unknown X event, type = %d.", event.type);

            break;
        }
    }

    if (mouse_active) {

        // Figure out how far the pointer has moved away form the window center, derive the motion
        // vector, then warp pointer back to center. Pitch is inverted because origin is top-left.
        controller.yaw_delta   = m_sensitivity * (m_xpos - viewport_center_x);
        controller.pitch_delta = m_sensitivity * -(viewport_center_y - m_ypos);

        // XWarpPointer doesn't erase subpixel mouse position so this check isn't really neccesary.
        // Still, it does have some overhead which I guess never hurts to get rid of.
        if ( controller.pitch_delta != 0.0 || controller.yaw_delta != 0.0 ) {

            XWarpPointer(dpy, None, wnd, 0,0,0,0, viewport_center_x, viewport_center_y);

            // XSync is needed here to prevent MotionNotify event generated by XWarpPointer arriving
            // after real MotionNotify events from the user.
            XSync(dpy, False);
        }
    } else {

        controller.pitch_delta = 0;
        controller.yaw_delta = 0;
    }
}



static int zInitGLXExt(Display *display)
{
    int version_major, version_minor;

    if ( False == glXQueryExtension(display, NULL, NULL) ) {
        zError("GLX not supported.");
        return FALSE;
    } else {

        glXQueryVersion(display, &version_major, &version_minor);

        if ( !((version_major == 1 && version_minor >= 1) || version_major > 2 ) ) {
            zError("Incompatible GLX version (1.1+ required, %d.%d found).", version_major,
                version_minor);
            return FALSE;
        }
    }

    return TRUE;
}



// Setup IM related stuff. This is all done so I can use the Xutf8* Xlib API to get UTF-8 encoded
// text input without forcing a utf8 locale or having to convert myself. As a bonus I will gain
// support for some simple input methods.
static int zSetupIM(void)
{
    char *lcmods;
    XIMStyle my_style = XIMPreeditNothing | XIMStatusNothing;
    XIMStyles *imstyles = NULL;
    int imstyle_supported = 0;
    unsigned long im_event_mask;


    // See if X supports the locale we have set. Not sure what happens if it isn't, but at least
    // show a warning.
    if (!XSupportsLocale())
        zWarning("X doesn't support user locale.");


    // Set X locale modifiers.
    if (! (lcmods = XSetLocaleModifiers("")) )
        zWarning("Failed to set X locale modifiers.");


    // Open the input method.
    if ( !(im = XOpenIM(dpy, NULL, NULL, NULL)) ) {
        zError("Failed to open X input method.");
        return FALSE;
    }

    // Check that im supports (XIMPreeditNothing | XIMStatusNothing) im-style.
    // TODO: The style matching I do here may need to be improved. Since I require an exact match
    // right now for my_style even though other styles may work. See example at
    // http://www.sbin.org/doc/Xlib/chapt_11.html
    // It should be safe to assume only one flag for XIMStatus or XIMPreedit is set.
    if (XGetIMValues(im, XNQueryInputStyle, &imstyles, NULL)) {
        zWarning("Failed to query IM values.");
    } else {
        int i;

        for (i = 0; i < imstyles->count_styles; i++) {
            if ( my_style == imstyles->supported_styles[i] )
                imstyle_supported = 1;
        }
        XFree(imstyles);
    }

    if (!imstyle_supported) {
        zError("Input method style is not supported.");
        XCloseIM(im);
        return FALSE;
    }


    // Create the input context.
    if ( !(ic = XCreateIC(im, XNInputStyle, my_style, XNClientWindow, wnd, NULL)) ) {
        zError("Failed to create input context.");
        XCloseIM(im);
        return FALSE;
    }


    // Setup event mask on my window for the events the input method needs.
    if (XGetICValues(ic, XNFilterEvents, &im_event_mask, NULL))
        zWarning("Failed to get input method event mask.");
    else
        XSelectInput(dpy, wnd, default_event_mask | im_event_mask);

    return TRUE;
}



// Close IM, not sure if I need it..
static void zCloseIM(void)
{
    XUnsetICFocus(ic);
    XDestroyIC(ic);
    XCloseIM(im);
}



// The xrandr code here attempts to temporarily switch the CRTC (on which the Zark window resides)
// to the configured display mode, and restore the original display mode of the CRTC on exit.


// Dump the entire Xrandr configuration.
static void zDumpXrandrConfig(void)
{
    int min_width, min_height, max_width, max_height;
    XRRScreenResources *res = NULL;

    if ( XRRGetScreenSizeRange(dpy, wnd, &min_width, &min_height, &max_width, &max_height) ) {
        zDebug("XRRGetScreenSizeRange: min = %dx%d, max = %dx%d",
            min_width, min_height, max_width, max_height);
        zDebug("");
    } else {
        zError("XRRGetScreenSizeRange failed.");
    }

    if ( (res = XRRGetScreenResources(dpy,wnd)) ) {

        int i, j;

        zDebug("screen resources:");
        zDebug("  timestamp       = %d", res->timestamp);
        zDebug("  configTimestamp = %d", res->configTimestamp);
        zDebug("");

        // Print crtcs.
        for (i = 0; i < res->ncrtc; i++) {
            XRRCrtcInfo *crtc = NULL;
            crtc = XRRGetCrtcInfo(dpy, res, res->crtcs[i]);
            assert(crtc);
            zDebug("  crtc %d:", res->crtcs[i]);
            zDebug("    timestamp = %d",     crtc->timestamp);
            zDebug("    x, y      = %d, %d", crtc->x, crtc->y);
            zDebug("    w, h      = %d, %d", crtc->width, crtc->height);
            zDebug("    mode      = %d",     crtc->mode);
            zDebug("    rotation  = %hd",    crtc->rotation);
            zDebug("    rotations = %hd",    crtc->rotations);
            zDebug("    outputs:");
            for (j = 0; j < crtc->noutput; j++)
                zDebug("      %d", crtc->outputs[j]);
            zDebug("    possible outputs:");
            for (j = 0; j < crtc->npossible; j++)
                zDebug("      %d", crtc->possible[j]);
            XRRFreeCrtcInfo(crtc);
            zDebug("");
        }
        zDebug("");

        // Print outputs.
        for (i = 0; i < res->noutput; i++) {
            XRROutputInfo *output = NULL;
            output = XRRGetOutputInfo(dpy, res, res->outputs[i]);
            assert(output);
            zDebug("  output %d:",       res->outputs[i]);
            zDebug("    timestamp      = %d",  output->timestamp);
            zDebug("    crtc           = %d",  output->crtc);
            zDebug("    name           = %s",  output->name);
            zDebug("    mm_width       = %d",  output->mm_width);
            zDebug("    mm_height      = %d",  output->mm_height);
            zDebug("    connection     = %hd", output->connection);
            zDebug("    subpixel_order = %hd", output->subpixel_order);
            zDebug("    npreferred     = %d",  output->npreferred);
            zDebug("    crtcs:");
            for (j = 0; j < output->ncrtc; j++)
                zDebug("      %d", output->crtcs[j]);
            zDebug("    clone outputs:");
            for (j = 0; j < output->nclone; j++)
                zDebug("      %d", output->clones[j]);
            zDebug("    modes:");
            for (j = 0; j < output->nmode; j++)
                zDebug("      %d", output->modes[j]);
            zDebug("");
            XRRFreeOutputInfo(output);
        }
        zDebug("");

        // Print modes.
        for (i = 0; i < res->nmode; i++) {

            XRRModeInfo *mode = &res->modes[i];
            float refresh = (float) mode->dotClock / (float) (mode->vTotal * mode->hTotal);

            zDebug("  mode %3d:  %4d x %4d at %.1fHz", mode->id, mode->width, mode->height,
                refresh);
        }
        zDebug("");
    } else {
        zError("XRRGetScreenSizeRange failed.");
    }
    XRRFreeScreenResources(res);
}



// Returns TRUE if Xrandr 1.2 or higher is supported (and initializes some things the first time it
// is called), else FALSE.
static int zXrandrSupported(void)
{
    int vmajor, vminor;
    Status status;

    static int checked = FALSE;
    static int supported = FALSE;

    if (checked)
        return supported;

    if (XRRQueryExtension(dpy, &randr_event_base, &randr_error_base)) {

        if ( (status = XRRQueryVersion(dpy, &vmajor, &vminor)) ) {

            if ( vmajor < 1 || (vmajor < 2 && vminor < 2) ) {
                zError("Unable to change display mode, your version of xrandr is too old: found"
                    " %d.%d, need 1.2 or higher.", vmajor, vminor);
            } else {
                supported = TRUE;
            }

            if (r_windebug) zDebug("Found Xrandr version %d.%d.", vmajor, vminor);

        } else {
            zError("Failed to query Xrandr version (return status %d).", status);
        }
    } else {
        zError("Xrandr extension is not supported, not setting display mode.");
    }

    if (r_windebug)
        zDumpXrandrConfig();

    checked = TRUE;
    return supported;
}



// Handle xrandr events. Called by my X event handler only for Xrandr events.
static void zHandleXrandrEvent(XEvent *xev, int type)
{
    // This union is to comply with C99 strict aliasing (and happens to be a convenient way to
    // access the different event types with only a single type cast)..
    union ZExtendedXEvent {
        XEvent                       xev;
        XRRScreenChangeNotifyEvent   screen;
        XRRNotifyEvent               notify;
        XRRCrtcChangeNotifyEvent     crtc;
        XRROutputChangeNotifyEvent   output;
        XRROutputPropertyNotifyEvent property;
    } *ev = (union ZExtendedXEvent *) xev;

    // Not entirely sure why I need to call this, but the Xrandr man page says I should
    XRRUpdateConfiguration(xev);

    // I want to skip restoring the display mode if something else touches the CRTC (that I changed
    // the display mode for) after I changed it, but I'm not really sure how to seperate events
    // caused by my own mode change from those events caused by subsequent mode changes from other
    // clients.. so the best I can do is to just ignore all changes except if the mode was changed
    // to sometihing other than the mode I changed it to..
    if (restore_mode && type == RRNotify && ev->notify.subtype == RRNotify_CrtcChange) {

        if (ev->crtc.crtc == mode_crtc && ev->crtc.mode != mode_new) {

            zWarning("%s: Display mode for CRTC %d was changed to %d, not restoring it.", __func__,
                mode_crtc, ev->crtc.mode);

            restore_mode = FALSE;
            mode_crtc    = None;
            mode_old     = None;
            mode_new     = None;
        }
    }
}



// Figure out which CRTC our window is on, or is closest to.
static RRCrtc zFindCrtcForWindow(XRRScreenResources *res)
{
    int i;
    float window_distance = 1000000.0f;
    int center_x, center_y;
    XWindowAttributes win_attribs;
    Window ignore_win;
    RRCrtc window_crtc = None;

    // Ideally I would work with the window manager here since it really depends on where the WM
    // puts the window when it is made fullscreen, but I don't really know if this is possible so
    // I'll try to determine what CRTC to use myself, and hope the WM behaves the same.
    //
    // NOTE: If this becomes a problem ever, an alternative strategy might be to make the window
    // fullscreen first, then check where the WM placed it before changing the display mode. This
    // would depend on the WM being smart enough to keep the window fullscreened after I change
    // display mode (metacity does this at least).
    //
    // My method is to first check if the window center is inside the viewport of a CRTC, and simply
    // use that CRTC if it is, else I'll use the CRTC which has the shortest distance from its
    // center to the window's center.
    XGetWindowAttributes(dpy, wnd, &win_attribs);
    XTranslateCoordinates(dpy, wnd, root, 0,0, &center_x, &center_y, &ignore_win);
    center_x += win_attribs.width / 2;
    center_y += win_attribs.height / 2;

    // Check if window center is inside any of the CRTCs.
    for (i = 0; i < res->ncrtc; i++) {

        XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, res->crtcs[i]);
        assert(crtc);

        if (center_x >= crtc->x && center_x < (crtc->x + (int) crtc->width) &&
            center_y >= crtc->y && center_y < (crtc->y + (int) crtc->height)) {

            if (r_windebug) {
                zDebug("%s: Window center (%d,%d) is inside crtc %d (pos %d,%d size %d,%d)",
                    __func__, center_x, center_y, res->crtcs[i], crtc->x, crtc->y, crtc->width,
                    crtc->height);
            }

            window_crtc = res->crtcs[i];
        }

        XRRFreeCrtcInfo(crtc);
    }

    // If that didn't work, check window center distance from CRTC viewport center.
    if (window_crtc == None) {

        if (r_windebug)
            zDebug("%s: Window center not contained in any CRTC viewport, checking distance"
                " instead.", __func__);

        for (i = 0; i < res->ncrtc; i++) {

            float dist;
            XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, res->crtcs[i]);
            ZVec2 win_center = {center_x, center_y};
            ZVec2 crtc_center;

            assert(crtc);
            crtc_center.x = crtc->x + ((int)crtc->width/2);
            crtc_center.y = crtc->y + ((int)crtc->height/2);
            zSubtractVec2(&win_center, &crtc_center);
            dist = zLength2(&win_center);

            if (r_windebug)
                zDebug("%s: Distance between win_center and crtc_center for CRTC %d is %f.",
                    __func__, res->crtcs[i], dist);

            if ( dist < window_distance) {

                if (r_windebug)
                    zDebug("%s: Distance is smaller for CRTC %d than previous one.", __func__,
                        res->crtcs[i]);

                window_crtc = res->crtcs[i];
                window_distance = dist;
            }
            XRRFreeCrtcInfo(crtc);
        }
    }

    if (r_windebug)
        zDebug("%s: Found with CRTC %d.", __func__, window_crtc);

    return window_crtc;
}



// Save the current display configuration and set a different display mode for the display that our
// window is on.
static void zSetVideoMode(void)
{
    XRRScreenResources *res;
    RRCrtc picked_crtc;
    int i, j;

    assert(window_active);

    if (!zXrandrSupported()) return;

    if ( !(res = XRRGetScreenResources(dpy,wnd)) ) {
        zError("Failed to get Xrandr screen resources, not setting mode.");
        return;
    }

    // NOTE: If the configured mode is larger than the initial mode there is a chance that there
    // isn't enough virtual screen space to set the new mode, but since I can't think of a way to
    // handle that sanely I won't even bother to check and just let Xrandr fail...

    picked_crtc = zFindCrtcForWindow(res);

    if ( picked_crtc != None) {

        XRRCrtcInfo *crtc = XRRGetCrtcInfo(dpy, res, picked_crtc);
        XRROutputInfo *output;
        RRMode picked_mode = None;
        int picked_mode_refresh = 0;

        assert(crtc);

        if (crtc->noutput < 1) {
            zError("%s: CRTC has no outputs, unable to set mode.");
            XRRFreeScreenResources(res);
            XRRFreeCrtcInfo(crtc);
            return;
        }

        output = XRRGetOutputInfo(dpy, res, crtc->outputs[0]);
        assert(output);

        // Find a matching mode with the highest refresh rate.
        for (i = 0; i < res->nmode; i++) {

            XRRModeInfo mode = res->modes[i];
            int mode_refresh = ( (float) mode.dotClock / (float) (mode.hTotal*mode.vTotal) );

            if ( (int) mode.width == r_screenwidth && (int) mode.height == r_screenheight &&
                 (r_screenrefresh == 0 || r_screenrefresh == mode_refresh) ) {

                if ( !picked_mode || (picked_mode && picked_mode_refresh < mode_refresh) ) {

                    if (r_windebug)
                        zDebug("Mode %d with refresh %d matched configuration, checking if output"
                            " supports it.", mode.id, mode_refresh);

                    // Actually check if the mode is supported by the CRTC's output.
                    for (j = 0; j < output->nmode; j++ ) {
                        if (mode.id == output->modes[j]) {
                            if (r_windebug)
                                zDebug("Output %d supports mode %d.", crtc->outputs[0], mode.id);
                            picked_mode = mode.id;
                            picked_mode_refresh = mode_refresh;
                        }
                    }
                }
            }
        }

        if (picked_mode != None) {

            Status status;

            if (r_windebug)
                zDebug("Setting mode %d on crtc %d.", picked_mode, picked_crtc);

            status = XRRSetCrtcConfig(dpy, res, picked_crtc, CurrentTime, crtc->x, crtc->y,
                picked_mode, crtc->rotation, crtc->outputs, crtc->noutput);

            if (status == RRSetConfigSuccess) {

                restore_mode = TRUE;
                mode_old = crtc->mode;
                mode_new = picked_mode;
                mode_crtc = picked_crtc;

            } else {
                zError("Failed to set mode, XRRSetCrtcConfig returned status %d.", status);
            }

        } else {
            zError("Failed get a suitable display mode, not setting mode.");
        }

        XRRFreeCrtcInfo(crtc);
        XRRFreeOutputInfo(output);

    } else {
        zError("Failed to find CRTC for window, not setting mode.");
    }

    XRRFreeScreenResources(res);
}



// Restore preveously saved display configuration if the video mode was changed.
static void zRestoreVideoMode(void)
{
    assert(window_active);

    if (!zXrandrSupported()) return;

    if (restore_mode) {

        XRRScreenResources *res = XRRGetScreenResources(dpy,wnd);
        XRRCrtcInfo *crtc = NULL;

        if (res)
            crtc = XRRGetCrtcInfo(dpy, res, mode_crtc);

        if (res && crtc) {
            Status status;

            // Restore original mode for the CRTC I changed the display mode for.
            status = XRRSetCrtcConfig(dpy, res, mode_crtc, CurrentTime, crtc->x, crtc->y,
                mode_old, crtc->rotation, crtc->outputs, crtc->noutput);

            if (status != RRSetConfigSuccess) {
                zError("Failed to restore display mode, XRRSetCrtcConfig returned status %d.",
                    status);
            }

        } else {
            zError("%s: Failed to get Xrandr screen resources, unable to restore mode.", __func__);
        }

        restore_mode = FALSE;
        mode_old     = None;
        mode_new     = None;
        mode_crtc    = None;

        XRRFreeCrtcInfo(crtc);
        XRRFreeScreenResources(res);
    }
}



void zSetFullscreen(int state)
{
    XEvent ev;
    Atom atom_state, atom_state_fs;

    assert(window_active);

    if (state == Z_FULLSCREEN_TOGGLE)
        fullscreen = !fullscreen;
    else
        fullscreen = state == Z_FULLSCREEN_ON ? TRUE : FALSE;

    zUpdatePointerGrab(NULL);

    if (fullscreen && r_screenwidth && r_screenheight)
        zSetVideoMode();
    else
        zRestoreVideoMode();

    // Make window full screen if desired, using WM spec (see
    // http://freedesktop.org/wiki/Specifications/wm-spec). This method of making the window
    // fullscreen wouldn't work if I changed the resolution using xf86vidmode, since that extension
    // would make the display a viewport into a possibly larger virtual screen, and so the window
    // would end up being larger than the display and the user would have to use the mouse to 'pan'
    // the viewport.. Now that I'm changing the resolution with Xrandr this works however.
    atom_state    = XInternAtom(dpy, "_NET_WM_STATE", False);
    atom_state_fs = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    memset(&ev, 0, sizeof(ev));

    ev.type = ClientMessage;
    ev.xclient.window = wnd;
    ev.xclient.message_type = atom_state;
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = fullscreen;
    ev.xclient.data.l[1] = atom_state_fs;
    ev.xclient.data.l[2] = 0;

    XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureNotifyMask, &ev);

    // This ensures the window really is on top (I found that in metacity with focus-on-mouseover it
    // sometimes isn't) and that the cursor is centered in the fullscreen window.
    if (fullscreen) {

        XWindowChanges changes;

        changes.stack_mode = Above;

        XConfigureWindow(dpy, wnd, CWStackMode, &changes);
    }
}



void zOpenWindow(void)
{
    GLenum glew_err;
    XVisualInfo *visinfo = NULL;
    unsigned long winmask;
    XSetWindowAttributes winattribs;
    int attrib_count = 0;
#define MAX_GLX_ATTRIBS 30
    int attribs[MAX_GLX_ATTRIBS];


    // Set some default visual attributes
    attribs[attrib_count++] = GLX_RED_SIZE;
    attribs[attrib_count++] = 8;
    attribs[attrib_count++] = GLX_GREEN_SIZE;
    attribs[attrib_count++] = 8;
    attribs[attrib_count++] = GLX_RED_SIZE;
    attribs[attrib_count++] = 8;
    attribs[attrib_count++] = GLX_RGBA;

    if (r_doublebuffer)
        attribs[attrib_count++] = GLX_DOUBLEBUFFER;

    if (r_depthbuffer) {
        attribs[attrib_count++] = GLX_DEPTH_SIZE;
        attribs[attrib_count++] = 24;
    }

    if (r_stencilbuffer) {
        attribs[attrib_count++] = GLX_STENCIL_SIZE;
        attribs[attrib_count++] = 8;
    }

    // Terminate list and ensure I didn't write past the end of the array.
    attribs[attrib_count++] = 0;
    assert(attrib_count <= MAX_GLX_ATTRIBS);

    dpy = XOpenDisplay(getenv("DISPLAY"));

    if ( NULL == dpy ) {
        zError("Failed to open display.");
        goto zOpenWindow_0;
    }

    if (!zInitGLXExt(dpy)) {
        zError("Failed to initialize GLX.");
        goto zOpenWindow_0;
    }

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);
    visinfo = glXChooseVisual(dpy, screen, attribs);

    if ( NULL == visinfo ) {
        zError("No suitable visual was found.");
        goto zOpenWindow_1;
    }

    //zDebug("VisualID is %#x", visinfo->visualid);

    winattribs.colormap = XCreateColormap(dpy, root, visinfo->visual, AllocNone);
    winattribs.event_mask = default_event_mask;
    winattribs.override_redirect = False; // Set to true do disable WM for this window.
    winmask = CWColormap | CWEventMask | CWOverrideRedirect;

    wnd = XCreateWindow(dpy, root, 0, 0, r_winwidth, r_winheight, 0, visinfo->depth, InputOutput,
        visinfo->visual, winmask, &winattribs);

    if (zXrandrSupported()) {
        XRRSelectInput(dpy, wnd, RRScreenChangeNotifyMask | RRCrtcChangeNotifyMask |
                                 RROutputChangeNotifyMask | RROutputPropertyNotifyMask );
    }

    XStoreName(dpy, wnd, PACKAGE_STRING);

    // Let the window manager know we want to be notified when user closes window.
    atom_wmdelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(dpy, wnd, &atom_wmdelete, 1);

    // Create GL context.
    context = glXCreateContext(dpy, visinfo, NULL, True);

    if (NULL == context) {
        zError("Failed to create OpenGL context.");
        goto zOpenWindow_2;
    }

    XMapWindow(dpy, wnd);

    if ( !glXMakeCurrent(dpy, wnd, context) ) {
        zError("Failed to make OpenGL context current.");
        goto zOpenWindow_3;
    }

    // Init glew, check for required OpenGL support.
    if ( GLEW_OK != (glew_err = glewInit()) ) {
        zError("Failed to initialize glew. %s", glewGetErrorString(glew_err));
        goto zOpenWindow_3;
    }

    if (!zCheckOpenGLSupport())
        goto zOpenWindow_3;

    if ( !zSetupIM() ) {
        zError("Failed to setup input method.");
        goto zOpenWindow_3;
    }

    window_active = 1;

    // Going fullscreen later on will cause a ConfigureNotify event, so it's okay to just set it
    // like this here..
    viewport_width = r_winwidth;
    viewport_height = r_winheight;


    if (r_fullscreen) {
        zSetFullscreen(Z_FULLSCREEN_ON);
    }


    zRendererInit();

    XFree(visinfo);
    return;

    // Error-handling.
zOpenWindow_3:
    glXDestroyContext(dpy, context);
zOpenWindow_2:
    XFree(visinfo);
    XDestroyWindow(dpy, wnd);
zOpenWindow_1:
    XCloseDisplay(dpy);
zOpenWindow_0:
    zFatal("Failed to open window.");
    zShutdown();
    exit(EXIT_FAILURE);
}



void zCloseWindow()
{
    assert(window_active);

    zRendererDeinit();

    zCloseIM();
    glXMakeCurrent(dpy, None, NULL);
    glXDestroyContext(dpy, context);

    zRestoreVideoMode();

    XDestroyWindow(dpy, wnd);
    XCloseDisplay(dpy);
    window_active = 0;
}



void zSwapInterval(int interval)
{
    assert(window_active);

    if (GLXEW_SGI_swap_control)
        glXSwapIntervalSGI(interval);
    else
        zError("Swap control not supported, unable to set interval.");
}



void zSwapBuffers()
{
    assert(window_active);
    glXSwapBuffers(dpy, wnd);
}



void zShutdown(void)
{
}



char *zGetUserDir(void)
{
    static char userdir[Z_PATH_SIZE];
    static int initialized;

    if (!initialized) {

        char *env_home;
        size_t len;

        // Lookup homedir. Try HOME env var first, or getentpw if that fails.
        if ( (env_home = getenv("HOME")) && (len = strlen(env_home)) ) {

            if (len < Z_PATH_SIZE)
                strcat(userdir, env_home);
            else
                zWarning("Home directory from $HOME exceeded buffer size, using empty string");

        } else {
            struct passwd *pwd;

            if ( (pwd = getpwuid(getuid())) ) {

                if (pwd->pw_dir && (len = strlen(pwd->pw_dir)) ) {

                    if (len < Z_PATH_SIZE)
                        strcat(userdir, pwd->pw_dir);
                    else
                        zWarning("Home directory from passwd entry exceeded buffer size, using"
                            " empty string");
                }
            }
        }

        // Append app-specific component.
        if ( (strlen(userdir) + strlen(Z_DIR_USERDATA) + 1) < Z_PATH_SIZE ) {

            strcat(userdir, Z_DIR_SEPARATOR);
            strcat(userdir, Z_DIR_USERDATA);

            // Make sure the directory exists (create if not) and that it is a directory.
            if (!zPathExists(userdir)) {

                zPrint("Creating user data directory at \"%s\".\n", userdir);

                if (mkdir(userdir, 0755) != 0) {
                    zWarning("Failed to create user directory.");
                    userdir[0] = '\0';
                }
            } else if (zPathExists(userdir) != Z_EXISTS_DIR) {
                zWarning("User directory \"%s\" exists but is not a directory.", userdir);
                userdir[0] = '\0';
            }
        } else {
            zWarning("User data directory exceeded buffer dize, using empty string.");
            userdir[0] = '\0';
        }

        initialized = 1;
    }

    // Only return if it's not an empty string.
    if (*userdir) return userdir;

    return NULL;
}



int zPathExists(const char *path)
{
    struct stat s;

    if (stat(path, &s) == 0) {

        if ( S_ISREG(s.st_mode) )
            return Z_EXISTS_REGULAR;
        else if (S_ISDIR(s.st_mode) )
            return Z_EXISTS_DIR;
    }

    return FALSE;
}



char *zGetFileFromDir(const char *path)
{
    static int start = 1;
    static DIR *dir;
    static char path_dir[Z_PATH_SIZE];  // Full path to the directory.
    static char path_file[Z_PATH_SIZE]; // Full path to the file inside directory, a pointer to this
                                        // is returned.
    struct dirent *entry;

    // When called for the first time for a new directory, build the full path to the directory and
    // open a directory stream.
    if (start) {

        // Append dirsep + path + dirsep to data dir, be sure it fits..
        if ( (strlen(path) + strlen(Z_DIR_SYSDATA) + 1) > (Z_PATH_SIZE-1) ) {
            zWarning("Failed to open directory \"%s\", path length exceeded Z_PATH_SIZE.", path);
            return NULL;
        }

        path_dir[0] = '\0';
        strcat(path_dir, Z_DIR_SYSDATA);
        strcat(path_dir, Z_DIR_SEPARATOR);
        strcat(path_dir, path);
        strcat(path_dir, Z_DIR_SEPARATOR);

        // Make sure we use native directory separators, only really needed for path, but I need to
        // work on a copy of it, so this is easier..
        zRewriteDirsep(path_dir);

        dir = opendir(path_dir);
        if (!dir) {
            zWarning("Failed to open directory \"%s\".", path_dir);
            return NULL;
        }
        start = 0;
    }

    // Keep reading direntries from dir until we hit a regular file, or if end of directory is
    // reached.
    while ( (entry = readdir(dir)) ) {

        // Entry filename is relative to the directory we opened, so append it to path_dir.
        if ( (strlen(path_dir) + strlen(entry->d_name)) > (Z_PATH_SIZE-1) ) {
            zWarning("%s: Path for file \"%s\" under \"%s\" exceeded Z_PATH_SIZE, skipping.",
                __func__, entry->d_name, path_dir);
            continue;
        }

        path_file[0] = '\0';
        strcat(path_file, path_dir);
        strcat(path_file, entry->d_name);

        // If file exists and is a regular file, break out of loop.
        if (zPathExists(path_file) == Z_EXISTS_REGULAR)
            return path_file;
    }

    // If entry is still NULL we reached end of directory.
    assert(!entry);

    closedir(dir);
    start = 1;
    return NULL;
}

