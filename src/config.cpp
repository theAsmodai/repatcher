#include "precompiled.h"
#include "config.h"

CConfig cfg;

bool CConfig::load()
{
	enum section_e
	{
		sect_no,
		sect_types
	};

	char path[MAX_PATH];
	char line[512];
	char* pos;
	section_e sect = sect_no;
	int line_number = 0;

	if (g_amxxAttached)
	{
#ifdef _WIN32
		snprintf(path, sizeof path - 1, "%s\\%s", g_gameName, g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(NULL), "amxx_configsdir"));
#else
		snprintf(path, sizeof path - 1, "%s/%s", g_gameName, g_engfuncs.pfnInfoKeyValue(g_engfuncs.pfnGetInfoKeyBuffer(NULL), "amxx_configsdir"));
#endif
	}
	else
	{
		strncpy(path, mUtil->pfnGetPluginPath(PLID), sizeof path - 1);
		path[sizeof path - 1] = '\0';
		pos = strrchr(path, '/');
		if (pos) *pos = '\0';
	}

	strcat(path, "/repatcher.ini");
	FILE* fp = fopen(path, "rt");

	if (!fp) return false;

	while (!feof(fp))
	{
		fgets(line, sizeof line, fp);
		line_number++;

		if (line[0] == ';' || line[0] == '#' || (line[0] == '/' && line[1] == '/'))
			continue;

		if (line[0] == '[')
		{
			sect = sect_no;

			char* send = strchr(line, ']');
			if (!send)
			{
				Con_Printf("Unclosed config section name at line %i\n", line_number);
				continue;
			}

			*send = '\0';

			if (!strcmp(line + 1, "types"))
				sect = sect_types;
			else
				Con_Printf("Unnknown config section name '%s' at line %i\n", line + 1, line_number);

			continue;
		}

		if (sect == sect_no)
			continue;

		char* v = strchr(line, '=');
		if (!v) continue;
		*v++ = '\0';

		trim(line);
		trim(v);

		switch (sect)
		{
		case sect_types:
			addType(line, v);
		}
	}

	sortTypes();
	return true;
}