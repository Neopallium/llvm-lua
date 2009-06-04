/*
** See Copyright Notice in lua.h
*/

/*
 * lua_normal.c -- Lua core, libraries and interpreter in a single file
 */

#define lua_normal_c

#define luaall_c
#define LUA_CORE
#include "lapi.c"
#include "lcode.c"
#include "ldebug.c"
#include "ldump.c"
#include "lfunc.c"
#include "lgc.c"
#include "llex.c"
#include "lmem.c"
#include "lobject.c"
#include "lopcodes.c"
#include "lparser.c"
#include "lstate.c"
#include "lstring.c"
#include "ltable.c"
#include "ltm.c"
#include "lundump.c"
#include "lvm.c"
#include "lzio.c"

#include "lbaselib.c"
#include "lcoco.c"
#include "ldblib.c"
#include "liolib.c"
#include "linit.c"
#include "lmathlib.c"
#include "loadlib.c"
#include "loslib.c"
#include "lstrlib.c"
#include "ltablib.c"

#include "lauxlib.c"

#include "ldo.c"
#include "lua.c"

#ifdef __cplusplus
}
#endif

