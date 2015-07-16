#include "precompiled.h"
#include "chookmanager.h"
#include "repatcher.h"
#include "mod_rehlds_api.h"

conversiondata_t g_conversiondata;
CModule* g_engine;
CModule* g_gamedll;

bool Parse_HldsData()
{
	Con_DPrintf("Server engine is HLDS.\n");

#ifdef _WIN32
	void* addr = g_engine->findStringReference("Client ping times:\n");
	if(!addr)
	{
		Con_Printf("[RePatcher]: Can't find Host_Ping_f function.\n");
		return false;
	}

	// 8B 35 E4 3D 1D 02                             mov     esi, dword [svs.clients]
	// 83 C4 04                                      add     esp, 4
	addr = g_engine->findPattern(addr, 128, "8B 35 ? ? ? ? 83 C4 04");
	if (!addr)
	{
		Con_Printf("[RePatcher]: Can't find svs.clients.\n");
		return false;
	}
	g_conversiondata.clients = *(dword *)((dword)addr + 2);

	// 47                                            inc     edi
	// 81 C6 18 50 00 00                             add     esi, 5018h
	addr = g_engine->findPattern(addr, 128, "47 81 C6 ? ? 00 00");
	if(!addr)
	{
		Con_Printf("[RePatcher]: Can't find sizeof(client_t).\n");
		return false;
	}
	g_conversiondata.client_size = *(dword *)((dword)addr + 3);
#else
	void* addr = g_engine->getSymbolAddress("Host_Ping_f");
	if (!addr)
	{
		Con_Printf("[RePatcher]: Can't find Host_Ping_f function.\n");
		return false;
	}

	// 46                                            inc     esi
	// 81 C3 F4 4E 00 00                             add     ebx, 4EF4h
	addr = g_engine->findPattern(addr, 128, "46 81 C3 ? ? 00 00");
	if (!addr)
	{
		Con_Printf("[RePatcher]: Can't find sizeof(client_t).\n");
		return false;
	}
	g_conversiondata.client_size = *(dword *)((dword)addr + 3);

	addr = g_engine->getSymbolAddress("svs");
	if (!addr)
	{
		Con_Printf("[RePatcher]: Can't find svs.\n");
		return false;
	}
	g_conversiondata.clients = *(dword *)((dword)addr + 4);
#endif
	return true;
}

bool Repatcher_Init()
{
	g_rehldsEngine = RehldsApi_Init();

#ifdef _WIN32
	g_engine = g_hldsProcess.getModule("swds.dll");
	g_gamedll = g_hldsProcess.getModule("mp.dll");
#else
	g_gamedll = g_hldsProcess.getModule("cs.so");
	if (!g_gamedll)
		g_gamedll = g_hldsProcess.getModule("cs_i386.so");

	g_engine = g_hldsProcess.getModule("engine_i486.so");
	if (!g_engine)
		g_engine = g_hldsProcess.getModule("engine_i686.so");
#endif

	if (!g_engine || !g_gamedll)
	{
		Con_Printf("[RePatcher]: Can't locate engine and gamedll modules.\n");
		return false;
	}

	if (g_rehldsEngine)
	{
		Con_Printf("[RePatcher]: Server engine is ReHLDS.\n");

		g_conversiondata.clients = (dword)g_RehldsSvs->GetClient_t(0);
		g_conversiondata.client_size = (dword)g_RehldsSvs->GetClient_t(1) - g_conversiondata.clients;
		return true;
	}
	
	return Parse_HldsData();
}

void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
	g_conversiondata.edicts = pEdictList;

	edict_t* ent = g_engfuncs.pfnCreateEntity();
	mUtil->pfnCallGameEntity(PLID, "player", &ent->v);

	// scan for pev
	for (size_t i = 0; i < 256; i += 4)
	{
		if (&ent->v == (entvars_t *)*(dword *)((dword)ent->pvPrivateData + i))
		{
			g_conversiondata.pev_offset = i;
			break;
		}
	}

	g_engfuncs.pfnRemoveEntity(ent);
	g_pFunctionTable->pfnThink = RunThink;

#ifdef SELF_TEST
	static bool tested = false;
	if (!tested)
	{
		tested = true;
		Self_Test();
	}
