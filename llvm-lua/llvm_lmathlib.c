/*
** $Id: lmathlib.c,v 1.67.1.1 2007/12/27 13:02:25 roberto Exp $
** Standard mathematical library
** See Copyright Notice in lua.h
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <math.h>

#define lmathlib_c
#define LUA_LIB

#include "lua.h"

#include "lauxlib.h"
#include "lualib.h"


#undef PI
#define PI (3.14159265358979323846)
#define RADIANS_PER_DEGREE (PI/180.0)


#define llvm_arg_tonumber(L, arg, narg) \
  if(!ttisnumber(arg)) { \
    goto fallback; \
  }

#define MATH_FASTCALL1(name, fname) \
static int math_ ## name ## _precall (lua_State *L, StkId func, int nresults) { \
  StkId arg1 = func + 1; \
  llvm_arg_tonumber(L, arg1, 1); \
  setnvalue(func, fname(nvalue(arg1))); \
  L->ci--; \
  L->top = func + 1; \
  L->base = L->ci->base; \
  return PCRC; \
fallback: \
  return luaD_precall_c(L, func, nresults); \
}

#define MATH_FASTCALL2(name, fname) \
static int math_ ## name ## _precall (lua_State *L, StkId func, int nresults) { \
  StkId arg1 = func + 1; \
  StkId arg2 = func + 2; \
  llvm_arg_tonumber(L, arg1, 1); \
  llvm_arg_tonumber(L, arg2, 2); \
  setnvalue(func, fname(nvalue(arg1), nvalue(arg2))); \
  L->ci--; \
  L->top = func + 1; \
  L->base = L->ci->base; \
  return PCRC; \
fallback: \
  return luaD_precall_c(L, func, nresults); \
}



static int math_abs (lua_State *L) {
  lua_pushnumber(L, fabs(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(abs, fabs)

static int math_sin (lua_State *L) {
  lua_pushnumber(L, sin(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(sin, sin)

static int math_sinh (lua_State *L) {
  lua_pushnumber(L, sinh(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(sinh, sinh)

static int math_cos (lua_State *L) {
  lua_pushnumber(L, cos(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(cos, cos)

static int math_cosh (lua_State *L) {
  lua_pushnumber(L, cosh(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(cosh, cosh)

static int math_tan (lua_State *L) {
  lua_pushnumber(L, tan(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(tan, tan)

static int math_tanh (lua_State *L) {
  lua_pushnumber(L, tanh(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(tanh, tanh)

static int math_asin (lua_State *L) {
  lua_pushnumber(L, asin(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(asin, asin)

static int math_acos (lua_State *L) {
  lua_pushnumber(L, acos(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(acos, acos)

static int math_atan (lua_State *L) {
  lua_pushnumber(L, atan(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(atan, atan)

static int math_atan2 (lua_State *L) {
  lua_pushnumber(L, atan2(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
  return 1;
}

MATH_FASTCALL2(atan2, atan2)

static int math_ceil (lua_State *L) {
  lua_pushnumber(L, ceil(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(ceil, ceil)

static int math_floor (lua_State *L) {
  lua_pushnumber(L, floor(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(floor, floor)

static int math_fmod (lua_State *L) {
  lua_pushnumber(L, fmod(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
  return 1;
}

MATH_FASTCALL2(fmod, fmod)

static int math_modf (lua_State *L) {
  double ip;
  double fp = modf(luaL_checknumber(L, 1), &ip);
  lua_pushnumber(L, ip);
  lua_pushnumber(L, fp);
  return 2;
}

static int math_sqrt (lua_State *L) {
  lua_pushnumber(L, sqrt(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(sqrt, sqrt)

static int math_pow (lua_State *L) {
  lua_pushnumber(L, pow(luaL_checknumber(L, 1), luaL_checknumber(L, 2)));
  return 1;
}

MATH_FASTCALL2(pow, pow)

static int math_log (lua_State *L) {
  lua_pushnumber(L, log(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(log, log)

static int math_log10 (lua_State *L) {
  lua_pushnumber(L, log10(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(log10, log10)

static int math_exp (lua_State *L) {
  lua_pushnumber(L, exp(luaL_checknumber(L, 1)));
  return 1;
}

MATH_FASTCALL1(exp, exp)

static int math_deg (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1)/RADIANS_PER_DEGREE);
  return 1;
}

#define radians_to_degree(num) ((num) / RADIANS_PER_DEGREE)
MATH_FASTCALL1(deg, radians_to_degree)

static int math_rad (lua_State *L) {
  lua_pushnumber(L, luaL_checknumber(L, 1)*RADIANS_PER_DEGREE);
  return 1;
}

#define degree_to_radians(num) ((num) * RADIANS_PER_DEGREE)
MATH_FASTCALL1(rad, degree_to_radians)

static int math_frexp (lua_State *L) {
  int e;
  lua_pushnumber(L, frexp(luaL_checknumber(L, 1), &e));
  lua_pushinteger(L, e);
  return 2;
}

static int math_ldexp (lua_State *L) {
  lua_pushnumber(L, ldexp(luaL_checknumber(L, 1), luaL_checkint(L, 2)));
  return 1;
}

MATH_FASTCALL2(ldexp, ldexp)


static int math_min (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  lua_Number dmin = luaL_checknumber(L, 1);
  int i;
  for (i=2; i<=n; i++) {
    lua_Number d = luaL_checknumber(L, i);
    if (d < dmin)
      dmin = d;
  }
  lua_pushnumber(L, dmin);
  return 1;
}


static int math_max (lua_State *L) {
  int n = lua_gettop(L);  /* number of arguments */
  lua_Number dmax = luaL_checknumber(L, 1);
  int i;
  for (i=2; i<=n; i++) {
    lua_Number d = luaL_checknumber(L, i);
    if (d > dmax)
      dmax = d;
  }
  lua_pushnumber(L, dmax);
  return 1;
}


