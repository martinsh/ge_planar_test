/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_CollisionEventManager.cpp
 *  \ingroup ketsji
 */


#include "KX_CollisionEventManager.h"
#include "SCA_ISensor.h"
#include "KX_CollisionSensor.h"
#include "KX_GameObject.h"
#include "KX_CollisionContactPoints.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IPhysicsController.h"


KX_CollisionEventManager::KX_CollisionEventManager(class SCA_LogicManager* logicmgr,
	PHY_IPhysicsEnvironment* physEnv)
	: SCA_EventManager(logicmgr, TOUCH_EVENTMGR),
	  m_physEnv(physEnv)
{
	//notm_scene->addCollisionCallback(STATIC_RESPONSE, KX_CollisionEventManager::collisionResponse, this);

	//m_scene->addCollisionCallback(OBJECT_RESPONSE, KX_CollisionEventManager::collisionResponse, this);
	//m_scene->addCollisionCallback(SENSOR_RESPONSE, KX_CollisionEventManager::collisionResponse, this);

	m_physEnv->AddCollisionCallback(PHY_OBJECT_RESPONSE, KX_CollisionEventManager::newCollisionResponse, this);
	m_physEnv->AddCollisionCallback(PHY_SENSOR_RESPONSE, KX_CollisionEventManager::newCollisionResponse, this);
	m_physEnv->AddCollisionCallback(PHY_BROADPH_RESPONSE, KX_CollisionEventManager::newBroadphaseResponse, this);

}

KX_CollisionEventManager::~KX_CollisionEventManager()
{
	RemoveNewCollisions();
}

void KX_CollisionEventManager::RemoveNewCollisions()
{
	for (std::set<NewCollision>::iterator it = m_newCollisions.begin(), end = m_newCollisions.end(); it != end; ++it) {
		delete it->colldata;
	}
	m_newCollisions.clear();
}

bool	KX_CollisionEventManager::NewHandleCollision(void* object1, void* object2, const PHY_CollData *coll_data)
{

	PHY_IPhysicsController* obj1 = static_cast<PHY_IPhysicsController*>(object1);
	PHY_IPhysicsController* obj2 = static_cast<PHY_IPhysicsController*>(object2);
	
	m_newCollisions.insert(NewCollision(obj1, obj2, coll_data));
		
	return false;
}


bool	 KX_CollisionEventManager::newCollisionResponse(void *client_data, 
							void *object1,
							void *object2,
							const PHY_CollData *coll_data)
{
	KX_CollisionEventManager *collisionmgr = (KX_CollisionEventManager *) client_data;
	collisionmgr->NewHandleCollision(object1, object2, coll_data);
	return false;
}

bool	 KX_CollisionEventManager::newBroadphaseResponse(void *client_data, 
							void *object1,
							void *object2,
							const PHY_CollData *coll_data)
{
	PHY_IPhysicsController* ctrl1 = static_cast<PHY_IPhysicsController*>(object1);
	PHY_IPhysicsController* ctrl2 = static_cast<PHY_IPhysicsController*>(object2);

	KX_ClientObjectInfo *info1 = (ctrl1) ? static_cast<KX_ClientObjectInfo*>(ctrl1->GetNewClientInfo()) : NULL;
	KX_ClientObjectInfo *info2 = (ctrl2) ? static_cast<KX_ClientObjectInfo*>(ctrl2->GetNewClientInfo()) : NULL;

	// This call back should only be called for controllers of Near and Radar sensor
	if (!info1)
		return true;

	// Get KX_GameObjects for callbacks
	KX_GameObject* gobj1 = info1->m_gameobject;
	KX_GameObject* gobj2 = (info2) ? info2->m_gameobject : NULL;

	bool has_py_callbacks = false;

#ifdef WITH_PYTHON
	// Consider callbacks for broadphase inclusion if it's a sensor object type
	if (gobj1 && gobj2)
		has_py_callbacks = gobj1->m_collisionCallbacks || gobj2->m_collisionCallbacks;
#else
	(void)gobj1;
	(void)gobj2;
#endif

	switch (info1->m_type)
	{
	case KX_ClientObjectInfo::SENSOR:
		if (info1->m_sensors.size() == 1)
		{
			// only one sensor for this type of object
			KX_CollisionSensor* collisionsensor = static_cast<KX_CollisionSensor*>(*info1->m_sensors.begin());
			return collisionsensor->BroadPhaseFilterCollision(object1, object2);
		}
		break;
	case KX_ClientObjectInfo::OBSENSOR:
	case KX_ClientObjectInfo::OBACTORSENSOR:
		// this object may have multiple collision sensors, 
		// check is any of them is interested in this object
		for (std::list<SCA_ISensor*>::iterator it = info1->m_sensors.begin();
			it != info1->m_sensors.end();
			++it)
		{
			if ((*it)->GetSensorType() == SCA_ISensor::ST_TOUCH) 
			{
				KX_CollisionSensor* collisionsensor = static_cast<KX_CollisionSensor*>(*it);
				if (collisionsensor->BroadPhaseSensorFilterCollision(object1, object2))
					return true;
			}
		}

		return has_py_callbacks;

	// quiet the compiler
	case KX_ClientObjectInfo::STATIC:
	case KX_ClientObjectInfo::ACTOR:
	case KX_ClientObjectInfo::RESERVED1:
		/* do nothing*/
		break;
	}
	return true;
}

