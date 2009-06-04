
#ifndef lua_vm_ops_h
#define lua_vm_ops_h

#ifdef __cplusplus
extern "C" {
#endif

#include "lua_core.h"
#include "lobject.h"

typedef struct {
  TValue *k;
  LClosure *cl;
} func_state;

#if defined(__x86_64__)
typedef long lua_Long;
#else
typedef long long lua_Long;
#endif

typedef unsigned int hint_t;
#define HINT_NONE							0
#define HINT_C_NUM_CONSTANT		(1<<0)
#define HINT_Bx_NUM_CONSTANT	(1<<1)
#define HINT_NOT							(1<<2)
#define HINT_FOR_M_N_N				(1<<3)
#define HINT_FOR_N_M_N				(1<<4)
#define HINT_FOR_N_N_N				(1<<5)
#define HINT_FOR_N_N					(1<<6)
#define HINT_SKIP_OP					(1<<7)
#define HINT_MINI_VM					(1<<8)
#define HINT_USE_LONG					(1<<9)
#define HINT_UP								(1<<10)
#define HINT_DOWN							(1<<11)
#define HINT_NO_SUB						(1<<12)

typedef enum {
	VAR_T_VOID = 0,
	VAR_T_INT,
	VAR_T_ARG_A,
	VAR_T_ARG_B,
	VAR_T_ARG_BK,
	VAR_T_ARG_Bx_NUM_CONSTANT,
	VAR_T_ARG_B_FB2INT,
	VAR_T_ARG_Bx,
	VAR_T_ARG_sBx,
	VAR_T_ARG_C,
	VAR_T_ARG_CK,
	VAR_T_ARG_C_NUM_CONSTANT,
	VAR_T_ARG_C_NEXT_INSTRUCTION,
	VAR_T_ARG_C_FB2INT,
	VAR_T_PC_OFFSET,
	VAR_T_INSTRUCTION,
	VAR_T_NEXT_INSTRUCTION,
	VAR_T_LUA_STATE_PTR,
	VAR_T_K,
	VAR_T_CL,
	VAR_T_OP_VALUE_0,
	VAR_T_OP_VALUE_1,
	VAR_T_OP_VALUE_2,
} val_t;

typedef struct {
	int opcode; /* Lua opcode */
	hint_t hint; /* Specialized version. [0=generic] */
	val_t ret_type; /* return type */
	const char *name; /* function name */
	val_t params[10]; /* an 'VOID' type ends the parameter list */
} vm_func_info;

extern const vm_func_info vm_op_functions[];

extern int vm_op_run_count[];

extern void vm_count_OP(const Instruction i);

extern void vm_print_OP(lua_State *L, LClosure *cl, const Instruction i, int pc_offset);

extern void vm_next_OP(lua_State *L, LClosure *cl, int pc_offset);

extern void vm_OP_MOVE(lua_State *L, int a, int b);

extern void vm_OP_LOADK(lua_State *L, TValue *k, int a, int bx);

extern void vm_OP_LOADK_N(lua_State *L, int a, lua_Number nb);

extern void vm_OP_LOADBOOL(lua_State *L, int a, int b, int c);

extern void vm_OP_LOADNIL(lua_State *L, int a, int b);

extern void vm_OP_GETUPVAL(lua_State *L, LClosure *cl, int a, int b);

extern void vm_OP_GETGLOBAL(lua_State *L, TValue *k, LClosure *cl, int a, int bx);

extern void vm_OP_GETTABLE(lua_State *L, TValue *k, int a, int b, int c);

extern void vm_OP_SETGLOBAL(lua_State *L, TValue *k, LClosure *cl, int a, int bx);

extern void vm_OP_SETUPVAL(lua_State *L, LClosure *cl, int a, int b);

extern void vm_OP_SETTABLE(lua_State *L, TValue *k, int a, int b, int c);

extern void vm_OP_NEWTABLE(lua_State *L, int a, int b, int c);

extern void vm_OP_SELF(lua_State *L, TValue *k, int a, int b, int c);

extern void vm_OP_ADD(lua_State *L, TValue *k, int a, int b, int c);
extern void vm_OP_ADD_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c);

extern void vm_OP_SUB(lua_State *L, TValue *k, int a, int b, int c);
extern void vm_OP_SUB_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c);

extern void vm_OP_MUL(lua_State *L, TValue *k, int a, int b, int c);
extern void vm_OP_MUL_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c);

extern void vm_OP_DIV(lua_State *L, TValue *k, int a, int b, int c);
extern void vm_OP_DIV_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c);

extern void vm_OP_MOD(lua_State *L, TValue *k, int a, int b, int c);
extern void vm_OP_MOD_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c);

extern void vm_OP_POW(lua_State *L, TValue *k, int a, int b, int c);
extern void vm_OP_POW_NC(lua_State *L, TValue *k, int a, int b, lua_Number nc, int c);

extern void vm_OP_UNM(lua_State *L, int a, int b);

extern void vm_OP_NOT(lua_State *L, int a, int b);

extern void vm_OP_LEN(lua_State *L, int a, int b);

extern void vm_OP_CONCAT(lua_State *L, int a, int b, int c);

extern void vm_OP_JMP(lua_State *L, int sbx);

