#include <string.h>
#include <assert.h>
#include <lua.h>

#include "common.h"

// Print a warning message that includes current file, line count, and msg. Should be called from a
// C function that is called from lua.
void zLuaWarning(lua_State *L, const char *msg, ...)
{
    va_list args;
    lua_Debug dbg;

    lua_getstack(L, 1, &dbg);

    if (!lua_getinfo(L, "Sl", &dbg)) {
        zError("%s: Failed to get lua error information.", __func__);
        return;
    }

    fprintf(stderr, "WARNING: %s:%d: ", dbg.short_src, dbg.currentline);

    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    fprintf(stderr, "\n");
}



// The zLuaGetData* functions are intended to make parsing of data-description files easier. Data
// entry is done by calling functions from lua with a single table as parameter. I.e.:
//
//   foo {
//     name         = "foobar",
//     some_number  = 42,
//     a_color      = { 1, 1, 1, 0.5 },
//     options      = { OPTION_A, OPTION_B }
//   }
//
// Property values can be simple strings, tuples for colors (passed in as tables), flags (tables
// where each member needs to be ORed), etc. These functions make getting these values less tedious.
// They all assume the table passed to the 'data entry' function is at the top of the stack, and
// return 1 when a value was succesfully read, or 0 on failure.


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


// Reads number from data table and stores it as unsigned int at *dest. If the value given is a
// table instead, its members are ORed together first (i.e. for parsing flags).
// XXX: I need to rethink wether using this for parsing flags is sensible, some values could be
// pretty big (1<<32), and they will be stored internally in lua as doubles, not sure if I can rely
// on there not being precision problems screwing things up.
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
            zLuaWarning(L, "bad syntax for key \"%s\", number or table containing flags expected.",
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
            zLuaWarning(L, "bad syntax for key \"%s\", number or table containing numbers "
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

