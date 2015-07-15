#include "precompiled.h"
#include "jit.h"
#include "repatcher.h"

int g_null = 0;
int* g_ptrnull = &g_null;
int g_ffffffff = -1; // xD

CHookHandlerJit::reg2offset_t CHookHandlerJit::reg2offset[] =
{
	{r_eax, offsetof(saveregs_t, eax)},
	{r_ecx, offsetof(saveregs_t, ecx)},
	{r_edx, offsetof(saveregs_t, edx)},
	{r_esp, offsetof(saveregs_t, esp)},

	{r_st0, offsetof(saveregs_t, st0)},

	{r_xmm0, offsetof(saveregs_t, xmm0)},
	{r_xmm1, offsetof(saveregs_t, xmm1)},
	{r_xmm2, offsetof(saveregs_t, xmm2)},
	{r_xmm3, offsetof(saveregs_t, xmm3)},
	{r_xmm4, offsetof(saveregs_t, xmm4)},
	{r_xmm5, offsetof(saveregs_t, xmm5)},
	{r_xmm6, offsetof(saveregs_t, xmm6)},
	{r_xmm7, offsetof(saveregs_t, xmm7)},

	{r_ax, offsetof(saveregs_t, eax)},
	{r_cx, offsetof(saveregs_t, ecx)},
	{r_dx, offsetof(saveregs_t, edx)},

	{r_al, offsetof(saveregs_t, eax)},
	{r_cl, offsetof(saveregs_t, ecx)},
	{r_dl, offsetof(saveregs_t, edx)},
	{r_ah, offsetof(saveregs_t, eax)},
	{r_ch, offsetof(saveregs_t, ecx)},
	{r_dh, offsetof(saveregs_t, edx)},
};

NOINLINE size_t CHookHandlerJit::getRegOffset(register_e reg)
{
	for (size_t i = 0; i < ARRAYSIZE(reg2offset); i++)
	{
		if (reg2offset[i].reg == reg)
			return reg2offset[i].offset;
	}
	return -1;
}

NOINLINE CHookHandlerJit::Reg32 CHookHandlerJit::getReg32(register_e reg)
{
	switch (reg)
	{
	case r_eax:
		return eax;
	case r_ebx:
		return ebx;
	case r_ecx:
		return ecx;
	case r_edx:
		return edx;
	case r_esi:
		return esi;
	case r_edi:
		return edi;
	case r_ebp:
		return ebp;
	case r_esp:
		return esp;
	}
	Sys_Error("%s: invalid reg id %i\n", __FUNCTION__, reg);
	return eax;
}

NOINLINE CHookHandlerJit::Reg16 CHookHandlerJit::getReg16(register_e reg)
{
	switch (reg)
	{
	case r_ax:
		return ax;
	case r_bx:
		return bx;
	case r_cx:
		return cx;
	case r_dx:
		return dx;
	}
	Sys_Error("%s: invalid reg id %i\n", __FUNCTION__, reg);
	return ax;
}

NOINLINE CHookHandlerJit::Reg8 CHookHandlerJit::getReg8(register_e reg)
{
	switch (reg)
	{
	case r_al:
		return al;
	case r_bl:
		return bl;
	case r_cl:
		return cl;
	case r_dl:
		return dl;
	case r_ah:
		return ah;
	case r_bh:
		return bh;
	case r_ch:
		return ch;
	case r_dh:
		return dh;
	}
	Sys_Error("%s: invalid reg id %i\n", __FUNCTION__, reg);
	return al;
}

NOINLINE jitasm::FpuReg CHookHandlerJit::getFpuReg(register_e reg)
{
	switch (reg)
	{
	case r_st0:
		return st0;
	case r_st1:
		return st1;
	case r_st2:
		return st2;
	case r_st3:
		return st3;
	case r_st4:
		return st4;
	case r_st5:
		return st5;
	case r_st6:
		return st6;
	case r_st7:
		return st7;
	}
	Sys_Error("%s: invalid reg id %i\n", __FUNCTION__, reg);
	return st0;
}

NOINLINE CHookHandlerJit::XmmReg CHookHandlerJit::getXmmReg(register_e reg)
{
	switch (reg)
	{
	case r_xmm0:
		return xmm0;
	case r_xmm1:
		return xmm1;
	case r_xmm2:
		return xmm2;
	case r_xmm3:
		return xmm3;
	case r_xmm4:
		return xmm4;
	case r_xmm5:
		return xmm5;
	case r_xmm6:
		return xmm6;
	case r_xmm7:
		return xmm7;
	}
	Sys_Error("%s: invalid reg id %i\n", __FUNCTION__, reg);
	return xmm0;
}