extern int vm_OP_EQ(lua_State *L, TValue *k, int a, int b, int c);
extern int vm_OP_EQ_NC(lua_State *L, TValue *k, int b, lua_Number nc);
extern int vm_OP_NOT_EQ_NC(lua_State *L, TValue *k, int b, lua_Number nc);

extern int vm_OP_LT(lua_State *L, TValue *k, int a, int b, int c);

extern int vm_OP_LE(lua_State *L, TValue *k, int a, int b, int c);

extern int vm_OP_TEST(lua_State *L, int a, int c);

extern int vm_OP_TESTSET(lua_State *L, int a, int b, int c);

extern int vm_OP_CALL(lua_State *L, int a, int b, int c);

extern int vm_OP_RETURN(lua_State *L, int a, int b);

extern int vm_OP_TAILCALL(lua_State *L, int a, int b);

extern int vm_OP_FORLOOP(lua_State *L, int a, int sbx);
extern int vm_OP_FORLOOP_N_N(lua_State *L, int a, int sbx, lua_Number limit, lua_Number step);
extern int vm_OP_FORLOOP_N_N_N(lua_State *L, int a, int sbx, lua_Number idx, lua_Number limit, lua_Number step);
extern int vm_OP_FORLOOP_up(lua_State *L, int a, int sbx, lua_Number idx, lua_Number limit);
extern int vm_OP_FORLOOP_down(lua_State *L, int a, int sbx, lua_Number idx, lua_Number limit);
extern int vm_OP_FORLOOP_long_up(lua_State *L, int a, int sbx, lua_Long idx, lua_Long limit);
extern int vm_OP_FORLOOP_long_down(lua_State *L, int a, int sbx, lua_Long idx, lua_Long limit);

extern void vm_OP_FORPREP_slow(lua_State *L, int a, int sbx);
extern void vm_OP_FORPREP(lua_State *L, int a, int sbx);
extern void vm_OP_FORPREP_no_sub(lua_State *L, int a, int sbx);
extern void vm_OP_FORPREP_M_N_N(lua_State *L, int a, int sbx, lua_Number limit, lua_Number step);
extern void vm_OP_FORPREP_N_M_N(lua_State *L, int a, int sbx, lua_Number init, lua_Number step);
extern void vm_OP_FORPREP_N_N_N(lua_State *L, int a, int sbx, lua_Number init, lua_Number step);

extern int vm_OP_TFORLOOP(lua_State *L, int a, int c);

extern void vm_OP_SETLIST(lua_State *L, int a, int b, int c);

extern void vm_OP_CLOSE(lua_State *L, int a);

extern void vm_OP_CLOSURE(lua_State *L, LClosure *cl, int a, int bx, int pseudo_ops_offset);

extern void vm_OP_VARARG(lua_State *L, LClosure *cl, int a, int b);

extern int is_mini_vm_op(int opcode);
extern void vm_mini_vm(lua_State *L, LClosure *cl, int count, int pseudo_ops_offset);

extern void vm_op_hint_locals(char *locals, int stacksize, TValue *k, const Instruction i);

extern LClosure *vm_get_current_closure(lua_State *L);

extern TValue *vm_get_current_constants(LClosure *cl);

extern lua_Number vm_get_number(lua_State *L, int idx);
extern void vm_set_number(lua_State *L, int idx, lua_Number num);

extern lua_Long vm_get_long(lua_State *L, int idx);
extern void vm_set_long(lua_State *L, int idx, lua_Long num);

/*
** some macros for common tasks in `vm_OP_*' functions.
*/

#define runtime_check(L, c) { if (!(c)) return; }

#define RA(i) (base+GETARG_A(i))
/* to be used after possible stack reallocation */
#define RB(i) check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base+GETARG_B(i))
#define RC(i) check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base+GETARG_C(i))
#define RKB(i)  check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
  ISK(GETARG_B(i)) ? k+INDEXK(GETARG_B(i)) : base+GETARG_B(i))
#define RKC(i)  check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
  ISK(GETARG_C(i)) ? k+INDEXK(GETARG_C(i)) : base+GETARG_C(i))
#define KBx(i)  check_exp(getBMode(GET_OPCODE(i)) == OpArgK, k+GETARG_Bx(i))

#define RK(arg)  (ISK(arg) ? k+INDEXK(arg) : base+arg)

#define dojump(i)  {luai_threadyield(L);}
#define skip_op()


#define Protect(x)  { {x;}; base = L->base; }

#define arith_op(op,tm) { \
        TValue *ra = base + a; \
        TValue *rb = RK(b); \
        TValue *rc = RK(c); \
        if (ttisnumber(rb) && ttisnumber(rc)) { \
          lua_Number nb = nvalue(rb), nc = nvalue(rc); \
          setnvalue(ra, op(nb, nc)); \
        } \
        else \
          luaV_arith(L, ra, rb, rc, tm); \
      }

#define arith_op_nc(op,tm) { \
        TValue *ra = base + a; \
        TValue *rb = RK(b); \
        if (ttisnumber(rb)) { \
          lua_Number nb = nvalue(rb); \
          setnvalue(ra, op(nb, nc)); \
        } \
        else \
          luaV_arith(L, ra, rb, RK(c), tm); \
      }


#ifdef __cplusplus
}
#endif

#endif

