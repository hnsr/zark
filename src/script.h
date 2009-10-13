#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include <lua.h>

void zLuaWarning(lua_State *L, const char *msg, ...);


int zLuaGetDataString(lua_State *L, const char *key, char *dest, size_t bufsize);

int zLuaGetDataUint  (lua_State *L, const char *key, unsigned int *dest);

int zLuaGetDataUchar (lua_State *L, const char *key, unsigned char *dest);

int zLuaGetDataFloats(lua_State *L, const char *key, float *dest, int count);

#endif // __SCRIPT_H__
