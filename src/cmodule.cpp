#include "precompiled.h"
#include "cmodule.h"
#include "chldsprocess.h"

CModule::CModule(const char* path, modhandle_t handle, dword base, dword size) : m_handle(handle), m_baseAddress(base), m_imageSize(size), m_delta(0)
{
	strncpy(m_path, path, sizeof m_path - 1);
	m_path[sizeof m_path - 1] = '\0';

#ifdef _WIN32
	m_name = strrchr(m_path, '\\') + 1;
	m_symbols = false;
#else
	m_name = strrchr(m_path, '/') + 1;
	m_symbols = NULL;
#endif
}

CModule::~CModule()
{
	unloadSymbols();
	closeHandle();
}

bool CModule::loadExtendedInfo()
{
	if (!parseSections())
	{
		setError("Can't parse module %s sections\n", m_name);
		return false;
	}

	if(!loadSymbols())
		Con_Printf("[RePatcher] error: %s", g_lastError);

	return true;
}

void CModule::unloadSymbols()
{
#ifdef _WIN32
	if (m_symbols)
	{
		SymUnloadModule64(g_hldsProcess.getHandle(), m_baseAddress);
		Con_DPrintf("Unloading symbols for %s.", m_name);
		m_symbols = false;
	}
#else
	freeSection(&m_symtab);
	freeSection(&m_strtab);
	freeSection(&m_dynsymtab);
	freeSection(&m_dynstrtab);
	if (m_symbols)
		delete m_symbols;
#endif
}

void CModule::closeHandle()
{
#ifndef _WIN32
	Con_DPrintf("Closing handle for %s.", m_name);
	dlclose(m_handle);
	m_handle = NULL;
#endif
}

#ifdef _WIN32
bool CModule::loadSymbols()
{
	DWORD64 ldwModBase = SymLoadModule64(g_hldsProcess.getHandle(), NULL, m_path, NULL, m_baseAddress, m_imageSize);

	if (ldwModBase == 0)
	{
		setError("Can't load symbols for module %s (%i)\n", m_name, GetLastError());
		return false;
	}

	m_delta = (dword)ldwModBase - m_baseAddress;
	m_symbols = true;
	return true;
}

bool CModule::parseSections()
{
	IMAGE_DOS_HEADER* idh;
	IMAGE_NT_HEADERS* inh;
	IMAGE_SECTION_HEADER* ish;
	size_t sections, i;

	idh = (IMAGE_DOS_HEADER *)m_baseAddress;
	if (idh->e_magic != IMAGE_DOS_SIGNATURE)
		return false;

	inh = (IMAGE_NT_HEADERS *)(m_baseAddress + idh->e_lfanew);
	if (inh->Signature != IMAGE_NT_SIGNATURE)
		return false;

	ish = (IMAGE_SECTION_HEADER *)((size_t)&inh->OptionalHeader + sizeof(IMAGE_OPTIONAL_HEADER));
	sections = (size_t)inh->FileHeader.NumberOfSections;

	for (i = 0; i < sections; ++i, ++ish)
	{
		if (ish->VirtualAddress == inh->OptionalHeader.BaseOfCode)
		{
			m_code.start = m_baseAddress + ish->VirtualAddress;
			m_code.end = m_code.start + ish->SizeOfRawData;
		}
		else if (ish->VirtualAddress == inh->OptionalHeader.BaseOfData)
		{
			m_data.start = m_baseAddress + ish->VirtualAddress;
			m_data.end = m_data.start + ish->SizeOfRawData;
		}
	}

	return m_code.start && m_code.end && m_data.start && m_data.end;
}
#else
void CModule::loadSymbolsFromSection(section_t* sect, section_t* strings)
{
	m_symcount = (sect->end - sect->start) / sizeof(Elf32_Sym);
	m_symbols = new symbol_t[sizeof(symbol_t) * m_symcount];

	Elf32_Sym* elfsym = (Elf32_Sym *)sect->start;
	m_gcc2 = false;

	for(size_t i = 0; i < m_symcount; i++)
	{
		m_symbols[i].name = (char *)(strings->start + elfsym[i].st_name);
		m_symbols[i].addr = (void *)(m_baseAddress + elfsym[i].st_value);

		if(!strcmp(m_symbols[i].name, "gcc2_compiled."))
		{
			m_gcc2 = true;
			Con_DPrintf("gcc2 compiled binary.");
		}
	}
}

