#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "common.h"


// The console VM is used for running simple commands that are typed at the console or bound to
// keys.
static lua_State *console_vm;

void zInitConsoleVM(lua_State *L); // Defined in zlua_console.c


// Initialize lua stuff, VM states etc.
void zLuaInit(void)
{
    console_vm = luaL_newstate();
    zInitConsoleVM(console_vm);
}


// Close VMs.
void zLuaDeinit(void)
{
    lua_close(console_vm);
}



// Compiles a lua string and initializes 'code' so it can be executed later with zLuaRunCode.
// Returns TRUE on success or FALSE when there was an error compiling the lua string. On error,
// attempting to run the resulting code struct will simply do nothing.
int zLuaCompileCode(unsigned int vm, ZLuaCode *code, const char *lua)
{
    int err, ref;

    code->vm = NULL;
    code->ref = LUA_NOREF;

    assert(lua);

    if (vm == Z_VM_CONSOLE)
        code->vm = console_vm;
    else
        assert(0 && "Invalid vm");

    // Attempt to pre-compile lua.
    if ( (err = luaL_loadstring(code->vm, lua)) ) {
        //zWarning("Failed to pre-compile code: %s", lua_tostring(code->vm, -1));
        return FALSE;
        lua_pop(code->vm, 1);
    } else {
        // Store compiled chunk in registry, and store a ref in code->ref.
        ref = luaL_ref(code->vm, LUA_REGISTRYINDEX);
        assert(ref != LUA_REFNIL && ref != LUA_NOREF);
        code->ref = ref;
    }

    return TRUE;
}



// Execute the ZLuaCode object (either from scratch with code->lua if not compiled already, or else
// using code->ref). If there is an error in the Lua code a warning will be printed. For other
// error no warnings are printed (zLuaInitCode will have warned about those already).
void zLuaRunCode(ZLuaCode *code)
{
    int err;

    assert(code->vm);

    if (code->ref != LUA_NOREF) {

        lua_rawgeti(code->vm, LUA_REGISTRYINDEX, code->ref);

        if (!lua_isfunction(code->vm, -1)) {
            zWarning("%s: Value for code->ref is not a function.", __func__);
            lua_pop(code->vm, 1);
        } else {
            if ( (err = lua_pcall(code->vm, 0, 0, 0)) ) {
                zWarning("Failed to run lua: %s", lua_tostring(code->vm, -1));
                lua_pop(code->vm, 1);
            }
        }
    }
}



// Deletes the compiled lua chunk.
void zLuaDeleteCode(ZLuaCode *code)
{
    luaL_unref(code->vm, LUA_REGISTRYINDEX, code->ref);
    code->vm = NULL;
    code->ref = LUA_NOREF;
}



// Run a snippet of lua on the fly in vm. Prints a warning if there were any errors running the
// code.
void zLuaRunString(unsigned int vm, const char *code)
{
    if (vm == Z_VM_CONSOLE) {
        if (luaL_dostring(console_vm, code)) {
            zWarning("Failed to run lua: %s", lua_tostring(console_vm, -1));
            lua_pop(console_vm, 1);
        }
    } else {
        assert(0 && "Invalid vm");
    }
}


// Run a lua script. Prints a warning if there were any errors running the code.
void zLuaRunFile(unsigned int vm, const char *filename)
{
    const char *path = zGetPath(filename, NULL, Z_FILE_TRYUSER);

    if (!path) {
        zError("%s: Failed to construct path for \"%s\".", __func__, filename);
        return;
    }

    if (vm == Z_VM_CONSOLE) {
        if (luaL_dofile(console_vm, path)) {
            zWarning("Failed to run lua: %s", lua_tostring(console_vm, -1));
            lua_pop(console_vm, 1);
        }
    } else {
        assert(0 && "Invalid vm");
    }
}


#if 0
// Helper function for C functions exported to Lua, that prints a warning message that includes
// current file, line count, and msg. Since this is typically called from a C function called by
// user Lua code, level should probably be 1, or higher depending on how I glue things together..
void zLuaWarning(lua_State *L, int level, const char *msg, ...)
{
    va_list args;
    const char *where;

    luaL_where(L, level);
    where = lua_tostring(L, -1);

    fprintf(stderr, "WARNING: %s: ", where);
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");

    return;
}
#endif



// The zLuaGetData* functions are for easily getting values from a table (at the top of the stack).

// Get a string from data table. Bufsize is the size of the buffer pointed to by dest, which is
// where the string is copied into (bufsize should include room for the terminating null byte).
// Note that even though lua strings may contain null bytes, this function won't read beyond the
// first null byte. If the string doesn't fit in dest, it will be truncated (but always properly
// null-terminated).
int zLuaGetDataString(lua_State *L, const char *key, char *dest, size_t bufsize)
{
    int res = 0;
    const char *name;

    // Push key on the stack and get the corresponding value.
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    name = lua_tostring(L, -1);

    if (name && strlen(name)) {
        strncpy(dest, name, bufsize);
        dest[bufsize-1] = '\0';
        res = 1;
    }

    // Pop value from the stack.
    lua_pop(L, 1);
    return res;
}

