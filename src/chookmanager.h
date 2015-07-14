#ifndef CHOOKMANAGER_H
#define CHOOKMANAGER_H

#include "cfunction.h"

typedef class CHookHandlerJit jitfunc_t;

enum hookflags_e
{
	hf_force_addr	= 1,
	hf_dec_edict	= 2,
	hf_allow_null	= 4
};

struct hookhandle_t
{
	CFunction		func;
	jitfunc_t*		jitfunc;
	char			flags;
	bool			enabled;
	bool			pre;
	AMX*			amx;
	union
	{
		int			fwd_id;
		void*		c_func;
	};

	hookhandle_t(const char* description, bool ispre, AMX* _amx, int forward, int _flags) : func(description), enabled(true), pre(ispre), amx(_amx), fwd_id(forward), flags(_flags) {}
	hookhandle_t(const char* description, bool ispre, void* handler, int _flags) : func(description), enabled(true), pre(ispre), amx(NULL), c_func(handler), flags(_flags) {}
};

class CHook
{
public:
	CHook(void* addr, bool force);
	~CHook();

	hookhandle_t* addHandler(const char* description, bool pre, AMX* amx, int forward, int flags);
	hookhandle_t* addHandler(const char* description, bool pre, void* handler, int flags);
	bool removeHandler(hookhandle_t* handler);
	void removeAmxHandlers();
	bool empty() const;
	void* getAddr() const;

	friend class CHookHandlerJit;

private:
	hookhandle_t* getFirstPreHandler() const;
	hookhandle_t* addHandler(hookhandle_t* handle);
	bool createTrampoline();
	void rebuildMainHandler();
	void repatch();
	const char* getSymbolName(const dword addr) const;

private:
	void*						m_addr;
	size_t						m_icc_fastcall;
	std::list<hookhandle_t *>	m_handlers;
	jitfunc_t*					m_jit;
	byte*						m_trampoline;
	size_t						m_bytes_patched;
	CModule*					m_module;
};

class CGate
{
public:
	CGate(const char* description, AMX* amx, int m_forward);

private:
	jitfunc_t*					m_jit;
	CFunction					m_func;
	AMX*						m_amx;
	int							m_forward;
};

class CHookManager
{
public:
	hookhandle_t* createHook(void* addr, const char* description, bool pre, AMX* amx, int forward, int flags = 0);
	hookhandle_t* createHook(void* addr, const char* description, bool pre, void* handler, int flags = 0);
	bool removeHook(hookhandle_t* handler);
	void removeAmxHooks();
	bool getReturn(dword* value) const;
	bool getReturn(double* value) const;
	bool getOriginalReturn(dword* value) const;
	bool getOriginalReturn(double* value) const;
	bool setReturnValue(dword value);
	bool setReturnValue(double value);
	bool setArg(dword index, dword value);
	bool setArg(dword index, double value);

private:
	CHook* hookAddr(void* addr, bool force);

private:
	std::map<void *, CHook *>	m_hooks;
};

extern CHookManager g_hookManager;
extern hookhandle_t* g_currentHandler;

void errorNoReturnValue(hookhandle_t* handle);
void errorNoMemory();
void errorAmxStack(AMX* amx);

#endif // CHOOKMANAGER_H