void CModule::freeSection(section_t* sect)
{
	if(sect->start)
	{
		free((void *)sect->start);
		sect->start = 0;
		sect->end = 0;
	}
}

bool CModule::loadSymbols()
{
	if(m_symtab.start)
	{
		loadSymbolsFromSection(&m_symtab, &m_strtab);
		freeSection(&m_dynstrtab);
	}
	else if(m_dynstrtab.start)
	{
		loadSymbolsFromSection(&m_dynsymtab, &m_dynstrtab);
		freeSection(&m_strtab);
	}
	else
	{
		freeSection(&m_strtab);
		freeSection(&m_dynstrtab);
		setError("Can't load symbols for module %s\n", m_name);
		return false;
	}

	freeSection(&m_symtab);
	freeSection(&m_dynsymtab);
	return true;
}

#ifdef _WIN32
typedef byte uint8;
typedef word uint16;

typedef struct
{
	uint8  e_ident[16];
	uint16 e_type;
	uint16 e_machine;
	uint32 e_version;
	uint32 e_entry;
	uint32 e_phoff;
	uint32 e_shoff;
	uint32 e_flags;
	uint16 e_ehsize;
	uint16 e_phentsize;
	uint16 e_phnum;
	uint16 e_shentsize;
	uint16 e_shnum;
	uint16 e_shstrndx;
} Elf32_Ehdr;

typedef struct
{
	uint32 sh_name;
	uint32 sh_type;
	uint32 sh_flags;
	uint32 sh_addr;
	uint32 sh_offset;
	uint32 sh_size;
	uint32 sh_link;
	uint32 sh_info;
	uint32 sh_addralign;
	uint32 sh_entsize;
} Elf32_Shdr;
#endif

// http://wiki.osdev.org/ELF_Tutorial
static inline Elf32_Shdr *elf_sheader(Elf32_Ehdr *hdr) {
	return (Elf32_Shdr *)((int)hdr + hdr->e_shoff);
}

static inline Elf32_Shdr *elf_section(Elf32_Ehdr *hdr, int idx) {
	return &elf_sheader(hdr)[idx];
}

static inline char *elf_str_table(Elf32_Ehdr *hdr) {
	if(hdr->e_shstrndx == SHN_UNDEF) return NULL;
	return (char *)hdr + elf_section(hdr, hdr->e_shstrndx)->sh_offset;
}
//

