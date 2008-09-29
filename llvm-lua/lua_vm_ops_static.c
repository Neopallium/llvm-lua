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

const func_types vm_functions[] = {
	{ VAR_TYPE_VOID, "vm_print_OP", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_next_OP", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_MOVE", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INT, VAR_TYPE_INT, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_LOADK", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_LOADBOOL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_LOADNIL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_GETUPVAL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INT, VAR_TYPE_INT, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_GETGLOBAL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_GETTABLE", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_SETGLOBAL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_SETUPVAL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INT, VAR_TYPE_INT, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_SETTABLE", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_NEWTABLE", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_SELF", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_ADD", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_SUB", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_MUL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_DIV", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_MOD", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_POW", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_UNM", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_NOT", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_LEN", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_CONCAT", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_JMP", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_EQ", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_LT", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_LE", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_TEST", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INT, VAR_TYPE_INT, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_TESTSET", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INT, VAR_TYPE_INT, VAR_TYPE_INT, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_CALL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_RETURN", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_TAILCALL", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_FORLOOP", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_FORPREP", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_INT, "vm_OP_TFORLOOP", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_SETLIST", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_INT, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_CLOSE", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_CLOSURE", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_INT, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_OP_VARARG", {VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_INSTRUCTION, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, "vm_func_state_init", {VAR_TYPE_LUA_STATE_PTR, VAR_TYPE_FUNC_STATE_PTR, VAR_TYPE_VOID} },
	{ VAR_TYPE_VOID, NULL, {VAR_TYPE_VOID} }
};

/*
 * TODO: move this function outside of lua_vm_ops.c
 */
void vm_print_OP(func_state *fs, const Instruction i) {
  int op = GET_OPCODE(i);
#ifndef LUA_NODEBUG
  fprintf(stderr, "%ld: '%s' (%d) = 0x%08X, pc=%p\n", (fs->pc - fs->cl->p->code),
    luaP_opnames[op], op, i, fs->pc);
  lua_assert(fs->pc[0] == i);
#else
  fprintf(stderr, "'%s' (%d) = 0x%08X\n", luaP_opnames[op], op, i);
#endif
}

/*
 * TODO: move this function outside of lua_vm_ops.c
 */
void vm_next_OP(func_state *fs) {
#ifndef LUA_NODEBUG
  //vm_print_OP(fs, fs->pc[0]);
  lua_assert(fs->pc >= fs->cl->p->code && (fs->pc < &(fs->cl->p->code[fs->cl->p->sizecode])));
  fs->pc++;
  if ((fs->L->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT)) &&
      (--fs->L->hookcount == 0 || fs->L->hookmask & LUA_MASKLINE)) {
    luaV_traceexec(fs->L, fs->pc);
    if (fs->L->status == LUA_YIELD) {  /* did hook yield? */
      fs->L->savedpc = fs->pc - 1;
      return;
    }
    fs->base = fs->L->base;
  }
#endif
}

/*
 * TODO: move this function outside of lua_vm_ops.c
 */
