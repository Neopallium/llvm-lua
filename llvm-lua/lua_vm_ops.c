/*
** See Copyright Notice in lua.h
*/

#ifdef __cplusplus
extern "C" {
#endif

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

void vm_OP_LOADK(lua_State *L, TValue *k, int a, int bx) {
  TValue *base = L->base;
  TValue *ra = base + a;
  setobj2s(L, ra, k + bx);
}

void vm_OP_LOADK_N(lua_State *L, int a, lua_Number nb) {
  TValue *base = L->base;
  TValue *ra = base + a;
  setnvalue(ra, nb);
}

void vm_OP_LOADBOOL(lua_State *L, int a, int b, int c) {
  TValue *base = L->base;
  TValue *ra = base + a;
  setbvalue(ra, b);
}

void vm_OP_LOADNIL(lua_State *L, int a, int b) {
  TValue *base = L->base;
  TValue *ra = base + a;
  TValue *rb = base + b;
  do {
    setnilvalue(rb--);
  } while (rb >= ra);
}

void vm_OP_GETUPVAL(lua_State *L, LClosure *cl, int a, int b) {
  TValue *base = L->base;
  TValue *ra = base + a;
  setobj2s(L, ra, cl->upvals[b]->v);
}

void vm_OP_GETGLOBAL(lua_State *L, TValue *k, LClosure *cl, int a, int bx) {
  TValue *base = L->base;
  TValue *ra = base + a;
  TValue *rb = k + bx;
  TValue g;
  sethvalue(L, &g, cl->env);
  lua_assert(ttisstring(rb));
  luaV_gettable(L, &g, rb, ra);
}

void vm_OP_GETTABLE(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  TValue *ra = base + a;
  luaV_gettable(L, base + b, RK(c), ra);
}

void vm_OP_SETGLOBAL(lua_State *L, TValue *k, LClosure *cl, int a, int bx) {
  TValue *base = L->base;
  TValue *ra = base + a;
  TValue g;
  sethvalue(L, &g, cl->env);
  lua_assert(ttisstring(k + bx));
  luaV_settable(L, &g, k + bx, ra);
}

void vm_OP_SETUPVAL(lua_State *L, LClosure *cl, int a, int b) {
  TValue *base = L->base;
  TValue *ra = base + a;
  UpVal *uv = cl->upvals[b];
  setobj(L, uv->v, ra);
  luaC_barrier(L, uv, ra);
}

void vm_OP_SETTABLE(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  TValue *ra = base + a;
  luaV_settable(L, ra, RK(b), RK(c));
}

void vm_OP_NEWTABLE(lua_State *L, int a, int b_fb2int, int c_fb2int) {
  Table *h;
  h = luaH_new(L, b_fb2int, c_fb2int);
  sethvalue(L, L->base + a, h);
  luaC_checkGC(L);
}

void vm_OP_SELF(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  TValue *ra = base + a;
  StkId rb = base + b;
  setobjs2s(L, ra+1, rb);
  luaV_gettable(L, rb, RK(c), ra);
}

void vm_OP_ADD(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  arith_op(luai_numadd, TM_ADD);
}

void vm_OP_ADD_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c) {
  TValue *base = L->base;
  arith_op_nc(luai_numadd, TM_ADD);
}

void vm_OP_SUB(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  arith_op(luai_numsub, TM_SUB);
}

void vm_OP_SUB_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c) {
  TValue *base = L->base;
  arith_op_nc(luai_numsub, TM_SUB);
}

void vm_OP_MUL(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  arith_op(luai_nummul, TM_MUL);
}

void vm_OP_MUL_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c) {
  TValue *base = L->base;
  arith_op_nc(luai_nummul, TM_MUL);
}

void vm_OP_DIV(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  arith_op(luai_numdiv, TM_DIV);
}

void vm_OP_DIV_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c) {
  TValue *base = L->base;
  arith_op_nc(luai_numdiv, TM_DIV);
}

void vm_OP_MOD(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  arith_op(luai_nummod, TM_MOD);
}

void vm_OP_MOD_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c) {
  TValue *base = L->base;
  arith_op_nc(luai_nummod, TM_MOD);
}

void vm_OP_POW(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  arith_op(luai_numpow, TM_POW);
}

void vm_OP_POW_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c) {
  TValue *base = L->base;
  arith_op_nc(luai_numpow, TM_POW);
}

void vm_OP_UNM(lua_State *L, int a, int b) {
  TValue *base = L->base;
  TValue *ra = base + a;
  TValue *rb = base + b;
  if (ttisnumber(rb)) {
    lua_Number nb = nvalue(rb);
    setnvalue(ra, luai_numunm(nb));
  }
  else {
    luaV_arith(L, ra, rb, rb, TM_UNM);
  }
}