bool CModule::parseSections()
{
	int fd;
	struct stat st;
	Elf32_Ehdr* ehdr;
	Elf32_Shdr* shdr;
	Elf32_Shdr* section;
	const char* strtbl;
	const char* sectname;
	size_t i;

	stat(m_path, &st);
	fd = open(m_path, O_RDONLY);
	ehdr = (Elf32_Ehdr *)mmap(0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);

	if (memcmp(ehdr->e_ident, "\x7f\x45\x4c\x46", 4) != 0)
		return false;

	shdr = elf_sheader(ehdr);
	strtbl = elf_str_table(ehdr);

	for (i = 0; i < ehdr->e_shnum; i++, shdr++)
	{
		if(shdr->sh_name == SHN_UNDEF)
			continue;

		sectname = strtbl + shdr->sh_name;

		if (shdr->sh_type == SHT_SYMTAB)
		{
			m_symtab.start = (dword)malloc(shdr->sh_size);
			memcpy((void *)m_symtab.start, (char *)ehdr + shdr->sh_offset, shdr->sh_size);
			m_symtab.end = m_symtab.start + shdr->sh_size;
		}
		else if (shdr->sh_type == SHT_DYNSYM)
		{
			m_dynsymtab.start = (dword)malloc(shdr->sh_size);
			memcpy((void *)m_dynsymtab.start, (char *)ehdr + shdr->sh_offset, shdr->sh_size);
			m_dynsymtab.end = m_dynsymtab.start + shdr->sh_size;
		}
		else if (!strcmp(sectname, ".text"))
		{
			m_code.start = m_baseAddress + shdr->sh_addr;
			m_code.end = m_code.start + shdr->sh_size;
		}
		else if (!strcmp(sectname, ".data"))
		{
			m_data.start = m_baseAddress + shdr->sh_addr;
			m_data.end = m_data.start + shdr->sh_size;
		}
		else if (!strcmp(sectname, ".strtab"))
		{
			m_strtab.start = (dword)malloc(shdr->sh_size);
			memcpy((void *)m_strtab.start, (char *)ehdr + shdr->sh_offset, shdr->sh_size);
			m_strtab.end = m_strtab.start + shdr->sh_size;
		}
		else if (!strcmp(sectname, ".dynstr"))
		{
			m_dynstrtab.start = (dword)malloc(shdr->sh_size);
			memcpy((void *)m_dynstrtab.start, (char *)ehdr + shdr->sh_offset, shdr->sh_size);
			m_dynstrtab.end = m_dynstrtab.start + shdr->sh_size;
		}
	}

	close(fd);
	munmap(ehdr, st.st_size);
	return m_code.start && m_code.end && m_data.start && m_data.end;
}
#endif

const char* CModule::getName() const
{
	return m_name;
}

modhandle_t CModule::getHandle() const
{
	return m_handle;
}

void CModule::reopenHandle()
{
#ifndef _WIN32
	m_handle = dlopen(m_path, RTLD_NOW);
#endif
}

dword CModule::getBase() const
{
	return m_baseAddress;
}

void* CModule::getExportedAddress(const char* name) const
{
#ifdef _WIN32
	return GetProcAddress(m_handle, name);
#else
	void* handle;

	if(m_handle)
		handle = m_handle;
	else
		handle = dlopen(m_path, RTLD_NOW);

	void* addr = dlsym(m_handle, name);

	if (!m_handle)
		dlclose(handle);

	return addr;
#endif
}