#endif

	RETURN_META(MRES_IGNORED);
}

int DispatchSpawn(edict_t* ed)
{
	g_pFunctionTable->pfnSpawn = NULL;
	Con_DPrintf("Reopening module handles.");
	g_hldsProcess.reopenHandles();
	RETURN_META_VALUE(MRES_IGNORED, TRUE);
}

void RunThink(edict_t* ed)
{
	g_pFunctionTable->pfnThink = NULL;
	Con_DPrintf("Freeing opened handles.");
	g_hldsProcess.freeOpenedHandles();
	RETURN_META(MRES_IGNORED);
}

void PluginsUnloading()
{
	Con_DPrintf("Removing amxx hooks.");
	g_hookManager.removeAmxHooks();
	g_pFunctionTable->pfnSpawn = DispatchSpawn;
}

void StartFrame()
{
	g_pFunctionTable->pfnStartFrame = NULL;
	RETURN_META(MRES_IGNORED);
}

int IndexOfEdict(edict_t* ed)
{
	return ed ? int(ed - g_conversiondata.edicts) : -1;
}

int IndexOfEntvars(entvars_t* pev)
{
	return pev ? IndexOfEdict(pev->pContainingEntity) : -1;
}

int IndexOfClient(client_t* cl)
{
	return cl ? ((dword(cl) - g_conversiondata.clients) / g_conversiondata.client_size) : -1;
}

int IndexOfCBase(void* obj)
{
	return obj ? int((*(entvars_t **)((dword)obj + g_conversiondata.pev_offset))->pContainingEntity - g_conversiondata.edicts) : -1;
}

edict_t* EdictOfIndex(dword index)
{
	return g_conversiondata.edicts + index;
}

entvars_t* EntvarsOfIndex(dword index)
{
	return &(g_conversiondata.edicts + index)->v;
}

client_t* ClientOfIndex(dword index)
{
	return (client_t *)(g_conversiondata.clients + g_conversiondata.client_size * index);
}

void* CBaseOfIndex(dword index)
{
	return (g_conversiondata.edicts + index)->pvPrivateData;
}

CModule* getModuleByName(const char* name)
{
	if (!strcmp(name, "engine"))
		return g_engine;

	if (!strcmp(name, "mod") || !strcmp(name, "gamedll"))
		return g_engine;

	return g_hldsProcess.getModule(name);
}

#ifdef SELF_TEST
AMX* g_testPlugin;
int g_fwdBeginTest;
int g_fwdChangeRetHook;
int g_fwdChangeArgHook;
CModule* g_repatcher;
int g_called_func = 0;
hookhandle_t* g_hpre;
hookhandle_t* g_hpost;
bool check_order;
int call_order[5];
int call_order_id;
int pre_return;
int post_return;
int ret_orig;
int ret_fact;
int ret_orig_value;
int ret_fact_value;
int testarg_id;
char* defarg = "-2";

enum test_e
{
	test_convertation,
	test_changestate,
	test_changestate2,
	test_supercede,
	test_supercede2,
	test_remove,
	test_return,
	test_args,
	test_finish
};

enum
{
	chook_pre = 1,
	chook_post = 2,
	amxhook_pre = 4,
	amxhook_post = 8,
	original_func = 16
};

void Amx_BeginTest(int id)
{
	g_amxxapi.ExecuteForward(g_fwdBeginTest, id);
}

int Hook_ArgConvertation(int a, int b, const char *str, int cl, int pl, float f, int x, float f2)
{
	if (a != 36 || b != 321 || strcmp(str, "tratata") || cl != 2 || pl != 37 || fabs(f - 98765.4) > 0.01 || x != 12345 || fabs(f2 - 67890.1) > 0.01)
		Sys_Error("%s: unexpected args (%i,%i,%s,%i,%i,%.2f,%i,%.2f) expected(36,321,tratata,2,37,98765.4,12345,67890.1)", __FUNCTION__, a, b, str, cl, pl, f, x, f2);

	__asm
	{
		xor eax, eax;
		xor ecx, ecx;
		pxor xmm4, xmm4;
		fldpi;
	}

	int func;

	if (g_currentHandler->amx)
	{
		if (g_currentHandler->pre)
			func = amxhook_pre;
		else
			func = amxhook_post;
	}
	else
	{
		if (g_currentHandler->pre)
			func = chook_pre;
		else
			func = chook_post;
	}

	if (check_order)
		call_order[call_order_id++] = func;

	//Con_Printf("called %i\n", func);
	g_called_func |= func;
	return g_currentHandler->pre ? pre_return : post_return;
}

