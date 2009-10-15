
#ifndef lua_core_h
#define lua_core_h

/* This is a hack to force 32bit alignment when compiling with clang-cc for the ARM architecture. */
#ifdef FORCE_ALIGNMENT
#pragma pack(FORCE_ALIGNMENT)
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define lua_c
#define loslib_c
#define LUA_CORE

/* Lua interpreter with LLVM JIT support. */
#define JIT_SUPPORT

/* extra variables for global_State */
#define JIT_COMPILER_STATE \
	void *llvm_compiler;

/* state */
#define JIT_PROTO_STATE \
	lua_CFunction jit_func; /* jit compiled function */ \
	void *func_ref; /* Reference to Function class */

#include <lua.h>
/* extern all lua core functions. */
#undef LUAI_FUNC
#define LUAI_FUNC extern

#ifdef __cplusplus
}
#endif

#endif

