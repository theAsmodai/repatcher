#ifndef CFUNCTION_H
#define CFUNCTION_H

struct arg_t
{
	basetype_e		type;
	register_e		reg;
	size_t			count;
};

class CFunction
{
public:
	CFunction(const char* description);

	register_e getReturnRegister() const;
	basetype_e getReturnType() const;
	arg_t* getArgs();
	size_t getArgsCount() const;
	size_t getArgsSize() const;
	size_t getStackArgsSize() const;
	bool hasConvertableArgs(bool amx) const;
	bool isCdecl() const;
	bool isValid() const;
	bool argsEqual(CFunction* with);
	void setDummyStackArgs(int count);

	friend class CHook;
	friend struct hookhandle_t;

private:
	basetype_e	m_rettype;
	register_e	m_retreg;
	arg_t		m_args[16];
	word		m_argscount;
	bool		m_valid;
	bool		m_cdecl;
	bool		m_retptr;
	bool		m_thiscall;
};

bool isConvertableArg(const arg_t* arg, bool amx);

#endif //CFUNCTION_H