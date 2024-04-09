//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Antlion Worker Variant that flings Headcrabs
//
//=============================================================================//

#include "cbase.h"
#include "npc_antlionflinger.h"
#include "npcevent.h"
#include "movevars_shared.h"
#include "ndebugoverlay.h"
#include "particle_parse.h"
#include "hl2_shareddefs.h"
#include "saverestore_utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS(npc_antlionflinger, CNPC_AntlionFlinger);

#define ANTLIONFLINGER_MODEL "models/antlion_worker.mdl"
#define ANTLION_FLINGER_MAX_HEADCRABS 10

// TODO: Make this a ConVar
#define ANTLION_HEADCRAB_FLING_SPEED 800
#define ANTLION_HEADCRAB_FLING_TOLERANCE (10 * 12)

enum eHeadcrabType
{
	HEADCRAB_CLASSIC = 0,
	HEADCRAB_FAST = 1,
	HEADCRAB_POISON = 2
};

// AnimEvents

// NOTE - This MUST be static so it doesn't clash with the Antlion Worker animevent.
static int AE_ANTLION_WORKER_SPIT;

// Storing off an index to the headcrab's classname here so we don't have
// to do string comparisons later
static string_t m_iszClassicHeadcrabClassname;
static string_t m_iszFastHeadcrabClassname;
static string_t m_iszPoisonHeadcrabClassname;

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CNPC_AntlionFlinger)

	DEFINE_FIELD(m_iNumActiveHeadcrabs, FIELD_INTEGER),
	DEFINE_UTLVECTOR(m_headcrabs, FIELD_EHANDLE),

	DEFINE_KEYFIELD(m_iHeadcrabType, FIELD_INTEGER, "headcrabType"),
	DEFINE_KEYFIELD(m_iHeadcrabCapacity, FIELD_INTEGER, "headcrabCapacity"),

END_DATADESC()

//---------------------------------------------------------
// Purpose: Constructor
//---------------------------------------------------------
CNPC_AntlionFlinger::CNPC_AntlionFlinger()
{
}

//---------------------------------------------------------
// Purpose: Precache needed resources
//---------------------------------------------------------
void CNPC_AntlionFlinger::Precache()
{
	PrecacheModel(ANTLIONFLINGER_MODEL);

	UTIL_PrecacheOther("npc_headcrab");
	UTIL_PrecacheOther("npc_headcrab_fast");
	UTIL_PrecacheOther("npc_headcrab_black");

	PrecacheScriptSound("NPC_Antlion.PoisonShoot");
	PrecacheParticleSystem("blood_impact_yellow_01");

	BaseClass::Precache();
}

//---------------------------------------------------------
// Purpose: Spawn entity into world
//---------------------------------------------------------
void CNPC_AntlionFlinger::Spawn()
{
	Precache();

	// Call the base class first so we can override with our own properties
	BaseClass::Spawn();

	SetModel(ANTLIONFLINGER_MODEL);
	SetHullType(HULL_MEDIUM);
	SetHullSizeNormal();

	SetSolid(SOLID_BBOX);
	AddSolidFlags(FSOLID_NOT_STANDABLE);
	SetMoveType(MOVETYPE_STEP);
	SetBloodColor(BLOOD_COLOR_GREEN);

	m_iHealth		= 50;
	m_flFieldOfView = -0.5;
	m_NPCState		= NPC_STATE_NONE;

	CapabilitiesClear();
	CapabilitiesAdd(bits_CAP_MOVE_GROUND | bits_CAP_INNATE_MELEE_ATTACK1 | bits_CAP_INNATE_MELEE_ATTACK2);
	//CapabilitiesAdd(bits_CAP_MOVE_JUMP);
	SetCollisionGroup(HL2COLLISION_GROUP_ANTLION);

	AddSpawnFlags(SF_NPC_LONG_RANGE | SF_ANTLION_WORKER);
	SetNavType(NAV_GROUND);
	SetMoveType(MOVETYPE_STEP);
	SetViewOffset(Vector(0, 0, 32));

	NPCInit();
}

//---------------------------------------------------------
// Purpose: Spawn entity into world
//---------------------------------------------------------
void CNPC_AntlionFlinger::Activate()
{
	m_iszClassicHeadcrabClassname = FindPooledString("npc_headcrab");
	m_iszFastHeadcrabClassname = FindPooledString("npc_headcrab_fast");
	m_iszPoisonHeadcrabClassname = FindPooledString("npc_headcrab_black");

	if (m_iHeadcrabCapacity > ANTLION_FLINGER_MAX_HEADCRABS)
		m_iHeadcrabCapacity = ANTLION_FLINGER_MAX_HEADCRABS;
	else if (m_iHeadcrabCapacity < 0)
		m_iHeadcrabCapacity = 0;

	m_headcrabs.SetSize(m_iHeadcrabCapacity);

	BaseClass::Activate();
}

