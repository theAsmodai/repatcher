#ifndef REPATCHER_H
#define REPATCHER_H

struct conversiondata_t
{
	edict_t*	edicts;
	dword		clients;
	size_t		client_size;
	size_t		pev_offset;
};

extern conversiondata_t g_conversiondata;
extern DLL_FUNCTIONS* g_pFunctionTable;

bool Repatcher_Init();

void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax);
void RunThink(edict_t* ed);
void PluginsUnloading();

int IndexOfEdict(edict_t* ed);
int IndexOfEntvars(entvars_t* pev);
int IndexOfClient(client_t* cl);
int IndexOfCBase(void* obj);

edict_t* EdictOfIndex(dword index);
entvars_t* EntvarsOfIndex(dword index);
client_t* ClientOfIndex(dword index);
void* CBaseOfIndex(dword index);

CModule* getModuleByName(const char* name);

#ifdef SELF_TEST
extern AMX* g_testPlugin;
extern int g_fwdBeginTest;
extern int ret_orig;
extern int ret_fact;
extern int ret_orig_value;
extern int ret_fact_value;

int Hook_ArgConversion(int a, int b, const char *str, int cl, int pl, float f, int x, float f2);
void Self_Test();
#endif

#endif // REPATCHER_H