#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include <lua.h>

void zLuaWarning(lua_State *L, const char *msg, ...);


int zLuaGetDataString(lua_State *L, const char *key, char *dest, size_t bufsize);

int zLuaGetDataUint  (lua_State *L, const char *key, unsigned int *dest);

int zLuaGetDataUchar (lua_State *L, const char *key, unsigned char *dest);

int zLuaGetDataFloats(lua_State *L, const char *key, float *dest, int count);


// Handy macro to easily run embedded snippets of Lua code. Should only be used for code that is
// unlikely to cause any non-programmer errors.
#define zLua(state, lua) \
if ( luaL_dostring(state, (lua)) == 1 ) {\
    zWarning("%s:%d: zLua: %s, while doing \"%s\".", __FILE__, __LINE__, lua_tostring(state, -1), lua);\
    lua_pop(state, 1);\
}


#endif // __SCRIPT_H__