void* CModule::getSymbolAddress(const char* name)
{
	auto cached = foundCachedSymbol(name);

	if (cached)
		return cached->addr;

#ifdef _WIN32
	DWORD dwOpts;
	SYMBOL_INFO_PACKAGE sip;
	memset(&sip, 0, sizeof sip);

	sip.si.SizeOfStruct = sizeof(SYMBOL_INFO);
	sip.si.MaxNameLen = sizeof(sip.name);

	if (!m_symbols)
	{
		SymInitialize(g_hldsProcess.getHandle(), NULL, FALSE);
		SymLoadModule64(g_hldsProcess.getHandle(), NULL, m_path, NULL, m_baseAddress, m_imageSize);
	}

	if (isMangledSymbol(name))
	{
		dwOpts = SymGetOptions();
		SymSetOptions(SYMOPT_EXACT_SYMBOLS);
	}

	SymFromName(g_hldsProcess.getHandle(), name, &sip.si);

	if (isMangledSymbol(name))
	{
		SymSetOptions(dwOpts);
	}

	if (!m_symbols)
	{
		SymUnloadModule64(g_hldsProcess.getHandle(), m_baseAddress);
		SymCleanup(g_hldsProcess.getHandle());
	}

	cached = new symbol_cache_t;
	strncpy(cached->name, name, sizeof cached->name - 1);
	cached->name[sizeof cached->name - 1] = '\0';

	if ((int)sip.si.Address && containAddr((void *)((int)sip.si.Address - m_delta)))
		cached->addr = (void *)((int)sip.si.Address - m_delta);
	else
		cached->addr = NULL;

	m_scache.push_back(cached);
	return cached->addr;
#else
	char buf[256] = {'\0'};
	bool mangled;
	size_t len = 0;

	if(m_gcc2)
	{
		mangled = strstr(name, "__") != NULL;

		if (!mangled)
		{
			// simple mangle
			const char* c = strstr(name, "::");
			const char* method;
			const char* space = name;

			if (c)
			{
				do method = c + 2;
				while ((c = strstr(method, "::")) != NULL);

				len = snprintf(buf, sizeof buf, "%s", method);

				while (space != method && (c = strstr(space, "::")) != NULL)
				{
					len += snprintf(buf + len, sizeof buf - len, "__%i%.*s", c - space, c - space, space);
					space = c + 2;
				}
			}
			else
				len = snprintf(buf, sizeof buf, "%s", name);
		}
	}
	else
	{
		mangled = isMangledSymbol(name);

		if (!mangled)
		{
			// simple mangle
			const char* end = name;
			const char* c;

			while ((c = strstr(end, "::")) != NULL)
			{
				len += snprintf(buf + len, sizeof buf - len, "%i%.*s", c - end, c - end, end);
				end = c + 2;
			}

			len += snprintf(buf + len, sizeof buf - len, "%i%s", strlen(end), end);
		}
	}

	for (size_t i = 0; i < m_symcount; i++)
	{
		const char* symName = m_symbols[i].name;
		bool symMangled = isMangledSymbol(symName);

		if (!mangled && symMangled)
		{
			symName += 2;
			while (*symName && !isdigit(*symName))
				symName++;
		}

		if (*symName == '\0')
			continue;

		if ((mangled || !symMangled) ? strcmp(symName, name) : strncmp(symName, buf, len) )
			continue;

		cached = new symbol_cache_t;
		strncpy(cached->name, name, sizeof cached->name - 1);
		cached->name[sizeof cached->name - 1] = '\0';
		cached->addr = m_symbols[i].addr;
		m_scache.push_back(cached);
		return cached->addr;
	}

	return NULL;
#endif
}

bool CModule::isMangledSymbol(const char* symbol)
{
#ifdef _WIN32
	return symbol[0] == '?';
#else
	return symbol[0] == '_' && symbol[1] == 'Z'; // only new gcc
#endif
}

void CModule::deMangleSymbol(const char* symbol, char* demangled, size_t maxlen) const
{
#ifdef _WIN32
	if (!CModule::isMangledSymbol(symbol))
	{
		strncpy(demangled, symbol, maxlen);
		demangled[sizeof demangled - 1] = '\0';
		return;
	}

	UnDecorateSymbolName(symbol, demangled, maxlen, UNDNAME_32_BIT_DECODE | UNDNAME_NAME_ONLY);
#else
	if(m_gcc2) // "DisplayMaps__18CHalfLifeMultiplayP11CBasePlayeri" or "SV_Career_Restart_f__Fv"
	{
		const char* symend = strstr(symbol, "__");
		const char* typeinfo;

		if (!symend)
		{
			strncpy(demangled, symbol, maxlen);
			demangled[sizeof demangled - 1] = '\0';
			return;
		}

		do typeinfo = symend + 2;
		while ((symend = strstr(typeinfo, "__")) != NULL);

		if (typeinfo[0] == 'F') // function
		{
			snprintf(demangled, maxlen, "%.*s", symend - symbol, symbol);
			return;
		}

		char* dst;
		size_t len = 0;
		char length[4];
		const char* methodend = strstr(symbol, "__");
		const char* src = methodend + 2;

		while (isdigit(*src))
		{
			if (len)
				len += snprintf(demangled + len, maxlen - len, "::");

			dst = length;
			do
				*dst++ = *src++;
			while (isdigit(*src));

			*dst = '\0';

			size_t partlen = atoi(length);
			len += snprintf(demangled + len, maxlen - len, "%.*s", partlen, src);
			src += partlen;
		}

		snprintf(demangled + len, maxlen - len, "::%.*s", methodend - symbol, symbol);
	}
	else
	{
		if (!CModule::isMangledSymbol(symbol))
		{
			strncpy(demangled, symbol, maxlen);
			demangled[sizeof demangled - 1] = '\0';
			return;
		}

		char* dst;
		const char* src = symbol + 2;
		size_t len = 0;
		char length[4];

		while (*src && !isdigit(*src))
			src++; // skip qualifiers

		while (isdigit(*src))
		{
			if (len)
				len += snprintf(demangled + len, maxlen - len, "::");

			dst = length;
			do
				*dst++ = *src++;
			while (isdigit(*src));

			*dst = '\0';

			size_t partlen = atoi(length);
			len += snprintf(demangled + len, maxlen - len, "%.*s", partlen, src);
			src += partlen;
		}
	}
#endif
}

