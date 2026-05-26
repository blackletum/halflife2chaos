//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: A copy of func_movelinear that uses a studio model.
//
//=============================================================================//

#include "cbase.h"
#include "prop_movelinear.h"
#include "entitylist.h"
#include "locksounds.h"
#include "ndebugoverlay.h"
#include "engine/IEngineSound.h"
#include "physics_saverestore.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// -------------------------------
//  SPAWN_FLAGS
// -------------------------------
#define SF_MOVELINEAR_NOTSOLID		8

enum togglemovetypes_t
{
	MOVE_TOGGLE_NONE = 0,
	MOVE_TOGGLE_LINEAR = 1,
	MOVE_TOGGLE_ANGULAR = 2,
};

LINK_ENTITY_TO_CLASS(prop_movelinear, CPropMoveLinear);


BEGIN_DATADESC(CPropMoveLinear)

	DEFINE_KEYFIELD(m_vecMoveDir, FIELD_VECTOR, "movedir"),
	DEFINE_KEYFIELD(m_soundStart, FIELD_SOUNDNAME, "StartSound"),
	DEFINE_KEYFIELD(m_soundStop, FIELD_SOUNDNAME, "StopSound"),
	DEFINE_FIELD(m_currentSound, FIELD_SOUNDNAME),
	DEFINE_KEYFIELD(m_flBlockDamage, FIELD_FLOAT, "BlockDamage"),
	DEFINE_KEYFIELD(m_flStartPosition, FIELD_FLOAT, "StartPosition"),
	DEFINE_KEYFIELD(m_flMoveDistance, FIELD_FLOAT, "MoveDistance"),

	DEFINE_INPUTFUNC(FIELD_VOID, "Open", InputOpen),
	DEFINE_INPUTFUNC(FIELD_VOID, "Close", InputClose),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetPosition", InputSetPosition),
	DEFINE_INPUTFUNC(FIELD_FLOAT, "SetSpeed", InputSetSpeed),

	DEFINE_OUTPUT(m_OnFullyOpen, "OnFullyOpen"),
	DEFINE_OUTPUT(m_OnFullyClosed, "OnFullyClosed"),

	//DEFINE_FUNCTION(StopMoveSound),

	//basetoggle
	DEFINE_FIELD(m_toggle_state, FIELD_INTEGER),
	DEFINE_FIELD(m_flMoveDistance, FIELD_FLOAT),
	DEFINE_FIELD(m_flWait, FIELD_FLOAT),
	DEFINE_FIELD(m_flLip, FIELD_FLOAT),
	DEFINE_FIELD(m_vecPosition1, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_vecPosition2, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_vecMoveAng, FIELD_VECTOR),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD(m_vecAngle1, FIELD_VECTOR),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD(m_vecAngle2, FIELD_VECTOR),		// UNDONE: Position could go through transition, but also angle?
	DEFINE_FIELD(m_flHeight, FIELD_FLOAT),
	DEFINE_FIELD(m_hActivator, FIELD_EHANDLE),
	DEFINE_FIELD(m_vecFinalDest, FIELD_POSITION_VECTOR),
	DEFINE_FIELD(m_vecFinalAngle, FIELD_VECTOR),
	DEFINE_FIELD(m_sMaster, FIELD_STRING),
	DEFINE_FIELD(m_movementType, FIELD_INTEGER),	// Linear or angular movement? (togglemovetypes_t)

	END_DATADESC()