CHookHandlerJit::CHookHandlerJit(CHook* owner) : m_hook(owner), m_regschanged(false), m_ret_eax(false), m_ret_st0(false), m_supercede(false)
{
	m_bool_replace_result = false;
	m_custom_result = 0;
	m_bool_replace_fresult = false;
	m_custom_fresult = 0.0;
	memset(&m_saveregs, 0, sizeof m_saveregs);
}

size_t CHookHandlerJit::setSaveRegs(register_e* saveregs)
{
	size_t count = 0;

	for (auto it = m_hook->m_handlers.begin(), end = m_hook->m_handlers.end(); it != end; it++)
	{
		auto func = &(*it)->func;
		arg_t* args = func->getArgs();
		size_t argscount = func->getArgsCount();

		switch (func->getReturnRegister())
		{
		case r_eax:
			m_ret_eax = true; break;
		case r_st0:
			m_ret_st0 = true; break;
		}

		for (size_t i = 0; i < argscount; i++)
		{
			auto reg = args[i].reg;

			if (reg == r_unknown)
				reg = r_esp;

			if (getRegOffset(reg) == -1) // callee-save reg
				continue;

			size_t j;
			for (j = 0; j < count; j++)
			{
				if (reg == saveregs[j])
					break;
			}

			if (j == count) // not set
				saveregs[count++] = reg;
		}
	}

	return count;
}

void CHookHandlerJit::storeSaveRegs(register_e* regs, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		size_t offs = getRegOffset(regs[i]);

		if (regs[i] >= r_eax && regs[i] <= r_esp)
			mov(dword_ptr[(size_t)&m_saveregs + offs], getReg32(regs[i]));

		else if (regs[i] == r_st0)
			fst(qword_ptr[(size_t)&m_saveregs.st0]);

		else if (regs[i] >= r_xmm0 && regs[i] <= r_xmm7)
			movups(xmmword_ptr[(size_t)&m_saveregs + offs], getXmmReg(regs[i]));
	}
}

void CHookHandlerJit::restoreSaveRegs(register_e* regs, size_t count)
{
	for (size_t i = 0; i < count; i++)
	{
		size_t offs = getRegOffset(regs[i]);

		if (regs[i] >= r_eax && regs[i] < r_esp) // don't restore esp reg
			mov(getReg32(regs[i]), dword_ptr[(size_t)&m_saveregs + offs]);

		else if (regs[i] == r_st0)
			fld(qword_ptr[(size_t)&m_saveregs.st0]);

		else if (regs[i] >= r_xmm0 && regs[i] <= r_xmm7)
			movups(getXmmReg(regs[i]), xmmword_ptr[(size_t)&m_saveregs + offs]);
	}
}

size_t CHookHandlerJit::pushSt0(basetype_e type, bool regsChanged, AMX* amx)
{
	if (regsChanged) // from mem
	{
		if (amx)
		{
			fld(qword_ptr[(size_t)&m_saveregs.st0]);
			sub(esp, sizeof(int));
			fstp(dword_ptr[esp]);
			pop(ecx);

			return amx_Push(amx, r_ecx);
		}

		if (type == bt_double)
		{
			push(dword_ptr[(size_t)&m_saveregs.st0 + 4]);
			push(dword_ptr[(size_t)&m_saveregs.st0]);
			return sizeof(double);
		}
		else
		{
			sub(esp, sizeof(float));
			fld(qword_ptr[(size_t)&m_saveregs.st0]);
			fstp(dword_ptr[esp]);
			return sizeof(float);
		}
	}
	else // from reg
	{
		if (type == bt_double)
		{
			sub(esp, sizeof(double));
			fst(qword_ptr[esp]);
			return sizeof(double);
		}
		else // float
		{
			sub(esp, sizeof(float));
			fst(dword_ptr[esp]);
			return sizeof(float);
		}
	}
	return 0;
}

