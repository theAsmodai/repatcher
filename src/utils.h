#ifndef UTILS_H
#define UTILS_H

enum register_e : byte
{
	r_unknown,

	r_eax,
	r_ebx,
	r_ecx,
	r_edx,
	r_esi,
	r_edi,
	r_ebp,
	r_esp,

	r_st0,

	r_ax,
	r_bx,
	r_cx,
	r_dx,
	// non 32-bit esi, edi, ebp and esp are not needed I think

	r_al,
	r_ah,
	r_bl,
	r_bh,
	r_cl,
	r_ch,
	r_dl,
	r_dh,

	r_xmm0,
	r_xmm1,
	r_xmm2,
	r_xmm3,
	r_xmm4,
	r_xmm5,
	r_xmm6,
	r_xmm7,

	r_st1,
	r_st2,
	r_st3,
	r_st4,
	r_st5,
	r_st6,
	r_st7
};

enum basetype_e : byte
{
	bt_unknown,
	bt_int,
	bt_short,
	bt_word,
	bt_char,
	bt_byte,
	bt_float,
	bt_double,
	bt_cbase,
	bt_entvars,
	bt_edict,
	bt_client,
	bt_string,
	bt_void
};

struct type_t
{
	char name[32];
	word len;
	bool pointer;
	bool ispart;
	basetype_e basetype;
};

#define Con_DPrintf(...) mUtil->pfnLogDeveloper(PLID, __VA_ARGS__);

extern char g_lastError[512];
extern bool g_amxxAttached;
extern bool g_rehldsEngine;

extern byte g_tempMemory[65536];
extern size_t g_tempUsed;
extern char g_gameName[32];

void Con_Printf(const char* fmt, ...);
void Log_Error(AMX* amx, const char* fmt, ...);
void __declspec(noreturn) Sys_Error(const char *error, ...);
void setError(const char* fmt, ...);
int parse(char* line, char** argv, int max_args, char token);
char* trim(char *str);

byte* allocExecutableMemory(size_t size);
void* allocTempMemory(size_t size);

basetype_e getBaseType(const char* name);
basetype_e getBaseForType(const char* name, bool* ptr);
size_t getTypeSize(basetype_e type);
size_t getTypePushSize(basetype_e type);
bool isTypeSigned(basetype_e type);
void addType(char* name, const char* base);
void sortTypes();
register_e getRegByName(const char* name);
int patchMemory(void* addr, void* patch, size_t size);

const char* getPluginName(AMX* amx);
char* getAmxStringTemp(AMX* amx, cell amx_addr, int* len = NULL);
void setAmxString(cell* amxstring, size_t max, const char* string, AMX* amx);
bool isAmxAddr(AMX* amx, cell addr);
size_t amxStrlen(cell addr);
byte* cellToByte(cell* src, size_t len);
word* cellToWord(cell* src, size_t len);
dword* cellToDword(cell* src, size_t len);
double* cellToDouble(cell* src, size_t len);
cell* ConvertToAmxArray(dword src, size_t len, basetype_e type);
dword ConvertFromAmxArray(cell* src, size_t len, basetype_e type);
dword ConvertToAmxType(dword value, basetype_e type);

bool isEntIndex(dword index);

#endif // UTILS_H