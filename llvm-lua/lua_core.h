
#ifndef lua_core_h
#define lua_core_h

#ifdef __cplusplus
extern "C" {
#endif

#define lua_c
#define loslib_c
#define LUA_CORE

/* Lua interpreter with LLVM JIT support. */
#define JIT_SUPPORT

/* state */
#define JIT_PROTO_STATE \
	lua_CFunction jit_func; /* jit compiled function */ \
	void *func_ref; /* Reference to Function class */

#include <lua.h>
/* extern all lua core functions. */
#undef LUAI_FUNC
#define LUAI_FUNC extern

typedef unsigned int LuaInstruction;

#ifdef __cplusplus
}
#endif

#endif

