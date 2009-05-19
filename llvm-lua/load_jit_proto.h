/*
  load_jit_proto.h -- load jit proto

  Copyright (c) 2008 Robert G. Jakabosky
  
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

#define constant_type_len(type, length) (((length & 0x3FFFFFFF) << 3) + (type & 0x03))
#define get_constant_type(type_length) (type_length & 0x03)
#define get_constant_length(type_length) ((type_length >> 3) & 0x3FFFFFFF)

#define TYPE_NIL			0
#define TYPE_NUMBER		1
#define TYPE_BOOLEAN	2
#define TYPE_STRING		3

typedef struct {
	int type_length; /* top 2 bits used for type, buttom 30 bits used for string length */
	union {
		/* nil doesn't need a value. */
		int b; /* Lua boolean */
		LUA_NUMBER num; /* Lua numbers */
		char *str; /* Lua string. */
	} val; /* value of Lua nil/boolean/number/string. */
} constant_type;

/* simplified version of Proto struct. */
typedef struct jit_proto {
	char *name;
	lua_CFunction jit_func;
	int linedefined;
	int lastlinedefined;
	unsigned char nups;
	unsigned char numparams;
	unsigned char is_vararg;
	unsigned char maxstacksize;
	int sizek;
	constant_type *k;
	int sizep;
	struct jit_proto *p;
	int sizecode;
	unsigned int *code;
	int sizelineinfo;
	int *lineinfo;
	int sizelocvars;
	struct jit_LocVar *locvars;
	int sizeupvalues;
	char **upvalues;
} jit_proto;

/* simplified version of LocVar struct. */
typedef struct jit_LocVar {
	char *varname;
	int startpc;
	int endpc;
} jit_LocVar;

Proto *load_jit_proto(lua_State *L, jit_proto *p);

LUALIB_API int load_compiled_protos(lua_State *L, jit_proto *p);
LUALIB_API int load_compiled_module(lua_State *L, jit_proto *p);

extern jit_proto jit_proto_init;

#ifdef __cplusplus
}
#endif

#endif

