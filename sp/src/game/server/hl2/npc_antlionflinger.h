//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Antlion Worker Variant that flings Headcrabs
//
//=============================================================================//

#ifndef NPC_ANTLIONFLINGER_H
#define NPC_ANTLIONFLINGER_H
#ifdef _WIN32
#pragma once
#endif

#include "npc_antlion.h"
#include "npc_headcrab.h"

// Forward declaration
class CBaseHeadcrab;

class CNPC_AntlionFlinger : public CNPC_Antlion
{
	DECLARE_CLASS(CNPC_AntlionFlinger, CNPC_Antlion);

public:

	CNPC_AntlionFlinger();

	void	Precache() override;
	void	Spawn() override;
	void	Activate() override;

	void	HandleAnimEvent(animevent_t* pEvent) override;

	int		SelectSchedule() override;
	bool	OverrideMoveFacing(const AILocalMoveGoal_t& move, float flInterval) override;
	void	StartTask(const Task_t* pTask) override;
	void	RunTask(const Task_t* pTask) override;

	void	Event_Killed(const CTakeDamageInfo& info) override;

	void	HeadcrabFling();
	void	Event_OnHeadcrabKilled() { m_iNumActiveHeadcrabs--; }

	DECLARE_DATADESC();
	DEFINE_CUSTOM_AI;

private:
	void	GetThrowVector(const Vector& vecStartPos, const Vector& vecTarget, float flSpeed, Vector* vecOut);

private:
	int		m_iNumActiveHeadcrabs;
	int		m_iHeadcrabType;
	int		m_iHeadcrabCapacity;

	CUtlVector<CHandle<CBaseHeadcrab>> m_headcrabs;


private:

	//==================================================
	// AntlionFlinger Conditions
	//==================================================

	enum
	{
		COND_ANTLIONFLINGER_HAS_ACTIVE_CRABS = COND_ANTLION_LAST_CONDITION,
	};

	//==================================================
	// AntlionFlinger Schedules
	//==================================================

	enum
	{
		SCHED_ANTLIONFLINGER_SHOOT_CRABS = SCHED_ANTLION_LAST_SHARED,
	};

	//==================================================
	// AntlionFlinger Tasks
	//==================================================

	enum
	{
		TASK_ANTLIONFLINGER_SHOOT_CRABS = TASK_ANTLION_LAST_SHARED,
	};
};

#endif // NPC_ANTLIONFLINGER_H
