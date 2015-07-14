#include "precompiled.h"
#include "repatcher.h"

enginefuncs_t g_engfuncs;
globalvars_t* gpGlobals;

meta_globals_t* gpMetaGlobals;
gamedll_funcs_t* gpGamedllFuncs;
mutil_funcs_t* mUtil;

DLL_FUNCTIONS* g_pFunctionTable;

plugin_info_t Plugin_info =
{
	META_INTERFACE_VERSION,	// ifvers
	"RePatcher",	// name
	"0.1 beta",	// version
	"2015/07/15",	// date
	"Asmodai",	// author
	"http://www.dedicated-server.ru/",	// url
	"RePatcher",	// logtag, all caps please
	PT_ANYTIME,	// (when) loadable
	PT_NEVER,	// (when) unloadable
};

C_DLLEXPORT int GetEntityAPI2( DLL_FUNCTIONS* pFunctionTable, int* interfaceVersion )
{
	g_pFunctionTable = pFunctionTable;
	pFunctionTable->pfnServerActivate = ServerActivate;
	return 1;
}

C_DLLEXPORT int Meta_Query(char* interfaceVersion, plugin_info_t** plinfo, mutil_funcs_t* pMetaUtilFuncs)
{
	*plinfo = &Plugin_info;
	mUtil = pMetaUtilFuncs;
	return 1;
}

C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME now, META_FUNCTIONS* pFunctionTable, meta_globals_t* pMGlobals, gamedll_funcs_t* pGamedllFuncs)
{
	pFunctionTable->pfnGetEntityAPI2 = (void *)GetEntityAPI2;
	gpMetaGlobals = pMGlobals;
	gpGamedllFuncs = pGamedllFuncs;
	cfg.load();

	if(Repatcher_Init())
	{
		Con_Printf("[RePatcher]: Successfully initialized.\n");
		return TRUE;
	}

	Con_Printf("[RePatcher]: Load failed.\n");
	return FALSE;
}

C_DLLEXPORT int Meta_Detach()
{
	return 1;
}

#if defined _WIN32
#pragma comment(linker, "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8,@1")
#endif

C_DLLEXPORT void WINAPI GiveFnptrsToDll(enginefuncs_t* pengfuncsFromEngine, globalvars_t *pGlobals)
{
	memcpy( &g_engfuncs, pengfuncsFromEngine, sizeof( enginefuncs_t ) );
	gpGlobals = pGlobals;
}