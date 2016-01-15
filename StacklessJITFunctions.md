# Introduction #


---

**Warning: This is a work in progress**

---


Currently Lua functions that are compiled to machine code need to use the c-stack to store function state for each called function, this causes the c-stack usage to grow with each function call.  This also causes coroutines to need there own c-stack to handle resuming.

The Lua core vm is able to use constant c-stack space when calling Lua functions from other Lua functions (calling C functions still causes the stack usage to grow).  To do this the Lua vm re-initializes the local state variables for the current Lua function after returning from the called Lua function.  The state variables(programcounter, stack base) are stored in a CallInfo structure before making a call to another Lua function.

One way to add continuation support to JIT functions is to use switch statement and gotos at the start of the function.  Before each vm\_OP\_CALL the continuation point would be saved in the CallInfo structure and the vm\_OP\_CALL call would be turned into a tail-call with the continuation label after the tail-call.  When the called function returns it would do a tail-call back into the caller function.  The switch statement would use the saved continuation point to jump back to the correct continuation label.

Links:
  * [Coroutines in C](http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html), A good example of how C functions can yield/resume.
  * [Resumable vm patch for Lua 5.1](http://lua-users.org/wiki/ResumableVmPatch), This patch could be using in-place of the LuaCoco patch.
  * [Continuations and Stackless Python](http://www.stackless.com/spcpaper.htm), interesting paper about how Stackless Python uses continuations.

# Details #

Test.lua:
```
local function add(a,b) return a + b end
local x = add(1,add(2,3))
print(x)
```

See [JITExample](http://code.google.com/p/llvm-lua/wiki/JITExample), for example of JITed functions without continuation support.

Pseudo JIT'ed C code for Test.lua (See [lua\_vm\_ops.c](http://code.google.com/p/llvm-lua/source/browse/trunk/llvm-lua/lua_vm_ops.c) for definition of `vm_OP_*` functions):
```

/* copied from src/lstate.h */
typedef struct CallInfo {
  StkId base;  /* base for this function */
  StkId func;  /* function index in the stack */
  StkId top;  /* top for this function */
  const Instruction *savedpc;
  int nresults;  /* expected number of results from this function */
  int tailcalls;  /* number of tail calls lost under this entry */
  int continue;  /* ADDED: continuation point, [default=0, start of function] */
  lua_CFunction continue_func; /* ADDED: continuation function. */
} CallInfo;

int vm_OP_CALL(func_state *fs, const Instruction i) {
  TValue *ra=RA(i);
  int b = GETARG_B(i);
  int nresults = GETARG_C(i) - 1;
  int ret;
  if (b != 0) fs->L->top = ra+b;  /* else previous instruction set top */
  fs->L->savedpc = fs->pc;
  ret = luaD_precall(fs->L, ra, nresults);
  switch (ret) {
    case PCRLUA: {
      luaV_execute(fs->L, 1); /* execute non-JITed lua function. */
      break;
    }
    case PCRC: {
      /* it was a C function (`precall' called it); adjust results */
      if (nresults >= 0) fs->L->top = fs->L->ci->top;
      break;
    }
    default: {
      return PCRYIELD;
    }
  }
  if(fs->L->ci->continue_func) {
    return fs->L->ci->continue_func(fs->L); /* tail-call back into calling function. */
  }
  return PCRC;
}

int vm_OP_RETURN(func_state *fs, const Instruction i) {
  TValue *ra = RA(i);
  int b = GETARG_B(i);
  if (b != 0) fs->L->top = ra+b-1;
  if (fs->L->openupval) luaF_close(fs->L, fs->base);
  fs->L->savedpc = fs->pc;
  b = luaD_poscall(fs->L, ra);
  return PCRC;
}

/* load function state from current callinfo structure. */
int vm_func_state_init(lua_State *L, func_state *fs) {
  CallInfo *ci = L->ci;
  fs->L = L;
  fs->cl = &clvalue(ci->func)->l;
  fs->base = L->base;
  fs->k = fs->cl->p->k;
  fs->pc = L->savedpc;
  return ci->continue; /* ADDED: return continuation point */
}

int jit_function_main_chunk(lua_State *L) {
  /* START function preamble */
  func_state fs; /* local function state (lua stack base, lua pc, constants) */
  switch(vm_func_state_init(L, &fs)) { /* initialize fs and get continuation point */
  default:
  case 0:
    break;
  case 1:
    goto continuation_point_1;
  case 2:
    goto continuation_point_2;
  case 3:
    goto continuation_point_3;
  }
  /* END function preamble */

continuation_point_0:
  /* closure: Create closure for 'add' function at stack0 */
  vm_OP_CLOSURE(&fs, 0x00000024);

  /* move: copy stack0(add func) to stack1 */
  vm_OP_MOVE(&fs, 0x00000040);

  /* loadk: load constant '1' to stack2 */
  vm_OP_LOADK(&fs, 0x00000081);

  /* move: copy stack0(add func) to stack3 */
  vm_OP_MOVE(&fs, 0x000000C0);

  /* loadk: load constant '2' to stack4 */
  vm_OP_LOADK(&fs, 0x00004101);

  /* loadk: load constant '3' to stack5 */
  vm_OP_LOADK(&fs, 0x00008141);

  /* call: execute 'stack3(stack4,stack5)' results at stack3 */
  fs->L->ci->continue = 1;
  return vm_OP_CALL(&fs, 0x018000DC);
continuation_point_1:

  /* call: execute 'stack1(stack2,stack3)' results at stack1 */
  fs->L->ci->continue = 2;
  return vm_OP_CALL(&fs, 0x0000805C);
continuation_point_2:

  /* getglobal: load global 'print' into stack2 */
  vm_OP_GETGLOBAL(&fs, 0x0000C085);

  /* move: copy stack1(local 'x') to stack3 */
  vm_OP_MOVE(&fs, 0x008000C0);

  /* call: execute 'stack2(stack3)' */
  fs->L->ci->continue = 3;
  return vm_OP_CALL(&fs, 0x0100409C);
continuation_point_3:

  /* return: */
  return vm_OP_RETURN(&fs, 0x0080001E);
}

int jit_function_add(lua_State *L) {
  /* START function preamble */
  func_state fs; /* local function state (lua stack base, lua pc, constants) */
  vm_func_state_init(L, &fs); /* initialize fs */
  /* no OP_CALLs, no continuation points, so we don't need a switch+gotos for this function */
  /* END function preamble */

  /* add: regA(return value) = regB(param 'a') + regC(param 'b') */
  vm_OP_ADD(&fs, 0x0000408C);

  /* return: regA */
  return vm_OP_RETURN(&fs, 0x0100009E);

  /* extra return opcode 0x0080001E omitted. */
}

```