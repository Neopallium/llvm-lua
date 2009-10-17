/*
** $Id: luac.c,v 1.54 2006/06/02 17:37:11 lhf Exp $
** Lua compiler (saves bytecodes to files; also list bytecodes)
** See Copyright Notice in lua.h
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define luac_c
#define LUA_CORE

#include "lua_core.h"
#include "lua_interpreter.h"
#include "llvm_compiler.h"
#include "llvm_dumper.h"

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#include "ldo.h"
#include "lfunc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstring.h"
#include "lundump.h"

#define ENABLE_PARSER_HOOK 0
#include "hook_parser.c"
#include "print.c"

#define PROGNAME	"luac"		/* default program name */
#define	OUTPUT		PROGNAME ".out"	/* default output file */

static int parse_only=0;	/* only parse the Lua scripts? */
static int llvm_bitcode=0;/* output llvm bitcode? */
static int listing=0;			/* list bytecodes? */
static int dumping=1;			/* dump bytecodes? */
static int stripping=0;			/* strip debug information? */
static char Output[]={ OUTPUT };	/* default output file name */
static const char* output=Output;	/* actual output file name */
static const char* progname=PROGNAME;	/* actual program name */

#define MAX_PRELOADS (MAXINDEXRK - 2)
static char* preload_libs[MAX_PRELOADS];
static int preloads=0;

static void fatal(const char* message) {
  fprintf(stderr,"%s: %s\n",progname,message);
  exit(EXIT_FAILURE);
}

static void cannot(const char* what) {
  fprintf(stderr,"%s: cannot %s %s: %s\n",progname,what,output,strerror(errno));
  exit(EXIT_FAILURE);
}

static void usage(const char* message) {
  if (*message=='-')
    fprintf(stderr,"%s: unrecognized option " LUA_QS "\n",progname,message);
  else
    fprintf(stderr,"%s: %s\n",progname,message);
  fprintf(stderr,
  "usage: %s [options] [filenames].\n"
  "Available options are:\n"
  "  -        process stdin\n"
  "  -bc      output LLVM bitcode\n"
  "  -l       list\n"
  "  -L name  preload lua library " LUA_QL("name") "\n"
  "  -o name  output to file " LUA_QL("name") " (default is \"%s\")\n"
  "  -p       parse only\n"
  "  -s       strip debug information\n"
  "  -v       show version information\n"
  "  --       stop handling options\n",
  progname,Output);
  exit(EXIT_FAILURE);
}

#define	IS(s)	(strcmp(argv[i],s)==0)

static int doargs(int argc, char* argv[]) {
  int i;
  int version=0;
  if (argv[0]!=NULL && *argv[0]!=0) progname=argv[0];
  for (i=1; i<argc; i++) {
    if (*argv[i]!='-')			/* end of options; keep it */
      break;
    else if (IS("--")) {		/* end of options; skip it */
      ++i;
      if (version) ++version;
      break;
    } else if (IS("-")) 		/* end of options; use stdin */
      break;
    else if (IS("-bc")) {		/* output LLVM bitcode */
      llvm_bitcode=1;
      dumping=0;
    } else if (IS("-L")) {	/*  */
      if (preloads >= MAX_PRELOADS) usage(LUA_QL("-L") " too many preloads");
      preload_libs[preloads]=argv[++i];
      if (IS("-")) preload_libs[preloads]=NULL;
      preloads++;
    } else if (IS("-l"))		/* list */
      ++listing;
    else if (IS("-o")) {		/* output file */
      output=argv[++i];
      if (output==NULL || *output==0) usage(LUA_QL("-o") " needs argument");
      if (IS("-")) output=NULL;
    } else if (IS("-p"))		/* parse only */
      parse_only=1;
    else if (IS("-s"))			/* strip debug information */
      stripping=1;
    else if (IS("-v"))			/* show version */
      ++version;
    else					/* unknown option */
      usage(argv[i]);
  }
  if (i==argc && (listing || !dumping)) {
    dumping=0;
    argv[--i]=Output;
  }
  if (version) {
    printf("%s  %s\n",LUA_RELEASE,LUA_COPYRIGHT);
    if (version==argc-1) exit(EXIT_SUCCESS);
  }
  return i;
}

#define toproto(L,i) (clvalue(L->top+(i))->l.p)

