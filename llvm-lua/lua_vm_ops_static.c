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
#include <string.h>
#include <assert.h>

#include "llvm_compiler.h"

const vm_func_info vm_op_functions[] = {
  { OP_MOVE, HINT_NONE, VAR_T_VOID, "vm_OP_MOVE",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_LOADK, HINT_NONE, VAR_T_VOID, "vm_OP_LOADK",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_Bx, VAR_T_VOID},
  },
  { OP_LOADK, HINT_Bx_NUM_CONSTANT, VAR_T_VOID, "vm_OP_LOADK_N",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_Bx_NUM_CONSTANT, VAR_T_VOID},
  },
  { OP_LOADBOOL, HINT_NONE, VAR_T_VOID, "vm_OP_LOADBOOL",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_LOADNIL, HINT_NONE, VAR_T_VOID, "vm_OP_LOADNIL",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_GETUPVAL, HINT_NONE, VAR_T_VOID, "vm_OP_GETUPVAL",
    {VAR_T_LUA_STATE_PTR, VAR_T_CL, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_GETGLOBAL, HINT_NONE, VAR_T_VOID, "vm_OP_GETGLOBAL",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_CL, VAR_T_ARG_A, VAR_T_ARG_Bx, VAR_T_VOID},
  },
  { OP_GETTABLE, HINT_NONE, VAR_T_VOID, "vm_OP_GETTABLE",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_SETGLOBAL, HINT_NONE, VAR_T_VOID, "vm_OP_SETGLOBAL",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_CL, VAR_T_ARG_A, VAR_T_ARG_Bx, VAR_T_VOID},
  },
  { OP_SETUPVAL, HINT_NONE, VAR_T_VOID, "vm_OP_SETUPVAL",
    {VAR_T_LUA_STATE_PTR, VAR_T_CL, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_SETTABLE, HINT_NONE, VAR_T_VOID, "vm_OP_SETTABLE",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_NEWTABLE, HINT_NONE, VAR_T_VOID, "vm_OP_NEWTABLE",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B_FB2INT, VAR_T_ARG_C_FB2INT, VAR_T_VOID},
  },
  { OP_SELF, HINT_NONE, VAR_T_VOID, "vm_OP_SELF",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_ADD, HINT_NONE, VAR_T_VOID, "vm_OP_ADD",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_ADD, HINT_C_NUM_CONSTANT, VAR_T_VOID, "vm_OP_ADD_NC",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C_NUM_CONSTANT, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_SUB, HINT_NONE, VAR_T_VOID, "vm_OP_SUB",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_SUB, HINT_C_NUM_CONSTANT, VAR_T_VOID, "vm_OP_SUB_NC",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C_NUM_CONSTANT, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_MUL, HINT_NONE, VAR_T_VOID, "vm_OP_MUL",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_MUL, HINT_C_NUM_CONSTANT, VAR_T_VOID, "vm_OP_MUL_NC",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C_NUM_CONSTANT, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_DIV, HINT_NONE, VAR_T_VOID, "vm_OP_DIV",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_DIV, HINT_C_NUM_CONSTANT, VAR_T_VOID, "vm_OP_DIV_NC",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C_NUM_CONSTANT, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_MOD, HINT_NONE, VAR_T_VOID, "vm_OP_MOD",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_MOD, HINT_C_NUM_CONSTANT, VAR_T_VOID, "vm_OP_MOD_NC",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C_NUM_CONSTANT, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_POW, HINT_NONE, VAR_T_VOID, "vm_OP_POW",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_POW, HINT_C_NUM_CONSTANT, VAR_T_VOID, "vm_OP_POW_NC",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C_NUM_CONSTANT, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_UNM, HINT_NONE, VAR_T_VOID, "vm_OP_UNM",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_NOT, HINT_NONE, VAR_T_VOID, "vm_OP_NOT",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_LEN, HINT_NONE, VAR_T_VOID, "vm_OP_LEN",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_CONCAT, HINT_NONE, VAR_T_VOID, "vm_OP_CONCAT",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_JMP, HINT_NONE, VAR_T_VOID, "vm_OP_JMP",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_sBx, VAR_T_VOID},
  },
  { OP_EQ, HINT_NONE, VAR_T_INT, "vm_OP_EQ",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_EQ, HINT_C_NUM_CONSTANT, VAR_T_VOID, "vm_OP_EQ_NC",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_B, VAR_T_ARG_C_NUM_CONSTANT, VAR_T_VOID},
  },
  { OP_EQ, HINT_C_NUM_CONSTANT|HINT_NOT, VAR_T_VOID, "vm_OP_NOT_EQ_NC",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_B, VAR_T_ARG_C_NUM_CONSTANT, VAR_T_VOID},
  },
  { OP_LT, HINT_NONE, VAR_T_INT, "vm_OP_LT",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_LE, HINT_NONE, VAR_T_INT, "vm_OP_LE",
    {VAR_T_LUA_STATE_PTR, VAR_T_K, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_TEST, HINT_NONE, VAR_T_INT, "vm_OP_TEST",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_TESTSET, HINT_NONE, VAR_T_INT, "vm_OP_TESTSET",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_CALL, HINT_NONE, VAR_T_INT, "vm_OP_CALL",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_TAILCALL, HINT_NONE, VAR_T_INT, "vm_OP_TAILCALL",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_RETURN, HINT_NONE, VAR_T_INT, "vm_OP_RETURN",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { OP_FORLOOP, HINT_NONE, VAR_T_INT, "vm_OP_FORLOOP",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_sBx, VAR_T_VOID},
  },
  { OP_FORLOOP, HINT_FOR_N_N, VAR_T_INT, "vm_OP_FORLOOP_N_N",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_sBx, VAR_T_OP_VALUE_1, VAR_T_OP_VALUE_2, VAR_T_VOID},
  },
  { OP_FORLOOP, HINT_FOR_N_N_N, VAR_T_INT, "vm_OP_FORLOOP_N_N_N",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_sBx, VAR_T_OP_VALUE_0, VAR_T_OP_VALUE_1, VAR_T_OP_VALUE_2, VAR_T_VOID},
  },
  { OP_FORPREP, HINT_NONE, VAR_T_VOID, "vm_OP_FORPREP",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_sBx, VAR_T_VOID},
  },
  { OP_FORPREP, HINT_FOR_M_N_N, VAR_T_VOID, "vm_OP_FORPREP_M_N_N",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_sBx, VAR_T_OP_VALUE_1, VAR_T_OP_VALUE_2, VAR_T_VOID},
  },
  { OP_FORPREP, HINT_FOR_N_M_N, VAR_T_VOID, "vm_OP_FORPREP_N_M_N",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_sBx, VAR_T_OP_VALUE_0, VAR_T_OP_VALUE_2, VAR_T_VOID},
  },
  { OP_FORPREP, HINT_FOR_N_N_N, VAR_T_VOID, "vm_OP_FORPREP_N_N_N",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_sBx, VAR_T_OP_VALUE_0, VAR_T_OP_VALUE_2, VAR_T_VOID},
  },
  { OP_TFORLOOP, HINT_NONE, VAR_T_INT, "vm_OP_TFORLOOP",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_C, VAR_T_VOID},
  },
  { OP_SETLIST, HINT_NONE, VAR_T_VOID, "vm_OP_SETLIST",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_ARG_C_NEXT_INSTRUCTION, VAR_T_VOID},
  },
  { OP_CLOSE, HINT_NONE, VAR_T_VOID, "vm_OP_CLOSE",
    {VAR_T_LUA_STATE_PTR, VAR_T_ARG_A, VAR_T_VOID},
  },
  { OP_CLOSURE, HINT_NONE, VAR_T_VOID, "vm_OP_CLOSURE",
    {VAR_T_LUA_STATE_PTR, VAR_T_CL, VAR_T_ARG_A, VAR_T_ARG_Bx, VAR_T_PC_OFFSET, VAR_T_VOID},
  },
  { OP_VARARG, HINT_NONE, VAR_T_VOID, "vm_OP_VARARG",
    {VAR_T_LUA_STATE_PTR, VAR_T_CL, VAR_T_ARG_A, VAR_T_ARG_B, VAR_T_VOID},
  },
  { -1, HINT_NONE, VAR_T_VOID, NULL, {VAR_T_VOID} }
};

