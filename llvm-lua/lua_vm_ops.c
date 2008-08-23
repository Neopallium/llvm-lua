/*
** See Copyright Notice in lua.h
*/

/*
 * lua_vm_ops.c -- Lua ops functions for use by LLVM IR gen.
 *
 * Most of this file was copied from Lua's lvm.c
 */

#include "lua_core.h"
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

typedef struct {
	lua_State *L;
	LClosure *cl;
	StkId base;
	TValue *k;
} func_state;

/*
** some macros for common tasks in `luaV_execute'
*/

#define runtime_check(L, c) { if (!(c)) return; }

#define RA(i) (fs->base+GETARG_A(i))
/* to be used after possible stack reallocation */
#define RB(i) check_exp(getBMode(GET_OPCODE(i)) == OpArgR, fs->base+GETARG_B(i))
#define RC(i) check_exp(getCMode(GET_OPCODE(i)) == OpArgR, fs->base+GETARG_C(i))
#define RKB(i)  check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
  ISK(GETARG_B(i)) ? fs->k+INDEXK(GETARG_B(i)) : fs->base+GETARG_B(i))
#define RKC(i)  check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
  ISK(GETARG_C(i)) ? fs->k+INDEXK(GETARG_C(i)) : fs->base+GETARG_C(i))
#define KBx(i)  check_exp(getBMode(GET_OPCODE(i)) == OpArgK, fs->k+GETARG_Bx(i))


/*
#define dojump(L,pc,i)  {(pc) += (i); luai_threadyield(L);}
*/
#define dojump(L,pc,i)  {luai_threadyield(L);}


#define Protect(x)  { {x;}; fs->base = fs->L->base; }

#define arith_op(op,tm) { \
        TValue *ra = RA(i); \
        TValue *rb = RKB(i); \
        TValue *rc = RKC(i); \
        if (ttisnumber(rb) && ttisnumber(rc)) { \
          lua_Number nb = nvalue(rb), nc = nvalue(rc); \
          setnvalue(ra, op(nb, nc)); \
        } \
        else \
          Protect(luaV_arith(fs->L, ra, rb, rc, tm)); \
      }


void vm_OP_MOVE(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  setobjs2s(fs->L, ra, RB(i));
}

void vm_OP_LOADK(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  setobj2s(fs->L, ra, KBx(i));
}

void vm_OP_LOADBOOL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  setbvalue(ra, GETARG_B(i));
}

void vm_OP_LOADNIL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  TValue *rb = RB(i);
  do {
    setnilvalue(rb--);
  } while (rb >= ra);
}

void vm_OP_GETUPVAL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  int b = GETARG_B(i);
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

void vm_OP_SETUPVAL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  UpVal *uv = fs->cl->upvals[GETARG_B(i)];
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
  dojump(fs->L, pc, GETARG_sBx(i));
}

int vm_OP_EQ(func_state *fs, const Instruction i) {
	int ret;
  TValue *rb = RKB(i);
  TValue *rc = RKC(i);
  Protect(
    ret = (equalobj(fs->L, rb, rc) == GETARG_A(i));
  )
	return ret;
}

int vm_OP_LT(func_state *fs, const Instruction i) {
	int ret;
  Protect(
    ret = (luaV_lessthan(fs->L, RKB(i), RKC(i)) == GETARG_A(i));
  )
	return ret;
}

int vm_OP_LE(func_state *fs, const Instruction i) {
	int ret;
  Protect(
    ret = (luaV_lessequal(fs->L, RKB(i), RKC(i)) == GETARG_A(i));
  )
	return ret;
}

int vm_OP_TEST(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  return (l_isfalse(ra) != GETARG_C(i));
}

int vm_OP_TESTSET(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  TValue *rb = RB(i);
  if (l_isfalse(rb) != GETARG_C(i)) {
    setobjs2s(fs->L, ra, rb);
		return 1;
  }
	return 0;
}

int vm_OP_CALL(func_state *fs, const Instruction i) {
	TValue *ra=RA(i);
	int b = GETARG_B(i);
	int nresults = GETARG_C(i) - 1;
	int ret;
	if (b != 0) fs->L->top = ra+b;  /* else previous instruction set top */
	ret = luaD_precall(fs->L, ra, nresults);
	switch (ret) {
		case PCRLUA: {
			luaV_execute(fs->L, cast_int(fs->L->ci - fs->L->base_ci));
			break;
		}
		case PCRC: {
			/* it was a C function (`precall' called it); adjust results */
			if (nresults >= 0) fs->L->top = fs->L->ci->top;
			fs->base = fs->L->base;
			break;
		}
    default: {
			return PCRYIELD;
    }
	}
	return 0;
}

int vm_OP_TAILCALL(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  int b = GETARG_B(i);
	int ret;
  if (b != 0) fs->L->top = ra+b;  /* else previous instruction set top */
  lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
  ret = luaD_precall(fs->L, ra, LUA_MULTRET);
	switch (ret) {
    case PCRLUA: {
			luaV_execute(fs->L, cast_int(fs->L->ci - fs->L->base_ci));
			break;
    }
    case PCRC: {  /* it was a C function (`precall' called it) */
      fs->base = fs->L->base;
      break;
    }
    default: {
			return PCRYIELD;
    }
  }
	return 0;
}

