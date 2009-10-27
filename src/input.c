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
// in textbuf, resize if neccesary. Returns TRUE if it fits, or FALSE if resizing failed for
// whatever reason.
static int zGrowTextBuffer(ZTextBuffer *textbuf, unsigned int bytes)
{
    size_t reqsize;
    char *tmp, *tmp_shadow;
    int init = 0;

    // Figure out the lowest multiple of Z_TEXTBUFFER_SIZE that I can fit current string size+bytes
    // in. Need to account for the null-byte as well since that is not accounted for in
    // textbuf->bytes.
    reqsize = (((textbuf->bytes+1 + bytes)/Z_TEXTBUFFER_SIZE)+1) * Z_TEXTBUFFER_SIZE;

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
        return TRUE;
    }

    // If any allocation failed, leave textbuf as is and return false.
    if ( !tmp || !tmp_shadow ) {

        free(tmp);
        free(tmp_shadow);
        zError("Failed to allocate memory for text buffer.");
        return FALSE;
    }

    textbuf->bufsize    = reqsize;
    textbuf->buf        = tmp;
    textbuf->buf_shadow = tmp_shadow;

    if (init) textbuf->buf[0] = '\0'; // Must null-terminate buffer on initial alloc.

    return TRUE;
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
static ZKeyBinding *zLookupKeyBinding(const ZKeyEvent *zkev)
{
    unsigned int i;
    ZKeyBinding *kb = keybindings;

    // Loop over all defined keybindings until there's a match with the given keyevent.
    // TODO: If I want to make this faster, just create an array of NUM_KEYS pointers to linked
    // lists of keybindings for each key.
    for (i = 0; i < numkeybindings; i++) {

        if ( zkev->key      == kb->keyevent.key &&
             zkev->keystate == kb->keyevent.keystate &&
             zkev->modmask  == kb->keyevent.modmask ) {
            return kb;
        }

        kb++;
    }

    return NULL;
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
// example "CTRL+ALT+Q press" or "Q release" (if show_state is 0, no press/release will be added).
// The string is allocated in static memory and should not be freed. It will be modified on a
// subsequent call of this function.
char *zKeyEventName(ZKeyEvent *kev, int show_state)
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

    if (kev->keystate && show_state)
        strcat(name, kev->keystate == Z_KEY_STATE_PRESS ? " press" : " release");

    return name;
}



