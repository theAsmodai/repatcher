#include "precompiled.h"
#include "repatcher.h"

std::vector<void *> g_executablePages;
size_t g_pageUsedSpace;
byte g_tempMemory[65536];
size_t g_tempUsed = 0;
char g_gameName[32];

std::vector<type_t *> g_types;

char g_lastError[512];
bool g_amxxAttached;
bool g_rehldsEngine;

struct ntr_t
{
	const char* reg_name;
	register_e	reg_id;
} g_name2reg[] =
{
	{"eax", r_eax},
	{"ebx", r_ebx},
	{"ecx", r_ecx},
	{"edx", r_edx},
	{"esi", r_esi},
	{"edi", r_edi},
	{"ebp", r_ebp},
	{"esp", r_esp},

	{"st0", r_st0},
	{"st1", r_st1},
	{"st2", r_st2},
	{"st3", r_st3},
	{"st4", r_st4},
	{"st5", r_st5},
	{"st6", r_st6},
	{"st7", r_st7},

	{"xmm0", r_xmm0},
	{"xmm1", r_xmm1},
	{"xmm2", r_xmm2},
	{"xmm3", r_xmm3},
	{"xmm4", r_xmm4},
	{"xmm5", r_xmm5},
	{"xmm6", r_xmm6},
	{"xmm7", r_xmm7},

	{"ax", r_ax},
	{"bx", r_bx},
	{"cx", r_cx},
	{"dx", r_dx},

	{"al", r_al},
	{"ah", r_ah},
	{"bl", r_bl},
	{"bh", r_bh},
	{"cl", r_cl},
	{"ch", r_ch},
	{"dl", r_dl},
	{"dh", r_dh}
};

NOINLINE void Con_Printf(const char* fmt, ...)
{
	va_list			argptr;
	char			string[1024];

	va_start(argptr, fmt);
	vsnprintf(string, sizeof string, fmt, argptr);
	va_end(argptr);

	g_engfuncs.pfnServerPrint(string);
}

NOINLINE void Log_Error(AMX* amx, const char* fmt, ...)
{
	va_list			argptr;
	char			string[1024];

	va_start(argptr, fmt);
	vsnprintf(string, sizeof string, fmt, argptr);
	va_end(argptr);

	if (amx)
		MF_LogError(amx, AMX_ERR_NATIVE, "%s", string);
	else if (g_amxxAttached)
		MF_Log("error: %s\n", string);
	else
		Con_Printf("[RePatcher] error: %s\n", string);
}

#ifdef _WIN32
__declspec(noreturn)
#else
__attribute__((noreturn))
#endif
NOINLINE void Sys_Error(const char *error, ...)
{
	va_list argptr;
	char text[1024];
	static qboolean bReentry;

	va_start(argptr, error);
	vsnprintf(text, ARRAYSIZE(text), error, argptr);
	va_end(argptr);

	Con_Printf("%s", text);

#ifdef _WIN32
	MessageBoxA(GetForegroundWindow(), text, "Fatal error - Dedicated server", MB_ICONERROR | MB_OK);
#endif // _WIN32

#ifdef SELF_TEST
	//Allahu akbar!
	*(int *)NULL = NULL;
#endif
	exit(-1);
}

NOINLINE void setError(const char* fmt, ...)
{
	va_list			argptr;

	va_start(argptr, fmt);
	vsnprintf(g_lastError, sizeof g_lastError, fmt, argptr);
	va_end(argptr);
}

int parse(char* line, char** argv, int max_args, char token)
{
	int count = 0;

	while (*line)
	{
		// null whitespaces
		while (*line == token)
			*line++ = '\0';

		if (*line)
		{
			argv[count++] = line; // save arg address

			if (count == max_args)
				break;

			// skip arg
			while (*line != '\0' && *line != token)
				line++;
		}
	}

	return count;
}

char* trim(char* str)
{
	char *ibuf = str;
	int i = 0;

	if (str == NULL) return NULL;
	for (ibuf = str; *ibuf && (byte)(*ibuf) < (byte)0x80 && isspace(*ibuf); ++ibuf)
		;

	i = strlen(ibuf);
	if (str != ibuf)
		memmove(str, ibuf, i);

	str[i] = 0;
	while (--i >= 0)
	{
		if (!isspace(str[i]) && str[i] != '\n')
			break;
	}
	str[++i] = 0;
	return str;
}

