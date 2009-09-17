/*
  load_jit_proto.h -- load jit proto

  Copyright (c) 2009 Robert G. Jakabosky
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
  
  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
  
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.

  MIT License: http://www.opensource.org/licenses/mit-license.php
*/

#ifndef load_jit_proto_h
#define load_jit_proto_h

#ifdef __cplusplus
extern "C" {
#endif

#include "lobject.h"
#include <stdint.h>

#define TYPE_NIL			0
#define TYPE_NUMBER		1
#define TYPE_BOOLEAN	2
#define TYPE_STRING		3

typedef union constant_value {
	/* nil doesn't need a value. */
	uint32_t b; /* Lua boolean */
	LUA_NUMBER num; /* Lua numbers */
	char *str; /* Lua string. */
} constant_value;

typedef struct constant_type {
	uint32_t type;   /* constant type. */
	uint32_t length; /* string length */
	constant_value val; /* value of Lua nil/boolean/number/string. */
} constant_type;

/* simplified version of LocVar struct. */
typedef struct jit_LocVar {
	char *varname;
	uint32_t startpc;
	uint32_t endpc;
} jit_LocVar;

/* simplified version of Proto struct. */
typedef struct jit_proto jit_proto;
struct jit_proto {
	char          *name;
	lua_CFunction jit_func;
	uint32_t      linedefined;
	uint32_t      lastlinedefined;
	uint8_t       nups;
	uint8_t       numparams;
	uint8_t       is_vararg;
	uint8_t       maxstacksize;
	uint16_t      sizek;
	uint16_t      sizelocvars;
	uint32_t      sizeupvalues;
	uint32_t      sizep;
	uint32_t      sizecode;
	uint32_t      sizelineinfo;
	constant_type *k;
	jit_LocVar    *locvars;
	char          **upvalues;
	jit_proto     *p;
	uint32_t      *code;
	uint32_t      *lineinfo;
};

Proto *load_jit_proto(lua_State *L, jit_proto *p);

LUALIB_API int load_compiled_protos(lua_State *L, jit_proto *p);
LUALIB_API int load_compiled_module(lua_State *L, jit_proto *p);

extern jit_proto jit_proto_init;

#ifdef __cplusplus
}
#endif

#endif

