/*
** See Copyright Notice in lua.h
*/

/*
* hook_parser.c - Add a hook to the parser in ldo.c
*
* Most of this code is from ldo.c
*/

#if !ENABLE_PARSER_HOOK

#include "ldo.c"

int llvm_precall_lua (lua_State *L, StkId func, int nresults) {
  return luaD_precall_lua(L, func, nresults);
}

#else

#include "llvm_compiler.h"

#define luaD_protectedparser luaD_protectedparser_old
#include "ldo.c"
#undef luaD_protectedparser

static void llvm_f_parser (lua_State *L, void *ud) {
  int i;
  Proto *tf;
  Closure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = luaZ_lookahead(p->z);
  luaC_checkGC(L);
  set_block_gc(L);  /* stop collector during parsing */
  tf = ((c == LUA_SIGNATURE[0]) ? luaU_undump : luaY_parser)(L, p->z,
                                                             &p->buff, p->name);
  llvm_compiler_compile_all(L, tf);
  cl = luaF_newLclosure(L, tf->nups, hvalue(gt(L)));
  cl->l.p = tf;
  for (i = 0; i < tf->nups; i++)  /* initialize eventual upvalues */
    cl->l.upvals[i] = luaF_newupval(L);
  setclvalue(L, L->top, cl);
  incr_top(L);
  unset_block_gc(L);
}


int luaD_protectedparser (lua_State *L, ZIO *z, const char *name) {
  struct SParser p;
  int status;
  p.z = z; p.name = name;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, llvm_f_parser, &p, savestack(L, L->top), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  return status;
}

int llvm_precall_jit (lua_State *L, StkId func, int nresults) {
  Closure *cl;
  ptrdiff_t funcr;
  CallInfo *ci;
  StkId st, base;
  Proto *p;

  funcr = savestack(L, func);
  cl = clvalue(func);
  L->ci->savedpc = L->savedpc;
  p = cl->l.p;
  luaD_checkstack(L, p->maxstacksize);
  func = restorestack(L, funcr);
  base = func + 1;
  if (L->top > base + p->numparams)
    L->top = base + p->numparams;
  ci = inc_ci(L);	/* now `enter' new function */
  ci->func = func;
  L->base = ci->base = base;
  ci->top = L->base + p->maxstacksize;
  lua_assert(ci->top <= L->stack_last);
  L->savedpc = p->code;	/* starting point */
  ci->nresults = nresults;
  for (st = L->top; st < ci->top; st++)
    setnilvalue(st);
  L->top = ci->top;
  if (L->hookmask & LUA_MASKCALL) {
    L->savedpc++;	/* hooks assume 'pc' is already incremented */
    luaD_callhook(L, LUA_HOOKCALL, -1);
    L->savedpc--;	/* correct 'pc' */
  }
  return (p->jit_func)(L); /* do the actual call */
}

int llvm_precall_jit_vararg (lua_State *L, StkId func, int nresults) {
  Closure *cl;
  ptrdiff_t funcr;
  CallInfo *ci;
  StkId st, base;
  Proto *p;
  int nargs;

  funcr = savestack(L, func);
  cl = clvalue(func);
  L->ci->savedpc = L->savedpc;
  p = cl->l.p;
  luaD_checkstack(L, p->maxstacksize);
  func = restorestack(L, funcr);
  nargs = cast_int(L->top - func) - 1;
  base = adjust_varargs(L, p, nargs);
  func = restorestack(L, funcr);	/* previous call may change the stack */
  ci = inc_ci(L);	/* now `enter' new function */
  ci->func = func;
  L->base = ci->base = base;
  ci->top = L->base + p->maxstacksize;
  lua_assert(ci->top <= L->stack_last);
  L->savedpc = p->code;	/* starting point */
  ci->nresults = nresults;
  for (st = L->top; st < ci->top; st++)
    setnilvalue(st);
  L->top = ci->top;
  if (L->hookmask & LUA_MASKCALL) {
    L->savedpc++;	/* hooks assume 'pc' is already incremented */
    luaD_callhook(L, LUA_HOOKCALL, -1);
    L->savedpc--;	/* correct 'pc' */
  }
  return (p->jit_func)(L); /* do the actual call */
}

int llvm_precall_lua (lua_State *L, StkId func, int nresults) {
  Closure *cl;
  Proto *p;

  cl = clvalue(func);
  p = cl->l.p;
  /* check if Function needs to be compiled. */
  if(p->jit_func == NULL) {
    llvm_compiler_compile(L, p);
  }
  if(p->jit_func != NULL) {
    if (!p->is_vararg) {  /* no varargs? */
      cl->l.precall = llvm_precall_jit;
      return llvm_precall_jit(L, func, nresults);
    } else {
      cl->l.precall = llvm_precall_jit_vararg;
      return llvm_precall_jit_vararg(L, func, nresults);
    }
  }
  /* function didn't compile, fall-back to lua interpreter */
  cl->l.precall = luaD_precall_lua;
  return luaD_precall_lua(L, func, nresults);
}

#endif

