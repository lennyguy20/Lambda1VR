/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/
//=========================================================
// Generic Monster - purely for scripted sequence work.
//=========================================================
#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monsters.h"
#include	"schedule.h"
#include	"weapons.h"

// For holograms, make them not solid so the player can walk through them
#define	SF_GENERICMONSTER_NOTSOLID					4 
#define SF_GENERICMONSTER_DONTBLEED					8
//=========================================================
// Monster's Anim Events Go Here
//=========================================================

class CGenericMonster : public CBaseMonster
{
public:
	void Spawn( void );
	void Precache( void );
	void SetYawSpeed( void );
	int Classify( void );
	void HandleAnimEvent( MonsterEvent_t *pEvent );
	int ISoundMask( void );
	void TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType );
	int TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType );
};

LINK_ENTITY_TO_CLASS( monster_generic, CGenericMonster )

//=========================================================
// Classify - indicates this monster's place in the 
// relationship table.
//=========================================================
int CGenericMonster::Classify( void )
{
	return CLASS_PLAYER_ALLY;
}

//=========================================================
// SetYawSpeed - allows each sequence to have a different
// turn rate associated with it.
//=========================================================
void CGenericMonster::SetYawSpeed( void )
{
	int ys;

	switch( m_Activity )
	{
	case ACT_IDLE:
	default:
		ys = 90;
	}

	pev->yaw_speed = ys;
}

//=========================================================
// HandleAnimEvent - catches the monster-specific messages
// that occur when tagged animation frames are played.
//=========================================================
void CGenericMonster::HandleAnimEvent( MonsterEvent_t *pEvent )
{
	switch( pEvent->event )
	{
	case 0:
	default:
		CBaseMonster::HandleAnimEvent( pEvent );
		break;
	}
}

//=========================================================
// ISoundMask - generic monster can't hear.
//=========================================================
int CGenericMonster::ISoundMask( void )
{
	return 0;
}

//=========================================================
// Spawn
//=========================================================
void CGenericMonster::Spawn()
{
	Precache();

	SET_MODEL( ENT( pev ), STRING( pev->model ) );
/*
	if( FStrEq( STRING( pev->model ), "models/player.mdl" ) )
		UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX);
*/
	if( FStrEq( STRING( pev->model ), "models/player.mdl" ) || FStrEq( STRING( pev->model ), "models/holo.mdl" ) )
		UTIL_SetSize( pev, VEC_HULL_MIN, VEC_HULL_MAX );
	else
		UTIL_SetSize( pev, VEC_HUMAN_HULL_MIN, VEC_HUMAN_HULL_MAX );

	pev->solid = SOLID_SLIDEBOX;
	pev->movetype = MOVETYPE_STEP;
	if( pev->spawnflags & SF_GENERICMONSTER_DONTBLEED )
	{
		m_bloodColor = DONT_BLEED;
		pev->health = 100;
	}
	else
	{
		m_bloodColor = BLOOD_COLOR_RED;
		pev->health = 8;
	}
	m_flFieldOfView = 0.5;// indicates the width of this monster's forward view cone ( as a dotproduct result )
	m_MonsterState = MONSTERSTATE_NONE;

	MonsterInit();

	if( pev->spawnflags & SF_GENERICMONSTER_NOTSOLID )
	{
		pev->solid = SOLID_NOT;
		pev->takedamage = DAMAGE_NO;
	}
}

//=========================================================
// Precache - precaches all resources this monster needs
//=========================================================
void CGenericMonster::Precache()
{
	PRECACHE_MODEL( STRING( pev->model ) );
}

void CGenericMonster::TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	if( pev->spawnflags & SF_GENERICMONSTER_DONTBLEED )
	{
		UTIL_Ricochet( ptr->vecEndPos, 1.0f );
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );
		return;
	}

	CBaseMonster::TraceAttack( pevAttacker, flDamage, vecDir, ptr, bitsDamageType );
}

//=========================================================
// AI Schedules Specific to this monster
//=========================================================
int CGenericMonster::TakeDamage( entvars_t *pevInflictor, entvars_t *pevAttacker, float flDamage, int bitsDamageType )
{
	if( pev->spawnflags & SF_GENERICMONSTER_DONTBLEED )
	{
		pev->health = pev->max_health / 2; // always trigger the 50% damage aitrigger

		if( flDamage > 0 )
		{
			SetConditions( bits_COND_LIGHT_DAMAGE );
		}

		if( flDamage >= 20 )
		{
			SetConditions( bits_COND_HEAVY_DAMAGE );
		}
		return TRUE;
	}

	return CBaseMonster::TakeDamage( pevInflictor, pevAttacker, flDamage, bitsDamageType );
}
