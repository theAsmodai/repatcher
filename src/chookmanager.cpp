#include "precompiled.h"
#include "chookmanager.h"
#include "jit.h"
#include "repatcher.h"

CHookManager g_hookManager;
hookhandle_t* g_currentHandler;

CHook::CHook(void* addr, bool force) : m_addr(addr), m_icc_fastcall(0), m_jit(NULL), m_trampoline(NULL), m_bytes_patched(0)
{
	m_module = g_hldsProcess.getModule(m_addr);

	if (!force)
	{
		dword reg_from_stack[] = {0x0424448B, 0x0824548B, 0x0C244C8B}; // SV_RecursiveHullCheck problem
		dword* p = (dword *)addr;

		for (size_t i = 0; i < 3; i++)
			if (p[i] == reg_from_stack[i])
				m_icc_fastcall += 4;

		if (m_icc_fastcall == 0)
		{
			for (size_t i = 3; i > 0; i--)
			{
				if (!memcmp(p - i, reg_from_stack, i * 4))
				{
					m_icc_fastcall = i * 4;
					break;
				}
			}
		}
		else
			m_addr = (void *)((dword)m_addr + m_icc_fastcall);
	}

	createTrampoline();
}

CHook::~CHook()
{
	if (m_jit)
		delete m_jit;

	patchMemory(m_addr, m_trampoline, m_bytes_patched); // lost 5-15 bytes without free. not a problem I think.
}

void CHook::rebuildMainHandler()
{
	if (m_jit)
		delete m_jit;
	m_jit = NULL;

	if (m_handlers.size())
	{
		m_jit = new CHookHandlerJit(this);
		m_jit->Assemble();

		for (auto it = m_handlers.begin(), end = m_handlers.end(); it != end; it++)
			(*it)->jitfunc = m_jit;
	
		/*if (m_jit)
		{
			ud_t ud_obj;
			ud_init(&ud_obj);
			ud_set_mode(&ud_obj, 32);
			ud_set_syntax(&ud_obj, UD_SYN_INTEL);
			ud_set_input_buffer(&ud_obj, (uint8_t*)m_jit->GetCode(), m_jit->GetCodeSize());

			Con_Printf("Main JIT handler builded [%i]:\n", m_jit->GetCodeSize());
			while (ud_disassemble(&ud_obj))
			{
				dword addr = 0x87654321;
				auto op = ud_insn_opr(&ud_obj, 0);

				if (op)
				{
					if (op->type == UD_OP_MEM && op->offset == 32)
					{
						addr = op->lval.udword;
					}
					else if (op->type == UD_OP_IMM)
					{
						addr = op->lval.udword;
					}
					else
					{
						op = ud_insn_opr(&ud_obj, 1);
						if (op)
						{
							if (op->type == UD_OP_MEM && op->offset == 32)
							{
								addr = op->lval.udword;
							}
							else if (op->type == UD_OP_IMM)
							{
								addr = op->lval.udword;
							}
						}
					}
				}

				const char* comment = getSymbolName(addr);
				if (comment)
					Con_Printf("\t%04x: %-20s %-36s; %s\n", (size_t)ud_insn_off(&ud_obj), ud_insn_hex(&ud_obj), ud_insn_asm(&ud_obj), comment);
				else
					Con_Printf("\t%04x: %-20s %s\n", (size_t)ud_insn_off(&ud_obj), ud_insn_hex(&ud_obj), ud_insn_asm(&ud_obj));
			}

			Con_Printf("end.\n");
		}*/
	}
}

void CHook::repatch()
{
	if (m_handlers.size() && m_jit)
	{
		byte patch[5];
		patch[0] = '\xE9';
		*(dword *)&patch[1] = (dword)m_jit->GetCode() - (dword)m_addr - 5;

		patchMemory(m_addr, patch, sizeof patch);
	}
	else
	{
		patchMemory(m_addr, m_trampoline, m_bytes_patched); // restore
	}
}

hookhandle_t* CHook::addHandler(const char* description, bool pre, AMX* amx, int forward, int flags)
{
	return addHandler(new hookhandle_t(description, pre, amx, forward, flags));
}

