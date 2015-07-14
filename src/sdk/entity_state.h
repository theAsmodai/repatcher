#ifndef ENTITY_STATE_H
#define ENTITY_STATE_H

// For entityType below
#define ENTITY_NORMAL		(1<<0)
#define ENTITY_BEAM			(1<<1)

// Entity state is used for the baseline and for delta compression of a packet of 
// entities that is sent to a client.
typedef struct entity_state_s
{
// Fields which are filled in by routines outside of delta compression
	int			entityType;
	// Index into cl_entities array for this entity.
	int			number;      
	float		msg_time;

	// Message number last time the player/entity state was updated.
	int			messagenum;		

	// Fields which can be transitted and reconstructed over the network stream
	vec3_t		origin;
	vec3_t		angles;

	int			modelindex;
	int			sequence;
	float		frame;
	int			colormap;
	short		skin;
	short		solid;
	int			effects;
	float		scale;

	byte		eflags;
	
	// Render information
	int			rendermode;
	int			renderamt;
	color24		rendercolor;
	int			renderfx;

	int			movetype;
	float		animtime;
	float		framerate;
	int			body;
	byte		controller[4];
	byte		blending[4];
	vec3_t		velocity;

	// Send bbox down to client for use during prediction.
	vec3_t		mins;    
	vec3_t		maxs;

	int			aiment;
	// If owned by a player, the index of that player ( for projectiles ).
	int			owner; 

	// Friction, for prediction.
	float		friction;       
	// Gravity multiplier
	float		gravity;		

// PLAYER SPECIFIC
	int			team;
	int			playerclass;
	int			health;
	qboolean	spectator;  
	int         weaponmodel;
	int			gaitsequence;
	// If standing on conveyor, e.g.
	vec3_t		basevelocity;   
	// Use the crouched hull, or the regular player hull.
	int			usehull;		
	// Latched buttons last time state updated.
	int			oldbuttons;     
	// -1 = in air, else pmove entity number
	int			onground;		
	int			iStepLeft;
	// How fast we are falling
	float		flFallVelocity;  

	float		fov;
	int			weaponanim;

	// Parametric movement overrides
	vec3_t				startpos;
	vec3_t				endpos;
	float				impacttime;
	float				starttime;

	// For mods
	int			iuser1;
	int			iuser2;
	int			iuser3;
	int			iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
	vec3_t		vuser1;
	vec3_t		vuser2;
	vec3_t		vuser3;
	vec3_t		vuser4;
} entity_state_t;

typedef struct clientdata_s
{
	vec3_t		origin;
	vec3_t		velocity;

	int			viewmodel;
	vec3_t		punchangle;
	int			flags;
	int			waterlevel;
	int			watertype;
	vec3_t		view_ofs;
	float		health;

	int			bInDuck;

	int			weapons; // remove?
	
	int			flTimeStepSound;
	int			flDuckTime;
	int			flSwimTime;
	int			waterjumptime;

	float		maxspeed;

	float		fov;
	int			weaponanim;

	int			m_iId;
	int			ammo_shells;
	int			ammo_nails;
	int			ammo_cells;
	int			ammo_rockets;
	float		m_flNextAttack;
	
	int			tfstate;

	int			pushmsec;

	int			deadflag;

	char		physinfo[256];

	// For mods
	int			iuser1;
	int			iuser2;
	int			iuser3;
	int			iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
	vec3_t		vuser1;
	vec3_t		vuser2;
	vec3_t		vuser3;
	vec3_t		vuser4;
} clientdata_t;

// Info about weapons player might have in his/her possession
typedef struct weapon_data_s
{
	int			m_iId;
	int			m_iClip;

	float		m_flNextPrimaryAttack;
	float		m_flNextSecondaryAttack;
	float		m_flTimeWeaponIdle;

	int			m_fInReload;
	int			m_fInSpecialReload;
	float		m_flNextReload;
	float		m_flPumpTime;
	float		m_fReloadTime;

	float		m_fAimedDamage;
	float		m_fNextAimBonus;
	int			m_fInZoom;
	int			m_iWeaponState;

	int			iuser1;
	int			iuser2;
	int			iuser3;
	int			iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
} weapon_data_t;

typedef struct local_state_s
{
	entity_state_t	playerstate;
	clientdata_t	client;
	weapon_data_t	weapondata[64];
} local_state_t;

#endif // ENTITY_STATE_H