size_t CHookHandlerJit::pushXmm(register_e reg, bool regsChanged, AMX* amx)
{
	if (regsChanged)
	{
		if (amx)
			return amx_PushAddr(amx, (size_t)&m_saveregs + getRegOffset(reg));

		push(dword_ptr[(size_t)&m_saveregs + getRegOffset(reg)]);
	}
	else
	{
		sub(esp, sizeof(int));
		movd(dword_ptr[esp], getXmmReg(reg));
	}
	return sizeof(int);
}

size_t CHookHandlerJit::push8(register_e reg, bool regsChanged, AMX* amx)
{
	if (regsChanged)
	{
		size_t offs = getRegOffset(reg);

		if (offs != -1)
			movzx(eax, byte_ptr[(size_t)&m_saveregs + offs]);
		else
			movzx(eax, getReg8(reg));

		if (amx)
			return amx_Push(amx, r_eax);
		push(eax);
	}
	else
	{
		if (amx)
			Sys_Error("%s: registers not marked as changed for amx hook.\n", __FUNCTION__);

		push(0);
		mov(byte_ptr[esp], getReg8(reg));
	}
	return sizeof(int); // stack align
}

size_t CHookHandlerJit::push16(register_e reg, bool regsChanged, AMX* amx)
{
	if (regsChanged)
	{
		size_t offs = getRegOffset(reg);

		if (offs != -1)
			movzx(eax, word_ptr[(size_t)&m_saveregs + getRegOffset(reg)]);
		else
			movzx(eax, getReg16(reg));

		if (amx)
			return amx_Push(amx, r_eax);
		push(eax);
	}
	else
	{
		if (amx)
			Sys_Error("%s: registers not marked as changed for amx hook.\n", __FUNCTION__);

		push(0);
		mov(word_ptr[esp], getReg16(reg));
	}
	return sizeof(int);
}

NOINLINE size_t CHookHandlerJit::amx_Push(AMX* amx, register_e reg, bool isarray)
{
	AMX_HEADER* hdr = (AMX_HEADER *)amx->base;
	Reg32 temp;
	Reg32 value;
	size_t data = (size_t)(amx->base + (int)hdr->dat);

	if (reg == r_eax)
	{
		temp = ecx;
		value = eax;
	}
	else
	{
		temp = eax;
		value = getReg32(reg);
	}

	mov(temp, (size_t)&amx->stk); // ecx = amx stack ptr address
	sub(dword_ptr[temp], sizeof(cell)); // alloc on stack
	mov(temp, dword_ptr[temp]); // stack ptr value

	if (isarray)
		sub(value, data);
	mov(dword_ptr[temp + data], value);
	if (isarray)
		add(value, data); // :(

	return 0;
}

NOINLINE size_t CHookHandlerJit::amx_PushEsp(AMX* amx, size_t offset, size_t size, bool isarray)
{
	switch (size)
	{
	case sizeof(char):
		movzx(ecx, byte_ptr[esp + offset]);
		break;

	case sizeof(short):
		movzx(ecx, word_ptr[esp + offset]);
		break;

	case sizeof(int):
		mov(ecx, dword_ptr[esp + offset]);
		break;
	}

	return amx_Push(amx, r_ecx, isarray);
}

NOINLINE size_t CHookHandlerJit::amx_PushAddr(AMX* amx, size_t addr, bool isarray)
{
	mov(ecx, dword_ptr[addr]);
	return amx_Push(amx, r_ecx, isarray);
}

