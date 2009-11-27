#ifndef __INPUT_H__
#define __INPUT_H__

#include <stdio.h>
#include "impulse.h"

// List of physical keys I support for keybindings. For now I'll just add all of those on my own
// QWERTY keyboard, will have to investigate about all the possible keys of other keyboard types.
typedef enum ZKey
{
    // Generate keys enum.
    #define MAKE_ENUM
    #include "keys.def"
    #undef MAKE_ENUM
    Z_NUM_KEYS
} ZKey;


#define Z_KEY_STATE_PRESS   1
#define Z_KEY_STATE_RELEASE 2
#define Z_KEY_STATE_MASK    (Z_KEY_STATE_PRESS | Z_KEY_STATE_RELEASE)

#define Z_KEY_MOD_CTRL   (1 << 0)
#define Z_KEY_MOD_LALT   (1 << 1)
#define Z_KEY_MOD_RALT   (1 << 2)
#define Z_KEY_MOD_SHIFT  (1 << 3)
#define Z_KEY_MOD_SUPER  (1 << 4)
#define Z_KEY_MOD_MASK   (Z_KEY_MOD_CTRL | Z_KEY_MOD_LALT | Z_KEY_MOD_RALT |\
                          Z_KEY_MOD_SHIFT | Z_KEY_MOD_SUPER)

// ZKeyEvent - everything that needs to be known for a key event.
typedef struct ZKeyEvent
{
    enum ZKey key;
    unsigned int keystate;
    unsigned int modmask;
    struct ZKeyEvent *next; // Only used for pressed_keys list.

} ZKeyEvent;


// ZKeyBinding - represents the binding between a key event and an impulse, or Lua script for
// running console commands.
typedef struct ZKeyBinding
{
    ZKeyEvent keyevent;
    ZImpulse *impulse;
    char *lua;
    ZLuaCode code;

} ZKeyBinding;

extern ZKeyBinding *keybindings;
extern unsigned int numkeybindings;



#define Z_CONTROL_AIM_UP     (1 << 0)
#define Z_CONTROL_AIM_DOWN   (1 << 1)
#define Z_CONTROL_AIM_LEFT   (1 << 2)
#define Z_CONTROL_AIM_RIGHT  (1 << 3)
#define Z_CONTROL_AIM        (1 << 4)
#define Z_CONTROL_UP         (1 << 5)
#define Z_CONTROL_DOWN       (1 << 6)
#define Z_CONTROL_LEFT       (1 << 7)
#define Z_CONTROL_RIGHT      (1 << 8)
#define Z_CONTROL_FORWARD    (1 << 9)
#define Z_CONTROL_BACK       (1 << 10)
#define Z_CONTROL_ZOOM       (1 << 11)
#define Z_CONTROL_ROLL       (1 << 12)
#define Z_CONTROL_ROLL_LEFT  (1 << 13)
#define Z_CONTROL_ROLL_RIGHT (1 << 14)


// ZController - Maintains controller states and motions for objects under user control.
typedef struct ZController
{
    // Indicates how the object/camera needs to be updated.
    unsigned int update_flags;

    float pitch_delta;
    float yaw_delta;

} ZController;

extern ZController controller;



#define Z_TEXTBUFFER_SIZE 1024

// ZTextBuffer - Keeps track of text input from by user and cursor state. Every text input area
// (console, GUI widget) will be backed by one of these.
typedef struct ZTextBuffer
{
    size_t  cursor_bytes; // Cursor offset in bytes.
    size_t  bytes;        // Size of string in bytes (excluding null terminator).

    size_t  bufsize;      // Size of buf in bytes.
    char    *buf;         // Pointer to allocated memory.
    char    *buf_shadow;  // Extra storage to make string manipulation easier.

} ZTextBuffer;

typedef void (*ZTextInputCallback)(ZKeyEvent *zkev, char *str);

extern int text_input;

extern ZTextInputCallback text_input_cb;




void zUpdateTextBuffer(ZTextBuffer *textbuf, ZKeyEvent *zkev, char *str);

void zResetTextBuffer(ZTextBuffer *textbuf);

ZKey zKeyByName(const char *keyname);

const char *zKeyName(ZKey key);

const char *zKeyDesc(ZKey key);

char *zKeyEventName(ZKeyEvent *kev, int show_state);

void zReleaseKeys(int skip_keybinds);

void zDispatchKeyEvent(const ZKeyEvent *zkev);

void zLoadKeyBindings(void);

void zSaveKeyBindings(void);

#endif
