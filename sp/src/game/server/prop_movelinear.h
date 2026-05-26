//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef PROP_MOVELINEAR_H
#define PROP_MOVELINEAR_H

#pragma once

#include "props.h"
#include "entityoutput.h"


class IPhysicsFluidController;


class CPropMoveLinear : public CDynamicProp
{
public:
	DECLARE_CLASS(CPropMoveLinear, CDynamicProp);

	void		Spawn( void );
	void		Precache( void );
	bool		CreateVPhysics( void );
	bool		ShouldSavePhysics( void );
	virtual void LogicExplode();
	void		MoveTo(Vector vPosition, float flSpeed);
	void		Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	void		MoveDone( void );
	//void		StopMoveSound( void );//i don't know why, but this feature was giving me a C2440 in the datadesc. we didn't need this for the ground turrets, so bye bye.
	void		Blocked( CBaseEntity *pOther );
	void		SetPosition( float flPosition );

	int			DrawDebugTextOverlays(void);

	// Input handlers
	void InputOpen( inputdata_t &inputdata );
	void InputClose( inputdata_t &inputdata );
	void InputSetPosition( inputdata_t &inputdata );
	void InputSetSpeed( inputdata_t &inputdata );
	
	DECLARE_DATADESC();

	Vector		m_vecMoveDir;			// Move direction.

	string_t	m_soundStart;			// start and looping sound
	string_t	m_soundStop;			// stop sound
	string_t	m_currentSound;			// sound I'm playing

	float		m_flBlockDamage;		// Damage inflicted when blocked.
	float		m_flStartPosition;		// Position of brush when spawned
	float		m_flMoveDistance;		// Total distance the brush can move

	IPhysicsFluidController *m_pFluidController;

	// Outputs
	COutputEvent m_OnFullyOpen;
	COutputEvent m_OnFullyClosed;

	//basetoggle
	CPropMoveLinear();

	virtual bool		KeyValue(const char* szKeyName, const char* szValue);
	virtual bool		KeyValue(const char* szKeyName, Vector vec) { return CBaseEntity::KeyValue(szKeyName, vec); };
	virtual bool		KeyValue(const char* szKeyName, float flValue) { return CBaseEntity::KeyValue(szKeyName, flValue); };

	TOGGLE_STATE		m_toggle_state;
	float				m_flWait;
	float				m_flLip;

	Vector				m_vecPosition1;
	Vector				m_vecPosition2;

	QAngle				m_vecMoveAng;
	QAngle				m_vecAngle1;
	QAngle				m_vecAngle2;

	float				m_flHeight;
	EHANDLE				m_hActivator;
	Vector				m_vecFinalDest;
	QAngle				m_vecFinalAngle;

	int					m_movementType;

	virtual float	GetDelay(void) { return m_flWait; }

	// common member functions
	void LinearMove(const Vector& vecDest, float flSpeed);
	void LinearMoveDone(void);
	void AngularMove(const QAngle& vecDestAngle, float flSpeed);
	void AngularMoveDone(void);
	bool IsLockedByMaster(void);

	static float AxisValue(int flags, const QAngle& angles);
	void AxisDir(void);
	static float AxisDelta(int flags, const QAngle& angle1, const QAngle& angle2);

	string_t m_sMaster;		// If this button has a master switch, this is the targetname.
	// A master switch must be of the multisource type. If all 
	// of the switches in the multisource have been triggered, then
	// the button will be allowed to operate. Otherwise, it will be
	// deactivated.
};
#endif // PROP_MOVELINEAR_H
