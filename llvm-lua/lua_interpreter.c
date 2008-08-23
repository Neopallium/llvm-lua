/*
 * lua_interpreter.c -- Lua interpreter
 */

#include "lua_core.h"
#include "lua_interpreter.h"

#define ENABLE_PARSER_HOOK 1
#include "hook_parser.c"

#define main lua_main
#include "lua.c"
#undef main