#ifdef SELF_TEST
bool CModule::testDeMangle() const
{
#ifdef _WIN32
	struct test_t
	{
		const char* mangled;
		const char* demangled;
	} test[] =
	{
		{"?StaticDecal@CDecal@@QAEXXZ", "CDecal::StaticDecal"},
		{"?BombTargetUse@CBombTarget@@QAEXPAVCBaseEntity@@0W4USE_TYPE@@M@Z", "CBombTarget::BombTargetUse"},
		{"??0system_error@std@@QAE@Verror_code@1@PBD@Z", "std::system_error::system_error"},
		{"??0?$ctype@D@std@@QAE@ABV_Locinfo@1@I@Z", "std::ctype<char>::ctype<char>"}
	};
#else
	struct test_t
	{
		const char* mangled;
		const char* demangled;
	} test[] =
	{
		{"_ZN13CSteam3Client22OnGameOverlayActivatedEP22GameOverlayActivated_t", "CSteam3Client::OnGameOverlayActivated"},
		{"_Z16DELTA_ParseDeltaPhS_P7delta_s", "DELTA_ParseDelta"},
		{"NotifyBotConnect__13CSteam3ServerP8client_s", "CSteam3Server::NotifyBotConnect"},
		{"RefreshSkillData__18CHalfLifeMultiplay", "CHalfLifeMultiplay::RefreshSkillData"},
		{"Broadcast__FPCc", "Broadcast"}
	};
#endif
	char demangled[256];

	for (size_t i = 0; i < sizeof test / sizeof test[0]; i++)
	{
		deMangleSymbol(test[i].mangled, demangled, sizeof demangled - 1);

		if (strcmp(test[i].demangled, demangled) != 0)
		{
			Con_Printf("DeMangle failed. Expected: %s, get %s\n", test[i].demangled, demangled);
			return false;
		}
	}

	return true;
}
#endif

size_t CModule::parsePattern(const char* pattern, char* sig, bool* check, size_t maxlen) const
{
	char buf[128];
	char* octets[64];
	size_t len;

	strncpy(buf, pattern, sizeof buf - 1);
	buf[sizeof buf - 1] = '\0';
	len = parse(buf, octets, sizeof octets - 1, ' ');

	if (len > maxlen)
	{
		setError("Pattern is too long.");
		return 0;
	}

	for (size_t i = 0; i < len; i++)
	{
		if (octets[i][0] == '?')
		{
			sig[i] = 0;
			check[i] = false;
			continue;
		}

		for (char* c = octets[i]; *c; c++)
		{
			if (( *c < '0' || *c > '9' ) && ( *c < 'a' || *c > 'f' ) && ( *c < 'A' || *c > 'F' ))
			{
				setError("Invalid character in pattern.");
				return 0;
			}
		}

		sig[i] = (char)strtol(octets[i], NULL, 16);
		check[i] = true;
	}

	return len;
}

static inline bool checkPattern(const char* addr, const char* sig, const bool* check, size_t size)
{
	for (size_t i = 0; i < size; i++)
	{
		if (addr[i] == sig[i] || check[i] == false)
			continue;

		return false;
	}

	return true;
}

