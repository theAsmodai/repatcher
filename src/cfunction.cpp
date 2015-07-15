#include "precompiled.h"
#include "cfunction.h"

static const char* remove_words[] =
{
	"const",
	"__cdecl",
	"__fastcall",
	"__usercall",
	"__stdcall",
	"__thiscall",
	"unsigned", // TODO: remove after type system refactoring
	"signed"
};

CFunction::CFunction(const char* description)
{
	char desc[512];
	char* c;

	memset(&m_args, 0, sizeof m_args);
	m_valid = false;
	m_thiscall = strstr(description, "__thiscall") != NULL;

	strncpy(desc, description, sizeof desc - 1);
	desc[sizeof desc - 1] = '\0';

	// remove const, __cdecl... etc
	for (size_t i = 0; i < ARRAYSIZE(remove_words); i++)
	{
		c = desc;
		while ((c = strstr(c, remove_words[i])) != NULL)
		{
			char* from = c + strlen(remove_words[i]);
			memmove(c, from, strlen(from) + 1);
		}
	}

	char* args_start = strchr(desc, '(');
	if (!args_start)
	{
		setError("Function description must contain arguments in ()");
		return;
	}
	*args_start++ = '\0';

	c = strchr(args_start, ')');
	if (!c)
	{
		setError("Function description must contain arguments in ()");
		return;
	}
	*c = '\0';

	char* nptr = strrchr(desc, '*');
	char* name;

	if (nptr)
	{
		m_retptr = true;
		*nptr = '\0';
		name = nptr + 1;
	}
	else
	{
		m_retptr = false;
		name = strrchr(desc, ' ');
		if (!name)
		{
			setError("Function without name");
			return;
		}
		*name++ = '\0';
	}

	m_rettype = getBaseForType(desc, m_retptr);

	switch (m_rettype)
	{
	case bt_float:
	case bt_double:
		m_retreg = r_st0;
		break;

	case bt_int:
	case bt_short:
	case bt_word:
	case bt_char:
	case bt_byte:
	case bt_cbase:
	case bt_entvars:
	case bt_edict:
	case bt_client:
	case bt_string:
		m_retreg = r_eax;
		break;

	case bt_void:
		m_retreg = r_unknown;
		m_rettype = bt_void;
		break;

	default:
		Log_Error(NULL, "Invalid function return type '%s', assumed void", desc);
		m_retreg = r_unknown;
		m_rettype = bt_void;
	}

	/*char* retreg = NULL;
	if ((retreg = strchr(name, '@')) != NULL)
		*retreg++ = NULL;*/

	char* args[16];
	m_argscount = parse(args_start, args, sizeof args - 1, ',');
	m_cdecl = true;

	for (int i = 0; i < m_argscount; i++)
	{
		auto arg = &m_args[i];

		trim(args[i]);

		char* reg;
		if ((reg = strchr(args[i], '@')) != NULL)
		{
			*reg++ = '\0';
			arg->reg = getRegByName(reg);
		}
		else
			arg->reg = r_unknown;

		if (arg->reg != r_unknown)
			m_cdecl = false;

		c = strchr(args[i], '[');

		if (c)
		{
			*c++ = '\0';
			char* e = strchr(c, ']');
			if (!e)
			{
				setError("Unclosed array bracket [");
				return;
			}
			*e = '\0';
			arg->count = atoi(c);
		}
		else
			arg->count = 1;

		char* arg_name;
		char* ptr = strrchr(args[i], '*');

		if (ptr)
		{
			*ptr = '\0';
			arg_name = ptr + 1;
		}
		else
		{
			arg_name = strrchr(args[i], ' ');
			if (!arg_name)
			{
				setError("Argument %i name missed.", i + 1);
				return;
			}
			*arg_name++ = '\0';
		}

		arg->type = getBaseForType(args[i], ptr != NULL);

		if (arg->type == bt_unknown)
		{
			setError("Unsupportable argument %i type name %s.", i + 1, args[i]);
			return;
		}

		if (arg->count > 1)
		{
			size_t arg_size = getTypeSize(arg->type);

			if (arg_size == sizeof(double) && arg->type != bt_double)
			{
				setError("Only double allowed as 64 bit array element.");
				return;
			}

			// simplify type
			switch (arg_size)
			{
			case sizeof(char) :
				arg->type = bt_char; break;
			case sizeof(short) :
				arg->type = bt_short; break;
			case sizeof(int) :
				arg->type = bt_int; break;
			case sizeof(double) :
				arg->type = bt_double; break;
			}
		}

		switch (arg->reg)
		{
		case r_eax:
		case r_ebx:
		case r_ecx:
		case r_edx:
		case r_esi:
		case r_edi:
		case r_ebp:
		case r_esp:
			if (arg->type == bt_double)
			{
				setError("Argument %i type 'double' can't be passed to function via 32-bit register.", i);
				return;
			}
			break;

		case r_st0:
			if (arg->type != bt_float && arg->type != bt_double)
			{
				setError("Only 'float' and 'double' types can be passed to function via st0 register (arg %i).", i);
				return;
			}
			break;

		case r_st1:
		case r_st2:
		case r_st3:
		case r_st4:
		case r_st5:
		case r_st6:
		case r_st7:
			setError("Only st0 fpu register can be used in fastcall (arg %i).", i);
			return;

		case r_xmm0:
		case r_xmm1:
		case r_xmm2:
		case r_xmm3:
		case r_xmm4:
		case r_xmm5:
		case r_xmm6:
		case r_xmm7:
			if (arg->type != bt_int && arg->type != bt_float)
			{
				setError("Only 'int' and 'float' types can be passed to function via xmm register (arg %i).", i);
				return;
			}
			break;

		case r_ax:
		case r_bx:
		case r_cx:
		case r_dx:
			if (getTypeSize(arg->type) != sizeof(short))
			{
				setError("Incompatible type and register size %i/%i (arg %i).", getTypeSize(arg->type), sizeof(short));
				return;
			}
			break;

		case r_al:
		case r_ah:
		case r_bl:
		case r_bh:
		case r_cl:
		case r_ch:
		case r_dl:
		case r_dh:
			if (getTypeSize(arg->type) != sizeof(char))
			{
				setError("Incompatible type and register size %i/%i (arg %i).", getTypeSize(arg->type), sizeof(char));
				return;
			}
			break;
		}
	}

	if (m_thiscall)
	{
		if (!m_argscount)
		{
			setError("__thiscall function without arguments.");
			return;
		}

		m_args[0].reg = r_ecx;
	}

	m_valid = true;
}