hookhandle_t* CHook::addHandler(const char* description, bool pre, void* _handler, int flags)
{
	return addHandler(new hookhandle_t(description, pre, _handler, flags));
}

hookhandle_t* CHook::addHandler(hookhandle_t* handle)
{
	if (!handle->func.isValid())
		return NULL;

	m_handlers.push_back(handle);
	rebuildMainHandler();
	repatch();

	/*auto args = handle->func.getArgs();
	switch (m_icc_fastcall)
	{
	case 4:
		if (handle->func.getArgsCount() >= 2)
			args[1].reg = args[0].reg;
		break;

	case 8:
		if (handle->func.getArgsCount() >= 4)
		{
			args[2].reg = args[0].reg;
			args[3].reg = args[1].reg;
		}
		break;

	case 12:
		if (handle->func.getArgsCount() >= 6)
		{
			args[3].reg = args[0].reg;
			args[4].reg = args[1].reg;
			args[5].reg = args[2].reg;
		}
		break;
	}*/

	return handle;
}

bool CHook::removeHandler(hookhandle_t* handler)
{
	auto it = std::find(m_handlers.begin(), m_handlers.end(), handler);

	if (it != m_handlers.end())
	{
		m_handlers.erase(it);
		delete handler;
		rebuildMainHandler();
		repatch();
		return true;
	}

	return false;
}

void CHook::removeAmxHandlers()
{
	for (auto it = m_handlers.begin(), end = m_handlers.end(); it != end;)
	{
		auto handler = *it;

		if (handler->amx)
		{
			Con_DPrintf("Removing handler registered by %s.", getPluginName(handler->amx));
			m_handlers.erase(it++);
			delete handler;
		}
		else it++;
	}

	rebuildMainHandler();
	repatch();
}

bool CHook::createTrampoline()
{
	size_t len = 0;
	ud_t ud_obj;

	ud_init(&ud_obj);
	ud_set_mode(&ud_obj, 32);
	ud_set_input_buffer(&ud_obj, (uint8_t *)m_addr, 32);

	while (len < 5)
	{
		len += ud_disassemble(&ud_obj);
		auto op = ud_insn_opr(&ud_obj, 0);
		if (op && op->type == UD_OP_JIMM) // relative value
			return false;
	}

	m_trampoline = allocExecutableMemory(len + 5);
	memcpy(m_trampoline, m_addr, len);
	m_trampoline[len] = '\xE9'; // jmp
	*(dword *)(m_trampoline + len + 1) = (dword)m_addr + len - ((dword)m_trampoline + len + 5); // dst - src
	m_bytes_patched = len;
	return true;
}

hookhandle_t* CHook::getFirstPreHandler() const
{
	for (auto it = m_handlers.begin(), end = m_handlers.end(); it != end; it++)
	{
		auto h = *it;

		if (h->pre && !h->amx)
			return h;
	}

	for (auto it = m_handlers.begin(), end = m_handlers.end(); it != end; it++)
	{
		auto h = *it;

		if (h->pre)
			return h;
	}

	return NULL;
}

bool CHook::empty() const
{
	return m_handlers.empty();
}

void* CHook::getAddr() const
{
	return m_addr;
}

const char* saveregs_symbols[] =
{
	"saveregs.eax",
	"saveregs.ecx",
	"saveregs.edx",
	"saveregs.esp",

	"saveregs.ret",

	"saveregs.result",
	"saveregs.fresult [0]",
	"saveregs.fresult [1]",

	"saveregs.st0 [0]",
	"saveregs.st0 [1]",

	"xmm0", "", "", "",
	"xmm1", "", "", "",
	"xmm2", "", "", "",
	"xmm3", "", "", "",
	"xmm4", "", "", "",
	"xmm5", "", "", "",
	"xmm6", "", "", "",
	"xmm7", "", "", "",

	"saveregs.flags",
};