byte* allocExecutableMemory(size_t size)
{
	void* page;
	byte* mem;

	if (g_executablePages.size() == 0 || g_pageUsedSpace + size > g_hldsProcess.getPageSize())
	{
		g_pageUsedSpace = 0;
#ifdef WIN32
		page = VirtualAlloc(NULL, g_hldsProcess.getPageSize(), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
#else
		page = mmap(NULL, g_hldsProcess.getPageSize(), PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
		g_executablePages.push_back(page);
	}
	else
		page = g_executablePages.back();

	mem = (byte *)((dword)page + (dword)g_pageUsedSpace);
	g_pageUsedSpace += size;
	return mem;
}

void* allocTempMemory(size_t size)
{
	if (g_tempUsed + size > sizeof g_tempMemory)
		errorNoMemory();

	dword addr = (dword)g_tempMemory + g_tempUsed;
	g_tempUsed += size;
	return (void *)addr;
}

basetype_e getBaseType(const char* name)
{
	if (!strcmp(name, "int"))
		return bt_int;
	if (!strcmp(name, "short"))
		return bt_short;
	if (!strcmp(name, "word"))
		return bt_word;
	if (!strcmp(name, "char"))
		return bt_char;
	if (!strcmp(name, "byte"))
		return bt_byte;
	if (!strcmp(name, "float"))
		return bt_float;
	if (!strcmp(name, "double"))
		return bt_double;
	if (!strcmp(name, "cbase"))
		return bt_cbase;
	if (!strcmp(name, "entvars"))
		return bt_entvars;
	if (!strcmp(name, "edict"))
		return bt_edict;
	if (!strcmp(name, "client"))
		return bt_client;
	if (!strcmp(name, "string"))
		return bt_string;
	if (!strcmp(name, "void"))
		return bt_void;
	
	return bt_unknown;
}

size_t getTypeSize(basetype_e type)
{
	switch (type)
	{
	//case bt_unknown:
	/*case bt_int:
	case bt_float:
	case bt_cbase:
	case bt_entvars:
	case bt_edict:
	case bt_client:
	case bt_string:*/
	default:
		return sizeof(int);

	case bt_short:
	case bt_word:
		return sizeof(short);

	case bt_char:
	case bt_byte:
		return sizeof(char);

	case bt_double:
		return sizeof(double);

	//case bt_void:
	}

	return 0;
}

size_t getTypePushSize(basetype_e type)
{
	switch (type)
	{
	case bt_unknown:
	case bt_void:
		return 0;

	case bt_double:
		return sizeof(double);
	}

	return sizeof(int);
}

basetype_e makeSigned(basetype_e type)
{
	switch (type)
	{
	case bt_word:
		type = bt_short;
		break;

	case bt_byte:
		type = bt_char;
		break;

	default: break;
	}
	return type;
}

basetype_e makeUnsigned(basetype_e type)
{
	switch (type)
	{
	case bt_short:
		type = bt_word;
		break;

	case bt_char:
		type = bt_byte;
		break;

	default: break;
	}
	return type;
}

#define CHECK_KEYWORD(x, e) if (!strncmp(c, x, sizeof(x) - 1) && strchr(" \t*&", c[sizeof(x) - 1])) {c += sizeof(x) - 2; e; continue;}

basetype_e getBaseForType(const char* name, bool* ptr)
{
	basetype_e type = bt_unknown;
	size_t ptrlvl = 0;
	bool _signed = false;
	bool _unsigned = false;

	for (const char* c = name; *c; c++)
		if (*c == '*')
			ptrlvl++;

	for (const char* c = name; *c; c++)
	{
		if (*c == ' ' || *c == '\t')
			continue;
		
		CHECK_KEYWORD("const", (void *)NULL)
		CHECK_KEYWORD("class", (void *)NULL)
		CHECK_KEYWORD("struct", (void *)NULL)
		CHECK_KEYWORD("signed", _signed = true)
		CHECK_KEYWORD("unsigned", _unsigned = true)

		for (auto it = g_types.begin(), end = g_types.end(); it != end; it++)
		{
			auto t = *it;

			if (t->pointer && !ptrlvl)
				continue;

			if (strnicmp(c, t->name, t->len))
				continue;

			if (t->ispart)
			{
				c += t->len - 1;
				while (isalpha(c[1])) c++;
			}
			else
			{
				if (!strchr(" \t*&", c[t->len]))
					continue;

				c += t->len - 1;
			}

			if (t->pointer)
				ptrlvl--;
			*ptr = ptrlvl != 0;
			type = t->basetype;

			if (_signed)
				type = makeSigned(type);
			if (_unsigned)
				type = makeUnsigned(type);

			return type;
		}

		if (ptrlvl)
			*ptr = true;

		break;
	}

	return bt_unknown;
}

bool isTypeSigned(basetype_e type)
{
	switch (type)
	{
	case bt_int:
	case bt_short:
	case bt_char:
		return true;
	default:
		;
	}

	return false;
}

void addType(char* name, const char* base)
{
	type_t* t;
	basetype_e b = getBaseType(base);

	if (!b)
	{
		Con_Printf("[RePatcher] error: unknown base type '%s' in config.\n", base);
		return;
	}

	char* ptr = strchr(name, '*');

	if (ptr)
	{
		*ptr = '\0';
		trim(name);
	}

	char* part = strchr(name, '?');

	if (part)
	{
		*part = '\0';
		trim(name);
	}

	for (auto it = g_types.begin(), end = g_types.end(); it != end; it++)
	{
		t = *it;
		if (!strcmp(name, t->name) && (ptr != NULL) == t->pointer && (part != NULL) == t->ispart)
			return; // exist
	}

	t = new type_t;
	strncpy(t->name, name, sizeof t->name - 1);
	t->name[sizeof t->name - 1] = '\0';
	t->len = strlen(name);
	t->pointer = ptr != NULL;
	t->ispart = part != NULL;
	t->basetype = b;
	g_types.push_back(t);
}

void sortTypes()
{
	std::sort(g_types.begin(), g_types.end(), [](type_t* i, type_t* j) { return i->pointer && !j->pointer; });
}

register_e getRegByName(const char* name)
{
	char rname[8];

	if (name[0] == '<')
	{
		strncpy(rname, name + 1, sizeof rname - 1);
		rname[sizeof rname - 1] = '\0';
		char* c = strchr(rname, '>');
		if (c)
			*c = '\0';
		name = rname;
	}

	for (size_t i = 0; i < sizeof g_name2reg / sizeof g_name2reg[0]; i++)
	{
		if (!strcmp(name, g_name2reg[i].reg_name))
			return g_name2reg[i].reg_id;
	}

	return r_unknown;
}

int patchMemory(void* addr, void* patch, size_t size)
{
#if defined _WIN32
	DWORD OldProtection, NewProtection = PAGE_EXECUTE_READWRITE;
	FlushInstructionCache(g_hldsProcess.getHandle(), addr, size);

	if (VirtualProtect(addr, size, NewProtection, &OldProtection))
	{
		memcpy(addr, patch, size);
		return VirtualProtect(addr, size, OldProtection, &NewProtection);
	}
#else
#define Align(addr)	(void *)((unsigned long)(addr) & ~(g_hldsProcess.getPageSize() - 1))

	void* alignedAddress = Align(addr);
	size_t region = g_hldsProcess.getPageSize();

	if (Align(addr + size - 1) != alignedAddress)
		region *= 2;

	if (!mprotect(alignedAddress, region, (PROT_READ | PROT_WRITE | PROT_EXEC)))
	{
		memcpy(addr, patch, size);
		return !mprotect(alignedAddress, region, (PROT_READ | PROT_EXEC));
	}
#endif

	return 0;
}

const char* getPluginName(AMX* amx)
{
	return g_amxxapi.GetAmxScriptName(g_amxxapi.FindAmxScriptByAmx(amx));
}

char* getAmxStringTemp(AMX* amx, cell amx_addr, int* len)
{
	cell* src = (cell *)(amx->base + (size_t)(((AMX_HEADER *)amx->base)->dat + amx_addr));
	char* dest = (char *)(g_tempMemory + g_tempUsed);
	char* start = dest;
	size_t max = sizeof(g_tempMemory) - g_tempUsed;

	while (*src && --max)
		*dest++ = (char)*src++;
	*dest = '\0';

	if (!max)
		errorNoMemory();

	if (len)
		*len = dest - start;

	g_tempUsed += dest - start + 1;
	return start;
}

void setAmxString(cell* amxstring, size_t max, const char* string, AMX* amx)
{
	cell* dest = (cell *)(amx->base + (size_t)(((AMX_HEADER *)amx->base)->dat + amxstring));

	while (*string && max--)
		*dest++ = (cell)*string++;

	*dest = 0;
}

bool isAmxAddr(AMX* amx, cell addr)
{
	return (size_t)addr < (size_t)amx->stk;
}

size_t amxStrlen(cell addr)
{
	cell* s = (cell *)addr;

	while (*s)
		++s;

	return (size_t)s - addr;
}

bool isEntIndex(dword index)
{
	return index >= 0 && index <= (dword)gpGlobals->maxEntities;
}

byte* cellToByte(cell* src, size_t len)
{
	if (g_tempUsed + len > sizeof(g_tempMemory))
		errorNoMemory();

	byte* dst = g_tempMemory + g_tempUsed;
	byte* ret = dst;
	for (size_t i = 0; i < len; i++)
		*dst++ = *src++;

	g_tempUsed += len;
	return ret;
}

word* cellToWord(cell* src, size_t len)
{
	if (g_tempUsed + len * sizeof(word) > sizeof(g_tempMemory))
		errorNoMemory();

	word* dst = (word *)(g_tempMemory + g_tempUsed);
	word* ret = dst;
	for (size_t i = 0; i < len; i++)
		*dst++ = *src++;

	g_tempUsed += len * sizeof(word);
	return ret;
}

dword* cellToDword(cell* src, size_t len)
{
	if (g_tempUsed + len * sizeof(dword) > sizeof(g_tempMemory))
		errorNoMemory();

	dword* dst = (dword *)(g_tempMemory + g_tempUsed);
	dword* ret = dst;
	for (size_t i = 0; i < len; i++)
		*dst++ = *src++;

	g_tempUsed += len * sizeof(dword);
	return ret;
}

double* cellToDouble(cell* src, size_t len)
{
	if (g_tempUsed + len * sizeof(double) > sizeof(g_tempMemory))
		errorNoMemory();

	double* dst = (double *)(g_tempMemory + g_tempUsed);
	double* ret = dst;
	for (size_t i = 0; i < len; i++)
		*dst++ = *(float *)src++;

	g_tempUsed += len * sizeof(double);
	return ret;
}

cell* ConvertToAmxArray(dword src, size_t len, basetype_e type)
{
	if (g_tempUsed + len * sizeof(cell) > sizeof(g_tempMemory))
		errorNoMemory();

	cell* dst = (cell *)(g_tempMemory + g_tempUsed);
	cell* ret = dst;

	switch (type)
	{
	case bt_int:
	case bt_float:
	case bt_cbase:
	case bt_entvars:
	case bt_edict:
	case bt_client:
	case bt_string:
		for (size_t i = 0; i < len; i++, src += sizeof(cell))
			*dst++ = *(cell *)src;
		break;

	case bt_short:
		for (size_t i = 0; i < len; i++, src += sizeof(short))
			*dst++ = *(short *)src;
		break;

	case bt_word:
		for (size_t i = 0; i < len; i++, src += sizeof(word))
			*dst++ = *(word *)src;
		break;

	case bt_char:
		for (size_t i = 0; i < len; i++, src += sizeof(char))
			*dst++ = *(char *)src;
		break;

	case bt_byte:
		for (size_t i = 0; i < len; i++, src += sizeof(byte))
			*dst++ = *(byte *)src;
		break;

	case bt_double:
		for (size_t i = 0; i < len; i++, src += sizeof(double))
		{
			float val = (float)*(double *)src;
			*dst++ = *(cell *)&val;
		}
		break;
	}

	g_tempUsed += len * sizeof(cell);
	return ret;
}

dword ConvertFromAmxArray(cell* src, size_t len, basetype_e type)
{
	dword value;

	switch (getTypeSize(type))
	{
	case sizeof(char):
		value = (dword)cellToByte(src, len);
		break;
	case sizeof(short):
		value = (dword)cellToWord(src, len);
		break;
	case sizeof(int):
		value = (dword)cellToDword(src, len);
		break;
	case sizeof(double):
		value = (dword)cellToDouble(src, len);
		break;
	}

	return value;
}

dword ConvertToAmxType(dword value, basetype_e type)
{
	switch (type)
	{
	//case bt_int:
	//case bt_string:
	case bt_short:
		value = (dword)short(value);
		break;
	case bt_word:
		value &= 0xFFFF;
		break;
	case bt_char:
		value = (dword)char(value);
		break;
	case bt_byte:
		value &= 0xFF;
		break;
	//case bt_float:
	//case bt_double:
	case bt_cbase:
		value = (dword)IndexOfCBase((void *)value);
		break;
	case bt_entvars:
		value = (dword)IndexOfEntvars((entvars_t *)value);
		break;
	case bt_edict:
		value = (dword)IndexOfEdict((edict_t *)value);
		break;
	case bt_client:
		value = (dword)IndexOfClient((client_t *)value);
		break;
	}
	return value;
}