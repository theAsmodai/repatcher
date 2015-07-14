#ifndef CJITHANDLER_H
#define CJITHANDLER_H

class CHook;

struct saveregs_t
{
	dword eax;
	dword ecx;
	dword edx;
	dword esp;

	dword ret;

	dword result;
	double fresult;

	double st0;

	// TODO: align it
	dword xmm0[4];
	dword xmm1[4];
	dword xmm2[4];
	dword xmm3[4];
	dword xmm4[4];
	dword xmm5[4];
	dword xmm6[4];
	dword xmm7[4];

	dword flags;
};

class CHookHandlerJit : public jitasm::function<void, CHookHandlerJit>
{
public:
	CHookHandlerJit(CHook* owner);
	void	naked_main();

	bool	setReg(register_e reg, dword value);
	bool	setReg(register_e reg, double value);
	void	setEsp(size_t offset, dword value);
	void	setEsp(size_t offset, double value);

	friend	class CHook;
	friend	class CHookManager;
	friend	struct hookhandle_t;

private:
	size_t	setSaveRegs(register_e* regs);
	void	storeSaveRegs(register_e* regs, size_t count);
	void	restoreSaveRegs(register_e* regs, size_t count);
	size_t	pushSt0(basetype_e type, bool fromSave, AMX* amx);
	size_t	pushXmm(register_e reg, bool fromSave, AMX* amx);
	size_t	push8(register_e reg, bool fromSave, AMX* amx);
	size_t	push16(register_e reg, bool fromSave, AMX* amx);
	size_t	amx_Push(AMX* amx, register_e reg, bool isarray = false);
	size_t	amx_PushEsp(AMX* amx, size_t offset, size_t size, bool isarray = false);
	size_t	amx_PushAddr(AMX* amx, size_t addr, bool isarray = false);
	size_t	pushArg(arg_t* arg, AMX* amx, bool fromSave, size_t pushed_args_size, size_t& stack_offs, int flags);
	void	setAmxArgsCount(AMX* amx, size_t count, const char* failLabel);
	size_t	callHandlers(bool amx, bool pre);
	void	generateFooter(size_t preCount);

private:
	CHook*						m_hook;

	bool						m_regschanged;
	bool						m_ret_eax;
	bool						m_ret_st0;
	bool						____dummy_padding;

	// runtime
	bool						m_runtime_vars;
	bool						m_supercede;
	bool						m_bool_replace_result;
	bool						m_bool_replace_fresult;
	dword						m_custom_result;
	double						m_custom_fresult;

	saveregs_t					m_saveregs;

	static struct reg2offset_t
	{
		register_e			reg;
		size_t				offset;
	} reg2offset[];

	size_t getRegOffset(register_e reg);

	Reg32 getReg32(register_e reg);
	Reg16 getReg16(register_e reg);
	Reg8 getReg8(register_e reg);
	jitasm::FpuReg getFpuReg(register_e reg);
	XmmReg getXmmReg(register_e reg);
};

#endif // CJITHANDLER_H