void Func_ArgConvertation(edict_t* a, const char *str, void* pl, float f2)
{
	int b;
	client_t* client;
	float f;
	int x;

	__asm
	{
		movd dword ptr [x], xmm4;
		fst dword ptr [f];
		mov dword ptr [client], ecx;
		mov dword ptr [b], eax;
	}

	if (check_order)
		call_order[call_order_id++] = original_func;

	g_called_func |= original_func;

#ifdef NDEBUG
	if (a != EdictOfIndex(36) || b != 321 || strcmp(str, "tratata") || client != ClientOfIndex(2) || pl != EdictOfIndex(37)->pvPrivateData || fabs(f - 98765.4) > 0.01 || x != 12345 || fabs(f2 - 67890.1) > 0.01)
		Sys_Error("%s: unexpected args (%p,%i,%s,%p,%p,%.2f,%i,%.2f) expected(%p,321,tratata,%p,%p,98765.40,12345,67890.10)", __FUNCTION__, a, b, str, client, pl, f, x, f2, EdictOfIndex(36), ClientOfIndex(2), EdictOfIndex(37)->pvPrivateData);
#else
	if (a != EdictOfIndex(36) || strcmp(str, "tratata") || pl != EdictOfIndex(37)->pvPrivateData || fabs(f - 98765.4) > 0.01 || x != 12345 || fabs(f2 - 67890.1) > 0.01)
		Sys_Error("%s: unexpected args (%p,%s,%p,%.2f,%i,%.2f) expected(%p,tratata,%p,98765.40,12345,67890.10)", __FUNCTION__, a, str, pl, f, x, f2, EdictOfIndex(36), EdictOfIndex(37)->pvPrivateData);
#endif
}

void Call_ArgConvertation()
{
	edict_t* a = EdictOfIndex(36);
	int b = 321;
	const char *string = "tratata";
	client_t* client = ClientOfIndex(2);
	void* pl = EdictOfIndex(37)->pvPrivateData;
	float f = 98765.40f;
	int x = 12345;
	float f2 = 67890.10f;
	
	__asm
	{
		push dword ptr [f2];
		movd xmm4, dword ptr [x];
		fld dword ptr [f];
		push dword ptr [pl];
		mov ecx, dword ptr [client];
		push dword ptr [string];
		mov eax, dword ptr [b];
		push dword ptr [a];
		call Func_ArgConvertation;
		add esp, 4 * 4;
	}
}

void Test_ArgConvertation()
{
	void* func = g_repatcher->getSymbolAddress("Func_ArgConvertation");

	if (!func)
		Sys_Error("%s: can't find Func_ArgConvertation function\n", __FUNCTION__);

	const char* desc = "void Func_ArgConvertation(edict_t* a, int b@<eax>, const char *str, client_t* cl@<ecx>, CBaseMonster* pl, float f@st0, int x@xmm4, float f2)";
	g_hpre = g_hookManager.createHook(func, desc, true, (void *)Hook_ArgConvertation);
	g_hpost = g_hookManager.createHook(func, desc, false, (void *)Hook_ArgConvertation);

	if (!g_hpre || !g_hpost)
		Sys_Error("%s: can't create hooks.\n", __FUNCTION__);

	Amx_BeginTest(test_convertation);
	check_order = true;
	Call_ArgConvertation();
	check_order = false;

	int true_order[5] = {chook_pre, amxhook_pre, original_func, chook_post, amxhook_post};
	if (memcmp(call_order, true_order, sizeof true_order))
		Sys_Error("%s: invalid call order [%i|%i|%i|%i|%i], expected [%i|%i|%i|%i|%i].\n", __FUNCTION__, call_order[0], call_order[1], call_order[2], call_order[3], call_order[4], true_order[0], true_order[1], true_order[2], true_order[3], true_order[4]);
	if (g_called_func != (chook_pre|chook_post|amxhook_pre|amxhook_post|original_func))
		Sys_Error("%s: not all functions are called %i.\n", __FUNCTION__, g_called_func);
	Con_Printf("[RePatcher]: Test_ArgConvertation passed.\n");
}

