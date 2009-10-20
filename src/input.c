#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#endif

#include "common.h"


// Array with names and descriptions for each key, used for getting friendly names from keysymbols
// and for looking up keysymbols from a string (for now just by linearly searching through it).
struct
{
    char *name;
    char *desc;

} keynames[NUM_KEYS] = {
    #define MAKE_ARRAYINITS
    #include "keys.def"
    #undef MAKE_ARRAYINITS
};


// Global array for all the defined keybindings.
ZKeyBinding *keybindings;

// Real element size of keybindings array, used for dynamically growing it.
size_t keybindings_size;

// Number of keybindings actually added to global array.
unsigned int numkeybindings;


// The default (and for now only) controller.
ZController controller;


// Indicates if we're reading text input and not just key events.
int text_input;

// Gets called in text input mode for key press events, and for any strings that were read (usually
// just a single character for simple input methods). text_input_cb is called seperately for key
// events and translated strings; only use the key argument if str == NULL.
ZTextInputCallback text_input_cb;


// Make sure there is enough room for inserting a string of len bytes (excluding null-terminator)
// in textbuf, resize if neccesary. Returns 1 if it fits, or 0 if resizing failed for whatever
// reason.
static int zGrowTextBuffer(ZTextBuffer *textbuf, unsigned int bytes)
{
    size_t reqsize;
    char *tmp, *tmp_shadow;
    int init = 0;

    // Figure out the lowest multiple of Z_TEXTBUFFER_SIZE that I can fit current string size+bytes
    // in. Need to account for the null-byte as well since that is not accounted for in
    // textbuf->bytes.
    reqsize = (((textbuf->bytes+1 + bytes)/Z_TEXTBUFFER_SIZE)+1) * Z_TEXTBUFFER_SIZE;

    //zDebug("reqsize=%d, bufsize=%d", reqsize, textbuf->bufsize);

    // See if I either need to do the initial alloc, grow it, or nothing if it already fits.
    if ( !(textbuf->buf) ) {

        assert(!textbuf->buf_shadow);
        tmp         = malloc(reqsize);
        tmp_shadow  = malloc(reqsize);
        init = 1;

    } else if (textbuf->bufsize < reqsize) {

        tmp         = realloc(textbuf->buf, reqsize);
        tmp_shadow  = realloc(textbuf->buf_shadow, reqsize);

    } else {
        return 1;
    }

    // If any allocation failed, leave textbuf as is and return false.
    if ( !tmp || !tmp_shadow ) {

        free(tmp);
        free(tmp_shadow);
        zError("Failed to allocate memory for text buffer.");
        return 0;
    }

    textbuf->bufsize    = reqsize;
    textbuf->buf        = tmp;
    textbuf->buf_shadow = tmp_shadow;

    if (init) textbuf->buf[0] = '\0'; // Must null-terminate buffer on initial alloc.

    return 1;
}