//---------------------------------------------------------
// Purpose: AnimEvent Handling
//---------------------------------------------------------
void CNPC_AntlionFlinger::HandleAnimEvent(animevent_t* pEvent)
{
	if (pEvent->event == AE_ANTLION_WORKER_SPIT)
	{
		HeadcrabFling();
		return;
	}

	BaseClass::HandleAnimEvent(pEvent);
}

//---------------------------------------------------------
// Purpose: Schedule Selection
//---------------------------------------------------------
int CNPC_AntlionFlinger::SelectSchedule()
{
	switch (m_NPCState)
	{
		case NPC_STATE_COMBAT:
		{
			if (HasCondition(COND_SEE_ENEMY && COND_CAN_RANGE_ATTACK1) && (m_flNextAttack < gpGlobals->curtime) && (m_iNumActiveHeadcrabs < m_iHeadcrabCapacity))
			{
				return SCHED_ANTLIONFLINGER_SHOOT_CRABS;
			}

			// Melee if the enemy gets too close
			if (HasCondition(COND_CAN_MELEE_ATTACK1))
				return SCHED_ANTLION_POUNCE;

			return SCHED_ANTLION_WORKER_RUN_RANDOM;
			//return BaseClass::SelectSchedule();
		}
		break;
	}

	return BaseClass::SelectSchedule();
}

//---------------------------------------------------------
// Purpose: The Antlion should always be facing their enemy
//			otherwise they look like they're retreating when
//			moving ever so slightly.
//---------------------------------------------------------
bool CNPC_AntlionFlinger::OverrideMoveFacing(const AILocalMoveGoal_t& move, float flInterval)
{
	if (GetEnemy())
	{
		AddFacingTarget(GetEnemy(), GetEnemy()->WorldSpaceCenter(), 1.0f, 0.2f);
		return BaseClass::OverrideMoveFacing(move, flInterval);
	}

	return BaseClass::OverrideMoveFacing(move, flInterval);
}

//---------------------------------------------------------
// Purpose: Task handling
//---------------------------------------------------------
void CNPC_AntlionFlinger::StartTask(const Task_t* pTask)
{
	switch (pTask->iTask)
	{
		case TASK_ANTLIONFLINGER_SHOOT_CRABS:
			EmitSound("NPC_Antlion.MeleeAttackSingle");
			TaskComplete();
			break;

		default:
			BaseClass::StartTask(pTask);
			break;
	}
}

//---------------------------------------------------------
// Purpose: Task handling
//---------------------------------------------------------
void CNPC_AntlionFlinger::RunTask(const Task_t* pTask)
{
	BaseClass::RunTask(pTask);
}

//---------------------------------------------------------
// Purpose: Notify child crabs of my death
//---------------------------------------------------------
void CNPC_AntlionFlinger::Event_Killed(const CTakeDamageInfo& info)
{
	CBaseHeadcrab* pHeadcrab;
	for (int i = 0; i < m_iNumActiveHeadcrabs; i++)
	{
		pHeadcrab = dynamic_cast<CBaseHeadcrab*>(m_headcrabs[i].Get());
		if (pHeadcrab)
			pHeadcrab->Event_AntlionKilled();
	}

	BaseClass::Event_Killed(info);
}