size_t CHookHandlerJit::pushArg(arg_t* arg, AMX* amx, bool regsChanged, size_t pushed_args_size, size_t& stack_offs, int flags)
{
	bool conv = isConvertableArg(arg, amx != NULL);
	bool amxarray = amx && (arg->count > 1 || arg->type == bt_string);
	auto reg = arg->reg;
	
	if (reg != r_unknown)
	{
		if (reg == r_st0)
			return pushSt0(arg->type, regsChanged, amx);

		if (reg >= r_xmm0 && reg <= r_xmm7)
			return pushXmm(reg, regsChanged, amx);

		if (reg >= r_ax && reg <= r_dx)
			return push16(reg, regsChanged, amx);

		if (reg >= r_al && reg <= r_dh)
			return push8(reg, regsChanged, amx);

		if (reg < r_eax && reg > r_esp)
		{
			Sys_Error("Trying push unsupportable register %i\n", arg->reg); // impossible
			return 0;
		}

		// eax-esp
		size_t offs = getRegOffset(reg);

		if (regsChanged && offs != -1) // get value from save
		{
			if (conv)
			{
				if (amxarray)
				{
					reg = r_esi;
					push(esi);
				}
				else
					reg = r_eax;
				mov(amxarray ? esi : eax, dword_ptr[(size_t)&m_saveregs + offs]);
			}
			else
			{
				if (amx)
					return amx_PushAddr(amx, (size_t)&m_saveregs + offs);

				push(dword_ptr[(size_t)&m_saveregs + offs]);
				return sizeof(int);
			}
		}
		else // reg unchanged
		{
			if (!conv)
			{
				if (amx)
					return amx_Push(amx, reg);

				push(getReg32(reg));
				return sizeof(int);
			}

			if (amxarray)
			{
				push(esi);
				if (reg != r_esi)
				{
					reg = r_esi;
					mov(esi, getReg32(reg));
				}
			}
		}
	}
	else // from stack
	{
		size_t push_size = getTypePushSize(arg->type);
		stack_offs -= push_size;
		size_t offset = stack_offs + pushed_args_size;

		if (push_size == 8)
		{
			if (amx)
			{
				fld(qword_ptr[esp + offset]);
				sub(esp, 4);
				fstp(dword_ptr[esp]);
				pop(ecx);
				return amx_Push(amx, r_ecx);
			}

			push(dword_ptr[esp + offset + 4]);
			push(dword_ptr[esp + offset]);
			return sizeof(double);
		}
		else
		{
			if (conv)
			{
				if (amxarray)
				{
					reg = r_esi;
					push(esi);
					offset += 4; // temporary
				}
				else
					reg = r_eax;
				mov(amxarray ? esi : eax, dword_ptr[esp + offset]);
			}
			else
			{
				if (amx)
					return amx_PushEsp(amx, offset, getTypeSize(arg->type));

				push(dword_ptr[esp + offset]); // TODO: smaller types?
				return sizeof(int);
			}
		}
	}

	static int s_index = 0; s_index++;
	char breakloop[32], loop[32], loopnull[32], dopush[32], nullp[32], notnull[32];
	snprintf(breakloop, sizeof breakloop, "eos%i", s_index);
	snprintf(loopnull, sizeof loopnull, "loopnull%i", s_index);
	snprintf(loop, sizeof loop, "loop%i", s_index);
	snprintf(notnull, sizeof notnull, "notnull%i", s_index);
	snprintf(dopush, sizeof dopush, "dopush%i", s_index);
	snprintf(nullp, sizeof nullp, "nullp%i", s_index);

	// do array conversion to amx array
	if (amx && arg->count > 1)
	{
		if (flags & hf_allow_null)
		{
			test(esi, esi);
			jnz(notnull);
			amx_Push(amx, r_esi); // push 0
			jmp(nullp);
			L(notnull);
		}

		mov(eax, (size_t)&g_tempUsed);
		mov(edx, dword_ptr[eax]);
		cmp(edx, sizeof(g_tempMemory) - arg->count * sizeof(cell)); // g_tempUsed + needed > sizeof
		jg("no_memory");
		mov(ecx, arg->count * sizeof(cell));
		add(dword_ptr[eax], ecx);
		add(edx, (size_t)&g_tempMemory);
		amx_Push(amx, r_edx, true); // eax changed

		if (!(flags & hf_allow_null))
		{
			test(esi, esi);
			jnz(notnull);

			shr(ecx, 2);
			L(loopnull);
			{
				dec(ecx);
				mov(dword_ptr[edx + ecx * 4], esi);
				jnz(loopnull);
			}

			jmp(nullp);
			L(notnull);
		}

		// optimize for vector. TODO: make autounroll for short arrays?
		if (arg->count == 3 && getTypeSize(arg->type) == sizeof(float))
		{
			mov(eax, dword_ptr[esi]);
			mov(ecx, dword_ptr[esi + 4]);
			mov(dword_ptr[edx], eax);
			mov(dword_ptr[edx + 4], ecx);
			mov(eax, dword_ptr[esi + 8]);
			mov(dword_ptr[edx + 8], eax);

			L(nullp);
			pop(esi); // restore
			return 0;
		}

		shr(ecx, 2);

		// Loop: esi = src, edx = dst, eax = scratch, ecx = end
		L(loop);
		{
			switch (getTypeSize(arg->type))
			{
			case sizeof(char):
				if (isTypeSigned(arg->type))
					movsx(eax, byte_ptr[esi]);
				else
					movzx(eax, byte_ptr[esi]);

				mov(dword_ptr[edx], eax);
				inc(esi);
				add(edx, 4);
				break;

			case sizeof(short):
				if (isTypeSigned(arg->type))
					movsx(eax, word_ptr[esi]);
				else
					movzx(eax, word_ptr[esi]);

				mov(dword_ptr[edx], eax);
				add(esi, 2);
				add(edx, 4);
				break;

			case sizeof(int):
				mov(eax, dword_ptr[esi]);
				mov(dword_ptr[edx], eax);
				add(esi, 4);
				add(edx, 4);
				break;

			case sizeof(double):
				fld(qword_ptr[esi]);
				fstp(dword_ptr[edx]);
				add(esi, 8);
				add(edx, 4);
				break;
			}

			dec(ecx);
			jnz(loop);
		}

		L(nullp);
		pop(esi);
		return 0;
	}
	else // do single type conversion
	{
		auto cReg = getReg32(reg);

		if (arg->type != bt_string)
		{
			test(cReg, cReg);
			if (!(flags & hf_dec_edict)) // will be decreased later
				cmovz(cReg, dword_ptr[(size_t)&g_ffffffff]);
			jz(dopush);
		}

		switch (arg->type)
		{
		case bt_cbase:
			mov(cReg, dword_ptr[cReg + g_conversiondata.pev_offset]); // cbase to entvars
			// continue

		case bt_entvars:
			mov(eax, dword_ptr[cReg + offsetof(entvars_t, pContainingEntity)]); // entvars to edict
			reg = r_eax;
			// continue

		case bt_edict:
			if (reg != r_eax)
				mov(eax, cReg);
			mov(ecx, sizeof(edict_t));
			sub(eax, dword_ptr[(size_t)&g_conversiondata.edicts]);
			xor_(edx, edx);
			div(ecx);
			break;

		case bt_client:
			if (reg != r_eax)
				mov(eax, cReg);
			mov(ecx, g_conversiondata.client_size);
			sub(eax, dword_ptr[(size_t)&g_conversiondata.clients]);
			xor_(edx, edx);
			div(ecx);
			break;

		case bt_string:
			test(esi, esi);

			if (flags & hf_allow_null)
			{
				jnz(notnull);
				amx_Push(amx, r_esi); // push 0
				jmp(nullp);
				L(notnull);
			}
			else
			{
				cmovz(esi, dword_ptr[(size_t)&g_ptrnull]); // src = ""
			}

			// get available temp space in ecx, and dest in edx
			mov(ecx, sizeof(g_tempMemory));
			mov(edx, dword_ptr[(size_t)&g_tempUsed]);
			sub(ecx, edx);
			add(edx, (size_t)&g_tempMemory);
			amx_Push(amx, r_edx, true); // eax changed

			// Loop: esi = src, edx = dst, ecx = avail space, eax = scratch
			L(loop);
			{
				sub(ecx, 4); // get 4 bytes from temp
				js("no_memory"); // error if avail space < 0

				movsx(eax, byte_ptr[esi]);
				test(eax, eax);
				jz(breakloop);
				mov(dword_ptr[edx], eax);

				inc(esi);
				add(edx, 4);
				jmp(loop);
			}

			L(breakloop);
			mov(dword_ptr[edx], eax);

			// update g_tempUsed
			sub(ecx, sizeof(g_tempMemory));
			neg(ecx);
			mov(dword_ptr[(size_t)&g_tempUsed], ecx);

			L(nullp);
			pop(esi);
			return 0;
		}

		L(dopush);

		if (flags & hf_dec_edict && arg->type != bt_client)
			dec(cReg);

		if (amx)
			amx_Push(amx, reg);
		else
			push(cReg);

		return amx ? 0 : sizeof(int);
	}

	return 0;
}