// Update text buffer. Intended to be called by the text input handler. The text input callback
// serves as a sort of filter that usually just passes it parameters along to this function to
// update a ZTextBuffer.
void zUpdateTextBuffer(ZTextBuffer *textbuf, ZKeyEvent *zkev, char *str)
{
    // If str was given we just need to insert text at curspor position.
    if (str) {

        unsigned int len = strlen(str);

        if (len > 0) {

            // Ignore control characters, I'm assuming these are never part of multi-character
            // strings.
            if ( (unsigned char) *str < 0x20 || *str == 0x7f ) {
                //zDebug("Ignoring ASCII control character: %#x", (unsigned int) *str);
                if (len > 1) zWarning("%s: Ignored control character that was part of"
                    " multicharacter string", __func__);
                return;
            }

            // Normal string, insert/append (in)to buffer.
            //zDebug("Would append %s to text buffer.", str);

            // Make sure str fits.
            if (zGrowTextBuffer(textbuf, len)) {

                // Copy stuff after cursorpos to shadow buffer, insert stuff at cursorpos, copy
                // string in shadow buffer back.

                // Copy stuff after cursorpos to temp buffer.
                strcpy(textbuf->buf_shadow, (textbuf->buf)+textbuf->cursor_bytes);

                // Append str at current cursor pos., update cursorpos.
                strcpy(textbuf->buf+textbuf->cursor_bytes, str);
                textbuf->cursor_bytes += len;

                // Copy tempbuffer back.
                strcat(textbuf->buf, textbuf->buf_shadow);
                textbuf->bytes += len;

                //zDebug("bytes=%d, cursor_bytes=%d", textbuf->bytes, textbuf->cursor_bytes);

            } else {
                zWarning("Failed to resize text buffer");
            }
        }

    // Else we handle some special keys for deletion and cursor movement. It's safe to skip all
    // that if the textbuffer is empty.
    } else if (textbuf->bytes) {

        char *next, *prev, *cursor_pos = textbuf->buf + textbuf->cursor_bytes;

        // Handle some special keys.
        switch (zkev->key) {

            case KEY_BACKSPACE:

                if (textbuf->cursor_bytes) {

                    // Find last character position that is before cursorpos.
                    prev = zUTF8FindPrevChar(textbuf->buf, cursor_pos);

                    // Copy stuff from cursorposition to the character directly before the cursor.
                    memmove(prev, cursor_pos, textbuf->bytes - textbuf->cursor_bytes + 1);

                    // Update bytecount and cursor position.
                    textbuf->bytes -= cursor_pos - prev;
                    textbuf->cursor_bytes = prev - textbuf->buf;
                }
                break;

            case KEY_DELETE:

                // Don't bother if cursor is at end of string.
                if (textbuf->cursor_bytes != textbuf->bytes) {

                    // Find next character, copy from there to cursor position.
                    next = zUTF8FindNextChar(cursor_pos);

                    if (next != cursor_pos ) {
                        memmove(cursor_pos, next, strlen(next)+1);
                        textbuf->bytes -= next - cursor_pos;
                    }
                }
                break;

            case KEY_LEFT: // Move cursor to previous character.

                if (textbuf->cursor_bytes > 0) {

                    // Find character position before cursorpos.
                    prev = zUTF8FindPrevChar(textbuf->buf, cursor_pos);

                    // Update cursor pos. and bytecount
                    textbuf->cursor_bytes = prev - textbuf->buf;

                    assert(textbuf->cursor_bytes >= 0);
                }
                break;

            case KEY_RIGHT: // Move cursor to next character.

                // Find character position before cursor.
                next = zUTF8FindNextChar(cursor_pos);

                // Update cursor pos.
                textbuf->cursor_bytes = next - textbuf->buf;

                assert(textbuf->cursor_bytes >= 0);
                break;

            case KEY_HOME:
                textbuf->cursor_bytes = 0;
                break;

            case KEY_END:
                textbuf->cursor_bytes = textbuf->bytes;
                break;

            default:
                break;
        }
    } else {
        assert(textbuf->cursor_bytes == 0);
    }
}



// Reset text buffer (empty string, reset cursor pos etc.).
void zResetTextBuffer(ZTextBuffer *textbuf)
{
    // buf can be NULL if no characters have ever been written to the buffer.
    if (textbuf->buf != NULL) textbuf->buf[0] = '\0';

    textbuf->cursor_bytes = 0;
    textbuf->bytes = 0;
}



// Lookup ZKeyBinding by ZKeyEvent. Returns pointer to ZKeyBinding on success or NULL otherwise.
ZKeyBinding *zLookupKeyBinding(const ZKeyEvent *zkev)
{
    unsigned int i;

    // Loop over all defined keybindings until we find a match with the given keyevent.
    for (i = 0; i < numkeybindings; i++) {

        // Test if keyevents match.
        if ( zkev->key      == keybindings[i].keyevent.key &&
             zkev->keystate == keybindings[i].keyevent.keystate &&
             zkev->modmask  == keybindings[i].keyevent.modmask ) {

            return &(keybindings[i]);
        }
    }

    return NULL;
}



// Lookup ZKey by keyname. keyname should be supplied without the "KEY_" prefix.
ZKey zLookupKey(const char *keyname)
{
    int i;
    char *prefixed_keyname = malloc(strlen(keyname)+5);

    if (prefixed_keyname == NULL) {
        zWarning("Failed to lookup key for \"%s\", failed to allocate memory.", keyname);
        return KEY_UNKNOWN;
    }

    prefixed_keyname[0] = '\0';
    strcat(prefixed_keyname, "KEY_");
    strcat(prefixed_keyname, keyname);

    for (i = 1; i < NUM_KEYS; i++) { // Might as well start at 1 (skip KEY_UNKNOWN)

        if (strcasecmp(prefixed_keyname, zKeyName(i)) == 0) {

            //zDebug("%s: found match for \"%s\": %d\n", __func__, prefixed_keyname, i);
            free(prefixed_keyname);
            return i;
        }
    }

    //zDebug("%s: failed to find match for \"%s\"\n", __func__, prefixed_keyname);

    free(prefixed_keyname);
    return KEY_UNKNOWN;
}



// Get the short symbolic name of key ("KEY_SPACEBAR").
const char *zKeyName(ZKey key)
{
    assert(key < NUM_KEYS);

    return keynames[key].name;
}



