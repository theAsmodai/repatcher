#include "precompiled.h"
#include "chookmanager.h"
#include "repatcher.h"
#include "natives.h"

#define PARAMS_COUNT			(params[0] / sizeof(cell))
#define PARAMS_REQUIRE(x)		if(params[0] != x*sizeof(cell)) {MF_LogError(amx, AMX_ERR_NATIVE, "Invalid parameters count in %s", __FUNCTION__);return NULL;}

// rp_find_library(const name[])
static cell AMX_NATIVE_CALL rp_find_library(AMX *amx, cell *params)
{
	int len;
	return (cell)getModuleByName(g_amxxapi.GetAmxString(amx, params[1], 0, &len));
}

// rp_get_exported(lib, const name[])
static cell AMX_NATIVE_CALL rp_get_exported(AMX *amx, cell *params)
{
	CModule* mod = (CModule *)params[1];
	int len;
	const char* name = g_amxxapi.GetAmxString(amx, params[2], 0, &len);
	
	if (!mod)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid library handle provided in rp_get_exported(0, \"%s\")", name);
		return NULL;
	}

	return (cell)mod->getExportedAddress(name);
}

// rp_get_symbol(lib, const symbol[])
static cell AMX_NATIVE_CALL rp_get_symbol(AMX *amx, cell *params)
{
	CModule* mod = (CModule *)params[1];
	int len;
	const char* symbol = g_amxxapi.GetAmxString(amx, params[2], 0, &len);

	if (!mod)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid library handle provided in rp_get_symbol(0, \"%s\")", symbol);
		return NULL;
	}

	return (cell)mod->getSymbolAddress(symbol);
}

// rp_find_signature(lib, const symbol[])
static cell AMX_NATIVE_CALL rp_find_signature(AMX *amx, cell *params)
{
	CModule* mod = (CModule *)params[1];
	int len;
	const char* sig = g_amxxapi.GetAmxString(amx, params[2], 0, &len);

	if (!mod)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid library handle provided in rp_find_signature(0, \"%s\")", sig);
		return NULL;
	}

	return (cell)mod->findPattern(sig, PARAMS_COUNT == 3 && params[3]);
}

// rp_add_hook(address, const description[], const handler[], bool:pre, flags)
static cell AMX_NATIVE_CALL rp_add_hook(AMX *amx, cell *params)
{
	enum args_e
	{
		arg_count,
		arg_address,
		arg_description,
		arg_handler,
		arg_pre,
		arg_flags
	};

	int funcid, len;
	void* addr = (void *)params[arg_address];
	const char* desc = g_amxxapi.GetAmxString(amx, params[arg_description], 0, &len);
	const char* funcname = g_amxxapi.GetAmxString(amx, params[arg_handler], 1, &len);

	if (!addr)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid library handle provided in rp_add_hook(0, \"%s\", \"%s\", ...)", desc, funcname);
		return NULL;
	}

	if (g_amxxapi.amx_FindPublic(amx, funcname, &funcid) != AMX_ERR_NONE)
	{
		setError("Public function \"%s\" not found", funcname);
		MF_LogError(amx, AMX_ERR_NATIVE, "%s", g_lastError);
		return NULL;
	}

	int fwdid = g_amxxapi.RegisterSPForward(amx, funcid, FP_DONE);
	return (cell)g_hookManager.createHook(addr, desc, params[arg_pre] != 0, amx, fwdid, params[arg_flags]);
}

// rp_remove_hook(Hook:handle)
static cell AMX_NATIVE_CALL rp_remove_hook(AMX *amx, cell *params)
{
	hookhandle_t** ptr = (hookhandle_t **)g_amxxapi.GetAmxAddr(amx, params[1]);
	hookhandle_t* handle = *ptr;
	*ptr = NULL;

	if (!handle)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid hook handle provided.");
		return NULL;
	}
	return g_hookManager.removeHook(handle) ? 1 : 0;
}

// rp_set_hook_state(Hook:handle, bool:active)
static cell AMX_NATIVE_CALL rp_set_hook_state(AMX *amx, cell *params)
{
	hookhandle_t* handle = (hookhandle_t *)params[1];
	if (!handle)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid hook handle provided.");
		return NULL;
	}
	handle->enabled = params[2] != 0;
	return 1;
}