// Same as zLuaGetDataString, but allocates new string and points *dest to it. If len is not NULL,
// length of string (excluding trailing NUL byte) is returned in *len.
int zLuaGetDataStringA(lua_State *L, const char *key, char **dest, size_t *len)
{
    int res = 0;
    const char *str;
    char *copy;

    // Push key on the stack and get the corresponding value.
    lua_pushstring(L, key);
    lua_gettable(L, -2);
    str = lua_tostring(L, -1);

    if (str && strlen(str)) {

        if ( (copy = strdup(str)) ) {

            if (len) *len = strlen(copy);
            *dest = copy;
            res = 1;

        } else {
            zWarning("%s: Failed to duplicate string.", __func__);
        }
    }

    // Pop value from the stack.
    lua_pop(L, 1);
    return res;
}



// Reads number from data table and stores it as unsigned int at *dest. If the value given is a
// table instead, its members are ORed together first (i.e. for parsing flags).
// XXX: As long as Lua uses doubles internally, it should be ok to use this for passing unsigned int
// bit flags of up to 1<<31.
int zLuaGetDataUint(lua_State *L, const char *key, unsigned int *dest)
{
    int res = 0;
    unsigned int flags = 0;

    // Get the table member.
    lua_pushstring(L, key);
    lua_gettable(L, -2);

    // If we got nil, the key didn't exist, which is fine.. (no warning is emitted)
    if ( !lua_isnil(L, -1) ) {

        // But if the key does exist, it needs to be a table or a number, and I should emit a
        // warning if it's neither.
        if ( lua_isnumber(L, -1) ) {

            *dest = (unsigned int) lua_tonumber(L, -1);
            res = 1;

        // If it's a table, or its members into flags.
        } else if (lua_istable(L, -1)) {

            lua_pushnil(L); // Start with nil key..

            while (lua_next(L, -2)) { // Takes key from stack, then pushes key+value (or nothing on
                                      // table end) on the stack.
                if (lua_isnumber(L, -1)) { // XXX: Warn when not a number?
                    res = 1;
                    flags |= (unsigned int) lua_tonumber(L, -1);
                }
                lua_pop(L, 1); // Pop value from the stack, leaving the key for the loop.
            }

            // Only write dest if anything was read from the flags table.
            if (res) *dest = flags;

        } else {
            zLuaWarning(L, 1, "bad syntax for key \"%s\", number or table containing flags expected.",
                key);
        }
    }

    // Pop the table (or nil/number)..
    lua_pop(L, 1);

    return res;
}



// Same as zLuaGetDataUint but for unsigned chars.
int zLuaGetDataUchar(lua_State *L, const char *key, unsigned char *dest)
{
    unsigned int uint_dest = 0;
    int res;

    // I guess I can just reuse the uint version..
    res = zLuaGetDataUint(L, key, &uint_dest);

    if (res) *dest = (unsigned char) uint_dest;

    return res;
}



// Read up to count floats. The value associated with key must be a number or table of numbers. If
// less than count floats were read, the remaining floats are initialized to 0.0f, except if no
// floats at all could be read, then nothing is written and 0 is returned.
int zLuaGetDataFloats(lua_State *L, const char *key, float *dest, int count)
{
    int res = 0, orig_count = count;

    lua_pushstring(L, key);
    lua_gettable(L, -2);

    assert(count);

    if ( !lua_isnil(L, -1) ) {

        if ( lua_isnumber(L, -1) && count ) { // Checking for count in the useless case of count=0..

            dest[0] = (float) lua_tonumber(L, -1);
            count--;
            res = 1;

        } else if (lua_istable(L, -1)) {

            lua_pushnil(L); // Start with nil key..

            // Iterate over every value in table, only write to dest if count > 0
            while (lua_next(L, -2)) {

                if (count) {

                    if (lua_isnumber(L, -1))
                        dest[orig_count-count] = (float) lua_tonumber(L, -1);
                    else
                        dest[orig_count-count] = 0.0f;

                    res = 1;
                    count--;
                }
                lua_pop(L, 1);
            }
        } else {
            zLuaWarning(L, 1, "bad syntax for key \"%s\", number or table containing numbers "
                "expected.", key);
        }
    }

    // If any floats were read (res!=0), make sure any remaining ones are initialized
    while (res && count) {
        dest[orig_count-count] = 0.0f;
        count--;
    }

    lua_pop(L, 1);
    return res;
}

