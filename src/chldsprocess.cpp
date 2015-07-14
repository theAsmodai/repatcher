#include "precompiled.h"
#include "chldsprocess.h"

CHldsProcess g_hldsProcess;

CHldsProcess::CHldsProcess()
{
#ifdef _WIN32
	SYSTEM_INFO SystemInfo;

	m_handle = GetCurrentProcess(); // actually -1
	GetSystemInfo(&SystemInfo);
	m_pagesize = SystemInfo.dwPageSize;
	SymInitialize(m_handle, NULL, FALSE); // don't load modules
#else
	m_pagesize = sysconf(_SC_PAGESIZE);
#endif
}

CHldsProcess::~CHldsProcess()
{
	for (auto it = m_modules.begin(), end = m_modules.end(); it != end; it++)
		delete *it;

	m_modules.clear();

#ifdef _WIN32
	SymCleanup(m_handle);
#endif
}

#ifdef _WIN32
handle_t CHldsProcess::getHandle() const
{
	return m_handle;
}
#endif

size_t CHldsProcess::getPageSize() const
{
	return m_pagesize;
}

#ifdef _WIN32
CModule* CHldsProcess::addProcessModuleByName(const char* name)
{
	HMODULE hMods[1024];
	DWORD cbNeeded;
	CHAR szModName[MAX_PATH];

	if (m_handle == NULL)
		return NULL;

	if (EnumProcessModules(m_handle, hMods, sizeof hMods, &cbNeeded))
	{
		for (size_t i = 0; i < (cbNeeded / sizeof(HMODULE)); i++)
		{
			if (GetModuleFileNameExA(m_handle, hMods[i], szModName, sizeof(szModName) / sizeof(TCHAR)) && !strcmp(strrchr(szModName, '\\') + 1, name))
			{
				MODULEINFO module_info;
				memset(&module_info, 0, sizeof module_info);

				if (GetModuleInformation(g_hldsProcess.getHandle(), hMods[i], &module_info, sizeof module_info))
				{
					auto module = new CModule(szModName, hMods[i], (dword)module_info.lpBaseOfDll, module_info.SizeOfImage);
					if (!module->loadExtendedInfo())
						return false;
					m_modules.push_back(module);
					Con_DPrintf("Added new module: %s. Modules count: %i/%i.", name, m_modules.size(), cbNeeded / sizeof(HMODULE));
					return module;
				}
				else
				{
					setError("Can't load module information for %s", name);
					return NULL;
				}
			}
		}
	}

	setError("No modules with name %s\n", name);
	return NULL;
}
#else
CModule* CHldsProcess::addProcessModuleByName(const char* name)
{
	char buf[2048];
	char* s;
	dword start;
	dword end;
	FILE* fp;
	CModule* module = NULL;
	size_t len;

	snprintf(buf, sizeof buf, "/proc/%i/maps", getpid());

	fp = fopen(buf, "rt");
	if (!fp)
	{
		setError("Can't open /proc/ for %s\n", name);
		return NULL;
	}

	len = strlen(name);

	while (!feof(fp))
	{
		fgets(buf, sizeof buf, fp);
		s = buf + strlen(buf) - 1;
		s[0] = '\0'; // '\n'
		s -= len;

		if (s < buf)
			continue;

		if (!strcmp(s, name))
		{
			s = strchr(buf, '/'); // start of path
			sscanf(buf, "%lx-%lx", &start, &end);
			module = new CModule(s, dlopen(s, RTLD_NOW), start, end - start);
			if (!module->loadExtendedInfo())
				return false;
			m_modules.push_back(module);
			Con_DPrintf("Added new module: %s. Modules count: %i.", name, m_modules.size());
			break;
		}
	}

	fclose(fp);
	if(!module)
		setError("No modules with name %s\n", name);
	return module;
}
#endif

CModule* CHldsProcess::findModuleByName(const char* name) const
{
	for (auto it = m_modules.begin(), end = m_modules.end(); it != end; it++)
	{
		if (!strcmp((*it)->getName(), name))
			return *it;
	}
	
	return NULL;
}

CModule* CHldsProcess::getModule(const char* name)
{
	CModule* module = findModuleByName(name);
	
	if (module)
		return module;

	return addProcessModuleByName(name);
}

CModule* CHldsProcess::getModule(void* addr)
{
	for (auto it = m_modules.begin(), end = m_modules.end(); it != end; it++)
	{
		auto module = *it;

		if (module->containAddr(addr))
			return module;
	}

	return NULL;
}

void CHldsProcess::freeOpenedHandles()
{
	for (auto it = m_modules.begin(), end = m_modules.end(); it != end; it++)
	{
		auto module = *it;

#ifdef _WIN32
		module->unloadSymbols();
#else
		module->closeHandle();
#endif
	}

#ifdef _WIN32
	SymCleanup(m_handle);
#endif
}

void CHldsProcess::reopenHandles()
{
#ifdef _WIN32
	SymInitialize(m_handle, NULL, FALSE);
#endif

	for (auto it = m_modules.begin(), end = m_modules.end(); it != end; it++)
	{
		auto module = *it;

#ifdef _WIN32
		module->loadSymbols();
#else
		module->reopenHandle();
#endif
	}
}