// Linked list of currently pressed keys (see zDispatchKeyEvent).
// XXX: Maybe a linked list isn't the best solution here
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

                if (kb->impulse && !kb->impulse->press_only) {
                    kb->impulse->handler(kb->impulse->arg, Z_IMPULSE_STOP);
                } else if (kb->lua) {
                    zLuaRunCode(&kb->code);
                }
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
            if (kb->impulse)
                kb->impulse->handler(kb->impulse->arg, Z_IMPULSE_START);
            else if (kb->lua)
                zLuaRunCode(&kb->code);
        }
    } else {

        // Go through pressed_keys and run keybinding if the keysym matches (ignoring modmask).
        ZKeyEvent *cur = pressed_keys, **prevptr = &pressed_keys;

        while (cur) {

            // XXX: Note that I just check the key value, not the modmask, these are ignored
            // intentionally for key releases, since that makes more sense to the user (i.e. if you
            // have something bound to CTRL+A press and release, the user might release CTRL before
            // it releases A, but will probably still want to run the release keybinding for CTRL+A.
            if (cur->key == zkev->key) {

                // Run keybinding for the key release.
                cur->keystate = Z_KEY_STATE_RELEASE;

                if ( (kb = zLookupKeyBinding(cur)) != NULL ) {

                    if (kb->impulse && !kb->impulse->press_only)
                        kb->impulse->handler(kb->impulse->arg, Z_IMPULSE_STOP);
                    else if (kb->lua)
                        zLuaRunCode(&kb->code);
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



// Adds a keybinding. Either impulse or lua must be given, but not both. The lua string should be
// dynamically allocated so it can be freed if the keybinding is replaced later. zAddKeyBinding will
// make sure to free lua if adding the keybinding fails. Returns Z_ERROR_LUA if pre-compiling lua
// fails, Z_ERROR on all other errors, or 0 on success.
static int zAddKeyBinding(const ZKeyEvent *zkev, ZImpulse *impulse, char *lua)
{
    ZKeyBinding *kb;
    ZLuaCode luacode;

    assert(!(impulse && lua) && (impulse || lua));

    if (zGrowKeyBindingsArray() == 0) {
        zWarning("%s: Failed to add keybinding, growing keybindings array failed.", __func__);
        free(lua);
        return Z_ERROR;
    }

    // Try to pre-compile lua string (if needed).
    if (lua) {
        if (!zLuaCompileCode(Z_VM_CONSOLE, &luacode, lua)) {
            free(lua);
            return Z_ERROR_LUA;
        }
    }

    // See if there already is a binding for this keyevent, overwrite it if there is.
    if ( (kb = zLookupKeyBinding(zkev)) != NULL) {

        // Maybe turn this into a nicer warning with keyname/modifiers.
        zDebug("%s: Overriding keybinding for %s.", __func__, zKeyName(zkev->key));

        // Free old lua string and delete compiled chunk.
        zLuaDeleteCode(&kb->code);
        free(kb->lua);

    } else {

        // None existed for this key, add new. Previous call to zGrowKeyBindingsArray ensures there
        // is enough room.
        kb = &(keybindings[numkeybindings++]);
    }

    kb->keyevent = *zkev;
    kb->impulse  = impulse;
    kb->lua      = lua;

    // Only copy this over if lua was given to prevent msvc run-time warning about using
    // uninitialized value..
    if (lua)
        kb->code = luacode;

    return 0;
}



// Processes a keybinding passed from lua. Called by the lua glue code with table as parameter.
static int zLuaBindKey(lua_State *L)
{
    unsigned int key = 0, state = 0, modmask = 0;
    ZKeyEvent zkev;
    ZImpulse *impulse;
    char *lua = NULL;
    unsigned int imp = 0;

    assert(lua_istable(L, -1));

    // Get key event properties from table
    zLuaGetDataUint(L, "state", &state);
    zLuaGetDataUint(L, "key",   &key);
    zLuaGetDataUint(L, "mods",  &modmask);

    // Check that key is valid, and mask away invalid modmask bits, state is given by glue lua and
    // should be correct)
    if ( !(key > 0 && key < Z_NUM_KEYS) ) {
        zLuaWarning(L, 2, "Invalid key given for keybinding.");
        return 0;
    }
    zkev.key = key;
    zkev.modmask = modmask & Z_KEY_MOD_MASK;
    zkev.keystate = state == 0 ? Z_KEY_STATE_RELEASE : Z_KEY_STATE_PRESS;

    // See if a Lua string was passed, or an impulse.
    if(zLuaGetDataStringA(L, "lua", &lua, NULL)) {

        if (zAddKeyBinding(&zkev, NULL, lua) == Z_ERROR_LUA) {
            zLuaWarning(L, 2, "Failed to add keybinding, bad lua for key \"%s\".",
                zKeyName(zkev.key));
        }

    } else if(zLuaGetDataUint(L, "imp", &imp) && (impulse = zLookupImpulse(imp))) {

        // For impulses, add bindings for both key up/down. No need to give a warning about errors
        // here since adding impulse keybindings is unlikely to fail.
        zkev.keystate = Z_KEY_STATE_PRESS;
        zAddKeyBinding(&zkev, impulse, NULL);
        zkev.keystate = Z_KEY_STATE_RELEASE;
        zAddKeyBinding(&zkev, impulse, NULL);

    } else {
        zLuaWarning(L, 2, "Invalid script or impulse given for keybinding.");
        return 0;
    }

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
        return Z_ERROR;
    }

    if (zPathExists(path) != Z_EXISTS_REGULAR)
        return Z_ERROR_FILE;


    zPrint("Loading keybindings from \"%s\".\n", path);

    L = luaL_newstate();
    lua_pushcfunction(L, luaopen_base);
    lua_call(L, 0, 0);

    // Export all the keys, impulses, and modifiers as globals.
    for (i = 0; i < Z_NUM_KEYS; i++) {
        lua_pushinteger(L, i);
        lua_setglobal(L, zKeyName(i));
    }

    for (i = 0; impulses[i].name; i++) {
        lua_pushinteger(L, i);
        lua_setglobal(L, impulses[i].name);
    }

    lua_pushinteger(L, Z_KEY_MOD_CTRL);  lua_setglobal(L, "MOD_CTRL");
    lua_pushinteger(L, Z_KEY_MOD_LALT);  lua_setglobal(L, "MOD_LALT");
    lua_pushinteger(L, Z_KEY_MOD_RALT);  lua_setglobal(L, "MOD_RALT");
    lua_pushinteger(L, Z_KEY_MOD_SHIFT); lua_setglobal(L, "MOD_SHIFT");
    lua_pushinteger(L, Z_KEY_MOD_SUPER); lua_setglobal(L, "MOD_SUPER");

    // Register keybinding handler function, with some glue so it makes more sense to the user.
    lua_register(L, "bindkey",  zLuaBindKey);
    zLua(L,
        "local b = bindkey\n"
        "script_down = function(k,l,...) arg.n=nil;b {state=1, key=k, lua=l, mods=arg} end\n"
        "script_up   = function(k,l,...) arg.n=nil;b {state=0, key=k, lua=l, mods=arg} end\n"
        "impulse     = function(k,i,...) arg.n=nil;b {state=1, key=k, imp=i, mods=arg} end\n"
        "bindkey = nil\n"
    );

    if (luaL_dofile(L, path)) {
        zWarning("Failed to parse keybindings: %s", lua_tostring(L, -1));
        lua_close(L);
        return Z_ERROR_SYNTAX;
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
        userkbs_badsyntax = err == Z_ERROR_SYNTAX;

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

    if (fs_nosave) {
        zPrint("Not writing keybindings (fs_nosave).\n");
        return;
    }


    if ( !(fp = zOpenFile(Z_FILE_KEYBINDINGS, NULL, &path, Z_FILE_FORCEUSER | Z_FILE_WRITE)) ) {
        zWarning("Failed to open \"%s\" for writing keybindings.", path);
        return;
    }

    zPrint("Writing keybindings to \"%s\".\n", path);

    for (i = 0; i < numkeybindings; i++) {

        kb = keybindings+i;

        // Append comma to keyname so it looks slightly nicer
        assert(strlen(zKeyName(kb->keyevent.key)) < 31);
        strcpy(keyparam, zKeyName(kb->keyevent.key));
        strcat(keyparam, ",");

        if (kb->impulse ) {
            // For impulses, since they are always bound to both key presses/releases, I only write
            // them out for the key press event.
            if (kb->keyevent.keystate == Z_KEY_STATE_PRESS)
                fprintf(fp, "impulse    (%-15s %s", keyparam, kb->impulse->name);
            else
                continue;

        } else {
            if (kb->keyevent.keystate == Z_KEY_STATE_RELEASE)
                fprintf(fp, "script_up  (%-15s \"%s\"", keyparam, kb->lua);
            else
                fprintf(fp, "script_down(%-15s \"%s\"", keyparam, kb->lua);
        }

        if (kb->keyevent.modmask & Z_KEY_MOD_CTRL)  fprintf(fp, ", MOD_CTRL");
        if (kb->keyevent.modmask & Z_KEY_MOD_LALT)  fprintf(fp, ", MOD_LALT");
        if (kb->keyevent.modmask & Z_KEY_MOD_RALT)  fprintf(fp, ", MOD_RALT");
        if (kb->keyevent.modmask & Z_KEY_MOD_SHIFT) fprintf(fp, ", MOD_SHIFT");
        if (kb->keyevent.modmask & Z_KEY_MOD_SUPER) fprintf(fp, ", MOD_SUPER");
        fprintf(fp, ")\n");
    }

    fclose(fp);
}

