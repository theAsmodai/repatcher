#ifndef CMODULE_H
#define CMODULE_H

#ifndef _WIN32
#include "elfsym.h"
#endif

struct section_t
{
	dword start;
	dword end;
};

#ifndef _WIN32
struct symbol_t
{
	const char*	name;
	void*		addr;
};
#endif

enum module_space_e
{
	ms_all,
	ms_code,
	ms_data
};

class CModule
{
public:
	CModule(const char* path, modhandle_t handle, dword base, dword size);
	~CModule();

	const char* getName() const;
	modhandle_t getHandle() const;
	dword getBase() const;

	bool loadSymbols();
	void unloadSymbols();
	bool parseSections();
	bool loadExtendedInfo();
	void closeHandle();
	void reopenHandle();

	void* getExportedAddress(const char* name) const;
	void* getSymbolAddress(const char* name);
	size_t parsePattern(const char* pattern, char* sig, bool* check, size_t maxlen) const;
	void* findPattern(const char* sig, const bool* check, size_t size) const;
	void* findPattern(const char* pattern, bool checkCache = true);
	void* findPattern(void* from, size_t range, const char* sig, const bool* check, size_t size) const;
	void* findPattern(void* from, size_t range, const char* pattern) const;
	const char* findString(const char* string) const;
	void* findPrefixedReference(byte prefix, void* ref, bool relative) const;
	void* findStringReference(const char* string) const;
	bool containAddr(void* addr, module_space_e ms = ms_all) const;

	static bool isMangledSymbol(const char* symbol);
	void deMangleSymbol(const char* symbol, char* demangled, size_t maxlen) const;
#ifdef SELF_TEST
	bool testDeMangle() const;
#endif

private:
	modhandle_t	m_handle;
	dword		m_baseAddress;
	dword		m_imageSize;
	dword		m_delta;
	section_t	m_code;
	section_t	m_data;
#ifdef _WIN32
	bool		m_symbols;
#else
	section_t	m_symtab;
	section_t	m_strtab;
	section_t	m_dynsymtab;
	section_t	m_dynstrtab;

	symbol_t*	m_symbols;
	size_t		m_symcount;

	void		loadSymbolsFromSection(section_t* sect, section_t* strings);
	void		freeSection(section_t* sect);

	bool		m_gcc2;
#endif
	const char* m_name;
	char		m_path[MAX_PATH];

	struct pattern_cache_t
	{
		char	sig[64];
		bool	check[64];
		size_t	size;
		void*	addr;
	};

	std::vector<pattern_cache_t *> m_pcache;
	pattern_cache_t* foundCachedPattern(const char* sig, const bool* check, size_t size) const;

	struct symbol_cache_t
	{
		char	name[128];
		void*	addr;
	};

	std::vector<symbol_cache_t *> m_scache;
	symbol_cache_t* foundCachedSymbol(const char* name) const;
};

#endif // CMODULE_H