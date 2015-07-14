// vi: set ts=4 sw=4 :
// vim: set tw=75 :

// mutil.h - prototypes for utility functions to provide to plugins

/*
 * Copyright (c) 2001-2003 Will Day <willday@hpgx.net>
 *
 *    This file is part of Metamod.
 *
 *    Metamod is free software; you can redistribute it and/or modify it
 *    under the terms of the GNU General Public License as published by the
 *    Free Software Foundation; either version 2 of the License, or (at
 *    your option) any later version.
 *
 *    Metamod is distributed in the hope that it will be useful, but
 *    WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Metamod; if not, write to the Free Software Foundation,
 *    Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *    In addition, as a special exception, the author gives permission to
 *    link the code of this program with the Half-Life Game Engine ("HL
 *    Engine") and Modified Game Libraries ("MODs") developed by Valve,
 *    L.L.C ("Valve").  You must obey the GNU General Public License in all
 *    respects for all of the code used other than the HL Engine and MODs
 *    from Valve.  If you modify this file, you may extend this exception
 *    to your version of the file, but you are not obligated to do so.  If
 *    you do not wish to do so, delete this exception statement from your
 *    version.
 *
 */

#ifndef MUTIL_H
#define MUTIL_H

// Meta Utility Function table type.
typedef struct meta_util_funcs_s {
	void		(*pfnLogConsole)		(plid_t plid, const char *fmt, ...);
	void		(*pfnLogMessage)		(plid_t plid, const char *fmt, ...);
	void		(*pfnLogError)			(plid_t plid, const char *fmt, ...);
	void		(*pfnLogDeveloper)		(plid_t plid, const char *fmt, ...);
	void		(*pfnCenterSay)			(plid_t plid, const char *fmt, ...);
	void*		pfnCenterSayParms;
	void*		pfnCenterSayVarargs;
	int			(*pfnCallGameEntity)	(plid_t plid, const char *entStr, struct entvars_s *pev);
	int			(*pfnGetUserMsgID)		(plid_t plid, const char* msgname, int* size);
	const char*	(*pfnGetUserMsgName)	(plid_t plid, int msgid, int* size);
	const char*	(*pfnGetPluginPath)		(plid_t plid);
} mutil_funcs_t;

// Convenience macros for MetaUtil functions
#define LOG_CONSOLE			(*gpMetaUtilFuncs->pfnLogConsole)
#define LOG_MESSAGE			(*gpMetaUtilFuncs->pfnLogMessage)
#define LOG_ERROR			(*gpMetaUtilFuncs->pfnLogError)
#define LOG_DEVELOPER		(*gpMetaUtilFuncs->pfnLogDeveloper)

#endif /* MUTIL_H */
