/*

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

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "lua_core.h"
#include "lobject.h"
#include "lfunc.h"
#include "ldo.h"
#include "lstring.h"
#include "lmem.h"
#include "load_jit_proto.h"
#include "lundump.h"

#define DUMP_PROTOS 0
#if DUMP_PROTOS
#include <stdio.h>

#define luaU_print luaU_dump_proto
#include "print.c"
#endif

Proto *load_jit_proto(lua_State *L, jit_proto *p) {
	Proto *f = luaF_newproto(L);
	unsigned int i;

	/* proto source */
	f->source = luaS_new(L, p->name);
	/* jit_func */
	f->jit_func = p->jit_func;
	/* linedefined */
	f->linedefined = p->linedefined;
	/* lastlinedefined */
	f->lastlinedefined = p->lastlinedefined;
	/* nups */
	f->nups = p->nups;
	/* numparams */
	f->numparams = p->numparams;
	/* is_vararg */
	f->is_vararg = p->is_vararg;
	/* maxstacksize */
	f->maxstacksize = p->maxstacksize;
	/* sizek */
	f->sizek = p->sizek;
	/* k */
	f->k=luaM_newvector(L,p->sizek,TValue);
	for(i = 0; i < p->sizek; i++) {
		TValue *o=&f->k[i];
		switch(p->k[i].type) {
			case TYPE_STRING:
				setsvalue2n(L,o, luaS_newlstr(L, p->k[i].val.str, p->k[i].length));
				break;
			case TYPE_BOOLEAN:
				setbvalue(o, p->k[i].val.b != 0);
				break;
			case TYPE_NUMBER:
				setnvalue(o, p->k[i].val.num);
				break;
			case TYPE_NIL:
			default:
				setnilvalue(o);
				break;
		}
	}
	/* sizep */
	f->sizep = p->sizep;
	/* p */
	f->p=luaM_newvector(L,(size_t)p->sizep,Proto*);
	for(i = 0; i < p->sizep; i++) {
		f->p[i] = load_jit_proto(L, &(p->p[i]));
	}
	/* sizecode */
	f->sizecode = p->sizecode;
	/* code */
	f->code=luaM_newvector(L,(size_t)p->sizecode,Instruction);
	for(i = 0; i < p->sizecode; i++) {
		f->code[i] = p->code[i];
	}
	/* sizelineinfo */
	f->sizelineinfo = p->sizelineinfo;
	/* lineinfo */
	f->lineinfo=luaM_newvector(L,(size_t)p->sizelineinfo,int);
	for(i = 0; i < p->sizelineinfo; i++) {
		f->lineinfo[i] = p->lineinfo[i];
	}
	/* sizelocvars */
	f->sizelocvars = p->sizelocvars;
	/* locvars */
	f->locvars=luaM_newvector(L,p->sizelocvars,LocVar);
	for(i = 0; i < p->sizelocvars; i++) {
		jit_LocVar *locvar = &(p->locvars[i]);
		f->locvars[i].varname = luaS_new(L, locvar->varname);
		f->locvars[i].startpc = locvar->startpc;
		f->locvars[i].endpc = locvar->endpc;
	}
	/* sizeupvalues */
	f->sizeupvalues = p->sizeupvalues;
	/* upvalues */
	f->upvalues=luaM_newvector(L,(size_t)p->sizeupvalues,TString*);
	for(i = 0; i < p->sizeupvalues; i++) {
		f->upvalues[i] = luaS_new(L, p->upvalues[i]);
	}
	return f;
}

LUALIB_API int load_compiled_protos(lua_State *L, jit_proto *p) {
  Closure *cl;
  Proto *tf;
  int i;

	// load compiled lua code.
  luaC_checkGC(L);
  set_block_gc(L);  /* stop collector during jit function loading. */
  tf = load_jit_proto(L, p);
#if DUMP_PROTOS
	luaU_dump_proto(tf,2);
#endif
  cl = luaF_newLclosure(L, tf->nups, hvalue(gt(L)));
  cl->l.p = tf;
  for (i = 0; i < tf->nups; i++)  /* initialize eventual upvalues */
    cl->l.upvals[i] = luaF_newupval(L);
  setclvalue(L, L->top, cl);
  incr_top(L);
  unset_block_gc(L);

	return 0;
}


LUALIB_API int load_compiled_module(lua_State *L, jit_proto *p) {
	// load compiled lua code.
	load_compiled_protos(L, p);
	// run compiled lua code.
	lua_insert(L, -2); // pass our first parameter to the lua module.
	lua_call(L, 1, 1);

	return 1;
}

#ifdef __cplusplus
}
#endif

