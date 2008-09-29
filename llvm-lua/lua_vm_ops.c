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

void vm_OP_MOVE(func_state *fs, int a, int b) {
  TValue *ra = fs->base + a;
  setobjs2s(fs->L, ra, fs->base + b);
}

void vm_OP_LOADK(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  setobj2s(fs->L, ra, KBx(i));
}

void vm_OP_LOADBOOL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  setbvalue(ra, GETARG_B(i));
#ifndef LUA_NODEBUG
  if (GETARG_C(i)) fs->pc++;
#endif
}

void vm_OP_LOADNIL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  TValue *rb = RB(i);
  do {
    setnilvalue(rb--);
  } while (rb >= ra);
}

void vm_OP_GETUPVAL(func_state *fs, int a, int b) {
  TValue *ra = fs->base + a;
  setobj2s(fs->L, ra, fs->cl->upvals[b]->v);
}

void vm_OP_GETGLOBAL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  TValue *rb = KBx(i);
  TValue g;
  sethvalue(fs->L, &g, fs->cl->env);
  lua_assert(ttisstring(rb));
  Protect(luaV_gettable(fs->L, &g, rb, ra));
}

void vm_OP_GETTABLE(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  Protect(luaV_gettable(fs->L, RB(i), RKC(i), ra));
}

void vm_OP_SETGLOBAL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  TValue g;
  sethvalue(fs->L, &g, fs->cl->env);
  lua_assert(ttisstring(KBx(i)));
  Protect(luaV_settable(fs->L, &g, KBx(i), ra));
}

void vm_OP_SETUPVAL(func_state *fs, int a, int b) {
  TValue *ra = fs->base + a;
  UpVal *uv = fs->cl->upvals[b];
  setobj(fs->L, uv->v, ra);
  luaC_barrier(fs->L, uv, ra);
}

void vm_OP_SETTABLE(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  Protect(luaV_settable(fs->L, ra, RKB(i), RKC(i)));
}

void vm_OP_NEWTABLE(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  int b = GETARG_B(i);
  int c = GETARG_C(i);
  sethvalue(fs->L, ra, luaH_new(fs->L, luaO_fb2int(b), luaO_fb2int(c)));
  Protect(luaC_checkGC(fs->L));
}

void vm_OP_SELF(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  StkId rb = RB(i);
  setobjs2s(fs->L, ra+1, rb);
  Protect(luaV_gettable(fs->L, rb, RKC(i), ra));
}

void vm_OP_ADD(func_state *fs, const Instruction i) {
  arith_op(luai_numadd, TM_ADD);
}

void vm_OP_SUB(func_state *fs, const Instruction i) {
  arith_op(luai_numsub, TM_SUB);
}

void vm_OP_MUL(func_state *fs, const Instruction i) {
  arith_op(luai_nummul, TM_MUL);
}

void vm_OP_DIV(func_state *fs, const Instruction i) {
  arith_op(luai_numdiv, TM_DIV);
}

void vm_OP_MOD(func_state *fs, const Instruction i) {
  arith_op(luai_nummod, TM_MOD);
}

void vm_OP_POW(func_state *fs, const Instruction i) {
  arith_op(luai_numpow, TM_POW);
}

void vm_OP_UNM(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  TValue *rb = RB(i);
  if (ttisnumber(rb)) {
    lua_Number nb = nvalue(rb);
    setnvalue(ra, luai_numunm(nb));
  }
  else {
    Protect(luaV_arith(fs->L, ra, rb, rb, TM_UNM));
  }
}

void vm_OP_NOT(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  int res = l_isfalse(RB(i));  /* next assignment may change this value */
  setbvalue(ra, res);
}

void vm_OP_LEN(func_state *fs, const Instruction i) {
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
        if (!luaV_call_binTM(fs->L, rb, luaO_nilobject, ra, TM_LEN))
          luaG_typeerror(fs->L, rb, "get length of");
      )
    }
  }
}

void vm_OP_CONCAT(func_state *fs, const Instruction i) {
  int b = GETARG_B(i);
  int c = GETARG_C(i);
  Protect(luaV_concat(fs->L, c-b+1, c); luaC_checkGC(fs->L));
  setobjs2s(fs->L, RA(i), fs->base+b);
}

void vm_OP_JMP(func_state *fs, const Instruction i) {
  dojump(fs, GETARG_sBx(i));
}

int vm_OP_EQ(func_state *fs, const Instruction i) {
  int ret;
  TValue *rb = RKB(i);
  TValue *rc = RKC(i);
  Protect(
    ret = (equalobj(fs->L, rb, rc) == GETARG_A(i));
  )
	if(ret)
  	dojump(fs, GETARG_sBx(*fs->pc));
  skip_op(fs);
  return ret;
}

int vm_OP_LT(func_state *fs, const Instruction i) {
  int ret;
  Protect(
    ret = (luaV_lessthan(fs->L, RKB(i), RKC(i)) == GETARG_A(i));
  )
	if(ret)
  	dojump(fs, GETARG_sBx(*fs->pc));
  skip_op(fs);
  return ret;
}

int vm_OP_LE(func_state *fs, const Instruction i) {
  int ret;
  Protect(
    ret = (luaV_lessequal(fs->L, RKB(i), RKC(i)) == GETARG_A(i));
  )
	if(ret)
  	dojump(fs, GETARG_sBx(*fs->pc));
  skip_op(fs);
  return ret;
}

int vm_OP_TEST(func_state *fs, int a, int c) {
  TValue *ra = fs->base + a;
  if (l_isfalse(ra) != c) {
  	dojump(fs, GETARG_sBx(*fs->pc));
  	skip_op(fs);
    return 1;
  }
  skip_op(fs);
  return 0;
}

int vm_OP_TESTSET(func_state *fs, int a, int b, int c) {
  TValue *ra = fs->base + a;
  TValue *rb = fs->base + b;
  if (l_isfalse(rb) != c) {
    setobjs2s(fs->L, ra, rb);
  	dojump(fs, GETARG_sBx(*fs->pc));
  	skip_op(fs);
    return 1;
  }
  skip_op(fs);
  return 0;
}

int vm_OP_FORLOOP(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  lua_Number step = nvalue(ra+2);
  lua_Number idx = luai_numadd(nvalue(ra), step); /* increment index */
  lua_Number limit = nvalue(ra+1);
  if (luai_numlt(0, step) ? luai_numle(idx, limit)
                          : luai_numle(limit, idx)) {
    dojump(fs, GETARG_sBx(i));  /* jump back */
    setnvalue(ra, idx);  /* update internal index... */
    setnvalue(ra+3, idx);  /* ...and external index */
    return 1;
  }
  return 0;
}

void vm_OP_CLOSE(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  luaF_close(fs->L, ra);
}

void vm_func_state_init(lua_State *L, func_state *fs) {
  fs->L = L;
  fs->cl = &clvalue(L->ci->func)->l;
  fs->base = L->base;
  fs->k = fs->cl->p->k;
#ifndef LUA_NODEBUG
  fs->pc = L->savedpc;
#endif
}