static int math_random (lua_State *L) {
  /* the `%' avoids the (rare) case of r==1, and is needed also because on
     some systems (SunOS!) `rand()' may return a value larger than RAND_MAX */
  lua_Number r = (lua_Number)(rand()%RAND_MAX) / (lua_Number)RAND_MAX;
  switch (lua_gettop(L)) {  /* check number of arguments */
    case 0: {  /* no arguments */
      lua_pushnumber(L, r);  /* Number between 0 and 1 */
      break;
    }
    case 1: {  /* only upper limit */
      int u = luaL_checkint(L, 1);
      luaL_argcheck(L, 1<=u, 1, "interval is empty");
      lua_pushnumber(L, floor(r*u)+1);  /* int between 1 and `u' */
      break;
    }
    case 2: {  /* lower and upper limits */
      int l = luaL_checkint(L, 1);
      int u = luaL_checkint(L, 2);
      luaL_argcheck(L, l<=u, 2, "interval is empty");
      lua_pushnumber(L, floor(r*(u-l+1))+l);  /* int between `l' and `u' */
      break;
    }
    default: return luaL_error(L, "wrong number of arguments");
  }
  return 1;
}


static int math_randomseed (lua_State *L) {
  srand(luaL_checkint(L, 1));
  return 0;
}


static const luaL_Reg3 mathlib[] = {
  {"abs",   math_abs, math_abs_precall},
  {"acos",  math_acos, math_acos_precall},
  {"asin",  math_asin, math_asin_precall},
  {"atan2", math_atan2, math_atan2_precall},
  {"atan",  math_atan, math_atan_precall},
  {"ceil",  math_ceil, math_ceil_precall},
  {"cosh",   math_cosh, math_cosh_precall},
  {"cos",   math_cos, math_cos_precall},
  {"deg",   math_deg, math_deg_precall},
  {"exp",   math_exp, math_exp_precall},
  {"floor", math_floor, math_floor_precall},
  {"fmod",   math_fmod, math_fmod_precall},
  {"frexp", math_frexp, NULL},
  {"ldexp", math_ldexp, math_ldexp_precall},
  {"log10", math_log10, math_log10_precall},
  {"log",   math_log, math_log_precall},
  {"max",   math_max, NULL},
  {"min",   math_min, NULL},
  {"modf",   math_modf, NULL},
  {"pow",   math_pow, math_pow_precall},
  {"rad",   math_rad, math_rad_precall},
  {"random",     math_random, NULL},
  {"randomseed", math_randomseed, NULL},
  {"sinh",   math_sinh, math_sinh_precall},
  {"sin",   math_sin, math_sin_precall},
  {"sqrt",  math_sqrt, math_sqrt_precall},
  {"tanh",   math_tanh, math_tanh_precall},
  {"tan",   math_tan, math_tan_precall},
  {NULL, NULL, NULL}
};


/*
** Open math library
*/
LUALIB_API int luaopen_math (lua_State *L) {
  luaL_register3(L, LUA_MATHLIBNAME, mathlib);
  lua_pushnumber(L, PI);
  lua_setfield(L, -2, "pi");
  lua_pushnumber(L, HUGE_VAL);
  lua_setfield(L, -2, "huge");
#if defined(LUA_COMPAT_MOD)
  lua_getfield(L, -1, "fmod");
  lua_setfield(L, -2, "mod");
#endif
  return 1;
}

#ifdef __cplusplus
}
#endif