void vm_OP_NOT(lua_State *L, int a, int b) {
  TValue *base = L->base;
  TValue *ra = base + a;
  int res = l_isfalse(base + b);  /* next assignment may change this value */
  setbvalue(ra, res);
}

void vm_OP_LEN(lua_State *L, int a, int b) {
  TValue *base = L->base;
  TValue *ra = base + a;
  const TValue *rb = base + b;
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
      ptrdiff_t br = savestack(L, rb);
      if (!luaV_call_binTM(L, rb, luaO_nilobject, ra, TM_LEN))
        luaG_typeerror(L, restorestack(L, br), "get length of");
    }
  }
}

void vm_OP_CONCAT(lua_State *L, int a, int b, int c) {
  TValue *base;
  luaV_concat(L, c-b+1, c); luaC_checkGC(L);
  base = L->base;
  setobjs2s(L, base + a, base + b);
}

void vm_OP_JMP(lua_State *L, int sbx) {
  dojump(sbx);
}

int vm_OP_EQ(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  int ret;
  TValue *rb = RK(b);
  TValue *rc = RK(c);
  ret = (equalobj(L, rb, rc) == a);
  if(ret)
    dojump(GETARG_sBx(*L->savedpc));
  return ret;
}

int vm_OP_EQ_NC(lua_State *L, TValue *k, int b, lua_Number nc) {
  TValue *base = L->base;
  int ret;
  TValue *rb = RK(b);
  if (ttisnumber(rb)) {
    ret = !luai_numeq(nvalue(rb), nc);
    if(ret)
      dojump(GETARG_sBx(*L->savedpc));
    return ret;
  }
  return 1;
}

int vm_OP_NOT_EQ_NC(lua_State *L, TValue *k, int b, lua_Number nc) {
  TValue *base = L->base;
  int ret;
  TValue *rb = RK(b);
  if (ttisnumber(rb)) {
    ret = luai_numeq(nvalue(rb), nc);
    if(ret)
      dojump(GETARG_sBx(*L->savedpc));
    return ret;
  }
  return 0;
}

int vm_OP_LT(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  int ret;
  ret = (luaV_lessthan(L, RK(b), RK(c)) == a);
  if(ret)
    dojump(GETARG_sBx(*L->savedpc));
  return ret;
}

int vm_OP_LE(lua_State *L, TValue *k, int a, int b, int c) {
  TValue *base = L->base;
  int ret;
  ret = (luaV_lessequal(L, RK(b), RK(c)) == a);
  if(ret)
    dojump(GETARG_sBx(*L->savedpc));
  return ret;
}

int vm_OP_TEST(lua_State *L, int a, int c) {
  TValue *ra = L->base + a;
  if (l_isfalse(ra) != c) {
    dojump(GETARG_sBx(*L->savedpc));
    return 1;
  }
  return 0;
}

int vm_OP_TESTSET(lua_State *L, int a, int b, int c) {
  TValue *base = L->base;
  TValue *ra = base + a;
  TValue *rb = base + b;
  if (l_isfalse(rb) != c) {
    setobjs2s(L, ra, rb);
    dojump(GETARG_sBx(*L->savedpc));
    return 1;
  }
  return 0;
}

/*
 * Generic number FORPREP
 *
 * Test if init/plimit/pstep are numbers, fallback to slower FORPREP
 * if one ore more of then are not numbers.
 *
 */
void vm_OP_FORPREP(lua_State *L, int a, int sbx) {
  TValue *ra = L->base + a;
  const TValue *init = ra;
  const TValue *plimit = ra+1;
  const TValue *pstep = ra+2;
  int valid=1;
  valid &= ttisnumber(init);
  valid &= ttisnumber(plimit);
  valid &= ttisnumber(pstep);
  if(!valid) {
    vm_OP_FORPREP_slow(L,a,sbx);
  }
  // subtract pstep from init.
  setnvalue(ra, luai_numsub(nvalue(init), nvalue(pstep)));
  dojump(sbx);
}

void vm_OP_FORPREP_no_sub(lua_State *L, int a, int sbx) {
  TValue *ra = L->base + a;
  const TValue *init = ra;
  const TValue *plimit = ra+1;
  const TValue *pstep = ra+2;
  int valid=1;
  valid &= ttisnumber(init);
  valid &= ttisnumber(plimit);
  valid &= ttisnumber(pstep);
  if(!valid) {
    vm_OP_FORPREP_slow(L,a,sbx);
  }
  dojump(sbx);
}