void Test_ChangeState()
{
	g_hpre->enabled = false;
	Amx_BeginTest(test_changestate);

	g_called_func = 0;
	Call_ArgConvertation();
	if (g_called_func != (chook_post|amxhook_pre|original_func))
		Sys_Error("%s: called [%i|%i|%i|%i|%i], expected [0|1|1|1|0].\n", __FUNCTION__, g_called_func & chook_pre, ( g_called_func & amxhook_pre ) ? 1 : 0, ( g_called_func & original_func ) ? 1 : 0, ( g_called_func & chook_post ) ? 1 : 0, ( g_called_func & amxhook_post ) ? 1 : 0);
	
	g_hpost->enabled = false;
	Amx_BeginTest(test_changestate2);
	g_called_func = 0;
	Call_ArgConvertation();
	if (g_called_func != (original_func|amxhook_post))
		Sys_Error("%s: called [%i|%i|%i|%i|%i], expected [0|0|1|0|1].\n", __FUNCTION__, g_called_func & chook_pre, ( g_called_func & amxhook_pre ) ? 1 : 0, ( g_called_func & original_func ) ? 1 : 0, ( g_called_func & chook_post ) ? 1 : 0, ( g_called_func & amxhook_post ) ? 1 : 0);
	
	Con_Printf("[RePatcher]: Test_ChangeState passed.\n");
}

void Test_Supercede()
{
	g_hpre->enabled = true;
	g_hpost->enabled = true;

	Amx_BeginTest(test_supercede);
	g_called_func = 0;
	Call_ArgConvertation();
	if (g_called_func != (chook_pre|amxhook_pre|chook_post|amxhook_post))
		Sys_Error("%s: called [%i|%i|%i|%i|%i], expected [1|1|0|1|1].\n", __FUNCTION__, g_called_func & chook_pre, ( g_called_func & amxhook_pre ) ? 1 : 0, ( g_called_func & original_func ) ? 1 : 0, ( g_called_func & chook_post ) ? 1 : 0, ( g_called_func & amxhook_post ) ? 1 : 0);

	Amx_BeginTest(test_supercede2);
	pre_return = -1;
	g_called_func = 0;
	Call_ArgConvertation();
	if (g_called_func != (chook_pre))
		Sys_Error("%s: called [%i|%i|%i|%i|%i], expected [1|0|0|0|0].\n", __FUNCTION__, g_called_func & chook_pre, ( g_called_func & amxhook_pre ) ? 1 : 0, ( g_called_func & original_func ) ? 1 : 0, ( g_called_func & chook_post ) ? 1 : 0, ( g_called_func & amxhook_post ) ? 1 : 0);

	pre_return = 0;
	post_return = -1;
	g_called_func = 0;
	Call_ArgConvertation();
	if (g_called_func != (chook_pre|chook_post|amxhook_pre|amxhook_post|original_func))
		Sys_Error("%s: called [%i|%i|%i|%i|%i], expected [1|1|1|1|1].\n", __FUNCTION__, g_called_func & chook_pre, ( g_called_func & amxhook_pre ) ? 1 : 0, ( g_called_func & original_func ) ? 1 : 0, ( g_called_func & chook_post ) ? 1 : 0, ( g_called_func & amxhook_post ) ? 1 : 0);

	Con_Printf("[RePatcher]: Test_Supercede passed.\n");
}

