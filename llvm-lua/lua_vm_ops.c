/*
** See Copyright Notice in lua.h
*/

/*
 * lua_vm_ops.c -- Lua ops functions for use by LLVM IR gen.
 *
 * Most of this file was copied from Lua's lvm.c
 */

#include "lua_vm_ops.h"

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lvm.h"
#include <stdio.h>
#include <assert.h>

void vm_OP_MOVE(lua_State *L, int a, int b) {
	TValue *base = L->base;
  TValue *ra = base + a;
  setobjs2s(L, ra, base + b);
}

void vm_OP_LOADK(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  setobj2s(L, ra, KBx(i));
}

void vm_OP_LOADBOOL(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  setbvalue(ra, GETARG_B(i));
#ifndef LUA_NODEBUG
  if (GETARG_C(i)) L->savedpc++;
#endif
}

void vm_OP_LOADNIL(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  TValue *rb = RB(i);
  do {
    setnilvalue(rb--);
  } while (rb >= ra);
}

void vm_OP_GETUPVAL(lua_State *L, LClosure *cl, int a, int b) {
	TValue *base = L->base;
  TValue *ra = base + a;
  setobj2s(L, ra, cl->upvals[b]->v);
}

void vm_OP_GETGLOBAL(lua_State *L, TValue *k, LClosure *cl, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  TValue *rb = KBx(i);
  TValue g;
  sethvalue(L, &g, cl->env);
  lua_assert(ttisstring(rb));
  Protect(luaV_gettable(L, &g, rb, ra));
}

void vm_OP_GETTABLE(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  Protect(luaV_gettable(L, RB(i), RKC(i), ra));
}

void vm_OP_SETGLOBAL(lua_State *L, TValue *k, LClosure *cl, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  TValue g;
  sethvalue(L, &g, cl->env);
  lua_assert(ttisstring(KBx(i)));
  Protect(luaV_settable(L, &g, KBx(i), ra));
}

void vm_OP_SETUPVAL(lua_State *L, LClosure *cl, int a, int b) {
	TValue *base = L->base;
  TValue *ra = base + a;
  UpVal *uv = cl->upvals[b];
  setobj(L, uv->v, ra);
  luaC_barrier(L, uv, ra);
}

void vm_OP_SETTABLE(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  Protect(luaV_settable(L, ra, RKB(i), RKC(i)));
}

void vm_OP_NEWTABLE(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  int b = GETARG_B(i);
  int c = GETARG_C(i);
  sethvalue(L, ra, luaH_new(L, luaO_fb2int(b), luaO_fb2int(c)));
  Protect(luaC_checkGC(L));
}

void vm_OP_SELF(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  StkId rb = RB(i);
  setobjs2s(L, ra+1, rb);
  Protect(luaV_gettable(L, rb, RKC(i), ra));
}

void vm_OP_ADD(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  arith_op(luai_numadd, TM_ADD);
}

void vm_OP_SUB(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  arith_op(luai_numsub, TM_SUB);
}

void vm_OP_MUL(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  arith_op(luai_nummul, TM_MUL);
}

void vm_OP_DIV(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  arith_op(luai_numdiv, TM_DIV);
}

void vm_OP_MOD(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  arith_op(luai_nummod, TM_MOD);
}

void vm_OP_POW(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  arith_op(luai_numpow, TM_POW);
}

void vm_OP_UNM(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  TValue *rb = RB(i);
  if (ttisnumber(rb)) {
    lua_Number nb = nvalue(rb);
    setnvalue(ra, luai_numunm(nb));
  }
  else {
    Protect(luaV_arith(L, ra, rb, rb, TM_UNM));
  }
}

void vm_OP_NOT(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  int res = l_isfalse(RB(i));  /* next assignment may change this value */
  setbvalue(ra, res);
}

void vm_OP_LEN(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  const TValue *rb = RB(i);
  switch (ttype(rb)) {
    case LUA_TTABLE: {
      setnvalue(ra, cast_num(luaH_getn(hvalue(rb))));
      break;
    }
    case LUA_TSTRING: {
      setnvalue(ra, cast_num(tsvalue(rb)->len));
      break;
    }
    default: {  /* try metamethod */
      Protect(
        if (!luaV_call_binTM(L, rb, luaO_nilobject, ra, TM_LEN))
          luaG_typeerror(L, rb, "get length of");
      )
    }
  }
}

void vm_OP_CONCAT(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  int b = GETARG_B(i);
  int c = GETARG_C(i);
  Protect(luaV_concat(L, c-b+1, c); luaC_checkGC(L));
  setobjs2s(L, RA(i), base+b);
}

void vm_OP_JMP(lua_State *L, const Instruction i) {
  dojump(GETARG_sBx(i));
}

int vm_OP_EQ(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  int ret;
  TValue *rb = RKB(i);
  TValue *rc = RKC(i);
  Protect(
    ret = (equalobj(L, rb, rc) == GETARG_A(i));
  )
	if(ret)
  	dojump(GETARG_sBx(*L->savedpc));
  skip_op();
  return ret;
}

int vm_OP_LT(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  int ret;
  Protect(
    ret = (luaV_lessthan(L, RKB(i), RKC(i)) == GETARG_A(i));
  )
	if(ret)
  	dojump(GETARG_sBx(*L->savedpc));
  skip_op();
  return ret;
}

int vm_OP_LE(lua_State *L, TValue *k, const Instruction i) {
	TValue *base = L->base;
  int ret;
  Protect(
    ret = (luaV_lessequal(L, RKB(i), RKC(i)) == GETARG_A(i));
  )
	if(ret)
  	dojump(GETARG_sBx(*L->savedpc));
  skip_op();
  return ret;
}

int vm_OP_TEST(lua_State *L, int a, int c) {
	TValue *base = L->base;
  TValue *ra = base + a;
  if (l_isfalse(ra) != c) {
  	dojump(GETARG_sBx(*L->savedpc));
  	skip_op();
    return 1;
  }
  skip_op();
  return 0;
}

int vm_OP_TESTSET(lua_State *L, int a, int b, int c) {
	TValue *base = L->base;
  TValue *ra = base + a;
  TValue *rb = base + b;
  if (l_isfalse(rb) != c) {
    setobjs2s(L, ra, rb);
  	dojump(GETARG_sBx(*L->savedpc));
  	skip_op();
    return 1;
  }
  skip_op();
  return 0;
}

int vm_OP_FORLOOP(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  lua_Number step = nvalue(ra+2);
  lua_Number idx = luai_numadd(nvalue(ra), step); /* increment index */
  lua_Number limit = nvalue(ra+1);
  if (luai_numlt(0, step) ? luai_numle(idx, limit)
                          : luai_numle(limit, idx)) {
    dojump(GETARG_sBx(i));  /* jump back */
    setnvalue(ra, idx);  /* update internal index... */
    setnvalue(ra+3, idx);  /* ...and external index */
    return 1;
  }
  return 0;
}

void vm_OP_CLOSE(lua_State *L, const Instruction i) {
	TValue *base = L->base;
  TValue *ra = RA(i);
  luaF_close(L, ra);
}

LClosure *vm_get_current_closure(lua_State *L) {
  return &clvalue(L->ci->func)->l;
}

TValue *vm_get_current_constants(LClosure *cl) {
  return cl->p->k;
}

void vm_func_state_init(lua_State *L, func_state *fs) {
}