void CHookHandlerJit::setAmxArgsCount(AMX* amx, size_t count, const char* failLabel)
{
	static int s_index = 0; s_index++;
	char stack_ok[32];
	snprintf(stack_ok, sizeof stack_ok, "stack_ok%i", s_index);

	mov(eax, (size_t)amx);
	mov(ecx, dword_ptr[eax + offsetof(AMX, stk)]);
	sub(ecx, count * sizeof(cell) + STKMARGIN);

	cmp(dword_ptr[eax + offsetof(AMX, hea)], ecx); // check if (heap) overlaps (stack - margin)
	jbe(stack_ok);
	{
		push(eax);
		mov(eax, (size_t)&errorAmxStack);
		call(eax);
		add(esp, 4);
		jmp(failLabel);
	}
	L(stack_ok);
	mov(byte_ptr[eax + offsetof(AMX, paramcount)], count);
}

size_t CHookHandlerJit::callHandlers(bool amx, bool pre)
{
	bool regsChanged = m_regschanged || !pre;
	size_t calledCount = 0;
	size_t pushed_args_size = 0;

	for (auto it = m_hook->m_handlers.begin(), end = m_hook->m_handlers.end(); it != end; it++)
	{
		auto handler = *it;

		if (!!handler->amx != amx)
			continue;

		if (handler->pre != pre)
			continue;

		static int s_index; s_index++;
		char label[32], retok[32];
		snprintf(label, sizeof label, "label%i", s_index);
		snprintf(retok, sizeof retok, "retok%i", s_index);

		cmp(byte_ptr[(size_t)&handler->enabled], 0);
		jz(label);

		if (amx)
			setAmxArgsCount(handler->amx, handler->func.getArgsCount(), label);

		if (handler->func.getArgsCount() == 0) // no args
			;
		else if (handler->func.isCdecl() && !handler->func.hasConvertableArgs(!!handler->amx)) // only stack args without conversions, use original
			;
		else // repush all args
		{
			arg_t* args = handler->func.getArgs();
			size_t stack_offs = /*m_hook->m_icc_fastcall*/ + handler->func.getStackArgsSize(); // offset to last non-register argument + sizeof(it)

			for (int i = (int)handler->func.getArgsCount() - 1; i >= 0; i--)
			{
				if (!regsChanged && isConvertableArg(&args[i], amx))
					regsChanged = true;

				pushed_args_size += pushArg(&args[i], handler->amx, regsChanged, pushed_args_size, stack_offs, handler->flags);
			}
		}

		// call
		if (amx)
		{
			if (pushed_args_size)
				Sys_Error("Esp changed on pushing amx args (%i)\n", pushed_args_size);

			push(handler->fwd_id);
			pushed_args_size += sizeof(int);
			mov(eax, (size_t)g_amxxapi.ExecuteForward); // TODO: use it only for debug version
		}
		else
			mov(eax, (size_t)handler->c_func);

		mov(dword_ptr[(size_t)&g_currentHandler], (size_t)handler);
		call(eax);

		// cleanup
		if (pushed_args_size)
		{
			add(esp, pushed_args_size);
			pushed_args_size = 0;
		}

		if (handler->pre)
		{
			// check result
			test(eax, eax);
			jz(label); // returned 0 = CONTINUE

			// we supercede. check that return value is set.
			if (handler->func.getReturnRegister() == r_eax)
			{
				cmp(byte_ptr[(size_t)&m_bool_replace_result], 0);
				jnz(retok);

				push((size_t)handler);
				mov(ecx, (size_t)errorNoReturnValue);
				call(ecx);
				add(esp, 4);
				jmp(label); // cancel supercede
			}
			else if (handler->func.getReturnRegister() == r_st0)
			{
				cmp(byte_ptr[(size_t)&m_bool_replace_fresult], 0);
				jnz(retok);

				push((size_t)handler);
				mov(ecx, (size_t)errorNoReturnValue);
				call(ecx);
				add(esp, 4);
				jmp(label); // cancel supercede
			}
			L(retok);

			inc(byte_ptr[(size_t)&m_supercede]);
			test(eax, eax);
			jns(label); // returned 1 = SUPERCEDE_MAIN
		
			// returned -1 = SUPERCEDE ALL
			jmp("supercede");
		}

		// CONTINUE
		L(label);
		calledCount++;
		m_regschanged = regsChanged = true;
	}

	return calledCount;
}

