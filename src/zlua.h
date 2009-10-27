#ifndef __ZLUA_H__
#define __ZLUA_H__

#include <lua.h>

// ZLuaCode - A pre-compiled lua chunk that can be executed with zLuaRunCode.
typedef struct ZLuaCode
{
    lua_State *vm; // VM for which the code has been compiled.
    int ref;       // A reference into the VM's registry for the compiled chunk. Equals LUA_NOREF if
                   // compilation failed.
} ZLuaCode;

#define Z_VM_CONSOLE 1


void zLuaInit(void);

void zLuaDeinit(void);


// Running lua code.

int zLuaCompileCode(unsigned int vm, ZLuaCode *code, const char *lua);

void zLuaRunCode(ZLuaCode *code);

void zLuaDeleteCode(ZLuaCode *code);

void zLuaRunString(unsigned int vm, const char *code);

void zLuaRunFile(unsigned int vm, const char *filename);


// Getting values from data tables.

int zLuaGetDataString (lua_State *L, const char *key, char *dest, size_t bufsize);

int zLuaGetDataStringA(lua_State *L, const char *key, char **dest, size_t *len);

int zLuaGetDataUint   (lua_State *L, const char *key, unsigned int *dest);

int zLuaGetDataUchar  (lua_State *L, const char *key, unsigned char *dest);

int zLuaGetDataFloats (lua_State *L, const char *key, float *dest, int count);


// Handy macro to easily run embedded snippets of Lua code. Should only be used for code that is
// unlikely to cause any non-programmer errors.
#define zLua(state, lua) \
if ( luaL_dostring(state, (lua)) == 1 ) {\
    zWarning("%s:%d: zLua: %s, while running \"%s\".", __FILE__, __LINE__, lua_tostring(state, -1),\
         lua);\
    lua_pop(state, 1);\
}


// Print a warning that includes a filename/source string and line number. Level refers to the call
// stack level (0 is current function, 1 is function calling current function, and so on).
// XXX: Uncomment the function version of this macro ever becomes problematic, not sure how well
// supported using varargs like this is. See http://gcc.gnu.org/ml/gcc/2000-09/msg00638.html and
// http://msdn.microsoft.com/en-us/library/ms177415.aspx
#define zLuaWarning(state, level, msg, ...) do {\
    const char *where;\
    luaL_where((state), (level));\
    where = lua_tostring((state), -1);\
    zWarning("%s" msg, where, ##__VA_ARGS__);\
    lua_pop((state), 1);\
} while (0)


#endif