// {_,Float}:rp_get_original_return(...)
static cell AMX_NATIVE_CALL rp_get_original_return(AMX *amx, cell *params)
{
	if (PARAMS_COUNT == 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s called without destination parameters.", __FUNCTION__);
		return 0;
	}

	if (!g_currentHandler)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Trying get return value without active hook.");
		return 0;
	}

	if (g_currentHandler->pre)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Trying get original return value in pre forward.");
		return 0;
	}

	auto func = &g_currentHandler->func;
	dword value;
	double fvalue;
	bool success;

	switch (func->getReturnRegister())
	{
	case r_eax:
		success = g_hookManager.getOriginalReturn(&value);
		break;
	case r_st0:
		success = g_hookManager.getOriginalReturn(&fvalue);
		*(float *)&value = (float)fvalue;
		break;
	default:
		setError("Function without return type.");
		return 0; // no return
	}

	if (!success)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s", g_lastError);
		return 0;
	}

	if (PARAMS_COUNT == 2)
	{
		if (func->getReturnType() != bt_string)
		{
			MF_LogError(amx, AMX_ERR_NATIVE, "Return value isn't string.");
			return 0;
		}

		g_amxxapi.SetAmxString(amx, params[1], (char *)value, params[2]);
		return value;
	}

	cell* addr = g_amxxapi.GetAmxAddr(amx, params[1]);
	*addr = ConvertToAmxType(value, func->getReturnType());
	return 1;
}

// {_,Float}:rp_get_original_return(...)
static cell AMX_NATIVE_CALL rp_get_return(AMX *amx, cell *params)
{
	if (PARAMS_COUNT == 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s called without destination parameters.", __FUNCTION__);
		return 0;
	}

	if (!g_currentHandler)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Trying get return value without active hook.");
		return 0;
	}

	if (g_currentHandler->pre)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Trying get original return value in pre forward.");
		return 0;
	}

	auto func = &g_currentHandler->func;
	dword value;
	double fvalue;
	bool success;

	switch (func->getReturnRegister())
	{
	case r_eax:
		success = g_hookManager.getReturn(&value);
		break;
	case r_st0:
		success = g_hookManager.getReturn(&fvalue);
		*(float *)&value = (float)fvalue;
		break;
	default:
		setError("Function without return type.");
		return 0; // no return
	}

	if (!success)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s", g_lastError);
		return 0;
	}

	if (PARAMS_COUNT == 2)
	{
		if (func->getReturnType() != bt_string)
		{
			MF_LogError(amx, AMX_ERR_NATIVE, "Return value isn't string.");
			return 0;
		}

		// TODO: implement getting of array via ConvertToAmxArray

		g_amxxapi.SetAmxString(amx, params[1], (char *)value, params[2]);
		return value;
	}

	cell* addr = g_amxxapi.GetAmxAddr(amx, params[1]);
	*addr = ConvertToAmxType(value, func->getReturnType());
	return 1;
}

// rp_set_return(...)
static cell AMX_NATIVE_CALL rp_set_return(AMX *amx, cell *params)
{
	if (PARAMS_COUNT == 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s called without source parameters.", __FUNCTION__);
		return 0;
	}

	if (!g_currentHandler)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Trying get return value without active hook.");
		return 0;
	}

	auto func = &g_currentHandler->func;

	if (PARAMS_COUNT == 2) // array
	{
		cell* src = (cell *)(amx->base + (size_t)(((AMX_HEADER *)amx->base)->dat + params[1]));
		dword value = ConvertFromAmxArray(src, *g_amxxapi.GetAmxAddr(amx, params[2]), func->getReturnType());
		return g_hookManager.setReturnValue(value) ? 1 : 0;
	}

	dword value = *g_amxxapi.GetAmxAddr(amx, params[1]);
	switch (func->getReturnType())
	{
	//cse bt_int:
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
	case bt_cbase:
		value = isEntIndex(value) ? (dword)CBaseOfIndex(value) : value;
		break;
	case bt_entvars:
		value = isEntIndex(value) ? (dword)EntvarsOfIndex(value) : value;
		break;
	case bt_edict:
		value = isEntIndex(value) ? (dword)EdictOfIndex(value) : value;
		break;
	case bt_client:
		value = (/*(int)value >= 0 &&*/ value <= (dword)gpGlobals->maxClients ) ? (dword)ClientOfIndex(value) : value;
		break;
	case bt_string:
		value = (dword)getAmxStringTemp(amx, params[1]);
		break;

	case bt_float:
	case bt_double:
		return g_hookManager.setReturnValue((double)*(float *)&value);

	case bt_unknown:
	case bt_void:
		setError("Function without return type.");
		return 0; // no return
	}

	return g_hookManager.setReturnValue(value) ? 1 : 0;
}

// rp_set_raw_return(aby:value)
static cell AMX_NATIVE_CALL rp_set_raw_return(AMX *amx, cell *params)
{
	if (PARAMS_COUNT == 0)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s called without source parameters.", __FUNCTION__);
		return 0;
	}

	if (!g_currentHandler)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Trying get return value without active hook.");
		return 0;
	}

	auto func = &g_currentHandler->func;
	dword value = params[1];

	switch (func->getReturnType())
	{
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

	case bt_float:
	case bt_double:
		return g_hookManager.setReturnValue((double)*(float *)&value);

	case bt_unknown:
	case bt_void:
		setError("Function without return type.");
		return 0; // no return
	}

	return g_hookManager.setReturnValue(value);
}

