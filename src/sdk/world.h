#ifndef WORLD_H
#define WORLD_H

#include "const.h"

typedef struct moveclip_s
{
	vec3_t			boxmins, boxmaxs;// enclose the test object along entire move
	float			*mins, *maxs;	// size of the moving object
	vec3_t			mins2, maxs2;	// size when clipping against mosnters
	float			*start, *end;
	trace_t			trace;
	short			type1;
	short			type2;
	struct edict_s	*passedict;
	int				hullnum;
} moveclip_t;

typedef struct areanode_s
{
	int					axis;      // -1 = leaf node
	float				dist;
	struct areanode_s*	children[2];
	link_t				trigger_edicts;
	link_t				solid_edicts;
} areanode_t;

#endif // WORLD_H