int vm_OP_CALL(func_state *fs, const Instruction i) {
  TValue *ra=RA(i);
  int b = GETARG_B(i);
  int nresults = GETARG_C(i) - 1;
  int ret;
  if (b != 0) fs->L->top = ra+b;  /* else previous instruction set top */
#ifndef LUA_NODEBUG
  fs->L->savedpc = fs->pc;
#endif
  ret = luaD_precall(fs->L, ra, nresults);
  switch (ret) {
    case PCRLUA: {
      luaV_execute(fs->L, 1);
      fs->base = fs->L->base;
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

/*
 * TODO: move this function outside of lua_vm_ops.c
 */
int vm_OP_RETURN(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  int b = GETARG_B(i);
  if (b != 0) fs->L->top = ra+b-1;
  if (fs->L->openupval) luaF_close(fs->L, fs->base);
#ifndef LUA_NODEBUG
  fs->L->savedpc = fs->pc;
#endif
  b = luaD_poscall(fs->L, ra);
  return PCRC;
}

/*
 * TODO: move this function outside of lua_vm_ops.c
 */
int vm_OP_TAILCALL(func_state *fs, const Instruction i, const Instruction ret_i) {
  TValue *ra = RA(i);
  int b = GETARG_B(i);
  int ret;
  if (b != 0) fs->L->top = ra+b;  /* else previous instruction set top */
#ifndef LUA_NODEBUG
  fs->L->savedpc = fs->pc;
#endif
  lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
  ret = luaD_precall(fs->L, ra, LUA_MULTRET);
  switch (ret) {
    case PCRLUA: {
      /* tail call: put new frame in place of previous one */
      CallInfo *ci = fs->L->ci - 1;  /* previous frame */
      int aux;
      StkId func = ci->func;
      StkId pfunc = (ci+1)->func;  /* previous function index */
      if (fs->L->openupval) luaF_close(fs->L, ci->base);
      fs->L->base = ci->base = ci->func + ((ci+1)->base - pfunc);
      for (aux = 0; pfunc+aux < fs->L->top; aux++)  /* move frame down */
        setobjs2s(fs->L, func+aux, pfunc+aux);
      ci->top = fs->L->top = func+aux;  /* correct top */
      lua_assert(fs->L->top == fs->L->base + clvalue(func)->l.p->maxstacksize);
#ifndef LUA_NODEBUG
      ci->savedpc = fs->L->savedpc;
#endif
      ci->tailcalls++;  /* one more call lost */
      fs->L->ci--;  /* remove new frame */
      luaV_execute(fs->L, 1);
      fs->base = fs->L->base;
      return PCRC;
    }
    case PCRC: {  /* it was a C function (`precall' called it) */
      fs->base = fs->L->base;
      break;
    }
    default: {
      return PCRYIELD;
    }
  }
  return vm_OP_RETURN(fs, ret_i);
}

/*
 * TODO: move this function outside of lua_vm_ops.c
 *
 * Notes: split function into two copies, one with number checks + (init - step) + jmp,
 * and the other with the same number checks + slow error throwing code.
 */
int vm_OP_FORPREP(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  const TValue *init = ra;
  const TValue *plimit = ra+1;
  const TValue *pstep = ra+2;
#ifndef LUA_NODEBUG
  fs->L->savedpc = fs->pc;
#endif
  if (!tonumber(init, ra))
    luaG_runerror(fs->L, LUA_QL("for") " initial value must be a number");
  else if (!tonumber(plimit, ra+1))
    luaG_runerror(fs->L, LUA_QL("for") " limit must be a number");
  else if (!tonumber(pstep, ra+2))
    luaG_runerror(fs->L, LUA_QL("for") " step must be a number");
  setnvalue(ra, luai_numsub(nvalue(ra), nvalue(pstep)));
  dojump(fs, GETARG_sBx(i));
  return 1;
}

/*
 * TODO: move this function outside of lua_vm_ops.c
 */
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
  	dojump(fs, GETARG_sBx(*fs->pc));
  	skip_op(fs);
    return 1;
  }
 	skip_op(fs);
  return 0;
}

/*
 * TODO: move this function outside of lua_vm_ops.c
 */
void vm_OP_SETLIST(func_state *fs, const Instruction i, int c) {
  TValue *ra = RA(i);
  int b=GETARG_B(i);
  int last;
  Table *h;
#ifndef LUA_NODEBUG
  if(GETARG_C(i) == 0) fs->pc++;
#endif
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

/*
 * TODO: move this function outside of lua_vm_ops.c
 */
void vm_OP_CLOSURE(func_state *fs, const Instruction i, int pseudo_ops_offset) {
  const Instruction *pc;
  TValue *ra = RA(i);
  Proto *p;
  Closure *ncl;
  int nup, j;

  p = fs->cl->p->p[GETARG_Bx(i)];
  pc=fs->cl->p->code + pseudo_ops_offset;
#ifndef LUA_NODEBUG
  lua_assert(fs->pc == pc);
#endif
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
#ifndef LUA_NODEBUG
  fs->pc += nup;
  lua_assert(fs->pc == pc);
#endif
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

