#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>

typedef HANDLE	handle_t;
typedef HMODULE	modhandle_t;

typedef DWORD	dword;
typedef WORD	word;
typedef BYTE	byte;

#define snprintf _snprintf
#else
#include <elf.h>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <link.h>
#include <errno.h>
#include <fcntl.h>

typedef void* handle_t;
typedef void* modhandle_t;

#define TRUE		1
#define FALSE		0

#define MAX_PATH	260

#define strnicmp strncasecmp
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#define _HAS_ITERATOR_DEBUGGING 0

#include <map>
#include <vector>
#include <list>

#include <eiface.h>
#include <meta_api.h>
#include "amxxmodule.h"

#include "jitasm.h"
#include "udis86.h"

#include "rehlds_api.h"
#include "chldsprocess.h"
#include "utils.h"
#include "chookmanager.h"
#include "config.h"

#undef ARRAYSIZE
#define ARRAYSIZE(x)	(sizeof(x)/sizeof(x[0]))

#undef DLLEXPORT
#ifdef _WIN32
#define DLLEXPORT __declspec(dllexport)
#define NOINLINE __declspec(noinline)
#else
#define DLLEXPORT __attribute__((visibility("default")))
#define NOINLINE __attribute__((noinline))
#define WINAPI		/* */
#endif

#define C_DLLEXPORT extern "C" DLLEXPORT