/*
 * limit & step are number constants.  Only test if init is a number.
 */
void vm_OP_FORPREP_M_N_N(lua_State *L, int a, int sbx, lua_Number limit, lua_Number step) {
  TValue *ra = L->base + a;
  const TValue *init = ra;
  if(!ttisnumber(init)) {
    setnvalue(ra+1, limit);
    setnvalue(ra+2, step);
    vm_OP_FORPREP_slow(L,a,sbx);
  }
  dojump(sbx);
}

/*
 * init & pstep are number constants.  Only test if plimit is a number.
 */
void vm_OP_FORPREP_N_M_N(lua_State *L, int a, int sbx, lua_Number init, lua_Number step) {
  TValue *ra = L->base + a;
  const TValue *plimit = ra+1;
  if(!ttisnumber(plimit)) {
    setnvalue(ra, init);
    setnvalue(ra+2, step);
    vm_OP_FORPREP_slow(L,a,sbx);
  }
  dojump(sbx);
}

/*
 * init, plimit & pstep are number constants.
 */
void vm_OP_FORPREP_N_N_N(lua_State *L, int a, int sbx, lua_Number init, lua_Number step) {
  dojump(sbx);
}

int vm_OP_FORLOOP(lua_State *L, int a, int sbx) {
  TValue *ra = L->base + a;
  lua_Number step = nvalue(ra+2);
  lua_Number idx = luai_numadd(nvalue(ra), step); /* increment index */
  lua_Number limit = nvalue(ra+1);
  if (luai_numlt(0, step) ? luai_numle(idx, limit)
                          : luai_numle(limit, idx)) {
    dojump(sbx);  /* jump back */
    setnvalue(ra, idx);  /* update internal index... */
    setnvalue(ra+3, idx);  /* ...and external index */
    return 1;
  }
  return 0;
}

int vm_OP_FORLOOP_N_N(lua_State *L, int a, int sbx, lua_Number limit, lua_Number step) {
  TValue *ra = L->base + a;
  lua_Number idx = luai_numadd(nvalue(ra), step); /* increment index */
  if (luai_numlt(0, step) ? luai_numle(idx, limit)
                          : luai_numle(limit, idx)) {
    dojump(sbx);  /* jump back */
    setnvalue(ra, idx);  /* update internal index... */
    return 1;
  }
  return 0;
}

int vm_OP_FORLOOP_N_N_N(lua_State *L, int a, int sbx, lua_Number idx, lua_Number limit, lua_Number step) {
  if (luai_numlt(0, step) ? luai_numle(idx, limit)
                          : luai_numle(limit, idx)) {
    dojump(sbx);  /* jump back */
    return 1;
  }
  return 0;
}

int vm_OP_FORLOOP_up(lua_State *L, int a, int sbx, lua_Number idx, lua_Number limit) {
  if (luai_numle(idx, limit)) {
    dojump(sbx);  /* jump back */
    return 1;
  }
  return 0;
}

int vm_OP_FORLOOP_down(lua_State *L, int a, int sbx, lua_Number idx, lua_Number limit) {
  if (luai_numle(limit, idx)) {
    dojump(sbx);  /* jump back */
    return 1;
  }
  return 0;
}

int vm_OP_FORLOOP_long_up(lua_State *L, int a, int sbx, lua_Long idx, lua_Long limit) {
  if (luai_numle(idx, limit)) {
    dojump(sbx);  /* jump back */
    return 1;
  }
  return 0;
}

int vm_OP_FORLOOP_long_down(lua_State *L, int a, int sbx, lua_Long idx, lua_Long limit) {
  if (luai_numle(limit, idx)) {
    dojump(sbx);  /* jump back */
    return 1;
  }
  return 0;
}

void vm_OP_CLOSE(lua_State *L, int a) {
  luaF_close(L, L->base + a);
}

LClosure *vm_get_current_closure(lua_State *L) {
  return &clvalue(L->ci->func)->l;
}

TValue *vm_get_current_constants(LClosure *cl) {
  return cl->p->k;
}

lua_Number vm_get_number(lua_State *L, int idx) {
  return nvalue(L->base + idx);
}

void vm_set_number(lua_State *L, int idx, lua_Number num) {
  setnvalue(L->base + idx, num);  /* write number to Lua-stack */
}

lua_Long vm_get_long(lua_State *L, int idx) {
  return (lua_Long)nvalue(L->base + idx);
}

void vm_set_long(lua_State *L, int idx, lua_Long num) {
  setnvalue(L->base + idx, (lua_Number)num);  /* write number to Lua-stack */
}

#ifdef __cplusplus
}
#endif