struct global_symbol_t
{
	dword		addr;
	const char*	name;
} global_symbols[] =
{
	{(dword)&g_currentHandler, "g_currentHandler"},
	{(dword)&g_tempUsed, "g_tempUsed"},
	{(dword)&g_tempMemory, "g_tempMemory"},
	{(dword)g_amxxapi.ExecuteForward, "g_amxxapi.ExecuteForward"},
	{(dword)&errorNoReturnValue, "errorNoReturnValue"},
	{(dword)&errorNoMemory, "errorNoMemory"},
	{(dword)&errorAmxStack, "errorAmxStack"},
	{sizeof(edict_t), "sizeof(edict_t)"},
	{(dword)&g_conversiondata.edicts, "sv.edicts"},
	{(dword)&g_conversiondata.clients, "svs.clients"},
};

const char* CHook::getSymbolName(const dword addr) const
{
	if (!addr || addr == 0x87654321)
		return NULL;

	if (addr == (dword)m_addr)
		return "original function";
	if (addr == (dword)m_trampoline)
		return "trampoline";
	if (addr == (dword)&m_jit->m_runtime_vars)
		return "jit runtime vars";
	if (addr == (dword)&m_jit->m_supercede)
		return "m_supercede";
	if (addr == (dword)&m_jit->m_bool_replace_result)
		return "m_bool_replace_result";
	if (addr == (dword)&m_jit->m_bool_replace_fresult)
		return "m_bool_replace_fresult";
	if (addr == (dword)&m_jit->m_custom_result)
		return "m_custom_result";
	if (addr == (dword)&m_jit->m_custom_fresult)
		return "m_custom_fresult";
	if (addr == g_conversiondata.client_size)
		return "sizeof(client_t)";

	for (size_t i = 0; i < sizeof m_jit->m_saveregs / sizeof(int); i++)
		if (addr == (dword)((dword*)&m_jit->m_saveregs + i))
			return saveregs_symbols[i];

	for (size_t i = 0; i < ARRAYSIZE(global_symbols); i++)
		if (addr == global_symbols[i].addr)
			return global_symbols[i].name;

	for (auto it = m_handlers.begin(), end = m_handlers.end(); it != end; it++)
	{
		auto h = *it;

		if (addr == (dword)h)
			return "handler";
		if (addr == (dword)&h->enabled)
			return "handler->enabled";
		if (h->amx)
		{
			if (addr == (dword)h->amx)
				return "handler->amx";
			if (addr == (dword)h->c_func)
				return "handler->fwd_id";
			if (addr == (dword)&h->amx->stk)
				return "amx->stk";
			if (addr == (dword)( h->amx->base + ( (AMX_HEADER *)( h->amx->base ) )->dat ))
				return "amx->dat";
		}
		else
			if (addr == (dword)h->c_func)
				return "handler->c_func";
	}

	return NULL;
}

CHook* CHookManager::hookAddr(void* addr, bool force)
{
	auto hook = m_hooks[addr];

	if (hook == NULL)
	{
		hook = new CHook(addr, force);
		m_hooks[addr] = hook;
	}

	return hook;
}

hookhandle_t* CHookManager::createHook(void* addr, const char* description, bool pre, AMX* amx, int forward, int flags)
{
	if (!addr)
		return NULL;
	CHook* hook = hookAddr(addr, (flags & hf_force_addr) != 0);
	return hook->addHandler(description, pre, amx, forward, flags);
}

hookhandle_t* CHookManager::createHook(void* addr, const char* description, bool pre, void* handler, int flags)
{
	if (!addr || !handler)
		return NULL;
	CHook* hook = hookAddr(addr, (flags & hf_force_addr) != 0);
	return hook->addHandler(description, pre, handler, flags);
}

bool CHookManager::removeHook(hookhandle_t* handler)
{
	if (!handler)
		return NULL;

	auto hook = handler->jitfunc->m_hook;
	if(hook->removeHandler(handler))
	{
		if (hook->empty())
		{
			m_hooks.erase(hook->getAddr());
			delete hook;
		}
		return true;
	}
	return false;
}

void CHookManager::removeAmxHooks()
{
	for (auto it = m_hooks.begin(), end = m_hooks.end(); it != end;)
	{
		auto hook = (*it++).second;
		hook->removeAmxHandlers();

		if (hook->empty())
		{
			m_hooks.erase(hook->getAddr());
			delete hook;
		}
	}
	Con_DPrintf("Active hooks after plugins unload: %i.", m_hooks.size());
}