int vm_op_run_count[NUM_OPCODES];

void vm_count_OP(const Instruction i) {
  vm_op_run_count[GET_OPCODE(i)]++;
}

void vm_print_OP(lua_State *L, LClosure *cl, const Instruction i, int pc_offset) {
  int op = GET_OPCODE(i);
  int line = -1;
  if(cl->p->sizelineinfo > pc_offset) {
    line = cl->p->lineinfo[pc_offset];
  }
  if(pc_offset <= cl->p->sizecode) {
    L->savedpc = cl->p->code + pc_offset;
  } else {
    L->savedpc = cl->p->code + cl->p->sizecode;
  }
  fprintf(stderr, "%d: '%s' (%d) = 0x%08X, pc=%p, line=%d\n", pc_offset,
    luaP_opnames[op], op, i, L->savedpc, line);
  lua_assert(pc_offset == (L->savedpc - cl->p->code));
  lua_assert(L->savedpc[0] == i);
}

void vm_next_OP(lua_State *L, LClosure *cl, int pc_offset) {
  if(pc_offset <= cl->p->sizecode) {
    L->savedpc = cl->p->code + pc_offset;
  } else {
    L->savedpc = cl->p->code;
  }
  //vm_print_OP(L, cl, L->savedpc[0], pc_offset);
  lua_assert(L->savedpc >= cl->p->code && (L->savedpc < &(cl->p->code[cl->p->sizecode])));
  if ((L->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT)) &&
      (--L->hookcount == 0 || L->hookmask & LUA_MASKLINE)) {
    luaV_traceexec(L, L->savedpc + 1);
    if(pc_offset <= cl->p->sizecode) {
      L->savedpc = cl->p->code + pc_offset;
    } else {
      L->savedpc = cl->p->code;
    }
#if 0
    if (L->status == LUA_YIELD) {  /* did hook yield? */
      // TODO: fix hook yield
      L->savedpc = L->savedpc - 1;
      return;
    }
#endif
  }
  L->savedpc++;
}