int vm_OP_RETURN(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  int b = GETARG_B(i);
  if (b != 0) fs->L->top = ra+b-1;
  if (fs->L->openupval) luaF_close(fs->L, fs->base);
  b = luaD_poscall(fs->L, ra);
	return PCRC;
}

int vm_OP_FORLOOP(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  lua_Number step = nvalue(ra+2);
  lua_Number idx = luai_numadd(nvalue(ra), step); /* increment index */
  lua_Number limit = nvalue(ra+1);
  if (luai_numlt(0, step) ? luai_numle(idx, limit)
                          : luai_numle(limit, idx)) {
    dojump(fs->L, pc, GETARG_sBx(i));  /* jump back */
    setnvalue(ra, idx);  /* update internal index... */
    setnvalue(ra+3, idx);  /* ...and external index */
		return 1;
  }
	return 0;
}

int vm_OP_FORPREP(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  const TValue *init = ra;
  const TValue *plimit = ra+1;
  const TValue *pstep = ra+2;
  if (!tonumber(init, ra))
    luaG_runerror(fs->L, LUA_QL("for") " initial value must be a number");
  else if (!tonumber(plimit, ra+1))
    luaG_runerror(fs->L, LUA_QL("for") " limit must be a number");
  else if (!tonumber(pstep, ra+2))
    luaG_runerror(fs->L, LUA_QL("for") " step must be a number");
  setnvalue(ra, luai_numsub(nvalue(ra), nvalue(pstep)));
  dojump(fs->L, pc, GETARG_sBx(i));
	return 1;
}

int vm_OP_TFORLOOP(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  StkId cb = ra + 3;  /* call base */
  setobjs2s(fs->L, cb+2, ra+2);
  setobjs2s(fs->L, cb+1, ra+1);
  setobjs2s(fs->L, cb, ra);
  fs->L->top = cb+3;  /* func. + 2 args (state and index) */
  Protect(luaD_call(fs->L, cb, GETARG_C(i)));
  fs->L->top = fs->L->ci->top;
  cb = RA(i) + 3;  /* previous call may change the stack */
  if (!ttisnil(cb)) {  /* continue loop? */
    setobjs2s(fs->L, cb-1, cb);  /* save control variable */
		return 1;
  }
	return 0;
}

void vm_OP_SETLIST(func_state *fs, int a, int b, int c) {
  TValue *ra = fs->base + a;/*RA(i);*/
  int last;
  Table *h;
  if (b == 0) {
    b = cast_int(fs->L->top - ra) - 1;
    fs->L->top = fs->L->ci->top;
  }
  runtime_check(fs->L, ttistable(ra));
  h = hvalue(ra);
  last = ((c-1)*LFIELDS_PER_FLUSH) + b;
  if (last > h->sizearray)  /* needs more space? */
    luaH_resizearray(fs->L, h, last);  /* pre-alloc it at once */
  for (; b > 0; b--) {
    TValue *val = ra+b;
    setobj2t(fs->L, luaH_setnum(fs->L, h, last--), val);
    luaC_barriert(fs->L, h, val);
  }
}

void vm_OP_CLOSE(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  luaF_close(fs->L, ra);
}

void vm_OP_CLOSURE(func_state *fs, const Instruction i, int pseudo_ops_offset) {
	const Instruction *pc;
  TValue *ra = RA(i);
  Proto *p;
  Closure *ncl;
  int nup, j;

  p = fs->cl->p->p[GETARG_Bx(i)];
	pc=fs->cl->p->code + pseudo_ops_offset;
  nup = p->nups;
  ncl = luaF_newLclosure(fs->L, nup, fs->cl->env);
  setclvalue(fs->L, ra, ncl);
  ncl->l.p = p;
  for (j=0; j<nup; j++, pc++) {
    if (GET_OPCODE(*pc) == OP_GETUPVAL)
      ncl->l.upvals[j] = fs->cl->upvals[GETARG_B(*pc)];
    else {
      lua_assert(GET_OPCODE(*pc) == OP_MOVE);
      ncl->l.upvals[j] = luaF_findupval(fs->L, fs->base + GETARG_B(*pc));
    }
  }
  Protect(luaC_checkGC(fs->L));
}

void vm_OP_VARARG(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  int b = GETARG_B(i) - 1;
  int j;
  CallInfo *ci = fs->L->ci;
  int n = cast_int(ci->base - ci->func) - fs->cl->p->numparams - 1;
  if (b == LUA_MULTRET) {
    Protect(luaD_checkstack(fs->L, n));
    ra = RA(i);  /* previous call may change the stack */
    b = n;
    fs->L->top = ra + n;
  }
  for (j = 0; j < b; j++) {
    if (j < n) {
      setobjs2s(fs->L, ra + j, ci->base - n + j);
    }
    else {
      setnilvalue(ra + j);
    }
  }
}

void vm_print_OP(func_state *fs, const Instruction i) {
	int op = GET_OPCODE(i);
	fprintf(stderr, "'%s' (%d) = 0x%08X\n", luaP_opnames[op], op, i);
}

void vm_func_state_init(lua_State *L, func_state *fs) {
	fs->L = L;
	fs->cl = &clvalue(L->ci->func)->l;
	fs->base = L->base;
	fs->k = fs->cl->p->k;
}

