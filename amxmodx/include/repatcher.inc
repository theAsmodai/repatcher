/*
 *	RePatcher Natives
 */

#if defined _repatcher_included
	#endinput
#endif
#define _repatcher_included

#if AMXX_VERSION_NUM >= 175
	#pragma reqlib repatcher
	#if !defined AMXMODX_NOAUTOLOAD
		#pragma loadlib repatcher
	#endif
#else
	#pragma library repatcher
#endif

/*
|    CONSTANTS
*/

#define RP_CONTINUE			0		// execute all hooks and original function
#define RP_SUPERCEDE		-1		// supercede all
#define RP_SUPERCEDE_MAIN	1		// execute all hooks but supercede original function

enum HookFlag (<<=1)
{
	RPF_FORCE_ADDR = 1,				// force hook address to specified
	RPF_DEC_EDICT,					// decrease edict, pev and cbase indexes by one (useful for data arrays)
	RPF_ALLOW_NULL,					// allow null amxx-arrays (unsafe)
};

enum Lib
{
	Invalid_Library = 0
};

enum Hook
{
	Invalid_Hook = 0
};


/*
|    NATIVES
*/

/**
 * Retrieves a handle of a library loaded in hlds process memory.
 *
 * @param name			Filename of library.
 *
 * @return 				Handle of the library or Invalid_Library if not found.
 */
native Lib:rp_find_library(const name[]);

/**
 * Retrieves the address of exported function or variable. Note, that not all symbols are exported.
 *
 * @param lib           Handle of library containing export.
 * @param name			Name of export.
 *
 * @return              Address of export or 0 if failed.
 */
native rp_get_exported(Lib:lib, const name[]);

/**
 * Retrieves the address of symbol in specified library.
 *
 * @param lib           Handle of library containing export.
 * @param name			Symbol name.
 *
 * @return              Address of export or 0 if failed.
 */
native rp_get_symbol(Lib:lib, const symbol[]);

/**
* Searches memory for a signature in specified library. Signature format is same as in IDA. Example: "55 8B EC ? ? ? ? 51".
*
* @param lib            Handle of library containing a symbol.
* @param signature      Signature for memory.
*
* @return				Address of founded memory or 0 if failed.
*/
native rp_find_signature(Lib:lib, const signature[]);

/**
* Creates hook for specified address. It will be automatically enabled on create.
*
* @param address        Address in memory for hook.
* @param header         Function header. Sets arguments and return value convertaion. Example: "int my::function(int this@<ecx>, char* arg2, bool arg3)".
* @param handler        Name of public function (forward) that will be called on execute of hooked address.
* @param post			If true, call the forward after original function, otherwise call before.
* @param flags  		Flags for this hook from enum HookFlag.
*
* @return				Handle of hook or Invalid_Hook if failed.
*/
native Hook:rp_add_hook(address, const header[], const handler[], bool:post = false, HookFlag:flags = HookFlag:0);

/**
* Removes hook. Note: if you want temporary disable hook, use rp_set_hook_state.
*
* @param handle         Handle returned by rp_add_hook.
*
* @return				1 if success, or 0 if fails.
*/
native rp_remove_hook(&Hook:handle);

/**
* Set hook enabled or disabled.
*
* @param handle         Handle returned by rp_add_hook.
* @param enabled        Enable or disable hook.
*
* @return				1 if success, or 0 if fails.
*/
native rp_set_hook_state(Hook:handle, bool:enabled);

/**
* Get return of original function. Available only in post hooks.
*
*   #Getting string:
* @param string         Array for string.
* @param maxlen	        Maximum length of string.
*
*   #Getting cell or Float:
* @param value          Cell or Float variable for return value.
*
* @return				1 if success, or 0 if fails.
*/
native any:rp_get_original_return(any:...);

/**
* Get value that be returned to hook caller. You can use it in pre hooks, but value will be returned only if some handler setted it.
*
*   #Getting string:
* @param string         Array for string.
* @param maxlen	        Maximum length of string.
*
*   #Getting cell or Float:
* @param value          Cell or Float variable for return value.
*
* @return				1 if success, or 0 if fails.
*/
native any:rp_get_return(any:...);

/**
* Set return value of hook. Note: you must do it if function returns value and call of original function was superceeded.
*
* @param value          Custom return value. String values automaticaly reallocates in temp memory.
*
* @return				1 if success, or 0 if fails.
*/
native rp_set_return(any:...);

/**
* Set raw return value of hook, without type convertation. You can use it for return null.
*
* @param value          Custom return value.
*
* @return				1 if success, or 0 if fails.
*/
native rp_set_raw_return(any:value);

/**
* Set argument value.
*
* @param number         Argument number.
* @param value          Custom value.
*
* @return				1 if success, or 0 if fails.
*/
native rp_set_arg(number, any:...);

/**
* Gets text description for last ocurred error.
*
* @param error          Array for error string.
* @param maxlen         Maximum length of error string.
*
* @return				Nothing.
*/
native rp_get_error(error[], maxlen);


/*
|    STOCKS
*/

stock is_null(const any:array[])
{
	#pragma unused array
	#emit LOAD.S.pri 0xC
	#emit NOT
	#emit RETN
	return 0;
}