//------------------------------------------------------------------------------
// Purpose: Called before spawning, after keyvalues have been parsed.
//------------------------------------------------------------------------------
void CPropMoveLinear::Spawn(void)
{
	// Convert movedir from angles to a vector
	QAngle angMoveDir = QAngle(m_vecMoveDir.x, m_vecMoveDir.y, m_vecMoveDir.z);
	AngleVectors(angMoveDir, &m_vecMoveDir);

	SetMoveType(MOVETYPE_PUSH);
	BaseClass::Spawn();
	SetModel(STRING(GetModelName()));

	// Don't allow zero or negative speeds
	if (m_flSpeed <= 0)
	{
		m_flSpeed = 100;
	}

	// If move distance is set to zero, use with width of the 
	// brush to determine the size of the move distance
	if (m_flMoveDistance <= 0)
	{
		Vector vecOBB = CollisionProp()->OBBSize();
		vecOBB -= Vector(2, 2, 2);
		m_flMoveDistance = DotProductAbs(m_vecMoveDir, vecOBB) - m_flLip;
	}

	m_vecPosition1 = GetAbsOrigin() - (m_vecMoveDir * m_flMoveDistance * m_flStartPosition);
	m_vecPosition2 = m_vecPosition1 + (m_vecMoveDir * m_flMoveDistance);
	m_vecFinalDest = GetAbsOrigin();

	SetTouch(NULL);

	// It is solid?
	SetSolid(SOLID_VPHYSICS);

	if (FBitSet(m_spawnflags, SF_MOVELINEAR_NOTSOLID))
	{
		AddSolidFlags(FSOLID_NOT_SOLID);
	}

	//CreateVPhysics();
}