int vm_OP_CALL(lua_State *L, int a, int b, int c) {
  TValue *base = L->base;
  TValue *ra=base + a;
  int nresults = c - 1;
  int ret;
  if (b != 0) L->top = ra+b;  /* else previous instruction set top */
  ret = luaD_precall(L, ra, nresults);
  switch (ret) {
    case PCRLUA: {
      luaV_execute(L, 1);
      if (nresults >= 0) L->top = L->ci->top;
      break;
    }
    case PCRC: {
      /* it was a C function (`precall' called it); adjust results */
      if (nresults >= 0) L->top = L->ci->top;
      break;
    }
    default: {
      /* TODO: fix yielding from C funtions, right now we can't resume a JIT with using COCO. */
      return PCRYIELD;
    }
  }
  return 0;
}

int vm_OP_RETURN(lua_State *L, int a, int b) {
  TValue *base = L->base;
  TValue *ra = base + a;
  if (b != 0) L->top = ra+b-1;
  if (L->openupval) luaF_close(L, base);
  b = luaD_poscall(L, ra);
  return PCRC;
}

int vm_OP_TAILCALL(lua_State *L, int a, int b) {
  TValue *func = L->base + a;
  Closure *cl;
  CallInfo *ci;
  StkId st, cur_func;
  Proto *p;
  int aux;
  int tail_recur;

  if (b != 0) L->top = func+b;  /* else previous instruction set top */

  /* current function index */
  ci = L->ci;
  cur_func = ci->func;
  /* check for tail recursive call */
  if(gcvalue(func) == gcvalue(cur_func)) {
    cl = clvalue(func);
    p = cl->l.p;
    /* if is not a vararg function. */
    tail_recur = !p->is_vararg;
    L->savedpc = p->code;
  } else {
    tail_recur=0;
    if (!ttisfunction(func)) /* `func' is not a function? */
      func = luaD_tryfuncTM(L, func);  /* check the `function' tag method */
    cl = clvalue(func);
  }

  /* clean up current frame to prepare to tailcall into next function. */
  if (L->openupval) luaF_close(L, ci->base);
  for (aux = 0; func+aux < L->top; aux++)  /* move frame down */
    setobjs2s(L, cur_func+aux, func+aux);
  L->top = cur_func+aux;
  ci->tailcalls++;  /* one more call lost */
  /* JIT function calling it's self. */
  if(tail_recur) {
    for (st = L->top; st < ci->top; st++)
      setnilvalue(st);
    return PCRTAILRECUR;
  }
  L->ci--;  /* remove new frame */
  L->savedpc = L->ci->savedpc;
  /* unwind stack back to luaD_precall */
  return PCRTAILCALL;
}

/*
 * Notes: split function into two copies, one with number checks + (init - step) + jmp,
 * and the other with the same number checks + slow error throwing code.
 */
void vm_OP_FORPREP_slow(lua_State *L, int a, int sbx) {
  TValue *base = L->base;
  TValue *ra = base + a;
  const TValue *init = ra;
  const TValue *plimit = ra+1;
  const TValue *pstep = ra+2;
  if (!tonumber(init, ra))
    luaG_runerror(L, LUA_QL("for") " initial value must be a number");
  else if (!tonumber(plimit, ra+1))
    luaG_runerror(L, LUA_QL("for") " limit must be a number");
  else if (!tonumber(pstep, ra+2))
    luaG_runerror(L, LUA_QL("for") " step must be a number");
  setnvalue(ra, luai_numsub(nvalue(ra), nvalue(pstep)));
  dojump(sbx);
}

