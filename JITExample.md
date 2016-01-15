# Introduction #

WARNING: this page is out-of-date, but it still shows the basic idea behind llvm-lua.

This page is going to show what Lua code looks like after it has been compiled by using Pseudo C code.

# Details #

test.lua:
```
local function add(a,b) return a + b end
local x = add(1,add(2,3))
print(x)
```

test.lua bytecode (used [ChunkSpy](http://chunkspy.luaforge.net/)):
```
Main chunk opcodes:
0x00000024           [01] closure    0   0        ; 0 upvalues
0x00000040           [02] move       1   0      
0x00000081           [03] loadk      2   0        ; 1
0x000000C0           [04] move       3   0      
0x00004101           [05] loadk      4   1        ; 2
0x00008141           [06] loadk      5   2        ; 3
0x018000DC           [07] call       3   3   0  
0x0000805C           [08] call       1   0   2  
0x0000C085           [09] getglobal  2   3        ; print
0x008000C0           [10] move       3   1      
0x0100409C           [11] call       2   2   1  
0x0080001E           [12] return     0   1      

'add' function chunk opcodes:
0x0000408C           [1] add        2   0   1  
0x0100009E           [2] return     2   2      
0x0080001E           [3] return     0   1      

```

Pseudo JIT'ed C code for Test.lua (See [lua\_vm\_ops.c](http://code.google.com/p/llvm-lua/source/browse/trunk/llvm-lua/lua_vm_ops.c) for definition of `vm_OP_*` functions):
```
/* load function state from current callinfo structure. */
void vm_func_state_init(lua_State *L, func_state *fs) {
  fs->L = L;
  fs->cl = &clvalue(L->ci->func)->l;
  fs->base = L->base;
  fs->k = fs->cl->p->k;
  fs->pc = L->savedpc;
}

int jit_function_main_chunk(lua_State *L) {
  /* START function preamble */
  func_state fs; /* local function state (lua stack base, lua pc, constants) */
  vm_func_state_init(L, &fs); /* initialize fs */
  /* END function preamble */

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
  vm_OP_CALL(&fs, 0x018000DC);

  /* call: execute 'stack1(stack2,stack3)' results at stack1 */
  vm_OP_CALL(&fs, 0x0000805C);

  /* getglobal: load global 'print' into stack2 */
  vm_OP_GETGLOBAL(&fs, 0x0000C085);

  /* move: copy stack1(local 'x') to stack3 */
  vm_OP_MOVE(&fs, 0x008000C0);

  /* call: execute 'stack2(stack3)' */
  vm_OP_CALL(&fs, 0x0100409C);

  /* return: */
  return vm_OP_RETURN(&fs, 0x0080001E);

}

int jit_function_add(lua_State *L) {
  /* START function preamble */
  func_state fs; /* local function state (lua stack base, lua pc, constants) */
  vm_func_state_init(L, &fs); /* initialize fs */
  /* END function preamble */

  /* add: regA(return value) = regB(param 'a') + regC(param 'b') */
  vm_OP_ADD(&fs, 0x0000408C);

  /* return: regA */
  return vm_OP_RETURN(&fs, 0x0100009E);

  /* extra return opcode 0x0080001E omitted. */
}

```