void* CModule::findPattern(const char* sig, const bool* check, size_t size) const
{
	for (char *addr = (char *)m_code.start, *end = (char *)m_code.end - size; addr < end; addr++)
		if (checkPattern(addr, sig, check, size))
			return (void *)addr;

	return NULL;
}

void* CModule::findPattern(const char* pattern, bool checkCache)
{
	int size;
	char sig[64];
	bool check[64];

	size = parsePattern(pattern, sig, check, sizeof sig);

	if (!size)
		return NULL;

	if (checkCache)
	{
		auto cached = foundCachedPattern(sig, check, size);

		if (cached)
		{
			Con_DPrintf("Pattern '%s' founded in cache.", pattern);
			return cached->addr;
		}
	}

	void* addr = findPattern(sig, check, size);

	if (checkCache || !foundCachedPattern(sig, check, size))
	{
		auto cache = new pattern_cache_t;
		memcpy(cache->sig, sig, size);
		memcpy(cache->check, check, size);
		cache->size = size;
		cache->addr = addr;
		m_pcache.push_back(cache);
	}

	return addr;
}

void* CModule::findPattern(void* from, size_t range, const char* sig, const bool* check, size_t size) const
{
	for (char *addr = (char *)from, *end = addr + range - size; addr < end; addr++)
		if (checkPattern(addr, sig, check, size))
			return (void *)addr;

	return NULL;
}

void* CModule::findPattern(void* from, size_t range, const char* pattern) const
{
	int size;
	char sig[64];
	bool check[64];

	size = parsePattern(pattern, sig, check, sizeof sig);

	if (!size)
		return NULL;

	return findPattern(from, range, sig, check, size);
}

const char* CModule::findString(const char* string) const
{
	size_t size = strlen(string + 1);

	for (dword i = m_data.start, end = m_data.end - size; i < end; i++)
	{
		if (!memcmp((void *)i, string, size))
			return (const char *)i;
	}

	return NULL;
}

void* CModule::findPrefixedReference(byte prefix, void* ref, bool relative) const
{
	for (byte *pos = (byte *)m_code.start, *end = (byte *)m_code.end - 5; pos < end; pos++)
	{
		if (*pos == prefix)
		{
			if (relative)
			{
				if ((dword)pos + 5 + *(dword *)(pos + 1) == (dword)ref)
					return pos;
			}
			else
			{
				if (*(dword *)(pos + 1) == (dword)ref)
					return pos;
			}
		}
	}

	return NULL;
}

void* CModule::findStringReference(const char* string) const
{
	void* data = (void *)findString(string);
	if (!data)
		return NULL;

	void* ref = findPrefixedReference('\x68', data, false);
	if (ref)
		return ref;

	char sig[7] = {'\xC7', '\x04', '\x24'};
	*(void **)&sig[3] = ref;
	bool mask[7];
	memset(&mask, true, sizeof mask);
	return findPattern(sig, mask, sizeof sig);
}

bool CModule::containAddr(void* addr, module_space_e ms) const
{
	switch (ms)
	{
	case ms_code:
		return (dword)addr >= m_code.start && (dword)addr < m_code.end;
	case ms_data:
		return (dword)addr >= m_data.start && (dword)addr < m_data.end;
	}

	return (dword)addr >= m_baseAddress && (dword)addr < m_baseAddress + m_imageSize;
}

CModule::pattern_cache_t* CModule::foundCachedPattern(const char* sig, const bool* check, size_t size) const
{
	for (auto it = m_pcache.begin(), end = m_pcache.end(); it != end; it++)
	{
		auto cached = *it;

		if (cached->size != size)
			continue;

		if (!memcmp(sig, cached->sig, size) && !memcmp(check, cached->check, size))
			return cached;
	}

	return NULL;
}

CModule::symbol_cache_t* CModule::foundCachedSymbol(const char* name) const
{
	for (auto it = m_scache.begin(), end = m_scache.end(); it != end; it++)
	{
		auto cached = *it;

		if (!strcmp(name, cached->name))
			return cached;
	}

	return NULL;
}