void Test_Remove()
{
	g_hookManager.removeHook(g_hpre);
	Amx_BeginTest(test_remove);
	g_called_func = 0;
	Call_ArgConvertation();
	if (g_called_func != (original_func|chook_post))
		Sys_Error("%s: called [%i|%i|%i|%i|%i], expected [0|0|1|1|0].\n", __FUNCTION__, g_called_func & chook_pre, ( g_called_func & amxhook_pre ) ? 1 : 0, ( g_called_func & original_func ) ? 1 : 0, ( g_called_func & chook_post ) ? 1 : 0, ( g_called_func & amxhook_post ) ? 1 : 0);

	g_hookManager.removeHook(g_hpost);
	g_called_func = 0;
	Call_ArgConvertation();
	if (g_called_func != original_func)
		Sys_Error("%s: called [%i|%i|%i|%i|%i], expected [0|0|1|0|0].\n", __FUNCTION__, g_called_func & chook_pre, ( g_called_func & amxhook_pre ) ? 1 : 0, ( g_called_func & original_func ) ? 1 : 0, ( g_called_func & chook_post ) ? 1 : 0, ( g_called_func & amxhook_post ) ? 1 : 0);

	Con_Printf("[RePatcher]: Test_Remove passed.\n");
}

int NOINLINE Func_Return()
{
	//Con_Printf("Default return %i\n", (int)&g_engfuncs.pfnPEntityOfEntIndex(11)->v);
	g_engfuncs.pfnPEntityOfEntIndex(11)->v.pContainingEntity = g_engfuncs.pfnPEntityOfEntIndex(11);
	return (int)&g_engfuncs.pfnPEntityOfEntIndex(11)->v;
}

void Test_Return()
{
	Amx_BeginTest(test_return);

	int true_value[10] = {(int)&g_engfuncs.pfnPEntityOfEntIndex(11)->v, 12345, -12345, 0x11, -123, 0x12, 0, (int)g_engfuncs.pfnPEntityOfEntIndex(7), (int)g_engfuncs.pfnPEntityOfEntIndex(37)->pvPrivateData, g_RehldsSvs ? (int)g_RehldsSvs->GetClient_t(9) : (int)ClientOfIndex(9)};
	int call_value[11]; // TODO: 12. implement array check
	//short arr[3] = {0x2345, -2, 19};

	for (size_t i = 0; i < ARRAYSIZE(call_value); i++)
	{
		call_value[i] = Func_Return();

		if (!ret_orig)
			Sys_Error("%s: %i getted incorrect original return value: %i", __FUNCTION__, i, ret_orig_value);
		if (!ret_fact)
			Sys_Error("%s: %i getted incorrect factual return value: %i", __FUNCTION__, i, ret_fact_value);
		ret_orig = ret_fact = 0;

		g_amxxapi.ExecuteForward(g_fwdChangeRetHook);
	}

	for (size_t i = 0; i < ARRAYSIZE(true_value); i++)
	{
		if (i == 6)
		{
			if (fabs(*(float *)&true_value[6] - *(float *)&call_value[6]) > 0.01)
				Sys_Error("%s: float return not equal. true: %.2f; call: %.2f", __FUNCTION__, true_value[6], call_value[6]);
		}
		else if (true_value[i] != call_value[i])
			Sys_Error("%s: returns in %i not equal. true: %i; call: %i", __FUNCTION__, i, true_value[i], call_value[i]);
	}

	if (strcmp((char *)call_value[10], "hehehe"))
		Sys_Error("%s: returns in 10 not equal. true: 'hehehe'; call: '%s'", __FUNCTION__, call_value[10]);

	//if (memcmp((void *)call_value[11], arr, 6))
		//Sys_Error("%s: returns in 11 not equal. true: '0x2345, -2, 19'; call: '%p, %i, %i'", __FUNCTION__, ((short *)call_value[11])[0], ((short *)call_value[11])[1], ((short *)call_value[11])[2]);

	Con_Printf("[RePatcher]: Test_Return passed.\n");
}

void Hook_Arg(int arg)
{
	switch (testarg_id++)
	{
	case 0:
		if (arg != 12345)
			Sys_Error("%s: invalid %i arg in hook exp 12345 get %i", __FUNCTION__, testarg_id - 1, arg);
		break;
	case 1:
		if (arg != 0x1234)
			Sys_Error("%s: invalid %i arg in hook exp 0x1234 get %x", __FUNCTION__, testarg_id - 1, arg);
		break;
	case 2:
		if (arg != 4)
			Sys_Error("%s: invalid %i arg in hook exp 4 get %x", __FUNCTION__, testarg_id - 1, arg);
		break;
	case 3:
		if (strcmp((char *)arg, "lalala"))
			Sys_Error("%s: invalid %i arg in hook exp \"lalala\" get %s", __FUNCTION__, testarg_id - 1, (char *)arg);
		break;
	case 4:
		if (arg != (int)defarg)
			Sys_Error("%s: invalid %i arg in hook exp -2 get %s", __FUNCTION__, testarg_id - 1, arg);
		break;
	}
}

