
#ifndef lua_vm_ops_h
#define lua_vm_ops_h

#ifdef __cplusplus
extern "C" {
#endif

#include "lua_core.h"
#include "lobject.h"

typedef struct {
  lua_State *L;
  StkId base;
  TValue *k;
  LClosure *cl;
#ifndef LUA_NODEBUG
  const Instruction *pc;
#endif
} func_state;

typedef enum {
	VAR_TYPE_VOID = 0,
	VAR_TYPE_INT,
	VAR_TYPE_INSTRUCTION,
	VAR_TYPE_FUNC_STATE,
	VAR_TYPE_FUNC_STATE_PTR,
	VAR_TYPE_LUA_STATE,
	VAR_TYPE_LUA_STATE_PTR,
} var_type;

typedef struct {
	var_type ret_type; /* return type */
	char *name; /* function name */
	var_type params[5]; /* an 'VOID' type ends the parameter list */
} func_types;

extern const func_types vm_functions[];

extern void vm_print_OP(func_state *fs, const Instruction i);

extern void vm_next_OP(func_state *fs);

extern void vm_OP_MOVE(func_state *fs, int a, int b);

extern void vm_OP_LOADK(func_state *fs, const Instruction i);

extern void vm_OP_LOADBOOL(func_state *fs, const Instruction i);

extern void vm_OP_LOADNIL(func_state *fs, const Instruction i);

extern void vm_OP_GETUPVAL(func_state *fs, int a, int b);

extern void vm_OP_GETGLOBAL(func_state *fs, const Instruction i);

extern void vm_OP_GETTABLE(func_state *fs, const Instruction i);

extern void vm_OP_SETGLOBAL(func_state *fs, const Instruction i);

extern void vm_OP_SETUPVAL(func_state *fs, int a, int b);

extern void vm_OP_SETTABLE(func_state *fs, const Instruction i);

extern void vm_OP_NEWTABLE(func_state *fs, const Instruction i);

extern void vm_OP_SELF(func_state *fs, const Instruction i);

extern void vm_OP_ADD(func_state *fs, const Instruction i);

extern void vm_OP_SUB(func_state *fs, const Instruction i);

extern void vm_OP_MUL(func_state *fs, const Instruction i);

extern void vm_OP_DIV(func_state *fs, const Instruction i);

extern void vm_OP_MOD(func_state *fs, const Instruction i);

extern void vm_OP_POW(func_state *fs, const Instruction i);

extern void vm_OP_UNM(func_state *fs, const Instruction i);

extern void vm_OP_NOT(func_state *fs, const Instruction i);

extern void vm_OP_LEN(func_state *fs, const Instruction i);

extern void vm_OP_CONCAT(func_state *fs, const Instruction i);

extern void vm_OP_JMP(func_state *fs, const Instruction i);

extern int vm_OP_EQ(func_state *fs, const Instruction i);

extern int vm_OP_LT(func_state *fs, const Instruction i);

extern int vm_OP_LE(func_state *fs, const Instruction i);

extern int vm_OP_TEST(func_state *fs, int a, int c);

extern int vm_OP_TESTSET(func_state *fs, int a, int b, int c);

extern int vm_OP_CALL(func_state *fs, const Instruction i);

extern int vm_OP_RETURN(func_state *fs, const Instruction i);

extern int vm_OP_TAILCALL(func_state *fs, const Instruction i, const Instruction ret_i);

extern int vm_OP_FORLOOP(func_state *fs, const Instruction i);

extern int vm_OP_FORPREP(func_state *fs, const Instruction i);

extern int vm_OP_TFORLOOP(func_state *fs, const Instruction i);

extern void vm_OP_SETLIST(func_state *fs, const Instruction i, int c);

extern void vm_OP_CLOSE(func_state *fs, const Instruction i);

extern void vm_OP_CLOSURE(func_state *fs, const Instruction i, int pseudo_ops_offset);

extern void vm_OP_VARARG(func_state *fs, const Instruction i);

extern void vm_func_state_init(lua_State *L, func_state *fs);

/*
** some macros for common tasks in `vm_OP_*' functions.
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


#ifndef LUA_NODEBUG
#define dojump(fs,i)  {(fs->pc) += (i); luai_threadyield(fs->L);}
#define skip_op(fs)   (fs->pc)++;
#else
#define dojump(fs,i)  {luai_threadyield(fs->L);}
#define skip_op(fs)
#endif


#ifndef LUA_NODEBUG
#define Protect(x)  { fs->L->savedpc = fs->pc; {x;}; fs->base = fs->L->base; }
#else
#define Protect(x)  { {x;}; fs->base = fs->L->base; }
#endif

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


#ifdef __cplusplus
}
#endif

#endif

