#ifndef CHLDSPROCESS_H
#define CHLDSPROCESS_H

#include "cmodule.h"

class CHldsProcess
{
public:
	CHldsProcess();
	~CHldsProcess();

#ifdef _WIN32
	handle_t getHandle() const;
#endif
	size_t getPageSize() const;
	CModule* getModule(const char* name);
	CModule* getModule(void* addr);
	void freeOpenedHandles();
	void reopenHandles();

private:
	CModule* addProcessModuleByName(const char* name);
	CModule* findModuleByName(const char* name) const;

private:
	std::vector<CModule *>			m_modules;
	size_t							m_pagesize;

#ifdef _WIN32
	handle_t						m_handle;
#endif
};

extern CHldsProcess g_hldsProcess;

#endif // CHLDSPROCESS_H