void Func_Arg(int arg)
{
	switch (testarg_id)
	{
	case 0:
		if (arg != 12345)
			Sys_Error("%s: invalid %i arg exp 12345 get %i", __FUNCTION__, testarg_id, arg);
		break;
	case 1:
		if (arg != 0x1234)
			Sys_Error("%s: invalid %i arg exp 0x1234 get %x", __FUNCTION__, testarg_id, arg);
		break;
	case 2:
		if ((client_t *)arg != (g_RehldsSvs ? g_RehldsSvs->GetClient_t(4) : ClientOfIndex(4)))
			Sys_Error("%s: invalid %i arg exp 4 get %x", __FUNCTION__, testarg_id, arg);
		break;
	case 3:
		if (strcmp((char *)arg, "lalala"))
			Sys_Error("%s: invalid %i arg exp \"lalala\" get %s", __FUNCTION__, testarg_id, (char *)arg);
		break;
	case 4:
		if (arg != (int)defarg)
			Sys_Error("%s: invalid %i arg exp -2 get %s", __FUNCTION__, testarg_id, arg);
		break;
	}
}

void Test_Args()
{
	const char* desc[] =
	{
		"void func(int a@<eax>)",
		"void func(word a@<eax>)",
		"void func(client_t* a@<eax>)",
		"void func(char* a@<eax>)",
		"void func(short a@<eax>)"
	};

	Amx_BeginTest(test_args);
	void* func = g_repatcher->getSymbolAddress("Func_Arg");

	if (!func)
		Sys_Error("%s: can't find Func_Arg function\n", __FUNCTION__);

	for (size_t i = 0; i < 5; i++)
	{
		auto phook = g_hookManager.createHook(func, desc[i], false, (void *)Hook_Arg);

		__asm
		{
			mov eax, dword ptr [defarg];
			push eax;
			call Func_Arg;
			add esp, 4;
		}

		g_amxxapi.ExecuteForward(g_fwdChangeArgHook);
		g_hookManager.removeHook(phook);
	}

	if (testarg_id != 5)
		Sys_Error("%s: Not all hooks called", __FUNCTION__);

	Amx_BeginTest(test_finish);
	Con_Printf("[RePatcher]: Test_Args passed.\n");
}

void Self_Test()
{
#ifdef _WIN32
	g_repatcher = g_hldsProcess.getModule("repatcher_amxx.dll");
#else
	g_repatcher = g_hldsProcess.getModule("repatcher_amxx_i386.so");
#endif

	if (!g_repatcher)
		Sys_Error("%s: can't load repatcher module - %s\n", __FUNCTION__, g_lastError);
	if (!g_repatcher->getSymbolAddress("getAmxStringTemp"))
		Sys_Error("%s: can't find symbol '%s'\n", __FUNCTION__, "getAmxStringTemp");
	if (!g_repatcher->getSymbolAddress("CHookHandlerJit::amx_Push"))
		Sys_Error("%s: can't find symbol '%s'\n", __FUNCTION__, "CHookHandlerJit::amx_Push");
	if (!g_testPlugin)
		Sys_Error("%s: can't find amxx test plugin\n", __FUNCTION__);

	g_fwdBeginTest = g_amxxapi.RegisterSPForwardByName(g_testPlugin, "rp_begin_test", FP_CELL, FP_DONE);
	g_fwdChangeRetHook = g_amxxapi.RegisterSPForwardByName(g_testPlugin, "change_rethook", FP_DONE);
	g_fwdChangeArgHook = g_amxxapi.RegisterSPForwardByName(g_testPlugin, "change_arghook", FP_DONE);

	Test_ArgConvertation();
	Test_ChangeState();
	Test_Supercede();
	Test_Remove();
	Test_Return();
	Test_Args();

	Con_Printf("[RePatcher]: All tests passed.\n");
}

void Test_NoReturnValue(hookhandle_t* handle)
{
}
#endif // SELF_TEST