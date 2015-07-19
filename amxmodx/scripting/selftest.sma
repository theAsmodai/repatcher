#include <amxmodx>
#include <repatcher>

enum
{
	test_conversion,
	test_changestate,
	test_changestate2,
	test_supercede,
	test_supercede2,
	test_remove,
	test_return,
	test_args,
	test_finish
}

native rp_i_am_here()
native rp_conversion(a, b, const str[], cl, pl, Float:f, x, Float:f2)
native rp_retcheck(a, b, c)

new Lib:rp
new Hook:pre
new Hook:post
new Hook:rethandle
new Hook:retchecker
new Hook:retchecker2
new Hook:arghandle
new pre_return
new post_return
new return_id
new argchange_id
new retfunc
new argfunc

new descriptions[][] =
{
	"entvars_t* func()",
	"int func()",
	"short func()",
	"word func()",
	"char func()",
	"byte func()",
	"float func()",
	"edict_t *func()",
	"CBaseEntity* func()",
	"client_t* func()",
	"char* func()",
	"short* func()"
}

new descriptions2[][] =
{
	"void func(int a, int b@<eax>)",
	"void func(word a, word b@<eax>)",
	"void func(client_t* a, client_t *b@<eax>)",
	"void func(char* a, char* b@<eax>)",
	"void func(char a, char b@<eax>)"
}

public plugin_precache()
{
	new error[256];

	if (is_linux_server())
		rp = rp_find_library("repatcher_amxx_i386.so")
	else
		rp = rp_find_library("repatcher_amxx.dll")
	
	if (!rp)
	{
		rp_get_error(error, sizeof error - 1)
		server_print("Can't find repatcher lib: %s", error)
	}
	
	rp_i_am_here();
}

public rp_begin_test(number)
{
	new error[256];
	new desc[] = "void Func_ArgConversion(edict_t* a, int b@<eax>, const char *str, client_t* cl@<ecx>, CBaseMonster* pl, float f@st0, int x@xmm4, float f2)"

	switch(number)
	{
		case test_conversion:
		{
			new func = rp_get_symbol(rp, "Func_ArgConversion")
			if (!func)
			{
				rp_get_error(error, sizeof error - 1)
				server_print("Can't find Func_ArgConversion: %s", error)
				return
			}
			pre = rp_add_hook(func, desc, "conversion_hook", false);
			post = rp_add_hook(func, desc, "conversion_hook_post", true);
		}
		case test_changestate:
		{
			rp_set_hook_state(post, false)
		}
		case test_changestate2:
		{
			rp_set_hook_state(pre, false)
			rp_set_hook_state(post, true)
		}
		case test_supercede:
		{
			rp_set_hook_state(pre, true)
			pre_return = RP_SUPERCEDE_MAIN
		}
		case test_supercede2:
		{
			pre_return = RP_CONTINUE
		}
		case test_remove:
		{
			rp_remove_hook(pre)
			rp_remove_hook(post)
		}
		case test_return:
		{
			retfunc = rp_get_symbol(rp, "Func_Return")
			if (!retfunc)
			{
				rp_get_error(error, sizeof error - 1)
				server_print("Can't find Func_Return: %s", error)
				return
			}

			rethandle = rp_add_hook(retfunc, descriptions[0], "return_value_hook", false)
			retchecker = rp_add_hook(retfunc, descriptions[0], "return_value_check", true)
			retchecker2 = rp_add_hook(retfunc, "entvars_t* func()", "return_value_check2", true)
		}
		case test_args:
		{
			rp_remove_hook(rethandle)
			rp_remove_hook(retchecker)
			rp_remove_hook(retchecker2)

			argfunc = rp_get_symbol(rp, "Func_Arg")
			if (!argfunc)
			{
				rp_get_error(error, sizeof error - 1)
				server_print("Can't find Func_Arg: %s", error)
				return
			}
			arghandle = rp_add_hook(argfunc, descriptions2[0], "change_arg", false)
		}
		case test_finish:
		{
			rp_remove_hook(arghandle)
		}
	}
}