// Get description of key ("Spacebar").
const char *zKeyDesc(ZKey key)
{
    assert(key < NUM_KEYS);

    return keynames[key].desc;
}



// Returns a pointer to a string containing a friendly name of the key event in the form of for
// example "CTRL+ALT+Q press  " or "Q release". The string is allocated in static memory and should
// not be freed. It will be modified on a subsequent call of this function.
char *zKeyEventName(ZKeyEvent *kev)
{
    // I'm strcatting into a statically allocated string of fixed size. Since the maximum length of
    // the key names are somewhat fixed this shouldnt be a problem.
    static char name[256];

    name[0] = '\0';

    if (kev->modmask & Z_KEY_MOD_CTRL)  strcat(name, "CTRL+");
    if (kev->modmask & Z_KEY_MOD_LALT)  strcat(name, "LALT+");
    if (kev->modmask & Z_KEY_MOD_RALT)  strcat(name, "RALT+");
    if (kev->modmask & Z_KEY_MOD_SHIFT) strcat(name, "SHIFT+");
    if (kev->modmask & Z_KEY_MOD_SUPER) strcat(name, "SUPER+");

    strcat(name, zKeyName(kev->key)+4);
    strcat(name, kev->keystate == Z_KEY_STATE_PRESS ? " press  " : " release");

    return name;
}



static ZKeyEvent *pressed_keys;



// Release currently pressed keys, and (unless skip_keybinds is set) run associated keybindings.
void zReleaseKeys(int skip_keybinds)
{
    ZKeyEvent *tmp, *cur = pressed_keys;
    ZKeyBinding *kb;

    while (cur != NULL) {

        // Run keybinding.
        if (!skip_keybinds) {
            cur->keystate = Z_KEY_STATE_RELEASE;
            if ( (kb = zLookupKeyBinding(cur)) != NULL ) {

                if (kb->numcommands > 0 )
                    zExecParsedCmds(kb->parsedcmds, kb->numcommands);
            }
        }
        tmp = cur->next;
        free(cur);
        cur = tmp;
    }
    pressed_keys = NULL;
}



// Add copy of keyevent to pressed_keys_left.
static void zAddKeyPressToList(const ZKeyEvent *zkev)
{
    ZKeyEvent *tmp, *new;

    // Scan through list and see if there's a match for given key event. If a match is found,
    // nothing needs to be added and we return. Since this should ideally not happen I'll print a
    // warning as well. I might be able to get away with skipping this and linking it in directly,
    // not sure..
    tmp = pressed_keys;

    while (tmp != NULL) {
        if (tmp->key == zkev->key && tmp->modmask == zkev->modmask) {
            zWarning("Key %s (modmask %d) was already in pressed key list.", zKeyName(zkev->key),
                zkev->modmask);
            return;
        }
        tmp = tmp->next;
    }

    // Make copy of zkev for linking into list.
    if ( (new = malloc(sizeof(ZKeyEvent))) == NULL ) {
        zError("%s: Failed to allocate memory.", __func__);
        return;
    }
    *new = *zkev;

    tmp = pressed_keys;
    pressed_keys = new;
    new->next = tmp;
}



// Execute any command(s) associated with key event. A key press event is recorded in a linked list
// so I always know which keys are down. This helps with preventing keys from getting 'stuck' like
// when the window loses focus, and lets me handle 'modified' keys more intuitively.
void zDispatchKeyEvent(const ZKeyEvent *zkev)
{
    ZKeyBinding *kb;

    // If its a key press, just add to pressed key list and run any bound commands. If it's a key
    // release, see if it's in the pressed key list and if so remove it and run bound commands.
    if ( zkev->keystate == Z_KEY_STATE_PRESS) {

        zAddKeyPressToList(zkev);

        if ( (kb = zLookupKeyBinding(zkev)) != NULL ) {
            if (kb->numcommands > 0 )
                zExecParsedCmds(kb->parsedcmds, kb->numcommands);
        }
    } else {

        ZKeyEvent *cur = pressed_keys, **prevptr = &pressed_keys;

        // Go through pressed_keys and run keybinding if the keysym matches (ignoring modmask).
        while (cur) {

            // Check if current key matches, if so, run keybinding and unlink.
            if (cur->key == zkev->key) {

                // Run keybinding for the key release.
                cur->keystate = Z_KEY_STATE_RELEASE;
                if ( (kb = zLookupKeyBinding(cur)) != NULL ) {

                    if (kb->numcommands > 0 )
                        zExecParsedCmds(kb->parsedcmds, kb->numcommands);
                }

                // Unlink.
                *prevptr = cur->next;
                free(cur);
                cur = *prevptr;

            } else {
                prevptr = &(cur->next);
                cur = cur->next;
            }
        }
    }

    return;
}