bool CPropMoveLinear::ShouldSavePhysics(void)
{
	// don't save physics for func_water_analog, regen
	return !FClassnameIs(this, "func_water_analog");

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPropMoveLinear::CreateVPhysics(void)
{
	if (!FClassnameIs(this, "func_water_analog"))
	{
		//normal door
		if (!IsSolidFlagSet(FSOLID_NOT_SOLID))
		{
			VPhysicsInitShadow(false, false);
		}
	}
	else
	{
		// special contents
		AddSolidFlags(FSOLID_VOLUME_CONTENTS);
		//SETBITS( m_spawnflags, SF_DOOR_SILENT );	// water is silent for now

		IPhysicsObject* pPhysics = VPhysicsInitShadow(false, false);
		fluidparams_t fluid;

		Assert(CollisionProp()->GetCollisionAngles() == vec3_angle);
		fluid.damping = 0.01f;
		fluid.surfacePlane[0] = 0;
		fluid.surfacePlane[1] = 0;
		fluid.surfacePlane[2] = 1;
		fluid.surfacePlane[3] = CollisionProp()->GetCollisionOrigin().z + CollisionProp()->OBBMaxs().z - 1;
		fluid.currentVelocity.Init(0, 0, 0);
		fluid.torqueFactor = 0.1f;
		fluid.viscosityFactor = 0.01f;
		fluid.pGameData = static_cast<void*>(this);

		//FIXME: Currently there's no way to specify that you want slime
		fluid.contents = CONTENTS_WATER;

		m_pFluidController = physenv->CreateFluidController(pPhysics, &fluid);
	}

	return true;
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropMoveLinear::Precache(void)
{
	if (m_soundStart != NULL_STRING)
	{
		PrecacheScriptSound((char*)STRING(m_soundStart));
	}
	if (m_soundStop != NULL_STRING)
	{
		PrecacheScriptSound((char*)STRING(m_soundStop));
	}
	m_currentSound = NULL_STRING;
	BaseClass::Precache();
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropMoveLinear::MoveTo(Vector vPosition, float flSpeed)
{
	if (flSpeed != 0)
	{
		if (m_soundStart != NULL_STRING)
		{
			if (m_currentSound == m_soundStart)
			{
				StopSound(entindex(), CHAN_BODY, (char*)STRING(m_soundStop));
			}
			else
			{
				m_currentSound = m_soundStart;
				CPASAttenuationFilter filter(this);

				EmitSound_t ep;
				ep.m_nChannel = CHAN_BODY;
				ep.m_pSoundName = (char*)STRING(m_soundStart);
				ep.m_flVolume = 1;
				ep.m_SoundLevel = SNDLVL_NORM;

				EmitSound(filter, entindex(), ep);
			}
		}

		LinearMove(vPosition, flSpeed);

		if (m_pFluidController)
		{
			m_pFluidController->WakeAllSleepingObjects();
		}

		// Clear think (that stops sounds)
		SetThink(NULL);
	}
}


//------------------------------------------------------------------------------
// Purpose: Stop sounds at the next think, rather than here as another
// SetPosition call might immediately follow the end of this move
//------------------------------------------------------------------------------
/*
void CPropMoveLinear::StopMoveSound(void)
{
	if (m_soundStart != NULL_STRING && (m_currentSound == m_soundStart))
	{
		StopSound(entindex(), CHAN_BODY, (char*)STRING(m_soundStart));
	}

	if (m_soundStop != NULL_STRING && (m_currentSound != m_soundStop))
	{
		m_currentSound = m_soundStop;
		CPASAttenuationFilter filter(this);

		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = (char*)STRING(m_soundStop);
		ep.m_flVolume = 1;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound(filter, entindex(), ep);
	}

	SetThink(NULL);
}
*/

//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropMoveLinear::MoveDone(void)
{
	if (m_soundStart != NULL_STRING && (m_currentSound == m_soundStart))
	{
		StopSound(entindex(), CHAN_BODY, (char*)STRING(m_soundStart));
	}

	if (m_soundStop != NULL_STRING && (m_currentSound != m_soundStop))
	{
		m_currentSound = m_soundStop;
		CPASAttenuationFilter filter(this);

		EmitSound_t ep;
		ep.m_nChannel = CHAN_BODY;
		ep.m_pSoundName = (char*)STRING(m_soundStop);
		ep.m_flVolume = 1;
		ep.m_SoundLevel = SNDLVL_NORM;

		EmitSound(filter, entindex(), ep);
	}

	SetThink(NULL);

	switch (m_movementType)
	{
	case MOVE_TOGGLE_LINEAR:
		LinearMoveDone();
		break;
	case MOVE_TOGGLE_ANGULAR:
		AngularMoveDone();
		break;
	}
	m_movementType = MOVE_TOGGLE_NONE;
	BaseClass::MoveDone();

	if (GetAbsOrigin() == m_vecPosition2)
	{
		m_OnFullyOpen.FireOutput(this, this);
	}
	else if (GetAbsOrigin() == m_vecPosition1)
	{
		m_OnFullyClosed.FireOutput(this, this);
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropMoveLinear::Use(CBaseEntity* pActivator, CBaseEntity* pCaller, USE_TYPE useType, float value)
{
	if (useType != USE_SET)		// Momentary buttons will pass down a float in here
		return;

	if (value > 1.0)
		value = 1.0;
	Vector move = m_vecPosition1 + (value * (m_vecPosition2 - m_vecPosition1));

	Vector delta = move - GetLocalOrigin();
	float speed = delta.Length() * 10;

	MoveTo(move, speed);
}


//-----------------------------------------------------------------------------
// Purpose: Sets the position as a value from [0..1].
//-----------------------------------------------------------------------------
void CPropMoveLinear::SetPosition(float flPosition)
{
	Vector vTargetPos = m_vecPosition1 + (flPosition * (m_vecPosition2 - m_vecPosition1));
	if ((vTargetPos - GetLocalOrigin()).Length() > 0.001)
	{
		MoveTo(vTargetPos, m_flSpeed);
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropMoveLinear::InputOpen(inputdata_t& inputdata)
{
	if (GetLocalOrigin() != m_vecPosition2)
	{
		MoveTo(m_vecPosition2, m_flSpeed);
	}
}


//------------------------------------------------------------------------------
// Purpose:
//------------------------------------------------------------------------------
void CPropMoveLinear::InputClose(inputdata_t& inputdata)
{
	if (GetLocalOrigin() != m_vecPosition1)
	{
		MoveTo(m_vecPosition1, m_flSpeed);
	}
}


//------------------------------------------------------------------------------
// Purpose: Input handler for setting the position from [0..1].
// Input  : Float position.
//-----------------------------------------------------------------------------
void CPropMoveLinear::InputSetPosition(inputdata_t& inputdata)
{
	SetPosition(inputdata.value.Float());
}


//-----------------------------------------------------------------------------
// Purpose: Called every frame when the bruch is blocked while moving
// Input  : pOther - The blocking entity.
//-----------------------------------------------------------------------------
void CPropMoveLinear::Blocked(CBaseEntity* pOther)
{
	// Hurt the blocker 
	if (m_flBlockDamage)
	{
		if (pOther->m_takedamage == DAMAGE_EVENTS_ONLY)
		{
			if (FClassnameIs(pOther, "gib"))
				UTIL_Remove(pOther);
		}
		else
			pOther->TakeDamage(CTakeDamageInfo(this, this, m_flBlockDamage, DMG_CRUSH));
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &inputdata - 
//-----------------------------------------------------------------------------
void CPropMoveLinear::InputSetSpeed(inputdata_t& inputdata)
{
	// Set the new speed
	m_flSpeed = inputdata.value.Float();

	// FIXME: This is a little questionable.  Do we want to fix the speed, or let it continue on at the old speed?
	float flDistToGoalSqr = (m_vecFinalDest - GetAbsOrigin()).LengthSqr();
	if (flDistToGoalSqr > Square(FLT_EPSILON))
	{
		// NOTE: We do NOT want to call sound functions here, just vanilla position changes
		LinearMove(m_vecFinalDest, m_flSpeed);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Draw any debug text overlays
// Output : Current text offset from the top
//-----------------------------------------------------------------------------
int CPropMoveLinear::DrawDebugTextOverlays(void)
{
	int text_offset = BaseClass::DrawDebugTextOverlays();

	if (m_debugOverlays & OVERLAY_TEXT_BIT)
	{
		char tempstr[512];
		float flTravelDist = (m_vecPosition1 - m_vecPosition2).Length();
		float flCurDist = (m_vecPosition1 - GetLocalOrigin()).Length();
		Q_snprintf(tempstr, sizeof(tempstr), "Current Pos: %3.3f", flCurDist / flTravelDist);
		EntityText(text_offset, tempstr, 0);
		text_offset++;

		float flTargetDist = (m_vecPosition1 - m_vecFinalDest).Length();
		Q_snprintf(tempstr, sizeof(tempstr), "Target Pos: %3.3f", flTargetDist / flTravelDist);
		EntityText(text_offset, tempstr, 0);
		text_offset++;
	}
	return text_offset;
}
void CPropMoveLinear::LogicExplode()
{
	int nRandom = RandomInt(0, 4);
	variant_t variant;
	switch (nRandom)
	{
	case 0:
		AcceptInput("Open", this, this, variant, 0);
		break;
	case 1:
		AcceptInput("Close", this, this, variant, 0);
		break;
	case 2:
		variant.SetFloat(RandomFloat(0, 1));
		AcceptInput("SetPosition", this, this, variant, 0);
		break;
	case 3:
		variant.SetFloat(RandomFloat(m_flSpeed / 2, m_flSpeed * 2));
		AcceptInput("SetSpeed", this, this, variant, 0);
		break;
	case 4:
		BaseClass::LogicExplode();
		break;
	}
}

//basetoggle
CPropMoveLinear::CPropMoveLinear()
{
#ifdef _DEBUG
	// necessary since in debug, we initialize vectors to NAN for debugging
	m_vecPosition1.Init();
	m_vecPosition2.Init();
	m_vecAngle1.Init();
	m_vecAngle2.Init();
	m_vecFinalDest.Init();
	m_vecFinalAngle.Init();
#endif
}

bool CPropMoveLinear::KeyValue(const char* szKeyName, const char* szValue)
{
	if (FStrEq(szKeyName, "lip"))
	{
		m_flLip = atof(szValue);
	}
	else if (FStrEq(szKeyName, "wait"))
	{
		m_flWait = atof(szValue);
	}
	else if (FStrEq(szKeyName, "master"))
	{
		m_sMaster = AllocPooledString(szValue);
	}
	else if (FStrEq(szKeyName, "distance"))
	{
		m_flMoveDistance = atof(szValue);
	}
	else
		return BaseClass::KeyValue(szKeyName, szValue);

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Calculate m_vecVelocity and m_flNextThink to reach vecDest from
//			GetOrigin() traveling at flSpeed.
// Input  : Vector	vecDest - 
//			flSpeed - 
//-----------------------------------------------------------------------------
void CPropMoveLinear::LinearMove(const Vector& vecDest, float flSpeed)
{
	ASSERTSZ(flSpeed != 0, "LinearMove:  no speed is defined!");

	m_vecFinalDest = vecDest;

	m_movementType = MOVE_TOGGLE_LINEAR;
	// Already there?
	if (vecDest == GetLocalOrigin())
	{
		MoveDone();
		return;
	}

	// set destdelta to the vector needed to move
	Vector vecDestDelta = vecDest - GetLocalOrigin();

	// divide vector length by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	// set m_flNextThink to trigger a call to LinearMoveDone when dest is reached
	SetMoveDoneTime(flTravelTime);

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalVelocity(vecDestDelta / flTravelTime);
}

//-----------------------------------------------------------------------------
// Purpose: After moving, set origin to exact final destination, call "move done" function.
//-----------------------------------------------------------------------------
void CPropMoveLinear::LinearMoveDone(void)
{
	UTIL_SetOrigin(this, m_vecFinalDest);
	SetAbsVelocity(vec3_origin);
	SetMoveDoneTime(-1);
}


// DVS TODO: obselete, remove?
bool CPropMoveLinear::IsLockedByMaster(void)
{
	if (m_sMaster != NULL_STRING && !UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return true;
	else
		return false;
}


//-----------------------------------------------------------------------------
// Purpose: Calculate m_vecVelocity and m_flNextThink to reach vecDest from
//			GetLocalOrigin() traveling at flSpeed. Just like LinearMove, but rotational.
// Input  : vecDestAngle - 
//			flSpeed - 
//-----------------------------------------------------------------------------
void CPropMoveLinear::AngularMove(const QAngle& vecDestAngle, float flSpeed)
{
	ASSERTSZ(flSpeed != 0, "AngularMove:  no speed is defined!");

	m_vecFinalAngle = vecDestAngle;

	m_movementType = MOVE_TOGGLE_ANGULAR;
	// Already there?
	if (vecDestAngle == GetLocalAngles())
	{
		MoveDone();
		return;
	}

	// set destdelta to the vector needed to move
	QAngle vecDestDelta = vecDestAngle - GetLocalAngles();

	// divide by speed to get time to reach dest
	float flTravelTime = vecDestDelta.Length() / flSpeed;

	const float MinTravelTime = 0.01f;
	if (flTravelTime < MinTravelTime)
	{
		// If we only travel for a short time, we can fail WillSimulateGamePhysics()
		flTravelTime = MinTravelTime;
		flSpeed = vecDestDelta.Length() / flTravelTime;
	}

	// set m_flNextThink to trigger a call to AngularMoveDone when dest is reached
	SetMoveDoneTime(flTravelTime);

	// scale the destdelta vector by the time spent traveling to get velocity
	SetLocalAngularVelocity(vecDestDelta * (1.0 / flTravelTime));
}


//-----------------------------------------------------------------------------
// Purpose: After rotating, set angle to exact final angle, call "move done" function.
//-----------------------------------------------------------------------------
void CPropMoveLinear::AngularMoveDone(void)
{
	SetLocalAngles(m_vecFinalAngle);
	SetLocalAngularVelocity(vec3_angle);
	SetMoveDoneTime(-1);
}


float CPropMoveLinear::AxisValue(int flags, const QAngle& angles)
{
	return angles.y;
}


void CPropMoveLinear::AxisDir(void)
{
	m_vecMoveAng = QAngle(0, 1, 0);// angles are yaw
}


float CPropMoveLinear::AxisDelta(int flags, const QAngle& angle1, const QAngle& angle2)
{
	return angle1.y - angle2.y;
}