//---------------------------------------------------------
// Purpose: Self-explanatory
//---------------------------------------------------------
void CNPC_AntlionFlinger::HeadcrabFling()
{
	if (m_iNumActiveHeadcrabs < m_iHeadcrabCapacity)
	{
		if (GetEnemy())
		{
			Vector vecSpitPos, vecTarget;
			GetAttachment("mouth", vecSpitPos);

			vecTarget = GetEnemy()->BodyTarget(vecSpitPos, true);

			CBaseHeadcrab* pHeadcrab;

			switch (m_iHeadcrabType)
			{
				case (HEADCRAB_CLASSIC):
				default:
				{
					pHeadcrab = (CBaseHeadcrab*)CreateEntityByName(m_iszClassicHeadcrabClassname.ToCStr());
					break;
				}
				case (HEADCRAB_FAST):
				{
					pHeadcrab = (CBaseHeadcrab*)CreateEntityByName(m_iszFastHeadcrabClassname.ToCStr());
					break;
				}
				case (HEADCRAB_POISON):
				{
					pHeadcrab = (CBaseHeadcrab*)CreateEntityByName(m_iszPoisonHeadcrabClassname.ToCStr());
					break;
				}
			}
			

			Vector throwVector;
			GetThrowVector(vecSpitPos, vecTarget, ANTLION_HEADCRAB_FLING_SPEED, &throwVector);

			pHeadcrab->SetAbsOrigin(vecSpitPos + Vector(0, 0, -8));
			pHeadcrab->SetAbsAngles(GetAbsAngles());
			pHeadcrab->SetOwnerEntity(this);

			EHANDLE hAntlion = this;
			pHeadcrab->FlungFromAntlion(hAntlion);
			DispatchSpawn(pHeadcrab);

			//throwVector[0] = throwVector[0] * 2;
			pHeadcrab->SetAbsVelocity(throwVector);

			// Save off this crab for later
			m_headcrabs[m_iNumActiveHeadcrabs] = pHeadcrab;

			// Make this headcrab like antlions (and vice-versa), TODO - NEED TO MAKE THIS SPECIFIC TO BIRTHED CRABS!
			pHeadcrab->AddClassRelationship(CLASS_ANTLION, D_LI, 50);
			AddClassRelationship(CLASS_HEADCRAB, D_LI, 50);

			m_iNumActiveHeadcrabs++;

			DispatchParticleEffect("blood_impact_yellow_01", vecSpitPos + RandomVector(-12.0f, -12.0f), RandomAngle(0, 360));

			EmitSound("NPC_Antlion.PoisonShoot");
			
			SetNextAttack(gpGlobals->curtime + RandomFloat(1.0f, 5.0f));
			return;
		}
	}
	else
	{
		Msg("Max of %d Headcrabs currently active! No more spawning", m_iHeadcrabCapacity);
		return;
	}
}


//---------------------------------------------------------
// Purpose: Calculates a throw vector for the headcrab fling
// 
// Code copied from Antlion worker spitball fling
// TODO: Clean this up
//---------------------------------------------------------
void CNPC_AntlionFlinger::GetThrowVector(const Vector& vecStartPos, const Vector& vecTarget, float flSpeed, Vector* vecOut)
{
	flSpeed = MAX(1.0f, flSpeed);

	float flGravity = GetCurrentGravity();

	Vector vecGrenadeVel = (vecTarget - vecStartPos);

	// throw at a constant time
	float time = vecGrenadeVel.Length() / flSpeed;
	vecGrenadeVel = vecGrenadeVel * (1.0 / time);

	// adjust upward toss to compensate for gravity loss
	vecGrenadeVel.z += flGravity * time * 0.5;

	Vector vecApex = vecStartPos + (vecTarget - vecStartPos) * 0.5;
	vecApex.z += 0.5 * flGravity * (time * 0.5) * (time * 0.5);


	trace_t tr;
	UTIL_TraceLine(vecStartPos, vecApex, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction != 1.0)
	{
		// fail!
		*vecOut = vec3_origin;
	}

	UTIL_TraceLine(vecApex, vecTarget, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr);
	if (tr.fraction != 1.0)
	{
		bool bFail = true;

		// Didn't make it all the way there, but check if we're within our tolerance range
		
		{
			float flNearness = (tr.endpos - vecTarget).LengthSqr();
			if (flNearness < Square(ANTLION_HEADCRAB_FLING_TOLERANCE))
			{
				bFail = false;
			}
		}

		if (bFail)
		{
			*vecOut = vec3_origin;
		}
	}

	*vecOut = vecGrenadeVel;
}


//-----------------------------------------------------------------------------
//
// Schedules
//
//-----------------------------------------------------------------------------
AI_BEGIN_CUSTOM_NPC(npc_antlionflinger, CNPC_AntlionFlinger)

	DECLARE_ANIMEVENT(AE_ANTLION_WORKER_SPIT);

	DECLARE_TASK(TASK_ANTLIONFLINGER_SHOOT_CRABS);

	//Schedules

	//==================================================
	// Fling those crabs!
	//==================================================

	DEFINE_SCHEDULE
	(
		SCHED_ANTLIONFLINGER_SHOOT_CRABS,

		"	Tasks"
		"		TASK_STOP_MOVING				0"
		"		TASK_FACE_ENEMY					0"
		"		TASK_SET_TOLERANCE_DISTANCE		512"
		"		TASK_ANTLIONFLINGER_SHOOT_CRABS 0"
		"		TASK_RANGE_ATTACK1				0"
		""
		"	Interrupts"
		"		COND_TASK_FAILED"
	)

AI_END_CUSTOM_NPC()