static Proto* combine(lua_State* L, int scripts) {
  if (scripts==1 && preloads==0)
    return toproto(L,-1);
  else {
    TString *s;
    TValue *k;
    int i,pc,n;
    Proto* f=luaF_newproto(L);
    setptvalue2s(L,L->top,f); incr_top(L);
    f->source=luaS_newliteral(L,"=(" PROGNAME ")");
    f->maxstacksize=1;
    pc=(2*scripts) + 1;
    if(preloads > 0) {
      pc+=(2*preloads) + 2;
    }
    f->code=luaM_newvector(L,pc,Instruction);
    f->sizecode=pc;
    n=(scripts + preloads);
    f->p=luaM_newvector(L,n,Proto*);
    f->sizep=n;
    pc=0;
    n=0;
    /* preload libraries. */
    if (preloads > 0) {
      /* create constants array. */
      f->k=luaM_newvector(L, (preloads + 2),TValue);
      f->sizek=(preloads + 2);
      /* make room for "local t" variable. */
      f->maxstacksize=2;
      /* add "package" & "preload" constants. */
      k=&(f->k[0]);
      s=luaS_newliteral(L, "package");
      setsvalue2n(L,k,s);
      k=&(f->k[1]);
      s=luaS_newliteral(L, "preload");
      setsvalue2n(L,k,s);
      /* code: local t = package.preload */
      f->code[pc++]=CREATE_ABx(OP_GETGLOBAL,0,0);
      f->code[pc++]=CREATE_ABC(OP_GETTABLE,0,0,RKASK(1));
    }
    /* add preload libraries to "package.preload" */
    for (i=0; i < preloads; i++) {
      /* create constant for library name. */
      k=&(f->k[i+2]);
      s=luaS_new(L, preload_libs[i]);
      setsvalue2n(L,k,s);
      /* code: t['name'] = function() --[[ lib code ]] end */
      f->code[pc++]=CREATE_ABx(OP_CLOSURE,1,n);
      f->code[pc++]=CREATE_ABC(OP_SETTABLE,0,RKASK(i+2),1);
      f->p[n++]=toproto(L,i-preloads-1);
    }
    /* call scripts. */
    for (i=0; i < scripts; i++) {
      /* code: (function() --[[ script code ]] end)() */
      f->code[pc++]=CREATE_ABx(OP_CLOSURE,0,n);
      f->code[pc++]=CREATE_ABC(OP_CALL,0,1,1);
      f->p[n++]=toproto(L,i-scripts-1-preloads);
    }
    f->code[pc++]=CREATE_ABC(OP_RETURN,0,1,0);
    return f;
  }
}

static int writer(lua_State* L, const void* p, size_t size, void* u) {
  UNUSED(L);
  return (fwrite(p,size,1,(FILE*)u)!=1) && (size!=0);
}

struct Smain {
  int argc;
  char** argv;
};

static int pmain(lua_State* L) {
  struct Smain* s = (struct Smain*)lua_touserdata(L, 1);
  int argc=s->argc;
  char** argv=s->argv;
  Proto* f;
  int scripts=0;
  int i;
  if (!lua_checkstack(L,argc)) fatal("too many input files");
  lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
  luaL_openlibs(L);  /* open libraries */
  lua_gc(L, LUA_GCRESTART, 0);
  /* compile each script from command line into a Lua function. */
  for (i=0; i<argc; i++) {
    const char* filename=IS("-") ? NULL : argv[i];
    if(IS("-L")) break;
    if (luaL_loadfile(L,filename)!=0) fatal(lua_tostring(L,-1));
    scripts++;
  }
  /* compile each preload library from the command line into a Lua function. */
  for (i=0; i<preloads; i++) {
    char* filename=preload_libs[i];
    char* p;
    /* try loading library as if it is a normal file. */
    if (luaL_loadfile(L,filename)!=0) {
      /* try pre-loading library with 'require' module loading system. */
      lua_getglobal(L, "require");
      lua_pushstring(L, filename);
      lua_pushboolean(L, 1);
      lua_call(L, 2, 1);
      if (lua_iscfunction(L, -1)) { /* make sure it is not a C-Function. */
        lua_pop(L, 1);
        lua_pushfstring(L, "\nCan't preload C module: '%s'\n", filename);
        lua_concat(L, 2);  /* accumulate with error from luaL_findfile */
        fatal(lua_tostring(L,-1));
      }
      if (!lua_isfunction(L, -1)) { /* did we get an error? */
        lua_pushliteral(L, "\n");
        lua_concat(L, 3);  /* accumulate with error from luaL_findfile */
        fatal(lua_tostring(L,-1));
      } else {
        lua_remove(L, -2); /* remove error from luaL_findfile. */
      }
    } else {
      /* convert filename into package name. */
      p= filename + strlen(filename);
      for(;p >= filename; p--) {
        if(p[0] == '.') { /* Remove file extension. */
          p[0] = '\0';
          continue;
        }
        if(p[0] == '/') { /* Remove file path. */
          preload_libs[i] = p+1;
          break;
        }
      }
    }
  }
  /* generate a new Lua function to combine all of the compiled scripts. */
  f=combine(L, scripts);
  if (listing) luaU_print(f,listing>1);
  if (llvm_bitcode && !parse_only) {
    lua_lock(L);
    llvm_dumper_dump(output, L, f, stripping);
    lua_unlock(L);
  }
  if (dumping && !parse_only) {
    FILE* D= (output==NULL) ? stdout : fopen(output,"wb");
    if (D==NULL) cannot("open");
    lua_lock(L);
    luaU_dump(L,f,writer,D,stripping);
    lua_unlock(L);
    if (ferror(D)) cannot("write");
    if (fclose(D)) cannot("close");
  }
  return 0;
}

int luac_main(int argc, char* argv[]) {
  lua_State* L;
  struct Smain s;
  int i=doargs(argc,argv);
  argc-=i; argv+=i;
  if (argc<=0) usage("no input files given");
  L=lua_open();
  if (L==NULL) fatal("not enough memory for state");
  s.argc=argc;
  s.argv=argv;
  if (lua_cpcall(L,pmain,&s)!=0) fatal(lua_tostring(L,-1));
  lua_close(L);
  return EXIT_SUCCESS;
}

#ifdef __cplusplus
}
#endif