public conversion_hook(a, b, const str[], cl, pl, Float:f, x, Float:f2)
{
	rp_conversion(a, b, str, cl, pl, f, x, f2)
	return pre_return
}

public conversion_hook_post(a, b, const str[], cl, pl, Float:f, x, Float:f2)
{
	rp_conversion(a, b, str, cl, pl, f, x, f2)
	return post_return
}

public return_value_hook()
{
	switch(return_id)
	{
		case 0: // entvars
		{
		}
		case 1: // int
		{
			rp_set_return(12345)
		}
		case 2: // short
		{
			rp_set_return(-12345)
		}
		case 3: // word
		{
			rp_set_return(0xf0011)
		}
		case 4: // char
		{
			rp_set_return(-123)
		}
		case 5: // byte
		{
			rp_set_return(0xf12)
		}
		case 6: // float
		{
			rp_set_return(56789.0)
		}
		case 7: // edict
		{
			rp_set_return(7)
		}
		case 8: // cbase
		{
			rp_set_return(37)
		}
		case 9: // client
		{
			rp_set_return(9)
		}
		case 10: // string
		{
			rp_set_return("hehehe")
		}
		case 11: // array
		{
			new value[3] = {0x12345, -2, 19}
			rp_set_return(value, 3)
		}
	}
}

public return_value_check()
{
	new value
	rp_get_return(value)
	new true_value

	switch(return_id++)
	{
		case 0: // entvars
		{
			true_value = (value == 11)
		}
		case 1: // int
		{
			true_value = (value == 12345)
		}
		case 2: // short
		{
			true_value = (value == -12345)
		}
		case 3: // word
		{
			true_value = (value == 0x0011)
		}
		case 4: // char
		{
			true_value = (value == -123)
		}
		case 5: // byte
		{
			true_value = (value == 0x12)
		}
		case 6: // float
		{
			true_value = floatabs(value - 56789.0) < 0.01
		}
		case 7: // edict
		{
			true_value = (value == 7)
		}
		case 8: // cbase
		{
			true_value = (value == 37)
		}
		case 9: // client
		{
			true_value = (value == 9)
		}
		case 10: // string
		{
			new string[32]
			rp_get_return(string, sizeof string - 1)
			true_value = !strcmp(string, "hehehe")
		}
		case 11: // array
		{
			new value[3] = {0x2345, -2, 19}
			new arr[3]
			rp_get_return(arr, 3)
			true_value = value[0] == arr[0] && value[1] == arr[1] && value[2] == arr[2]
			server_print("11val: %p %i %i", arr[0], arr[1], arr[2])
		}
	}

	rp_retcheck(0, true_value, value)
}

public return_value_check2()
{
	new value
	rp_get_original_return(value)
	rp_retcheck(1, value == 11, value)
}

public change_rethook()
{
	rp_remove_hook(rethandle)
	rethandle = rp_add_hook(retfunc, descriptions[return_id], "return_value_hook", false)
	rp_remove_hook(retchecker)
	retchecker = rp_add_hook(retfunc, descriptions[return_id], "return_value_check", true)
}

public change_arg()
{
	switch(argchange_id++)
	{
		case 0:
		{
			rp_set_arg(1, 12345)
			rp_set_arg(2, 12345)
		}
		case 1:
		{
			rp_set_arg(1, 0xF0001234)
			rp_set_arg(2, 0xF0001234)
		}
		case 2:
		{
			rp_set_arg(1, 4)
			rp_set_arg(2, 4)
		}
		case 3:
		{
			rp_set_arg(1, "lalala")
			rp_set_arg(2, "lalala")
		}
	}
}

public change_arghook()
{
	if(argchange_id < 5)
	{
		rp_remove_hook(arghandle)
		arghandle = rp_add_hook(argfunc, descriptions2[argchange_id], "change_arg", false)
	}
}