// Grow keybindings array if there's no room left. Returns the number of free elements that can
// still be used, which may be 0 if (re)allocating memory fails.
static int zGrowKeyBindingsArray(void)
{
    // Array should always be big enough to hold kbcount elements before I realloc.
    assert(keybindings_size >= numkeybindings);

    // Check if we need to either initially alloc the array, or grow it.
    if (keybindings_size == 0) {

        keybindings = malloc( sizeof(ZKeyBinding) * 50);

        if (keybindings == NULL) {
            // Failed to allocate, simply skip adding this key binding and happily continue.
            zDebug("%s:%s: Failed to alloc memory for keybindings array.", __FILE__, __LINE__);
            return 0;
        }
        keybindings_size = 50;

    // If the array is just big enough to hold current kbcount elements, it needs to grow to add
    // this one.
    } else if ( keybindings_size == numkeybindings ) {

        // Store new pointer in a temporary var so we can roll back if realloc fails and returns
        // NULL, instead of overwriting *kbs and losing the previous pointer.
        ZKeyBinding *tmp_kbs;
        tmp_kbs = realloc(keybindings, sizeof(ZKeyBinding) * (keybindings_size+50));
        if (tmp_kbs == NULL) {
            zDebug("%s:%s: Failed to realloc memory for keybindings array.", __FILE__, __LINE__);
            return 0;
        }
        keybindings = tmp_kbs;
        keybindings_size += 50;
    }

    return keybindings_size - numkeybindings;
}



// Add keybinding to global array for given key event. cmdstring contains the command string for the
// binding and should always be a valid pointer. If there is a syntax error in cmdstring it is not
// added and FALSE is returned, else TRUE. zAddKeyBinding will take care of making sure cmdstring is
// either freed (when parsing it fails) or attached to the keybinding.
int zAddKeyBinding(const ZKeyEvent *zkev, char *cmdstring)
{
    ZKeyBinding *kb;
    ZParsedCommand *parsedcmds;
    int numcmds;

    // If parsing the cmdline fails below I may have resized for nothing, but it shouldn't really be
    // a problem. If growing the array fails and there's no space left I return here to prevent
    // overflowing it.
    if (zGrowKeyBindingsArray() == 0) {
        free(cmdstring);
        return FALSE;
    }

    // Parse cmdstring, don't bother doing anything if 0 commands were parsed from the cmdstring.
    if ( (numcmds = zParseCmdString(cmdstring, &parsedcmds)) == 0) {

        //zDebug("%s: No commands parsed for %s, skipping.", __func__, zKeyName(zkev->key));
        free(cmdstring);
        return FALSE;
    }

    // See if there already is a binding for this keyevent, overwrite it if there is.
    if ( (kb = zLookupKeyBinding(zkev)) != NULL) {

        // Maybe turn this into a nicer warning with keyname/modifiers.
        //zDebug("%s: Overriding keybinding for %s.", __func__, zKeyName(zkev->key));

        // Free all the allocated data for the existing keybinding.
        zFreeParsedCmds(kb->parsedcmds, kb->numcommands);
        free(kb->cmdstring);

    } else {

        // None existed for this key, add new. Previous call to zGrowKeyBindingsArray ensures there
        // is enough room.
        kb = &(keybindings[numkeybindings++]);
    }

    kb->keyevent    = *zkev;
    kb->cmdstring   = cmdstring;
    kb->numcommands = numcmds;
    kb->parsedcmds  = parsedcmds;

    return TRUE;
}



// Load keybindings from file. Returns 0 on error, 1 otherwise.
int zLoadKeyBindings(const char *filename)
{
    FILE *fp;
    const char *fullpath;

    // Open file for input
    fp = zOpenFile(filename, NULL, &fullpath, Z_FILE_TRYUSER);

    if (fp == NULL) {
        zWarning("Failed to load keybindings from \"%s\".", fullpath);
        return 0;
    }

    zPrint("Loading keybindings from \"%s\".\n", fullpath);

    // Parse keybindings.
    // TODO: Maybe zParseKeyBindings should return to indicate failure success as well
    zParseKeyBindings(fp, filename);

    fclose(fp);
    return 1;
}


// If any keybindings were changed, dump keybindings to file. Returns 0 on error, 1 otherwise.
int zWriteKeyBindings(const char *filename)
{
    // TODO
    // XXX: Maybe I should set a flag when a keybinding is changed by the user, so I don't write it
    // if I don't have to.
    return 0;
}