void KX_CollisionEventManager::RegisterSensor(SCA_ISensor* sensor)
{
	KX_CollisionSensor* collisionsensor = static_cast<KX_CollisionSensor*>(sensor);
	if (m_sensors.AddBack(collisionsensor))
		// the sensor was effectively inserted, register it
		collisionsensor->RegisterSumo(this);
}

void KX_CollisionEventManager::RemoveSensor(SCA_ISensor* sensor)
{
	KX_CollisionSensor* collisionsensor = static_cast<KX_CollisionSensor*>(sensor);
	if (collisionsensor->Delink())
		// the sensor was effectively removed, unregister it
		collisionsensor->UnregisterSumo(this);
}



void KX_CollisionEventManager::EndFrame()
{
	SG_DList::iterator<KX_CollisionSensor> it(m_sensors);
	for (it.begin();!it.end();++it)
	{
		(*it)->EndFrame();
	}
}



void KX_CollisionEventManager::NextFrame()
{
		SG_DList::iterator<KX_CollisionSensor> it(m_sensors);
		for (it.begin();!it.end();++it)
			(*it)->SynchronizeTransform();
		
		for (std::set<NewCollision>::iterator cit = m_newCollisions.begin(); cit != m_newCollisions.end(); ++cit)
		{
			// Controllers
			PHY_IPhysicsController* ctrl1 = (*cit).first;
			PHY_IPhysicsController* ctrl2 = (*cit).second;
			// Sensor iterator
			list<SCA_ISensor*>::iterator sit;

			// First client info
			KX_ClientObjectInfo *client_info = static_cast<KX_ClientObjectInfo*>(ctrl1->GetNewClientInfo());
			// First gameobject
			KX_GameObject *kxObj1 = KX_GameObject::GetClientObject(client_info);
			// Invoke sensor response for each object
			if (client_info) {
				for ( sit = client_info->m_sensors.begin(); sit != client_info->m_sensors.end(); ++sit) {
					static_cast<KX_CollisionSensor*>(*sit)->NewHandleCollision(ctrl1, ctrl2, NULL);
				}
			}

			// Second client info
			client_info = static_cast<KX_ClientObjectInfo *>(ctrl2->GetNewClientInfo());
			// Second gameobject
			KX_GameObject *kxObj2 = KX_GameObject::GetClientObject(client_info);
			if (client_info) {
				for ( sit = client_info->m_sensors.begin(); sit != client_info->m_sensors.end(); ++sit) {
					static_cast<KX_CollisionSensor*>(*sit)->NewHandleCollision(ctrl2, ctrl1, NULL);
				}
			}
			// Run python callbacks
			const PHY_CollData *colldata = cit->colldata;
			KX_CollisionContactPointList contactPointList0 = KX_CollisionContactPointList(colldata, true);
			KX_CollisionContactPointList contactPointList1 = KX_CollisionContactPointList(colldata, false);
			kxObj1->RunCollisionCallbacks(kxObj2, contactPointList0);
			kxObj2->RunCollisionCallbacks(kxObj1, contactPointList1);
		}

		for (it.begin();!it.end();++it)
			(*it)->Activate(m_logicmgr);

	RemoveNewCollisions();
}

KX_CollisionEventManager::NewCollision::NewCollision(PHY_IPhysicsController *first,
                                                 PHY_IPhysicsController *second,
                                                 const PHY_CollData *colldata)
    : first(first), second(second), colldata(colldata)
{}

KX_CollisionEventManager::NewCollision::NewCollision(const NewCollision &to_copy)
	: first(to_copy.first), second(to_copy.second), colldata(to_copy.colldata)
{}

bool KX_CollisionEventManager::NewCollision::operator<(const NewCollision &other) const
{
	//see strict weak ordering: https://support.microsoft.com/en-us/kb/949171
	if (first == other.first) {
		if (second == other.second) {
			return colldata < other.colldata;
		}
		return second < other.second;
	}
	return first < other.first;
}