// rp_set_arg(number, ...)
static cell AMX_NATIVE_CALL rp_set_arg(AMX *amx, cell *params)
{
	if (PARAMS_COUNT < 2)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "%s called without source parameters.", __FUNCTION__);
		return 0;
	}

	if (!g_currentHandler)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Trying set arg without active hook.");
		return 0;
	}

	auto func = &g_currentHandler->func;

	if (func->getArgsCount() < (size_t)params[1])
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Can't set %i arg, function has only %i arguments.", params[1], func->getArgsCount());
		return 0;
	}

	auto arg = func->getArgs() + params[1] - 1;
	dword value;

	if (PARAMS_COUNT == 3) // array
	{
		if (arg->count <= 1)
		{
			MF_LogError(amx, AMX_ERR_NATIVE, "Argument %i is not array.", params[1]);
			return 0;
		}

		cell* src = (cell *)(amx->base + (size_t)(((AMX_HEADER *)amx->base)->dat + params[2]));
		value = ConvertFromAmxArray(src, params[3], func->getReturnType());
	}
	else
	{
		value = *g_amxxapi.GetAmxAddr(amx, params[2]);

		switch (arg->type)
		{
		//cse bt_int:
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
		case bt_cbase:
			value = isEntIndex(value) ? (dword)CBaseOfIndex(value) : value;
			break;
		case bt_entvars:
			value = isEntIndex(value) ? (dword)EntvarsOfIndex(value) : value;
			break;
		case bt_edict:
			value = isEntIndex(value) ? (dword)EdictOfIndex(value) : value;
			break;
		case bt_client:
			value = (value <= (dword)gpGlobals->maxClients ) ? (dword)ClientOfIndex(value) : value;
			break;
		case bt_string:
			value = (dword)getAmxStringTemp(amx, params[2]);
			break;

		case bt_float:
		case bt_double:
			return g_hookManager.setArg(params[1], (double)*(float *)&value);

		case bt_unknown:
		case bt_void:
			setError("Invalid argument type.");
			return 0;
		}
	}

	return g_hookManager.setArg(params[1], value);
}

// rp_create_gate(const description[], const handler[])
static cell AMX_NATIVE_CALL rp_create_gate(AMX *amx, cell *params)
{
	void* addr = (void *)params[1];

	if (!addr)
	{
		MF_LogError(amx, AMX_ERR_NATIVE, "Invalid library handle provided in rp_find_signature(0, \"%s\")", params[2]);
		return NULL;
	}

	int funcid;

	if (g_amxxapi.amx_FindPublic(amx, (char *)params[3], &funcid) != AMX_ERR_NONE)
	{
		setError("Public function \"%s\" not found", params[3]);
		MF_LogError(amx, AMX_ERR_NATIVE, "%s", g_lastError);
		return NULL;
	}

	return 1; // TODO
}

// rp_get_error(error[], maxlen)
static cell AMX_NATIVE_CALL rp_get_error(AMX *amx, cell *params)
{
	return g_amxxapi.SetAmxString(amx, params[1], g_lastError, params[2]);
}

// Self_Test
#ifdef SELF_TEST
static cell AMX_NATIVE_CALL rp_i_am_here(AMX *amx, cell *params)
{
	g_testPlugin = amx;
	return 1;
}

static cell AMX_NATIVE_CALL rp_convertation(AMX *amx, cell *params)
{
	int len;
	Hook_ArgConvertation(params[1], params[2], g_amxxapi.GetAmxString(amx, params[3], 0, &len), params[4], params[5], *(float *)&params[6], params[7], *(float *)&params[8]);
	return 1;
}

static cell AMX_NATIVE_CALL rp_retcheck(AMX *amx, cell *params)
{
	if(params[1])
	{
		ret_orig = params[2];
		ret_orig_value = params[3];
	}
	else
	{
		ret_fact = params[2];
		ret_fact_value = params[3];
	}
	return 1;
}
#endif

static AMX_NATIVE_INFO RePatcherNatives[] =
{
	{"rp_find_library",			rp_find_library},
	{"rp_get_exported",			rp_get_exported},
	{"rp_get_symbol",			rp_get_symbol},
	{"rp_find_signature",		rp_find_signature},
	{"rp_add_hook",				rp_add_hook},
	{"rp_remove_hook",			rp_remove_hook},
	{"rp_set_hook_state",		rp_set_hook_state},
	{"rp_get_original_return",	rp_get_original_return},
	{"rp_get_return",			rp_get_return},
	{"rp_set_return",			rp_set_return},
	{"rp_set_raw_return",		rp_set_raw_return},
	{"rp_set_arg",				rp_set_arg},
	{"rp_get_error",			rp_get_error},

#ifdef SELF_TEST
	{"rp_i_am_here",			rp_i_am_here},
	{"rp_convertation",			rp_convertation},
	{"rp_retcheck",				rp_retcheck},
#endif

	{NULL, NULL}
};

void RegisterNatives()
{
	g_amxxapi.AddNatives(RePatcherNatives);
}