int vm_OP_TFORLOOP(lua_State *L, int a, int c) {
  TValue *base = L->base;
  TValue *ra = base + a;
  StkId cb = ra + 3;  /* call base */
  setobjs2s(L, cb+2, ra+2);
  setobjs2s(L, cb+1, ra+1);
  setobjs2s(L, cb, ra);
  L->top = cb+3;  /* func. + 2 args (state and index) */
  Protect(luaD_call(L, cb, c));
  L->top = L->ci->top;
  cb = base + a + 3;  /* previous call may change the stack */
  if (!ttisnil(cb)) {  /* continue loop? */
    setobjs2s(L, cb-1, cb);  /* save control variable */
    dojump(GETARG_sBx(*L->savedpc));
    return 1;
  }
  return 0;
}

void vm_OP_SETLIST(lua_State *L, int a, int b, int c) {
  TValue *base = L->base;
  TValue *ra = base + a;
  int last;
  Table *h;
  if (b == 0) {
    b = cast_int(L->top - ra) - 1;
    L->top = L->ci->top;
  }
  runtime_check(L, ttistable(ra));
  h = hvalue(ra);
  last = ((c-1)*LFIELDS_PER_FLUSH) + b;
  if (last > h->sizearray)  /* needs more space? */
    luaH_resizearray(L, h, last);  /* pre-alloc it at once */
  for (; b > 0; b--) {
    TValue *val = ra+b;
    setobj2t(L, luaH_setnum(L, h, last--), val);
    luaC_barriert(L, h, val);
  }
}

void vm_OP_CLOSURE(lua_State *L, LClosure *cl, int a, int bx, int pseudo_ops_offset) {
  TValue *base = L->base;
  const Instruction *pc;
  TValue *ra = base + a;
  Proto *p;
  Closure *ncl;
  int nup, j;

  p = cl->p->p[bx];
  pc=cl->p->code + pseudo_ops_offset;
  nup = p->nups;
  ncl = luaF_newLclosure(L, nup, cl->env);
  setclvalue(L, ra, ncl);
  ncl->l.p = p;
  for (j=0; j<nup; j++, pc++) {
    if (GET_OPCODE(*pc) == OP_GETUPVAL)
      ncl->l.upvals[j] = cl->upvals[GETARG_B(*pc)];
    else {
      lua_assert(GET_OPCODE(*pc) == OP_MOVE);
      ncl->l.upvals[j] = luaF_findupval(L, base + GETARG_B(*pc));
    }
  }
  luaC_checkGC(L);
}

void vm_OP_VARARG(lua_State *L, LClosure *cl, int a, int b) {
  TValue *base = L->base;
  TValue *ra = base + a;
  int j;
  CallInfo *ci = L->ci;
  int n = cast_int(ci->base - ci->func) - cl->p->numparams - 1;
  b -= 1;
  if (b == LUA_MULTRET) {
    Protect(luaD_checkstack(L, n));
    ra = base + a;  /* previous call may change the stack */
    b = n;
    L->top = ra + n;
  }
  for (j = 0; j < b; j++) {
    if (j < n) {
      setobjs2s(L, ra + j, ci->base - n + j);
    }
    else {
      setnilvalue(ra + j);
    }
  }
}

int is_mini_vm_op(int opcode) {
  switch (opcode) {
    case OP_MOVE:
    case OP_LOADK:
    case OP_GETUPVAL:
    case OP_SETUPVAL:
    case OP_SETTABLE:
      return 1;
    default:
      return 0;
  }
  return 0;
}

void vm_mini_vm(lua_State *L, LClosure *cl, int count, int pseudo_ops_offset) {
  const Instruction *pc;
  StkId base;
  TValue *k;

  k = cl->p->k;
  pc = cl->p->code + pseudo_ops_offset;
  base = L->base;
  /* process next 'count' ops */
  for (; count > 0; count--) {
    const Instruction i = *pc++;
    StkId ra = RA(i);
    lua_assert(base == L->base && L->base == L->ci->base);
    lua_assert(base <= L->top && L->top <= L->stack + L->stacksize);
    lua_assert(L->top == L->ci->top || luaG_checkopenop(i));
    switch (GET_OPCODE(i)) {
      case OP_MOVE: {
        setobjs2s(L, ra, RB(i));
        continue;
      }
      case OP_LOADK: {
        setobj2s(L, ra, KBx(i));
        continue;
      }
      case OP_GETUPVAL: {
        int b = GETARG_B(i);
        setobj2s(L, ra, cl->upvals[b]->v);
        continue;
      }
      case OP_SETUPVAL: {
        UpVal *uv = cl->upvals[GETARG_B(i)];
        setobj(L, uv->v, ra);
        luaC_barrier(L, uv, ra);
        continue;
      }
      case OP_SETTABLE: {
        Protect(luaV_settable(L, ra, RKB(i), RKC(i)));
        continue;
      }
      default: {
        luaG_runerror(L, "Bad opcode: opcode=%d", GET_OPCODE(i));
        continue;
      }
    }
  }
}

