#ifndef __INPUT_H__
#define __INPUT_H__

#include <stdio.h>
#include "command.h"

// List of physical keys I support for keybindings. For now I'll just add all of those on my own
// QWERTY keyboard, will have to investigate about all the possible keys of other keyboard types.
typedef enum ZKey
{
    // Generate keys enum.
    #define MAKE_ENUM
    #include "keys.def"
    #undef MAKE_ENUM
    NUM_KEYS
} ZKey;


#define Z_KEY_STATE_PRESS   1
#define Z_KEY_STATE_RELEASE 2

#define Z_KEY_MOD_CTRL   (1 << 0)
#define Z_KEY_MOD_LALT   (1 << 1)
#define Z_KEY_MOD_RALT   (1 << 2)
#define Z_KEY_MOD_SHIFT  (1 << 3)
#define Z_KEY_MOD_SUPER  (1 << 4)

// ZKeyEvent - everything that needs to be known for a key event.
typedef struct ZKeyEvent
{
    enum ZKey key;          // The physical key that was pressed.
    unsigned int keystate;  // Was it a press or release?
    unsigned int modmask;   // Which modifier keys were down?
    struct ZKeyEvent *next; // Only used for list of currently down keys.

} ZKeyEvent;



// ZKeyBinding - represents the binding between a key with one or more commands. Some conditions can
// be set on wether or not it is a press or release event (keystate) and which (if any) of the
// modifier keys were down (modstate).
typedef struct ZKeyBinding
{
    ZKeyEvent keyevent;         // Key event for this binding.
    char *cmdstring;            // The unprocessed command string for this keybinding. Can be used
                                // to dump keybindings back to a file without loss of any
                                // information (from parsing).
    unsigned int numcommands;   // Number of commands this key is bound with.
    ZParsedCommand *parsedcmds; // Array of parsed commands

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

// ZTextBuffer - Keeps track of textual input given by user, cursor state etc.
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

void zParseKeyBindings(FILE *fp, const char *filename); // Actually defined in keybindings_yacc.y.

ZKeyBinding *zLookupKeyBinding(const ZKeyEvent *zkev);

ZKey zLookupKey(const char *keyname);

const char *zKeyName(ZKey key);

const char *zKeyDesc(ZKey key);

char *zKeyEventName(ZKeyEvent *kev);

void zReleaseKeys(int skip_keybinds);

void zDispatchKeyEvent(const ZKeyEvent *zkev);

int zAddKeyBinding(const ZKeyEvent *zkev, char *cmdstring);

int zLoadKeyBindings(const char *filename);

int zWriteKeyBindings(const char *filename);

#endif
