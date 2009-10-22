#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#ifndef WIN32
#include <strings.h>
#endif

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>


#include "common.h"


// Names and descriptions for each key.
struct
{
    char *name;
    char *desc;

} keynames[Z_NUM_KEYS] = {
    #define MAKE_ARRAYINITS
    #include "keys.def"
    #undef MAKE_ARRAYINITS
};


// Dynamically allocated array containing all the currently defined keybindings.
ZKeyBinding *keybindings;
size_t keybindings_size;     // Real size of array.
unsigned int numkeybindings; // Number of defined keybindings.


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
    // If str was given I just need to insert text at curspor position.
    if (str) {

        unsigned int len = strlen(str);

        if (len > 0) {

            // Ignore control characters, I'm assuming these are never part of multi-character
            // strings.
            if ( (unsigned char) *str < 0x20 || *str == 0x7f ) {

                if (len > 1) zWarning("%s: Ignored control character that was part of"
                    " multicharacter string", __func__);
                return;
            }

            // Normal string, insert/append (in)to buffer.
            if (zGrowTextBuffer(textbuf, len)) {

                // Copy stuff after cursorpos to shadow buffer.
                strcpy(textbuf->buf_shadow, (textbuf->buf)+textbuf->cursor_bytes);

                // Append str at current cursor pos., update cursorpos.
                strcpy(textbuf->buf+textbuf->cursor_bytes, str);
                textbuf->cursor_bytes += len;

                // Copy tempbuffer back.
                strcat(textbuf->buf, textbuf->buf_shadow);
                textbuf->bytes += len;

            } else {
                zWarning("Failed to resize text buffer");
            }
        }

    // Else I handle some special keys for deletion and cursor movement. It's safe to skip all
    // that if the textbuffer is empty.
    } else if (textbuf->bytes) {

        char *next, *prev, *cursor_pos = textbuf->buf + textbuf->cursor_bytes;

        // Handle some special keys.
        switch (zkev->key) {

            case Z_KEY_BACKSPACE:

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

            case Z_KEY_DELETE:

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

            case Z_KEY_LEFT: // Move cursor to previous character.

                if (textbuf->cursor_bytes > 0) {

                    // Find character position before cursorpos.
                    prev = zUTF8FindPrevChar(textbuf->buf, cursor_pos);

                    // Update cursor pos. and bytecount
                    textbuf->cursor_bytes = prev - textbuf->buf;

                    assert(textbuf->cursor_bytes >= 0);
                }
                break;

            case Z_KEY_RIGHT: // Move cursor to next character.

                // Find character position before cursor.
                next = zUTF8FindNextChar(cursor_pos);

                // Update cursor pos.
                textbuf->cursor_bytes = next - textbuf->buf;

                assert(textbuf->cursor_bytes >= 0);
                break;

            case Z_KEY_HOME:
                textbuf->cursor_bytes = 0;
                break;

            case Z_KEY_END:
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

    // Loop over all defined keybindings until there's a match with the given keyevent.
    for (i = 0; i < numkeybindings; i++) {

        if ( zkev->key      == keybindings[i].keyevent.key &&
             zkev->keystate == keybindings[i].keyevent.keystate &&
             zkev->modmask  == keybindings[i].keyevent.modmask ) {

            return &(keybindings[i]);
        }
    }

    return NULL;
}



// Lookup ZKey by keyname.
ZKey zLookupKey(const char *keyname)
{
    int i;

    // Might as well start at 1 (skip Z_KEY_UNKNOWN)
    for (i = 1; i < Z_NUM_KEYS; i++) {

        if (strcasecmp(keyname, zKeyName(i)) == 0)
            return i;
    }

    return Z_KEY_UNKNOWN;
}



// Get the short symbolic name of key ("KEY_KP_DIVIDE").
const char *zKeyName(ZKey key)
{
    assert(key < Z_NUM_KEYS);

    return keynames[key].name;
}



// Get description of key ("Keypad Divide").
const char *zKeyDesc(ZKey key)
{
    assert(key < Z_NUM_KEYS);

    return keynames[key].desc;
}



// Returns a pointer to a string containing a friendly name of the key event in the form of for
// example "CTRL+ALT+Q press" or "Q release". The string is allocated in static memory and should
// not be freed. It will be modified on a subsequent call of this function.
char *zKeyEventName(ZKeyEvent *kev)
{
    // I'm concatenating into a statically allocated string of fixed size. Since the maximum length
    // of the key names are somewhat fixed this shouldnt be a problem.
    static char name[128];

    name[0] = '\0';

    if (kev->modmask & Z_KEY_MOD_CTRL)  strcat(name, "CTRL+");
    if (kev->modmask & Z_KEY_MOD_LALT)  strcat(name, "LALT+");
    if (kev->modmask & Z_KEY_MOD_RALT)  strcat(name, "RALT+");
    if (kev->modmask & Z_KEY_MOD_SHIFT) strcat(name, "SHIFT+");
    if (kev->modmask & Z_KEY_MOD_SUPER) strcat(name, "SUPER+");

    strcat(name, zKeyName(kev->key)+4); // Remove KEY_ prefix.
    strcat(name, kev->keystate == Z_KEY_STATE_PRESS ? " press  " : " release");

    return name;
}



// Linked list of currently pressed keys (see zDispatchKeyEvent).
// XXX: Maybe it's better to just have a fixed size array or some kind of pooling to prevent a
// lot of allocating/freeing of memory.
static ZKeyEvent *pressed_keys;

// Release currently pressed keys, and, if skip_keybinds is not set, run associated keybindings.
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



// Add copy of keyevent to pressed_keys.
static void zAddKeyPressToList(const ZKeyEvent *zkev)
{
    ZKeyEvent *tmp, *new;

    // Scan through list and see if there's a match for given key event. If a match is found,
    // nothing needs to be added and I return. Since this should ideally not happen I'll print a
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
    if ( zkev->keystate == Z_KEY_STATE_PRESS ) {

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

    // Check if I need to either initially alloc the array.
    if (keybindings_size == 0) {

        keybindings = malloc( sizeof(ZKeyBinding) * 50);

        if (keybindings == NULL) {
            // Failed to allocate, simply skip adding this key binding and happily continue.
            zDebug("%s:%s: Failed to alloc memory for keybindings array.", __FILE__, __LINE__);
            return 0;
        }
        keybindings_size = 50;

    // Else, if the array is just big enough to hold current kbcount elements, it needs to grow to
    // add this one.
    } else if ( keybindings_size == numkeybindings ) {

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



// Process keybinding. This is called from two proxy functions keyup/keydown that indicate the
// keystate as the parameters, and passes on the other parameters iven by the user. The user should
// pass to keybind/keyup the key name, a command string, and zero or more modifier key names.
static int zLuaBindKey(lua_State *L)
{
    int modcount;
    int params = lua_gettop(L);
    ZKey key;
    ZKeyEvent zkev;
    const char *cmdstring;
    char *cmdstring_copy;

    if (params < 3 || !lua_isnumber(L, 1) || !lua_isnumber(L, 2) || !lua_isstring(L, 3) ) {
        zLuaWarning(L, 2, "Not enough or invalid parameters for keybinding.");
        return 0;
    }

    // Need to make a copy of the command string, for passing it to zAddKeyBinding
    cmdstring = lua_tostring(L, 3);
    if (!cmdstring || !strlen(cmdstring)) {
        zLuaWarning(L, 2, "No valid command string given.");
        return 0;
    }

    cmdstring_copy = malloc(strlen(cmdstring)+1);
    if (!cmdstring_copy) {
        zLuaWarning(L, 2, "Failed to allocate memory for cmdstring.");
        return 0;
    }
    strcpy(cmdstring_copy, cmdstring);

    key = (ZKey) lua_tonumber(L, 2);
    if ( !(key > 0 && key < Z_NUM_KEYS) ) {
        zLuaWarning(L, 2, "Invalid key given. %d", key);
        return 0;
    }

    zkev.key = key;
    zkev.keystate = ((int) lua_tonumber(L, 1)) ? Z_KEY_STATE_PRESS : Z_KEY_STATE_RELEASE;
    zkev.modmask = 0;

    // Now OR together all the extra parameters and use them as the modmask.
    modcount = params - 3;
    while (modcount--) {
        zkev.modmask |= (unsigned int) lua_tonumber(L, params-modcount);
    }
    // Make sure only valid bits are set.
    zkev.modmask &= Z_KEY_MOD_MASK;

    //zDebug("zLuaBindKey %s, state %u, modmask %u", zKeyName(key), zkev.keystate, zkev.modmask);

    if (!zAddKeyBinding(&zkev, cmdstring_copy))
        zLuaWarning(L, 2, "Failed to add keybinding, invalid command string.");

    return 0;
}



// Load keybindings from file. Returns 0 on success or an error code (see zParseVariables) on
// failure.
static int zParseKeyBindings(const char *filename, const char *prefix, int flags)
{
    int i;
    const char *path = zGetPath(filename, prefix, flags);
    lua_State *L;

    if (!path) {
        zWarning("%s: Failed to construct path for \"%s\".", __func__, filename);
        return Z_PARSE_ERROR;
    }

    if (zPathExists(path) != Z_EXISTS_REGULAR)
        return Z_PARSE_FILEERROR;


    zPrint("Loading keybindings from \"%s\".\n", path);

    L = luaL_newstate();
    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);

    // Export all the keys and modifiers as globals.
    for (i = 0; i < Z_NUM_KEYS; i++) {
        lua_pushinteger(L, i);
        lua_setglobal(L, zKeyName(i));
    }

    lua_pushinteger(L, Z_KEY_MOD_CTRL);  lua_setglobal(L, "MOD_CTRL");
    lua_pushinteger(L, Z_KEY_MOD_LALT);  lua_setglobal(L, "MOD_LALT");
    lua_pushinteger(L, Z_KEY_MOD_RALT);  lua_setglobal(L, "MOD_RALT");
    lua_pushinteger(L, Z_KEY_MOD_SHIFT); lua_setglobal(L, "MOD_SHIFT");
    lua_pushinteger(L, Z_KEY_MOD_SUPER); lua_setglobal(L, "MOD_SUPER");

    // I make sure bindkey can't be called directly so the call stack level I pass to zLuaWarning
    // always refers to the right chunk.
    lua_register(L, "bindkey", zLuaBindKey);
    zLua(L, "local b = bindkey\n"
            "keydown = function(key, cmd, ...) b(1, key, cmd, ...) end\n"
            "keyup   = function(key, cmd, ...) b(0, key, cmd, ...) end\n"
            "bindkey = nil\n");

    if (luaL_dofile(L, path)) {
        zWarning("Failed to parse keybindings: %s", lua_tostring(L, -1));
        lua_close(L);
        return Z_PARSE_SYNTAXERROR;
    }

    lua_close(L);
    return 0;
}



// Same thing as with loading config (see variables.c).
static int userkbs_badsyntax = 0;


// Load previously saved keybindings for user, or the default one (from the system data directory)
// if nothing has been saved yet.
void zLoadKeyBindings(void)
{
    int err;

    // Attempt to load for user first, if that fails, try loading from system data dir.
    if ( (err = zParseKeyBindings(Z_FILE_KEYBINDINGS, NULL, Z_FILE_FORCEUSER)) ) {

        // See if there was a syntax error.
        userkbs_badsyntax = err == Z_PARSE_SYNTAXERROR;

        zWarning("Failed to load user keybindings, falling back to default keybindings.");

        if ( (err = zParseKeyBindings(Z_FILE_KEYBINDINGS, NULL, 0)) )
            zWarning("Failed to fall back to default keybindings.");
    }
}



// If any keybindings were changed, dump keybindings to file. Returns 0 on error, 1 otherwise.
void zSaveKeyBindings(void)
{
    unsigned int i;
    const char *path;
    FILE *fp;
    ZKeyBinding *kb;
    char keyparam[32];

    if (userkbs_badsyntax) {
        zWarning("Not writing keybindings, check \"%s\" for syntax errors.", Z_FILE_KEYBINDINGS);
        return;
    }

    if ( !(fp = zOpenFile(Z_FILE_KEYBINDINGS, NULL, &path, Z_FILE_FORCEUSER | Z_FILE_WRITE)) ) {
        zWarning("Failed to open \"%s\" for writing keybindings.", path);
        return;
    }

    zPrint("Writing keybindings to \"%s\".\n", path);

    for (i = 0; i < numkeybindings; i++) {

        kb = keybindings+i;

        // Looks better if the comma follows the key name directly!
        assert(strlen(zKeyName(kb->keyevent.key)) < 31);
        strcpy(keyparam, zKeyName(kb->keyevent.key));
        strcat(keyparam, ",");

        if (kb->keyevent.keystate == Z_KEY_STATE_RELEASE)
            fprintf(fp, "keyup  (%-15s \"%s\"", keyparam, kb->cmdstring);
        else
            fprintf(fp, "keydown(%-15s \"%s\"", keyparam, kb->cmdstring);

        if (kb->keyevent.modmask & Z_KEY_MOD_CTRL)  fprintf(fp, ", MOD_CTRL");
        if (kb->keyevent.modmask & Z_KEY_MOD_LALT)  fprintf(fp, ", MOD_LALT");
        if (kb->keyevent.modmask & Z_KEY_MOD_RALT)  fprintf(fp, ", MOD_RALT");
        if (kb->keyevent.modmask & Z_KEY_MOD_SHIFT) fprintf(fp, ", MOD_SHIFT");
        if (kb->keyevent.modmask & Z_KEY_MOD_SUPER) fprintf(fp, ", MOD_SUPER");
        fprintf(fp, ")\n");
    }

    fclose(fp);
}