register_e CFunction::getReturnRegister() const
{
	return m_retreg;
}

basetype_e CFunction::getReturnType() const
{
	return m_rettype;
}

arg_t* CFunction::getArgs()
{
	return m_args;
}

size_t CFunction::getArgsCount() const
{
	return m_argscount;
}

bool CFunction::isCdecl() const
{
	return m_cdecl;
}

bool CFunction::isValid() const
{
	return m_valid;
}

bool CFunction::argsEqual(CFunction* with)
{
	return m_argscount == with->m_argscount && !memcmp(m_args, with->m_args, m_argscount * sizeof(arg_t));
}

size_t CFunction::getArgsSize() const
{
	size_t size = 0;

	for (size_t i = 0; i < m_argscount; i++)
		size += getTypePushSize(m_args[i].type);

	return size;
}

size_t CFunction::getStackArgsSize() const
{
	size_t size = 0;

	for (size_t i = 0; i < m_argscount; i++)
		if (m_args[i].reg == r_unknown)
			size += getTypePushSize(m_args[i].type);

	return size;
}

bool CFunction::hasConvertableArgs(bool amx) const
{
	for (size_t i = 0; i < m_argscount; i++)
		if (isConvertableArg(&m_args[i], amx))
			return true;

	return false;
}

bool isConvertableArg(const arg_t* arg, bool amx)
{
	if (amx)
		return true;

	switch (arg->type)
	{
	case bt_edict:
	case bt_entvars:
	case bt_cbase:
	case bt_client:
		return true;

	//case bt_string:
		//return amx;
	}

	return false;
}