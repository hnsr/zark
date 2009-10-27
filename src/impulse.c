#include <string.h>

#include "common.h"

// To reduce overhead on simple and common actions (like aiming and movement) bound to keys I define
// a static set of 'impulses' which are directly associated with some handler functions and which
// keys can be bound to. The keyevent dispatcher can then directly invoke the handler function
// passing it a start or stop state (depending on key up/down state).
//
// Since some impulses for movements have variations of left/right/up/down/etc., it makes sense to
// have impulses share a handler (instead of having to define a bunch of handlers that all sort of
// do the same thing, or having to use tiny wrapper functions). Thus an impulse definition includes
// an argument (ZImpulseArg) that is passed to the shared handler.




// Returns pointer to impulse at position n in impulses array, or NULL if the value was out of
// range.
ZImpulse *zLookupImpulse(unsigned int n)
{
    if (n >= 0 && n < zImpulseCount())
        return impulses+n;
    else
        return NULL;
}



// Returns number of impulses defined.
unsigned int zImpulseCount(void)
{
    int i;
    static unsigned int count = 0;

    // I could also define this at the bottom after the size of impulses is known and simply use
    // sizeof.. but meh

    if (!count) {
        for (i = 0; impulses[i].name; i++) {
            count++;
        }
    }

    return count;
}



// Impulse handlers/definitions --------------------------------------------------------------------

static void zImpulseAim(ZImpulseArg arg, unsigned int state)
{
    unsigned int flags = controller.update_flags;

    if (state) {
        if (arg == Z_IMPARG_NONE) {
            zEnableMouse();
            flags |= Z_CONTROL_AIM;
        }
        else if (arg == Z_IMPARG_LEFT)  flags |= Z_CONTROL_AIM_LEFT;
        else if (arg == Z_IMPARG_RIGHT) flags |= Z_CONTROL_AIM_RIGHT;
        else if (arg == Z_IMPARG_UP)    flags |= Z_CONTROL_AIM_UP;
        else if (arg == Z_IMPARG_DOWN)  flags |= Z_CONTROL_AIM_DOWN;

    } else {
        if (arg == Z_IMPARG_NONE) {
            zDisableMouse();
            flags &= ~Z_CONTROL_AIM;
        }
        else if (arg == Z_IMPARG_LEFT)  flags &= ~Z_CONTROL_AIM_LEFT;
        else if (arg == Z_IMPARG_RIGHT) flags &= ~Z_CONTROL_AIM_RIGHT;
        else if (arg == Z_IMPARG_UP)    flags &= ~Z_CONTROL_AIM_UP;
        else if (arg == Z_IMPARG_DOWN)  flags &= ~Z_CONTROL_AIM_DOWN;
    }

    controller.update_flags = flags;
}


static void zImpulseZoom(ZImpulseArg arg, unsigned int state)
{
    if (state) {
        zEnableMouse();
        controller.update_flags |= Z_CONTROL_ZOOM;
    } else {
        zDisableMouse();
        controller.update_flags &= ~Z_CONTROL_ZOOM;
    }
}


static void zImpulseMove(ZImpulseArg arg, unsigned int state)
{
    unsigned int flags = controller.update_flags;

    if (state) {
        switch(arg) {
            case Z_IMPARG_UP:
                flags |= Z_CONTROL_UP;
                break;
            case Z_IMPARG_DOWN:
                flags |= Z_CONTROL_DOWN;
                break;
            case Z_IMPARG_LEFT:
                flags |= Z_CONTROL_LEFT;
                break;
            case Z_IMPARG_RIGHT:
                flags |= Z_CONTROL_RIGHT;
                break;
            case Z_IMPARG_FORWARD:
                flags |= Z_CONTROL_FORWARD;
                break;
            case Z_IMPARG_BACK:
                flags |= Z_CONTROL_BACK;
                break;
            default:
                zWarning("%s: Invalid movement direction", __func__);
                return;
        }
    } else {
        switch(arg) {
            case Z_IMPARG_UP:
                flags &= ~Z_CONTROL_UP;
                break;
            case Z_IMPARG_DOWN:
                flags &= ~Z_CONTROL_DOWN;
                break;
            case Z_IMPARG_LEFT:
                flags &= ~Z_CONTROL_LEFT;
                break;
            case Z_IMPARG_RIGHT:
                flags &= ~Z_CONTROL_RIGHT;
                break;
            case Z_IMPARG_FORWARD:
                flags &= ~Z_CONTROL_FORWARD;
                break;
            case Z_IMPARG_BACK:
                flags &= ~Z_CONTROL_BACK;
                break;
            default:
                zWarning("%s: Invalid movement direction", __func__);
                return;
        }
    }

    controller.update_flags = flags;
}


static void zImpulseQuit(ZImpulseArg arg, unsigned int state)
{
    text_input = running = 0;
}


static void zImpulseTextConsole(ZImpulseArg arg, unsigned int state)
{
    zEnableTextConsole();
}



ZImpulse impulses[] = {
//  Name             P  Handler              ImpulseArg        Description
    {"AIM",          0, zImpulseAim,         Z_IMPARG_NONE,    "Aim with mouse"     },
    {"AIM_UP",       0, zImpulseAim,         Z_IMPARG_UP,      "Aim up"             },
    {"AIM_DOWN",     0, zImpulseAim,         Z_IMPARG_DOWN,    "Aim down"           },
    {"AIM_LEFT",     0, zImpulseAim,         Z_IMPARG_LEFT,    "Aim left"           },
    {"AIM_RIGHT",    0, zImpulseAim,         Z_IMPARG_RIGHT,   "Aim right"          },
    {"ZOOM",         0, zImpulseZoom,        Z_IMPARG_NONE,    "Zoom with mouse"    },
    {"ZOOM_IN",      0, zImpulseZoom,        Z_IMPARG_IN,      "Zoom in"            },
    {"ZOOM_OUT",     0, zImpulseZoom,        Z_IMPARG_OUT,     "Zoom out"           },
    {"MOVE_LEFT",    0, zImpulseMove,        Z_IMPARG_LEFT,    "Move left"          },
    {"MOVE_RIGHT",   0, zImpulseMove,        Z_IMPARG_RIGHT,   "Move right"         },
    {"MOVE_UP",      0, zImpulseMove,        Z_IMPARG_UP,      "Move up"            },
    {"MOVE_DOWN",    0, zImpulseMove,        Z_IMPARG_DOWN,    "Move down"          },
    {"MOVE_FORWARD", 0, zImpulseMove,        Z_IMPARG_FORWARD, "Move forward"       },
    {"MOVE_BACK",    0, zImpulseMove,        Z_IMPARG_BACK,    "Move back"          },
    {"QUIT",         1, zImpulseQuit,        Z_IMPARG_NONE,    "Exit " PACKAGE_NAME },
    {"TEXTCONSOLE",  1, zImpulseTextConsole, Z_IMPARG_NONE,    "Open text console " },
    {NULL, 0, NULL, 0, NULL}
};