bool CHookManager::getReturn(dword* value) const
{
	if (!g_currentHandler)
	{
		setError("Trying get return value without active hook.");
		return false;
	}

	if (g_currentHandler->func.getReturnRegister() != r_eax)
	{
		setError("Trying get return value of functon with another return type.");
		return false;
	}

	auto j = g_currentHandler->jitfunc;
	*value = j->m_bool_replace_result ? j->m_custom_result : j->m_saveregs.result;
	return true;
}

bool CHookManager::getReturn(double* value) const
{
	if (!g_currentHandler)
	{
		setError("Trying get return value without active hook.");
		return false;
	}

	if (g_currentHandler->func.getReturnRegister() != r_st0)
	{
		setError("Trying get return value of functon with another return type.");
		return false;
	}

	auto j = g_currentHandler->jitfunc;
	*value = j->m_bool_replace_fresult ? j->m_custom_fresult : j->m_saveregs.fresult;
	return true;
}

bool CHookManager::getOriginalReturn(dword* value) const
{
	if (!g_currentHandler)
	{
		setError("Trying get return value without active hook.");
		return false;
	}

	if (g_currentHandler->func.getReturnRegister() != r_eax)
	{
		setError("Trying get return value of functon with another return type.");
		return false;
	}

	*value = g_currentHandler->jitfunc->m_saveregs.result;
	return true;
}

bool CHookManager::getOriginalReturn(double* value) const
{
	if (!g_currentHandler)
	{
		setError("Trying get return value without active hook.");
		return false;
	}

	if (g_currentHandler->func.getReturnRegister() != r_st0)
	{
		setError("Trying get return value of functon with another return type.");
		return false;
	}

	*value = g_currentHandler->jitfunc->m_saveregs.fresult;
	return true;
}

bool CHookManager::setReturnValue(dword value)
{
	if (!g_currentHandler)
	{
		setError("Trying set return value without active hook.");
		return false;
	}

	if (g_currentHandler->func.getReturnRegister() != r_eax)
	{
		setError("Trying set return value to functon with another return type.");
		return false;
	}

	g_currentHandler->jitfunc->m_bool_replace_result = true;
	g_currentHandler->jitfunc->m_custom_result = value;
	return true;
}

bool CHookManager::setReturnValue(double value)
{
	if (!g_currentHandler)
	{
		setError("Trying set return value without active hook.");
		return false;
	}

	if (g_currentHandler->func.getReturnRegister() != r_st0)
	{
		setError("Trying set return value to functon with another return type.");
		return false;
	}

	g_currentHandler->jitfunc->m_bool_replace_fresult = true;
	g_currentHandler->jitfunc->m_custom_fresult = value;
	return true;
}

bool CHookManager::setArg(dword index, dword value)
{
	auto args = g_currentHandler->func.getArgs();
	auto arg = args + --index;

	if (arg->reg != r_unknown)
		return g_currentHandler->jitfunc->setReg(arg->reg, value);
	else
	{
		size_t offset = 0;

		for (size_t i = 0; i < index; i++)
			if (arg->reg == r_unknown)
				offset += 4;

		g_currentHandler->jitfunc->setEsp(offset, value);
	}

	return true;
}

bool CHookManager::setArg(dword index, double value)
{
	auto args = g_currentHandler->func.getArgs();
	auto arg = args + index - 1;

	if (arg->reg != r_unknown)
		return g_currentHandler->jitfunc->setReg(arg->reg, value);
	else
	{
		size_t offset = 0;

		for (size_t i = 0; i < index; i++)
			if (arg->reg == r_unknown)
				offset += 4;

		g_currentHandler->jitfunc->setEsp(offset, value);
	}

	return true;
}

void errorNoReturnValue(hookhandle_t* handle)
{
	Log_Error(handle->amx, "Supercede call without setting a return value.");

#ifdef SELF_TEST
	void Test_NoReturnValue(hookhandle_t* handle);
	Test_NoReturnValue(handle);
#endif
}

void errorNoMemory()
{
	// TODO: add debug mode for output extended information
	Sys_Error("[RePatcher]: no available memory for converting.");
}

void errorAmxStack(AMX* amx)
{
	MF_LogError(amx, AMX_ERR_STACKERR, "Arg push failed");
}