void CHookHandlerJit::generateFooter(size_t preCount)
{
	if (preCount != 0)
	{
		L("supercede");
		push(dword_ptr[(size_t)&m_saveregs.ret]);

		if (m_ret_eax) // some hook want return custom eax
			mov(eax, dword_ptr[(size_t)&m_custom_result]);
		if (m_ret_st0) // some hook want return custom st0
			fld(qword_ptr[(size_t)&m_custom_fresult]);

#ifndef SELF_TEST
		mov(dword_ptr[(size_t)&g_currentHandler], 0); // for debug
#endif
		ret();
	}

	L("no_memory");
	mov(eax, (size_t)&errorNoMemory);
	call(eax);
}

void CHookHandlerJit::naked_main()
{
	register_e saveregs[32];
	size_t savecount;
	size_t pre_count;
	size_t post_count;
	bool preHooks = false;

	// TODO: ebx = this. I'll do it after the first testing.
	mov(dword_ptr[(size_t)&m_runtime_vars], 0); // clean runtime flags
	mov(dword_ptr[(size_t)&g_tempUsed], 0); // TODO: select unused register for null

	savecount = setSaveRegs(saveregs);
	storeSaveRegs(saveregs, savecount);
	pop(dword_ptr[(size_t)&m_saveregs.ret]);

	// pre
	pre_count = callHandlers(false, true); // c handlers
	pre_count += callHandlers(true, true); // amx forwards

	// cleanup
	if (pre_count)
		restoreSaveRegs(saveregs, savecount);

	// call original
	cmp(byte_ptr[(size_t)&m_supercede], 0);
	jnz("no_call");
	{
		call(dword_ptr[(size_t)&m_hook->m_trampoline]); // added dword_ptr support

		// save original return for post hooks if they exists
		if (pre_count < m_hook->m_handlers.size())
		{
			if (m_ret_eax)
				mov(dword_ptr[(size_t)&m_saveregs.result], eax);
			if (m_ret_st0)
				fst(qword_ptr[(size_t)&m_saveregs.fresult]);
		}
	}
	L("no_call");

	// post. regs will be restored from memory if needed.
	post_count = callHandlers(false, false); // c handlers
	post_count += callHandlers(true, false); // amx forwards

	// set return value
	if (m_ret_eax)
	{
		cmp(byte_ptr[(size_t)&m_bool_replace_result], 0);
		cmovnz(eax, dword_ptr[(size_t)&m_custom_result]);

		if (pre_count < m_hook->m_handlers.size()) // has post hooks
			cmovz(eax, dword_ptr[(size_t)&m_saveregs.result]);
	}

	if (m_ret_st0)
	{
		if (pre_count < m_hook->m_handlers.size()) // has post hooks
		{
			cmp(byte_ptr[(size_t)&m_bool_replace_fresult], 0);
			jz("dont_replace");
			fld(qword_ptr[(size_t)&m_custom_fresult]);
			jmp("replaced");

			L("dont_replace");
			fld(dword_ptr[(size_t)&m_saveregs.fresult]);

			L("replaced");
		}
		else // no post hooks
		{
			cmp(byte_ptr[(size_t)&m_bool_replace_fresult], 0);
			jz("dont_replace");
			fld(qword_ptr[(size_t)&m_custom_fresult]);
			L("dont_replace");
		}
	}

	push(dword_ptr[(size_t)&m_saveregs.ret]);
#ifndef SELF_TEST
	mov(dword_ptr[(size_t)&g_currentHandler], 0); // for debug
#endif
	ret();

	// create SUPERCEDE way
	generateFooter(pre_count);
}

bool CHookHandlerJit::setReg(register_e reg, dword value)
{
	size_t offset = getRegOffset(reg);

	if (offset == -1)
	{
		setError("You can change arguments only in stack and scratch registers.");
		return false;
	}

	if (reg != r_unknown && reg != r_st0)
	{
		*(dword *)((dword)&m_saveregs + offset) = value;
		m_regschanged = true;
		return true;
	}

	setError("Trying set value to invalid register.");
	return false;
}

bool CHookHandlerJit::setReg(register_e reg, double value)
{
	size_t offset = getRegOffset(reg);

	if (offset == -1)
	{
		setError("You can change arguments only in stack and scratch registers.");
		return false;
	}

	if (reg == r_st0)
	{
		*(double *)((dword)&m_saveregs + getRegOffset(reg)) = value;
		m_regschanged = true;
		return true;
	}

	setError("Trying set value to invalid register.");
	return false;
}

void CHookHandlerJit::setEsp(size_t offset, dword value)
{
	*(dword *)(m_saveregs.esp + 4 + offset) = value;
}

void CHookHandlerJit::setEsp(size_t offset, double value)
{
	*(double *)(m_saveregs.esp + 4 + offset) = value;
}