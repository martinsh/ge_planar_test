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
 * Ketsji scene. Holds references to all scene data.
 */

/** \file gameengine/Ketsji/KX_Scene.cpp
 *  \ingroup ketsji
 */


#ifdef _MSC_VER
#  pragma warning (disable:4786)
#endif

#include <stdio.h>

#include "KX_Scene.h"
#include "KX_Globals.h"
#include "BLI_utildefines.h"
#include "KX_KetsjiEngine.h"
#include "KX_BlenderMaterial.h"
#include "KX_FontObject.h"
#include "RAS_IPolygonMaterial.h"
#include "EXP_ListValue.h"
#include "SCA_LogicManager.h"
#include "SCA_TimeEventManager.h"
#include "SCA_2DFilterActuator.h"
#include "SCA_PythonController.h"
#include "KX_CollisionEventManager.h"
#include "SCA_KeyboardManager.h"
#include "SCA_MouseManager.h"
#include "SCA_ActuatorEventManager.h"
#include "SCA_BasicEventManager.h"
#include "KX_Camera.h"
#include "SCA_JoystickManager.h"
#include "KX_PyMath.h"
#include "RAS_MeshObject.h"
#include "SCA_IScene.h"
#include "KX_LodManager.h"

#include "RAS_IRasterizer.h"
#include "RAS_ICanvas.h"
#include "RAS_2DFilterData.h"
#include "RAS_CubeMap.h"
#include "RAS_Planar.h"
#include "KX_2DFilterManager.h"
#include "KX_PlanarManager.h"
#include "KX_CubeMapManager.h"
#include "RAS_BucketManager.h"

#include "EXP_FloatValue.h"
#include "SCA_IController.h"
#include "SCA_IActuator.h"
#include "SG_Node.h"
#include "SG_Controller.h"
#include "SG_IObject.h"
#include "DNA_group_types.h"
#include "DNA_scene_types.h"
#include "DNA_property_types.h"

#include "KX_SG_NodeRelationships.h"

#include "KX_NetworkMessageScene.h"
#include "PHY_IPhysicsEnvironment.h"
#include "PHY_IGraphicController.h"
#include "PHY_IPhysicsController.h"
#include "KX_BlenderSceneConverter.h"
#include "KX_MotionState.h"

#include "BL_ModifierDeformer.h"
#include "BL_ShapeDeformer.h"
#include "BL_DeformableGameObject.h"
#include "KX_ObstacleSimulation.h"

#ifdef WITH_BULLET
#  include "KX_SoftBodyDeformer.h"
#endif

#ifdef WITH_PYTHON
#  include "EXP_PythonCallBack.h"
#endif

#include "KX_Light.h"

#include "BLI_task.h"

static void *KX_SceneReplicationFunc(SG_IObject* node,void* gameobj,void* scene)
{
	KX_GameObject* replica = ((KX_Scene*)scene)->AddNodeReplicaObject(node,(KX_GameObject*)gameobj);

	if (replica)
		replica->Release();

	return (void*)replica;
}

static void *KX_SceneDestructionFunc(SG_IObject* node,void* gameobj,void* scene)
{
	((KX_Scene*)scene)->RemoveNodeDestructObject(node,(KX_GameObject*)gameobj);

	return NULL;
};

bool KX_Scene::KX_ScenegraphUpdateFunc(SG_IObject* node,void* gameobj,void* scene)
{
	return ((SG_Node*)node)->Schedule(((KX_Scene*)scene)->m_sghead);
}

bool KX_Scene::KX_ScenegraphRescheduleFunc(SG_IObject* node,void* gameobj,void* scene)
{
	return ((SG_Node*)node)->Reschedule(((KX_Scene*)scene)->m_sghead);
}

SG_Callbacks KX_Scene::m_callbacks = SG_Callbacks(
	KX_SceneReplicationFunc,
	KX_SceneDestructionFunc,
	KX_GameObject::UpdateTransformFunc,
	KX_Scene::KX_ScenegraphUpdateFunc,
	KX_Scene::KX_ScenegraphRescheduleFunc);

KX_Scene::KX_Scene(SCA_IInputDevice *inputDevice,
				   const STR_String& sceneName,
				   Scene *scene,
				   class RAS_ICanvas* canvas,
				   KX_NetworkMessageManager *messageManager): 
	CValue(),
	m_keyboardmgr(NULL),
	m_mousemgr(NULL),
	m_sceneConverter(NULL),
	m_physicsEnvironment(0),
	m_sceneName(sceneName),
	m_active_camera(NULL),
	m_ueberExecutionPriority(0),
	m_blenderScene(scene),
	m_isActivedHysteresis(false),
	m_lodHysteresisValue(0)
{
	m_suspendedtime = 0.0;
	m_suspendeddelta = 0.0;

	m_dbvt_culling = false;
	m_dbvt_occlusion_res = 0;
	m_activity_culling = false;
	m_suspend = false;
	m_isclearingZbuffer = true;
	m_tempObjectList = new CListValue();
	m_objectlist = new CListValue();
	m_parentlist = new CListValue();
	m_lightlist= new CListValue();
	m_inactivelist = new CListValue();
	m_euthanasyobjects = new CListValue();
	m_animatedlist = new CListValue();
	m_cameralist = new CListValue();
	m_fontlist = new CListValue();

	m_filterManager = new KX_2DFilterManager();
	m_logicmgr = new SCA_LogicManager();
	
	m_timemgr = new SCA_TimeEventManager(m_logicmgr);
	m_keyboardmgr = new SCA_KeyboardManager(m_logicmgr, inputDevice);
	m_mousemgr = new SCA_MouseManager(m_logicmgr,inputDevice);
	
	SCA_ActuatorEventManager* actmgr = new SCA_ActuatorEventManager(m_logicmgr);
	SCA_BasicEventManager* basicmgr = new SCA_BasicEventManager(m_logicmgr);

	m_logicmgr->RegisterEventManager(actmgr);
	m_logicmgr->RegisterEventManager(m_keyboardmgr);
	m_logicmgr->RegisterEventManager(m_mousemgr);
	m_logicmgr->RegisterEventManager(m_timemgr);
	m_logicmgr->RegisterEventManager(basicmgr);

	SCA_JoystickManager *joymgr = new SCA_JoystickManager(m_logicmgr);
	m_logicmgr->RegisterEventManager(joymgr);

	m_networkScene = new KX_NetworkMessageScene(messageManager);
	
	m_rootnode = NULL;

	m_cubeMapManager = new KX_CubeMapManager(this);
	m_planarManager = new KX_PlanarManager(this);
	m_bucketmanager=new RAS_BucketManager();
	
	bool showObstacleSimulation = (scene->gm.flag & GAME_SHOW_OBSTACLE_SIMULATION) != 0;
	switch (scene->gm.obstacleSimulation)
	{
	case OBSTSIMULATION_TOI_rays:
		m_obstacleSimulation = new KX_ObstacleSimulationTOI_rays((MT_Scalar)scene->gm.levelHeight, showObstacleSimulation);
		break;
	case OBSTSIMULATION_TOI_cells:
		m_obstacleSimulation = new KX_ObstacleSimulationTOI_cells((MT_Scalar)scene->gm.levelHeight, showObstacleSimulation);
		break;
	default:
		m_obstacleSimulation = NULL;
	}
	
#ifdef WITH_PYTHON
	m_attr_dict = NULL;
	m_draw_call_pre = NULL;
	m_draw_call_post = NULL;
	m_draw_setup_call_pre = NULL;
#endif
}



KX_Scene::~KX_Scene()
{
	// The release of debug properties used to be in SCA_IScene::~SCA_IScene
	// It's still there but we remove all properties here otherwise some
	// reference might be hanging and causing late release of objects
	RemoveAllDebugProperties();

	while (GetRootParentList()->GetCount() > 0) 
	{
		KX_GameObject* parentobj = (KX_GameObject*) GetRootParentList()->GetValue(0);
		this->RemoveObject(parentobj);
	}

	if (m_obstacleSimulation)
		delete m_obstacleSimulation;

	if (m_objectlist)
		m_objectlist->Release();

	if (m_parentlist)
		m_parentlist->Release();
	
	if (m_inactivelist)
		m_inactivelist->Release();

	if (m_lightlist)
		m_lightlist->Release();
	
	if (m_tempObjectList)
		m_tempObjectList->Release();

	if (m_euthanasyobjects)
		m_euthanasyobjects->Release();

	if (m_animatedlist)
		m_animatedlist->Release();

	if (m_cameralist) {
		m_cameralist->Release();
	}

	if (m_fontlist) {
		m_fontlist->Release();
	}

	if (m_filterManager) {
		delete m_filterManager;
	}

	if (m_logicmgr)
		delete m_logicmgr;

	if (m_physicsEnvironment)
		delete m_physicsEnvironment;

	if (m_networkScene)
		delete m_networkScene;

	if (m_planarManager)
		delete m_planarManager;
	
	if (m_cubeMapManager) {
		delete m_cubeMapManager;
	}

	if (m_bucketmanager)
	{
		delete m_bucketmanager;
	}

#ifdef WITH_PYTHON
	if (m_attr_dict) {
		PyDict_Clear(m_attr_dict);
		/* Py_CLEAR: Py_DECREF's and NULL's */
		Py_CLEAR(m_attr_dict);
	}

	/* these may be NULL but the macro checks */
	Py_CLEAR(m_draw_call_pre);
	Py_CLEAR(m_draw_call_post);
#endif
}

STR_String& KX_Scene::GetName()
{
	return m_sceneName;
}

/// Set the name of the value
void KX_Scene::SetName(const char *name)
{
	m_sceneName = name;
}

RAS_BucketManager* KX_Scene::GetBucketManager()
{
	return m_bucketmanager;
}

KX_PlanarManager *KX_Scene::GetPlanarManager()
{
	return m_planarManager;
}

KX_CubeMapManager *KX_Scene::GetCubeMapManager()
{
	return m_cubeMapManager;
}

CListValue* KX_Scene::GetTempObjectList()
{
	return m_tempObjectList;
}

CListValue* KX_Scene::GetObjectList()
{
	return m_objectlist;
}


CListValue* KX_Scene::GetRootParentList()
{
	return m_parentlist;
}

CListValue* KX_Scene::GetInactiveList()
{
	return m_inactivelist;
}



CListValue* KX_Scene::GetLightList()
{
	return m_lightlist;
}

SCA_LogicManager* KX_Scene::GetLogicManager()
{
	return m_logicmgr;
}

SCA_TimeEventManager* KX_Scene::GetTimeEventManager()
{
	return m_timemgr;
}

CListValue* KX_Scene::GetCameraList()
{
	return m_cameralist;
}

CListValue* KX_Scene::GetFontList()
{
	return m_fontlist;
}

void KX_Scene::SetFramingType(RAS_FrameSettings & frame_settings)
{
	m_frame_settings = frame_settings;
};

/**
 * Return a const reference to the framing 
 * type set by the above call.
 * The contents are not guaranteed to be sensible
 * if you don't call the above function.
 */
const RAS_FrameSettings& KX_Scene::GetFramingType() const 
{
	return m_frame_settings;
}

void KX_Scene::SetWorldInfo(class KX_WorldInfo* worldinfo)
{
	m_worldinfo = worldinfo;
}



class KX_WorldInfo* KX_Scene::GetWorldInfo()
{
	return m_worldinfo;
}

void KX_Scene::Suspend()
{
	m_suspend = true;
}

void KX_Scene::Resume()
{
	m_suspend = false;
}

void KX_Scene::SetActivityCulling(bool b)
{
	m_activity_culling = b;
}

bool KX_Scene::IsSuspended()
{
	return m_suspend;
}

bool KX_Scene::IsClearingZBuffer()
{
	return m_isclearingZbuffer;
}

void KX_Scene::EnableZBufferClearing(bool isclearingZbuffer)
{
	m_isclearingZbuffer = isclearingZbuffer;
}

void KX_Scene::AddObjectDebugProperties(class KX_GameObject* gameobj)
{
	Object* blenderobject = gameobj->GetBlenderObject();
	bProperty* prop = (bProperty*)blenderobject->prop.first;

	while (prop) {
		if (prop->flag & PROP_DEBUG)
			AddDebugProperty(gameobj,STR_String(prop->name));
		prop = prop->next;
	}	

	if (blenderobject->scaflag & OB_DEBUGSTATE)
		AddDebugProperty(gameobj,STR_String("__state__"));
}

void KX_Scene::RemoveNodeDestructObject(class SG_IObject* node,class CValue* gameobj)
{
	KX_GameObject* orgobj = (KX_GameObject*)gameobj;
	if (NewRemoveObject(orgobj) != 0)
	{
		// object is not yet deleted because a reference is hanging somewhere.
		// This should not happen anymore since we use proxy object for Python
		// confident enough to put an assert?
		//assert(false);
		printf("Zombie object! name=%s\n", orgobj->GetName().ReadPtr());
		orgobj->SetSGNode(NULL);
		PHY_IGraphicController* ctrl = orgobj->GetGraphicController();
		if (ctrl)
		{
			// a graphic controller is set, we must delete it as the node will be deleted
			delete ctrl;
			orgobj->SetGraphicController(NULL);
		}
	}
	if (node)
		delete node;
}

KX_GameObject* KX_Scene::AddNodeReplicaObject(class SG_IObject* node, class CValue* gameobj)
{
	// for group duplication, limit the duplication of the hierarchy to the
	// objects that are part of the group. 
	if (!IsObjectInGroup(gameobj))
		return NULL;
	
	KX_GameObject* orgobj = (KX_GameObject*)gameobj;
	KX_GameObject* newobj = (KX_GameObject*)orgobj->GetReplica();
	m_map_gameobject_to_replica[orgobj] = newobj;

	// also register 'timers' (time properties) of the replica
	int numprops = newobj->GetPropertyCount();

	for (int i = 0; i < numprops; i++)
	{
		CValue* prop = newobj->GetProperty(i);

		if (prop->GetProperty("timer"))
			this->m_timemgr->AddTimeProperty(prop);
	}

	if (node)
	{
		newobj->SetSGNode((SG_Node*)node);
	}
	else
	{
		m_rootnode = new SG_Node(newobj,this,KX_Scene::m_callbacks);
	
		// this fixes part of the scaling-added object bug
		SG_Node* orgnode = orgobj->GetSGNode();
		m_rootnode->SetLocalScale(orgnode->GetLocalScale());
		m_rootnode->SetLocalPosition(orgnode->GetLocalPosition());
		m_rootnode->SetLocalOrientation(orgnode->GetLocalOrientation());

		// define the relationship between this node and it's parent.
		KX_NormalParentRelation * parent_relation = 
			KX_NormalParentRelation::New();
		m_rootnode->SetParentRelation(parent_relation);

		newobj->SetSGNode(m_rootnode);
	}
	
	SG_IObject* replicanode = newobj->GetSGNode();
//	SG_Node* rootnode = (replicanode == m_rootnode ? NULL : m_rootnode);

	// Add the object in the obstacle simulation if needed.
	if (m_obstacleSimulation && orgobj->GetBlenderObject()->gameflag & OB_HASOBSTACLE) {
		m_obstacleSimulation->AddObstacleForObj(newobj);
	}

	replicanode->SetSGClientObject(newobj);

	// this is the list of object that are send to the graphics pipeline
	m_objectlist->Add(newobj->AddRef());
	if (newobj->GetGameObjectType()==SCA_IObject::OBJ_LIGHT)
		m_lightlist->Add(newobj->AddRef());
	else if (newobj->GetGameObjectType()==SCA_IObject::OBJ_TEXT)
		m_fontlist->Add(newobj->AddRef());
	newobj->AddMeshUser();

	// logic cannot be replicated, until the whole hierarchy is replicated.
	m_logicHierarchicalGameObjects.push_back(newobj);
	//replicate controllers of this node
	SGControllerList	scenegraphcontrollers = orgobj->GetSGNode()->GetSGControllerList();
	replicanode->RemoveAllControllers();
	SGControllerList::iterator cit;
	//int numcont = scenegraphcontrollers.size();
	
	for (cit = scenegraphcontrollers.begin();!(cit==scenegraphcontrollers.end());++cit)
	{
		// controller replication is quite complicated
		// only replicate ipo controller for now

		SG_Controller* replicacontroller = (*cit)->GetReplica((SG_Node*) replicanode);
		if (replicacontroller)
		{
			replicacontroller->SetObject(replicanode);
			replicanode->AddSGController(replicacontroller);
		}
	}
	// replicate graphic controller
	if (orgobj->GetGraphicController())
	{
		PHY_IMotionState* motionstate = new KX_MotionState(newobj->GetSGNode());
		PHY_IGraphicController* newctrl = orgobj->GetGraphicController()->GetReplica(motionstate);
		newctrl->SetNewClientInfo(newobj->getClientInfo());
		newobj->SetGraphicController(newctrl);
	}

	// replicate physics controller
	if (orgobj->GetPhysicsController())
	{
		PHY_IMotionState* motionstate = new KX_MotionState(newobj->GetSGNode());
		PHY_IPhysicsController* newctrl = orgobj->GetPhysicsController()->GetReplica();

		KX_GameObject *parent = newobj->GetParent();
		PHY_IPhysicsController* parentctrl = (parent) ? parent->GetPhysicsController() : NULL;

		newctrl->SetNewClientInfo(newobj->getClientInfo());
		newobj->SetPhysicsController(newctrl);
		newctrl->PostProcessReplica(motionstate, parentctrl);

		// Child objects must be static
		if (parent)
			newctrl->SuspendDynamics();
	}

	return newobj;
}



// before calling this method KX_Scene::ReplicateLogic(), make sure to
// have called 'GameObject::ReParentLogic' for each object this
// hierarchy that's because first ALL bricks must exist in the new
// replica of the hierarchy in order to make cross-links work properly
// !
// It is VERY important that the order of sensors and actuators in
// the replicated object is preserved: it is used to reconnect the logic.
// This method is more robust then using the bricks name in case of complex 
// group replication. The replication of logic bricks is done in 
// SCA_IObject::ReParentLogic(), make sure it preserves the order of the bricks.
void KX_Scene::ReplicateLogic(KX_GameObject* newobj)
{
	/* add properties to debug list, for added objects and DupliGroups */
	if (KX_GetActiveEngine()->GetAutoAddDebugProperties()) {
		AddObjectDebugProperties(newobj);
	}
	// also relink the controller to sensors/actuators
	SCA_ControllerList& controllers = newobj->GetControllers();
	//SCA_SensorList&     sensors     = newobj->GetSensors();
	//SCA_ActuatorList&   actuators   = newobj->GetActuators();

	for (SCA_ControllerList::iterator itc = controllers.begin(); !(itc==controllers.end());itc++)
	{
		SCA_IController* cont = (*itc);
		cont->SetUeberExecutePriority(m_ueberExecutionPriority);
		vector<SCA_ISensor*> linkedsensors = cont->GetLinkedSensors();
		vector<SCA_IActuator*> linkedactuators = cont->GetLinkedActuators();

		// disconnect the sensors and actuators
		// do it directly on the list at this controller is not connected to anything at this stage
		cont->GetLinkedSensors().clear();
		cont->GetLinkedActuators().clear();
		
		// now relink each sensor
		for (vector<SCA_ISensor*>::iterator its = linkedsensors.begin();!(its==linkedsensors.end());its++)
		{
			SCA_ISensor* oldsensor = (*its);
			SCA_IObject* oldsensorobj = oldsensor->GetParent();
			// the original owner of the sensor has been replicated?
			SCA_IObject* newsensorobj = (SCA_IObject *)m_map_gameobject_to_replica[oldsensorobj];

			if (!newsensorobj)
			{
				// no, then the sensor points outside the hierarchy, keep it the same
				if (m_objectlist->SearchValue(oldsensorobj))
					// only replicate links that points to active objects
					m_logicmgr->RegisterToSensor(cont,oldsensor);
			}
			else
			{
				// yes, then the new sensor has the same position
				SCA_SensorList& sensorlist = oldsensorobj->GetSensors();
				SCA_SensorList::iterator sit;
				SCA_ISensor* newsensor = NULL;
				int sensorpos;

				for (sensorpos=0, sit=sensorlist.begin(); sit!=sensorlist.end(); sit++, sensorpos++)
				{
					if ((*sit) == oldsensor) 
					{
						newsensor = newsensorobj->GetSensors().at(sensorpos);
						break;
					}
				}
				assert(newsensor != NULL);
				m_logicmgr->RegisterToSensor(cont,newsensor);
			}
		}
		
		// now relink each actuator
		for (vector<SCA_IActuator*>::iterator ita = linkedactuators.begin();!(ita==linkedactuators.end());ita++)
		{
			SCA_IActuator* oldactuator = (*ita);
			SCA_IObject* oldactuatorobj = oldactuator->GetParent();
			SCA_IObject* newactuatorobj = (SCA_IObject *)m_map_gameobject_to_replica[oldactuatorobj];

			if (!newactuatorobj)
			{
				// no, then the sensor points outside the hierarchy, keep it the same
				if (m_objectlist->SearchValue(oldactuatorobj))
					// only replicate links that points to active objects
					m_logicmgr->RegisterToActuator(cont,oldactuator);
			}
			else
			{
				// yes, then the new sensor has the same position
				SCA_ActuatorList& actuatorlist = oldactuatorobj->GetActuators();
				SCA_ActuatorList::iterator ait;
				SCA_IActuator* newactuator = NULL;
				int actuatorpos;

				for (actuatorpos=0, ait=actuatorlist.begin(); ait!=actuatorlist.end(); ait++, actuatorpos++)
				{
					if ((*ait) == oldactuator) 
					{
						newactuator = newactuatorobj->GetActuators().at(actuatorpos);
						break;
					}
				}
				assert(newactuator != NULL);
				m_logicmgr->RegisterToActuator(cont,newactuator);
				newactuator->SetUeberExecutePriority(m_ueberExecutionPriority);
			}
		}
	}
	// ready to set initial state
	newobj->ResetState();
}

void KX_Scene::DupliGroupRecurse(CValue* obj, int level)
{
	KX_GameObject* groupobj = (KX_GameObject*) obj;
	KX_GameObject* replica;
	KX_GameObject* gameobj;
	Object* blgroupobj = groupobj->GetBlenderObject();
	Group* group;
	GroupObject *go;
	vector<KX_GameObject*> duplilist;

	if (!groupobj->GetSGNode() ||
		!groupobj->IsDupliGroup() ||
		level>MAX_DUPLI_RECUR)
		return;

	// we will add one group at a time
	m_logicHierarchicalGameObjects.clear();
	m_map_gameobject_to_replica.clear();
	m_ueberExecutionPriority++;
	// for groups will do something special: 
	// we will force the creation of objects to those in the group only
	// Again, this is match what Blender is doing (it doesn't care of parent relationship)
	m_groupGameObjects.clear();

	group = blgroupobj->dup_group;
	for (go=(GroupObject*)group->gobject.first; go; go=(GroupObject*)go->next) 
	{
		Object* blenderobj = go->ob;
		if (blgroupobj == blenderobj)
			// this check is also in group_duplilist()
			continue;

		gameobj = (KX_GameObject*)m_logicmgr->FindGameObjByBlendObj(blenderobj);
		if (gameobj == NULL) 
		{
			// this object has not been converted!!!
			// Should not happen as dupli group are created automatically 
			continue;
		}

		gameobj->SetBlenderGroupObject(blgroupobj);

		if ((blenderobj->lay & group->layer)==0)
		{
			// object is not visible in the 3D view, will not be instantiated
			continue;
		}
		m_groupGameObjects.insert(gameobj);
	}

	set<CValue*>::iterator oit;
	for (oit=m_groupGameObjects.begin(); oit != m_groupGameObjects.end(); oit++)
	{
		gameobj = (KX_GameObject*)(*oit);

		KX_GameObject *parent = gameobj->GetParent();
		if (parent != NULL)
		{
			// this object is not a top parent. Either it is the child of another
			// object in the group and it will be added automatically when the parent
			// is added. Or it is the child of an object outside the group and the group
			// is inconsistent, skip it anyway
			continue;
		}
		replica = (KX_GameObject*) AddNodeReplicaObject(NULL,gameobj);
		// add to 'rootparent' list (this is the list of top hierarchy objects, updated each frame)
		m_parentlist->Add(replica->AddRef());

		// recurse replication into children nodes
		NodeList& children = gameobj->GetSGNode()->GetSGChildren();

		replica->GetSGNode()->ClearSGChildren();
		for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
		{
			SG_Node* orgnode = (*childit);
			SG_Node* childreplicanode = orgnode->GetSGReplica();
			if (childreplicanode)
				replica->GetSGNode()->AddChild(childreplicanode);
		}
		// don't replicate logic now: we assume that the objects in the group can have
		// logic relationship, even outside parent relationship
		// In order to match 3D view, the position of groupobj is used as a 
		// transformation matrix instead of the new position. This means that 
		// the group reference point is 0,0,0

		// get the rootnode's scale
		MT_Vector3 newscale = groupobj->NodeGetWorldScaling();
		// set the replica's relative scale with the rootnode's scale
		replica->NodeSetRelativeScale(newscale);

		MT_Vector3 offset(group->dupli_ofs);
		MT_Vector3 newpos = groupobj->NodeGetWorldPosition() + 
			newscale*(groupobj->NodeGetWorldOrientation() * (gameobj->NodeGetWorldPosition()-offset));
		replica->NodeSetLocalPosition(newpos);
		// set the orientation after position for softbody!
		MT_Matrix3x3 newori = groupobj->NodeGetWorldOrientation() * gameobj->NodeGetWorldOrientation();
		replica->NodeSetLocalOrientation(newori);
		// update scenegraph for entire tree of children
		replica->GetSGNode()->UpdateWorldData(0);
		replica->GetSGNode()->SetBBox(gameobj->GetSGNode()->BBox());
		// we can now add the graphic controller to the physic engine
		replica->ActivateGraphicController(true);

		// done with replica
		replica->Release();
	}

	// the logic must be replicated first because we need
	// the new logic bricks before relinking
	vector<KX_GameObject*>::iterator git;
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		(*git)->ReParentLogic();
	}
	
	//	relink any pointers as necessary, sort of a temporary solution
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		// this will also relink the actuator to objects within the hierarchy
		(*git)->Relink(m_map_gameobject_to_replica);
		// add the object in the layer of the parent
		(*git)->SetLayer(groupobj->GetLayer());
	}

	// replicate crosslinks etc. between logic bricks
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		ReplicateLogic((*git));
	}
	
	// now look if object in the hierarchy have dupli group and recurse
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		/* Replicate all constraints. */
		if ((*git)->GetPhysicsController()) {
			(*git)->GetPhysicsController()->ReplicateConstraints((*git), m_logicHierarchicalGameObjects);
			(*git)->ClearConstraints();
		}

		if ((*git) != groupobj && (*git)->IsDupliGroup())
			// can't instantiate group immediately as it destroys m_logicHierarchicalGameObjects
			duplilist.push_back((*git));

		if ((*git)->GetBlenderGroupObject() == blgroupobj) {
			// set references for dupli-group
			// groupobj holds a list of all objects, that belongs to this group
			groupobj->AddInstanceObjects((*git));

			// every object gets the reference to its dupli-group object
			(*git)->SetDupliGroupObject(groupobj);
		}
	}

	for (git = duplilist.begin(); !(git == duplilist.end()); ++git)
	{
		DupliGroupRecurse((*git), level+1);
	}
}


SCA_IObject* KX_Scene::AddReplicaObject(class CValue* originalobject,
										class CValue* referenceobject,
										float lifespan)
{

	m_logicHierarchicalGameObjects.clear();
	m_map_gameobject_to_replica.clear();
	m_groupGameObjects.clear();

	KX_GameObject* originalobj = (KX_GameObject*) originalobject;
	KX_GameObject* referenceobj = (KX_GameObject*) referenceobject;

	m_ueberExecutionPriority++;

	// lets create a replica
	KX_GameObject* replica = (KX_GameObject*) AddNodeReplicaObject(NULL,originalobj);

	// add a timebomb to this object
	// lifespan of zero means 'this object lives forever'
	if (lifespan > 0.0f)
	{
		// for now, convert between so called frames and realtime
		m_tempObjectList->Add(replica->AddRef());
		// this convert the life from frames to sort-of seconds, hard coded 0.02 that assumes we have 50 frames per second
		// if you change this value, make sure you change it in KX_GameObject::pyattr_get_life property too
		CValue *fval = new CFloatValue(lifespan*0.02f);
		replica->SetProperty("::timebomb",fval);
		fval->Release();
	}

	// add to 'rootparent' list (this is the list of top hierarchy objects, updated each frame)
	m_parentlist->Add(replica->AddRef());

	// recurse replication into children nodes

	NodeList& children = originalobj->GetSGNode()->GetSGChildren();

	replica->GetSGNode()->ClearSGChildren();
	for (NodeList::iterator childit = children.begin();!(childit==children.end());++childit)
	{
		SG_Node* orgnode = (*childit);
		SG_Node* childreplicanode = orgnode->GetSGReplica();
		if (childreplicanode)
			replica->GetSGNode()->AddChild(childreplicanode);
	}

	if (referenceobj) {
		// At this stage all the objects in the hierarchy have been duplicated,
		// we can update the scenegraph, we need it for the duplication of logic
		MT_Vector3 newpos = referenceobj->NodeGetWorldPosition();
		replica->NodeSetLocalPosition(newpos);

		MT_Matrix3x3 newori = referenceobj->NodeGetWorldOrientation();
		replica->NodeSetLocalOrientation(newori);

		// get the rootnode's scale
		MT_Vector3 newscale = referenceobj->GetSGNode()->GetRootSGParent()->GetLocalScale();
		// set the replica's relative scale with the rootnode's scale
		replica->NodeSetRelativeScale(newscale);
	}

	replica->GetSGNode()->UpdateWorldData(0);
	replica->GetSGNode()->SetBBox(originalobj->GetSGNode()->BBox());
	// the size is correct, we can add the graphic controller to the physic engine
	replica->ActivateGraphicController(true);

	// now replicate logic
	vector<KX_GameObject*>::iterator git;
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		(*git)->ReParentLogic();
	}
	
	//	relink any pointers as necessary, sort of a temporary solution
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		// this will also relink the actuators in the hierarchy
		(*git)->Relink(m_map_gameobject_to_replica);
		if (referenceobj) {
			// add the object in the layer of the reference object
			(*git)->SetLayer(referenceobj->GetLayer());
		}
		else {
			// We don't know what layer set, so we set all visible layers in the blender scene.
			(*git)->SetLayer(m_blenderScene->lay);
		}
	}

	// replicate crosslinks etc. between logic bricks
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		ReplicateLogic((*git));
	}
	
	// check if there are objects with dupligroup in the hierarchy
	vector<KX_GameObject*> duplilist;
	for (git = m_logicHierarchicalGameObjects.begin();!(git==m_logicHierarchicalGameObjects.end());++git)
	{
		if ((*git)->IsDupliGroup())
		{
			// separate list as m_logicHierarchicalGameObjects is also used by DupliGroupRecurse()
			duplilist.push_back(*git);
		}
	}
	for (git = duplilist.begin();!(git==duplilist.end());++git)
	{
		DupliGroupRecurse(*git, 0);
	}

	// Initialize component in root parent object.
	replica->InitComponents();
	// Initialize components recursively.
	CListValue *childrecursive = replica->GetChildrenRecursive();
	for (CListValue::iterator it = childrecursive->GetBegin(), end = childrecursive->GetEnd(); it != end; ++it) {
		KX_GameObject *gameobj = (KX_GameObject *)*it;
		gameobj->InitComponents();
	}
	childrecursive->Release();

	//	don't release replica here because we are returning it, not done with it...
	return replica;
}



void KX_Scene::RemoveObject(class CValue* gameobj)
{
	KX_GameObject* newobj = (KX_GameObject*) gameobj;

	// disconnect child from parent
	SG_Node* node = newobj->GetSGNode();

	if (node)
	{
		node->DisconnectFromParent();

		// recursively destruct
		node->Destruct();
	}
	//no need to do that: the object is destroyed and memory released 
	//newobj->SetSGNode(0);
}

void KX_Scene::RemoveDupliGroup(class CValue *gameobj)
{
	KX_GameObject *newobj = (KX_GameObject *) gameobj;

	if (newobj->IsDupliGroup()) {
		for (int i = 0; i < newobj->GetInstanceObjects()->GetCount(); i++) {
			CValue *obj = newobj->GetInstanceObjects()->GetValue(i);
			DelayedRemoveObject(obj);
		}
	}
}

void KX_Scene::DelayedRemoveObject(class CValue* gameobj)
{
	RemoveDupliGroup(gameobj);

	if (!m_euthanasyobjects->SearchValue(gameobj))
	{
		m_euthanasyobjects->Add(gameobj->AddRef());
	}
}

int KX_Scene::NewRemoveObject(class CValue* gameobj)
{
	int ret;
	KX_GameObject* newobj = (KX_GameObject*) gameobj;

	/* remove property from debug list */
	RemoveObjectDebugProperties(newobj);

	/* Invalidate the python reference, since the object may exist in script lists
	 * its possible that it wont be automatically invalidated, so do it manually here,
	 * 
	 * if for some reason the object is added back into the scene python can always get a new Proxy
	 */
	newobj->InvalidateProxy();

	// keep the blender->game object association up to date
	// note that all the replicas of an object will have the same
	// blender object, that's why we need to check the game object
	// as only the deletion of the original object must be recorded
	if (newobj->GetBlenderObject()) {
		// In some case the game object can contains a NULL blender object e.g default camera.
		m_logicmgr->UnregisterGameObj(newobj->GetBlenderObject(), gameobj);
	}

	//todo: look at this
	//GetPhysicsEnvironment()->RemovePhysicsController(gameobj->getPhysicsController());

	// remove all sensors/controllers/actuators from logicsystem...
	
	SCA_SensorList& sensors = newobj->GetSensors();
	for (SCA_SensorList::iterator its = sensors.begin();
		 !(its==sensors.end());its++)
	{
		m_logicmgr->RemoveSensor(*its);
	}

	SCA_ControllerList& controllers = newobj->GetControllers();
	for (SCA_ControllerList::iterator itc = controllers.begin();
		 !(itc==controllers.end());itc++)
	{
		m_logicmgr->RemoveController(*itc);
		(*itc)->ReParent(NULL);
	}

	SCA_ActuatorList& actuators = newobj->GetActuators();
	for (SCA_ActuatorList::iterator ita = actuators.begin();
		 !(ita==actuators.end());ita++)
	{
		m_logicmgr->RemoveActuator(*ita);
	}
	// the sensors/controllers/actuators must also be released, this is done in ~SCA_IObject

	// now remove the timer properties from the time manager
	int numprops = newobj->GetPropertyCount();

	for (int i = 0; i < numprops; i++)
	{
		CValue* propval = newobj->GetProperty(i);
		if (propval->GetProperty("timer"))
		{
			m_timemgr->RemoveTimeProperty(propval);
		}
	}

	// if the object is the dupligroup proxy, you have to cleanup all m_pDupliGroupObject's in all
	// instances refering to this group
	if (newobj->GetInstanceObjects()) {
		for (int i = 0; i < newobj->GetInstanceObjects()->GetCount(); i++) {
			KX_GameObject* instance = (KX_GameObject*)newobj->GetInstanceObjects()->GetValue(i);
			instance->RemoveDupliGroupObject();
		}
	}

	// if this object was part of a group, make sure to remove it from that group's instance list
	KX_GameObject* group = newobj->GetDupliGroupObject();
	if (group)
		group->RemoveInstanceObject(newobj);

	if (m_obstacleSimulation) {
		m_obstacleSimulation->DestroyObstacleForObj(newobj);
	}

	newobj->RemoveMeshes();

	m_cubeMapManager->InvalidateCubeMapViewpoint(newobj);

	ret = 1;
	if (newobj->GetGameObjectType()==SCA_IObject::OBJ_LIGHT && m_lightlist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_objectlist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_tempObjectList->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_parentlist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_inactivelist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_euthanasyobjects->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_animatedlist->RemoveValue(newobj))
		ret = newobj->Release();
	if (m_fontlist->RemoveValue(newobj)) {
		ret = newobj->Release();
	}
	if (m_cameralist->RemoveValue(newobj)) {
		ret = newobj->Release();
	}

	/* Warning 'newobj' maye be freed now, only compare, don't access */


	if (newobj == m_active_camera)
	{
		//no AddRef done on m_active_camera so no Release
		//m_active_camera->Release();
		m_active_camera = NULL;
	}

	/* currently does nothing, keep in case we need to Unregister something */
#if 0
	if (m_sceneConverter)
		m_sceneConverter->UnregisterGameObject(newobj);
#endif
	
	// return value will be 0 if the object is actually deleted (all reference gone)
	
	return ret;
}



void KX_Scene::ReplaceMesh(class CValue* obj,void* meshobj, bool use_gfx, bool use_phys)
{
	KX_GameObject* gameobj = static_cast<KX_GameObject*>(obj);
	RAS_MeshObject* mesh = static_cast<RAS_MeshObject*>(meshobj);

	if (!gameobj) {
		std::cout << "KX_Scene::ReplaceMesh Warning: invalid object, doing nothing" << std::endl;
		return;
	}

	if (use_gfx && mesh != NULL)
	{
	gameobj->RemoveMeshes();
	gameobj->AddMesh(mesh);
	
	if (gameobj->IsDeformable())
	{
		BL_DeformableGameObject* newobj = static_cast<BL_DeformableGameObject*>( gameobj );
		
		if (newobj->GetDeformer())
		{
			delete newobj->GetDeformer();
			newobj->SetDeformer(NULL);
		}

		if (mesh->GetMesh()) 
		{
			// we must create a new deformer but which one?
			KX_GameObject* parentobj = newobj->GetParent();
			// this always return the original game object (also for replicate)
			Object* blendobj = newobj->GetBlenderObject();
			// object that owns the new mesh
			Object* oldblendobj = static_cast<struct Object*>(m_logicmgr->FindBlendObjByGameMeshName(mesh->GetName()));
			Mesh* blendmesh = mesh->GetMesh();

			bool bHasModifier = BL_ModifierDeformer::HasCompatibleDeformer(blendobj);
			bool bHasShapeKey = blendmesh->key != NULL && blendmesh->key->type==KEY_RELATIVE;
			bool bHasDvert = blendmesh->dvert != NULL;
			bool bHasArmature = 
				BL_ModifierDeformer::HasArmatureDeformer(blendobj) &&
				parentobj &&								// current parent is armature
				parentobj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE &&
				oldblendobj &&								// needed for mesh deform
				blendobj->parent &&							// original object had armature (not sure this test is needed)
				blendobj->parent->type == OB_ARMATURE &&
				blendmesh->dvert!=NULL;						// mesh has vertex group
#ifdef WITH_BULLET
			bool bHasSoftBody = (!parentobj && (blendobj->gameflag & OB_SOFT_BODY));
#endif
			
			if (oldblendobj==NULL) {
				if (bHasModifier || bHasShapeKey || bHasDvert || bHasArmature) {
					std::cout << "warning: ReplaceMesh() new mesh is not used in an object from the current scene, you will get incorrect behavior" << std::endl;
					bHasShapeKey= bHasDvert= bHasArmature=bHasModifier= false;
				}
			}
			
			if (bHasModifier)
			{
				BL_ModifierDeformer* modifierDeformer;
				if (bHasShapeKey || bHasArmature)
				{
					modifierDeformer = new BL_ModifierDeformer(
						newobj,
						m_blenderScene,
						oldblendobj, blendobj,
						mesh,
						true,
						static_cast<BL_ArmatureObject*>( parentobj->AddRef() )
					);
					modifierDeformer->LoadShapeDrivers(parentobj);
				}
				else
				{
					modifierDeformer = new BL_ModifierDeformer(
						newobj,
						m_blenderScene,
						oldblendobj, blendobj,
						mesh,
						false,
						NULL
					);
				}
				newobj->SetDeformer(modifierDeformer);
			}
			else if (bHasShapeKey) {
				BL_ShapeDeformer* shapeDeformer;
				if (bHasArmature) 
				{
					shapeDeformer = new BL_ShapeDeformer(
						newobj,
						oldblendobj, blendobj,
						mesh,
						true,
						true,
						static_cast<BL_ArmatureObject*>( parentobj->AddRef() )
					);
					shapeDeformer->LoadShapeDrivers(parentobj);
				}
				else
				{
					shapeDeformer = new BL_ShapeDeformer(
						newobj,
						oldblendobj, blendobj,
						mesh,
						false,
						true,
						NULL
					);
				}
				newobj->SetDeformer( shapeDeformer);
			}
			else if (bHasArmature) 
			{
				BL_SkinDeformer* skinDeformer = new BL_SkinDeformer(
					newobj,
					oldblendobj, blendobj,
					mesh,
					true,
					true,
					static_cast<BL_ArmatureObject*>( parentobj->AddRef() )
				);
				newobj->SetDeformer(skinDeformer);
			}
			else if (bHasDvert)
			{
				BL_MeshDeformer* meshdeformer = new BL_MeshDeformer(
					newobj, oldblendobj, mesh
				);
				newobj->SetDeformer(meshdeformer);
			}
#ifdef WITH_BULLET
			else if (bHasSoftBody)
			{
				KX_SoftBodyDeformer *softdeformer = new KX_SoftBodyDeformer(mesh, newobj);
				newobj->SetDeformer(softdeformer);
			}
#endif
		}
	}

	gameobj->AddMeshUser();
	}

	if (use_phys) { /* update the new assigned mesh with the physics mesh */
		if (gameobj->GetPhysicsController())
			gameobj->GetPhysicsController()->ReinstancePhysicsShape(NULL, use_gfx?NULL:mesh);
	}
	gameobj->UpdateBounds(true);
}


KX_Camera* KX_Scene::GetActiveCamera()
{
	// NULL if not defined
	return m_active_camera;
}


void KX_Scene::SetActiveCamera(KX_Camera* cam)
{
	// only set if the cam is in the active list? Or add it otherwise?
	if (!m_cameralist->SearchValue(cam)) {
		m_cameralist->Add(cam->AddRef());
		if (cam) std::cout << "Added cam " << cam->GetName() << std::endl;
	} 

	m_active_camera = cam;
}

void KX_Scene::SetCameraOnTop(KX_Camera* cam)
{
	if (!m_cameralist->SearchValue(cam)) {
		// adding is always done at the back, so that's all that needs to be done
		m_cameralist->Add(cam->AddRef());
		if (cam) std::cout << "Added cam " << cam->GetName() << std::endl;
	}
	else {
		// no release and addref just change camera place
		m_cameralist->RemoveValue(cam);
		m_cameralist->Add(cam);
	}
}

void KX_Scene::MarkVisible(RAS_IRasterizer* rasty, KX_GameObject* gameobj,KX_Camera*  cam,int layer)
{
	// User (Python/Actuator) has forced object invisible...
	if (!gameobj->GetSGNode() || !gameobj->GetVisible())
		return;
	
	// Shadow lamp layers
	if (layer && !(gameobj->GetLayer() & layer)) {
		gameobj->SetCulled(true);
		return;
	}

	// If Frustum culling is off, the object is always visible.
	bool vis = !cam->GetFrustumCulling();
	
	// If the camera is inside this node, then the object is visible.
	if (!vis)
	{
		vis = gameobj->GetSGNode()->inside( cam->GetCameraLocation() );
	}
		
	// Test the object's bound sphere against the view frustum.
	if (!vis)
	{
		SG_BBox &box = gameobj->GetSGNode()->BBox();
		const MT_Vector3& scale = gameobj->NodeGetWorldScaling();
		const MT_Scalar radius = fabs(scale[scale.closestAxis()] * box.GetRadius());
		const MT_Vector3 center = gameobj->NodeGetWorldPosition() + (box.GetCenter() * scale) * gameobj->NodeGetWorldOrientation();
		switch (cam->SphereInsideFrustum(center, radius))
		{
			case KX_Camera::INSIDE:
				vis = true;
				break;
			case KX_Camera::OUTSIDE:
				vis = false;
				break;
			case KX_Camera::INTERSECT:
				// Test the object's bound box against the view frustum.
				MT_Vector3 box[8];
				gameobj->GetSGNode()->getBBox(box); 
				vis = cam->BoxInsideFrustum(box) != KX_Camera::OUTSIDE;
				break;
		}
	}

	// Visibility/ non-visibility are marked
	// elsewhere now.
	gameobj->SetCulled(!vis);
}

void KX_Scene::PhysicsCullingCallback(KX_ClientObjectInfo *objectInfo, void* cullingInfo)
{
	KX_GameObject* gameobj = objectInfo->m_gameobject;
	if (!gameobj->GetVisible())
		// ideally, invisible objects should be removed from the culling tree temporarily
		return;
	if (((CullingInfo*)cullingInfo)->m_layer && !(gameobj->GetLayer() & ((CullingInfo*)cullingInfo)->m_layer))
		// used for shadow: object is not in shadow layer
		return;

	// make object visible
	gameobj->SetCulled(false);
}

void KX_Scene::CalculateVisibleMeshes(RAS_IRasterizer* rasty,KX_Camera* cam, int layer)
{
	// Update the object boudning volume box if the object had a deformer.
	for (int i = 0; i < m_objectlist->GetCount(); i++) {
		KX_GameObject *gameobj = static_cast<KX_GameObject*>(m_objectlist->GetValue(i));
		if (gameobj->GetDeformer()) {
			/** Update all the deformer, not only per material.
			 * One of the side effect is to clear some flags about AABB calculation.
			 * like in KX_SoftBodyDeformer.
			 */
			gameobj->GetDeformer()->UpdateBuckets();
		}
		gameobj->UpdateBounds();
	}

	bool dbvt_culling = false;
	if (m_dbvt_culling) 
	{
		/* Reset KX_GameObject m_bCulled to true before doing culling
		 * since DBVT culling will only set it to false.
		 * This is similar to what RAS_BucketManager does for RAS_MeshSlot culling.
		 */
		for (int i = 0; i < m_objectlist->GetCount(); i++) {
			KX_GameObject *gameobj = static_cast<KX_GameObject*>(m_objectlist->GetValue(i));
			gameobj->SetCulled(true);
		}

		// test culling through Bullet
		// get the clip planes
		const MT_Vector4* cplanes = cam->GetNormalizedClipPlanes();
		// and convert
		MT_Vector4 planes[6] = {cplanes[4], cplanes[5], cplanes[0], cplanes[1], cplanes[2], cplanes[3]};

		CullingInfo info(layer);

		float mvmat[16] = {0.0f};
		cam->GetModelviewMatrix().getValue(mvmat);
		float pmat[16] = {0.0f};
		cam->GetProjectionMatrix().getValue(pmat);

		dbvt_culling = m_physicsEnvironment->CullingTest(PhysicsCullingCallback,&info,planes,6,m_dbvt_occlusion_res,
		                                                 KX_GetActiveEngine()->GetCanvas()->GetViewPort(),
		                                                 mvmat, pmat);
	}
	if (!dbvt_culling) {
		// the physics engine couldn't help us, do it the hard way
		for (int i = 0; i < m_objectlist->GetCount(); i++)
		{
			MarkVisible(rasty, static_cast<KX_GameObject*>(m_objectlist->GetValue(i)), cam, layer);
		}
	}
}

void KX_Scene::DrawDebug(RAS_IRasterizer *rasty)
{
	const bool showBoundingBox = KX_GetActiveEngine()->GetShowBoundingBox();
	if (showBoundingBox) {
		for (CListValue::iterator it = m_objectlist->GetBegin(); it != m_objectlist->GetEnd(); ++it) {
			KX_GameObject *gameobj = (KX_GameObject *)*it;

			if (!gameobj->GetCulled() && gameobj->GetMeshCount() != 0) {
				const MT_Vector3& scale = gameobj->NodeGetWorldScaling();
				const MT_Vector3& position = gameobj->NodeGetWorldPosition();
				const MT_Matrix3x3& orientation = gameobj->NodeGetWorldOrientation();
				const SG_BBox& box = gameobj->GetSGNode()->BBox();
				const MT_Vector3& center = box.GetCenter();

				rasty->DrawDebugBox(this, position, orientation, box.GetMin() * scale, box.GetMax() * scale,
					MT_Vector4(1.0f, 0.0f, 1.0f, 1.0f));

				// Render center in red, green and blue.
				rasty->DrawDebugLine(this, orientation * center * scale + position,
					orientation * (center + MT_Vector3(1.0f, 0.0f, 0.0f)) * scale + position,
					MT_Vector4(1.0f, 0.0f, 0.0f, 1.0f));
				rasty->DrawDebugLine(this, orientation * center * scale + position,
					orientation * (center + MT_Vector3(0.0f, 1.0f, 0.0f)) * scale  + position,
					MT_Vector4(0.0f, 1.0f, 0.0f, 1.0f));
				rasty->DrawDebugLine(this, orientation * center * scale + position,
					orientation * (center + MT_Vector3(0.0f, 0.0f, 1.0f)) * scale  + position,
					MT_Vector4(0.0f, 0.0f, 1.0f, 1.0f));
			}
		}
	}
	// The side effect of a armature is that it was added in the animated object list.
	for (CListValue::iterator it = m_animatedlist->GetBegin(), end = m_animatedlist->GetEnd(); it != end; ++it) {
		KX_GameObject *gameobj = (KX_GameObject *)*it;
		if (gameobj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE) {
			BL_ArmatureObject *armature = (BL_ArmatureObject *)gameobj;
			if (armature->GetDrawDebug() || KX_GetActiveEngine()->GetShowArmatures()) {
				armature->DrawDebugArmature();
			}
		}
	}
}

// logic stuff
void KX_Scene::LogicBeginFrame(double curtime, double framestep)
{
	// have a look at temp objects ...
	int lastobj = m_tempObjectList->GetCount() - 1;
	
	for (int i = lastobj; i >= 0; i--)
	{
		CValue* objval = m_tempObjectList->GetValue(i);
		CFloatValue* propval = (CFloatValue*) objval->GetProperty("::timebomb");
		
		if (propval)
		{
			float timeleft = propval->GetNumber() - framestep;
			
			if (timeleft > 0)
			{
				propval->SetFloat(timeleft);
			}
			else
			{
				DelayedRemoveObject(objval);
				// remove obj
			}
		}
		else
		{
			// all object is the tempObjectList should have a clock
		}
	}
	m_logicmgr->BeginFrame(curtime, framestep);
}

void KX_Scene::AddAnimatedObject(CValue* gameobj)
{
	gameobj->AddRef();
	m_animatedlist->Add(gameobj);
}

static void update_anim_thread_func(TaskPool *pool, void *taskdata, int UNUSED(threadid))
{
	KX_GameObject *gameobj, *child, *parent;
	CListValue *children;
	bool needs_update;
	double curtime = *(double*)BLI_task_pool_userdata(pool);

	gameobj = (KX_GameObject*)taskdata;

	// Non-armature updates are fast enough, so just update them
	needs_update = gameobj->GetGameObjectType() != SCA_IObject::OBJ_ARMATURE;

	if (!needs_update) {
		// If we got here, we're looking to update an armature, so check its children meshes
		// to see if we need to bother with a more expensive pose update
		children = gameobj->GetChildren();

		bool has_mesh = false, has_non_mesh = false;

		// Check for meshes that haven't been culled
		for (int j=0; j<children->GetCount(); ++j) {
			child = (KX_GameObject*)children->GetValue(j);

			if (!child->GetCulled()) {
				needs_update = true;
				break;
			}

			if (child->GetMeshCount() == 0)
				has_non_mesh = true;
			else
				has_mesh = true;
		}

		// If we didn't find a non-culled mesh, check to see
		// if we even have any meshes, and update if this
		// armature has only non-mesh children.
		if (!needs_update && !has_mesh && has_non_mesh)
			needs_update = true;

		children->Release();
	}

	// If the object is a culled armature, then we manage only the animation time and end of its animations.
	gameobj->UpdateActionManager(curtime, needs_update);

	if (needs_update) {
		children = gameobj->GetChildren();
		parent = gameobj->GetParent();

		// Only do deformers here if they are not parented to an armature, otherwise the armature will
		// handle updating its children
		if (gameobj->GetDeformer() && (!parent || parent->GetGameObjectType() != SCA_IObject::OBJ_ARMATURE))
			gameobj->GetDeformer()->Update();

		for (int j=0; j<children->GetCount(); ++j) {
			child = (KX_GameObject*)children->GetValue(j);

			if (child->GetDeformer()) {
				child->GetDeformer()->Update();
			}
		}

		children->Release();
	}
}

void KX_Scene::UpdateAnimations(double curtime)
{
	TaskPool *pool = BLI_task_pool_create(KX_GetActiveEngine()->GetTaskScheduler(), &curtime);

	for (int i=0; i<m_animatedlist->GetCount(); ++i) {
		BLI_task_pool_push(pool, update_anim_thread_func, m_animatedlist->GetValue(i), false, TASK_PRIORITY_LOW);
	}

	BLI_task_pool_work_and_wait(pool);
	BLI_task_pool_free(pool);

	for (unsigned int i = 0; i < m_animatedlist->GetCount(); ++i) {
		((KX_GameObject *)m_animatedlist->GetValue(i))->UpdateActionIPOs();
	}
}

void KX_Scene::LogicUpdateFrame(double curtime, bool frame)
{
	// Update object components
	for (int i = 0; i < m_objectlist->GetCount(); ++i) {
		((KX_GameObject*)m_objectlist->GetValue(i))->UpdateComponents();
	}
	m_logicmgr->UpdateFrame(curtime, frame);
}

void KX_Scene::LogicEndFrame()
{
	m_logicmgr->EndFrame();
	int numobj;

	KX_GameObject* obj;

	while ((numobj = m_euthanasyobjects->GetCount()) > 0)
	{
		// remove the object from this list to make sure we will not hit it again
		obj = (KX_GameObject*)m_euthanasyobjects->GetValue(numobj-1);
		m_euthanasyobjects->Remove(numobj-1);
		obj->Release();
		RemoveObject(obj);
	}

	//prepare obstacle simulation for new frame
	if (m_obstacleSimulation)
		m_obstacleSimulation->UpdateObstacles();
}



/**
 * UpdateParents: SceneGraph transformation update.
 */
void KX_Scene::UpdateParents(double curtime)
{
	// we use the SG dynamic list
	SG_Node* node;

	while ((node = SG_Node::GetNextScheduled(m_sghead)) != NULL)
	{
		node->UpdateWorldData(curtime);
	}

	//for (int i=0; i<GetRootParentList()->GetCount(); i++)
	//{
	//	KX_GameObject* parentobj = (KX_GameObject*)GetRootParentList()->GetValue(i);
	//	parentobj->NodeUpdateGS(curtime);
	//}

	// the list must be empty here
	assert(m_sghead.Empty());
	// some nodes may be ready for reschedule, move them to schedule list for next time
	while ((node = SG_Node::GetNextRescheduled(m_sghead)) != NULL)
	{
		node->Schedule(m_sghead);
	}
}


RAS_MaterialBucket* KX_Scene::FindBucket(class RAS_IPolyMaterial* polymat, bool &bucketCreated)
{
	return m_bucketmanager->FindBucket(polymat, bucketCreated);
}



void KX_Scene::RenderBuckets(const MT_Transform & cameratransform,
                             class RAS_IRasterizer* rasty)
{
	for (CListValue::iterator it = m_objectlist->GetBegin(), end = m_objectlist->GetEnd(); it != end; ++it) {
		/* This function update all mesh slot info (e.g culling, color, matrix) from the game object.
		 * It's done just before the render to be sure of the object color and visibility. */
		((KX_GameObject *)*it)->UpdateBuckets();
	}

	m_bucketmanager->Renderbuckets(cameratransform,rasty);
	KX_BlenderMaterial::EndFrame(rasty);
}

void KX_Scene::RenderPlanars(RAS_IRasterizer *rasty)
{
	m_planarManager->Render(rasty);
}

void KX_Scene::RenderCubeMaps(RAS_IRasterizer *rasty)
{
	m_cubeMapManager->Render(rasty);
}

void KX_Scene::UpdateObjectLods()
{
	if (!m_active_camera)
		return;

	const MT_Vector3& cam_pos = m_active_camera->NodeGetWorldPosition();
	const float lodfactor = m_active_camera->GetLodDistanceFactor();

	for (CListValue::iterator it = m_objectlist->GetBegin(), end = m_objectlist->GetEnd(); it != end; ++it) {
		KX_GameObject *gameobj = (KX_GameObject *)*it;
		if (!gameobj->GetCulled()) {
			gameobj->UpdateLod(cam_pos, lodfactor);
		}
	}
}

void KX_Scene::SetLodHysteresis(bool active)
{
	m_isActivedHysteresis = active;
}

bool KX_Scene::IsActivedLodHysteresis(void)
{
	return m_isActivedHysteresis;
}

void KX_Scene::SetLodHysteresisValue(int hysteresisvalue)
{
	m_lodHysteresisValue = hysteresisvalue;
}

int KX_Scene::GetLodHysteresisValue(void)
{
	return m_lodHysteresisValue;
}

void KX_Scene::UpdateObjectActivity(void) 
{
	if (m_activity_culling) {
		/* determine the activity criterium and set objects accordingly */
		int i=0;
		
		MT_Vector3 camloc = GetActiveCamera()->NodeGetWorldPosition(); //GetCameraLocation();
		
		for (i=0;i<GetObjectList()->GetCount();i++)
		{
			KX_GameObject* ob = (KX_GameObject*) GetObjectList()->GetValue(i);
			
			if (!ob->GetIgnoreActivityCulling()) {
				/* Simple test: more than 10 away from the camera, count
				 * Manhattan distance. */
				MT_Vector3 obpos = ob->NodeGetWorldPosition();
				
				if ((fabsf(camloc[0] - obpos[0]) > m_activity_box_radius) ||
				    (fabsf(camloc[1] - obpos[1]) > m_activity_box_radius) ||
				    (fabsf(camloc[2] - obpos[2]) > m_activity_box_radius) )
				{
					ob->Suspend();
				}
				else {
					ob->Resume();
				}
			}
		}
	}
}

void KX_Scene::SetActivityCullingRadius(float f)
{
	if (f < 0.5f)
		f = 0.5f;
	m_activity_box_radius = f;
}

KX_NetworkMessageScene* KX_Scene::GetNetworkMessageScene()
{
	return m_networkScene;
}

void KX_Scene::SetNetworkMessageScene(KX_NetworkMessageScene *newScene)
{
	m_networkScene = newScene;
}

void	KX_Scene::SetGravity(const MT_Vector3& gravity)
{
	GetPhysicsEnvironment()->SetGravity(gravity[0],gravity[1],gravity[2]);
}

MT_Vector3 KX_Scene::GetGravity()
{
	MT_Vector3 gravity;

	GetPhysicsEnvironment()->GetGravity(gravity);

	return gravity;
}

void KX_Scene::SetSceneConverter(class KX_BlenderSceneConverter* sceneConverter)
{
	m_sceneConverter = sceneConverter;
}

void KX_Scene::SetPhysicsEnvironment(class PHY_IPhysicsEnvironment* physEnv)
{
	m_physicsEnvironment = physEnv;
	if (m_physicsEnvironment) {
		KX_CollisionEventManager* collisionmgr = new KX_CollisionEventManager(m_logicmgr, physEnv);
		m_logicmgr->RegisterEventManager(collisionmgr);
	}
}
 
void KX_Scene::setSuspendedTime(double suspendedtime)
{
	m_suspendedtime = suspendedtime;
}
double KX_Scene::getSuspendedTime()
{
	return m_suspendedtime;
}
void KX_Scene::setSuspendedDelta(double suspendeddelta)
{
	m_suspendeddelta = suspendeddelta;
}
double KX_Scene::getSuspendedDelta()
{
	return m_suspendeddelta;
}

short KX_Scene::GetAnimationFPS()
{
	return m_blenderScene->r.frs_sec;
}

static void MergeScene_LogicBrick(SCA_ILogicBrick* brick, KX_Scene *from, KX_Scene *to)
{
	SCA_LogicManager *logicmgr= to->GetLogicManager();

	brick->Replace_IScene(to);
	brick->Replace_NetworkScene(to->GetNetworkMessageScene());
	brick->SetLogicManager(to->GetLogicManager());

	// If we end up replacing a KX_CollisionEventManager, we need to make sure
	// physics controllers are properly in place. In other words, do this
	// after merging physics controllers!
	SCA_ISensor *sensor=  dynamic_cast<class SCA_ISensor *>(brick);
	if (sensor) {
		sensor->Replace_EventManager(logicmgr);
	}

	SCA_2DFilterActuator *filter_actuator = dynamic_cast<class SCA_2DFilterActuator*>(brick);
	if (filter_actuator) {
		filter_actuator->SetScene(to);
	}

#ifdef WITH_PYTHON
	// Python must be called from the main thread unless we want to deal
	// with GIL issues. So, this is delayed until here in case of async
	// libload (originally in KX_ConvertControllers)
	SCA_PythonController *pyctrl = dynamic_cast<SCA_PythonController*>(brick);
	if (pyctrl) {
		pyctrl->SetNamespace(KX_GetActiveEngine()->GetPyNamespace());

		if (pyctrl->m_mode==SCA_PythonController::SCA_PYEXEC_SCRIPT)
			pyctrl->Compile();
	}
#endif
}

static void MergeScene_GameObject(KX_GameObject* gameobj, KX_Scene *to, KX_Scene *from)
{
	{
		SCA_ActuatorList& actuators= gameobj->GetActuators();
		SCA_ActuatorList::iterator ita;

		for (ita = actuators.begin(); !(ita==actuators.end()); ++ita)
		{
			MergeScene_LogicBrick(*ita, from, to);
		}
	}


	{
		SCA_SensorList& sensors= gameobj->GetSensors();
		SCA_SensorList::iterator its;

		for (its = sensors.begin(); !(its==sensors.end()); ++its)
		{
			MergeScene_LogicBrick(*its, from, to);
		}
	}

	{
		SCA_ControllerList& controllers= gameobj->GetControllers();
		SCA_ControllerList::iterator itc;

		for (itc = controllers.begin(); !(itc==controllers.end()); ++itc)
		{
			SCA_IController *cont= *itc;
			MergeScene_LogicBrick(cont, from, to);
		}
	}

	/* graphics controller */
	PHY_IController *ctrl = gameobj->GetGraphicController();
	if (ctrl) {
		/* SHOULD update the m_cullingTree */
		ctrl->SetPhysicsEnvironment(to->GetPhysicsEnvironment());
	}

	ctrl = gameobj->GetPhysicsController();
	if (ctrl) {
		ctrl->SetPhysicsEnvironment(to->GetPhysicsEnvironment());
	}

	/* SG_Node can hold a scene reference */
	SG_Node *sg= gameobj->GetSGNode();
	if (sg) {
		if (sg->GetSGClientInfo() == from) {
			sg->SetSGClientInfo(to);

			/* Make sure to grab the children too since they might not be tied to a game object */
			NodeList children = sg->GetSGChildren();
			for (int i=0; i<children.size(); i++)
					children[i]->SetSGClientInfo(to);
		}
	}
	/* If the object is a light, update it's scene */
	if (gameobj->GetGameObjectType() == SCA_IObject::OBJ_LIGHT)
		((KX_LightObject*)gameobj)->UpdateScene(to);

	// All armatures should be in the animated object list to be umpdated.
	if (gameobj->GetGameObjectType() == SCA_IObject::OBJ_ARMATURE)
		to->AddAnimatedObject(gameobj);

	/* Add the object to the scene's logic manager */
	to->GetLogicManager()->RegisterGameObjectName(gameobj->GetName(), gameobj);
	to->GetLogicManager()->RegisterGameObj(gameobj->GetBlenderObject(), gameobj);

	for (int i = 0; i < gameobj->GetMeshCount(); ++i) {
		RAS_MeshObject *meshobj = gameobj->GetMesh(i);
		// Register the mesh object by name and blender object.
		to->GetLogicManager()->RegisterGameMeshName(meshobj->GetName(), gameobj->GetBlenderObject());
		to->GetLogicManager()->RegisterMeshName(meshobj->GetName(), meshobj);
	}
}

bool KX_Scene::MergeScene(KX_Scene *other)
{
	PHY_IPhysicsEnvironment *env = this->GetPhysicsEnvironment();
	PHY_IPhysicsEnvironment *env_other = other->GetPhysicsEnvironment();

	if ((env==NULL) != (env_other==NULL)) /* TODO - even when both scenes have NONE physics, the other is loaded with bullet enabled, ??? */
	{
		printf("KX_Scene::MergeScene: physics scenes type differ, aborting\n");
		printf("\tsource %d, terget %d\n", (int)(env!=NULL), (int)(env_other!=NULL));
		return false;
	}

	if (GetSceneConverter() != other->GetSceneConverter()) {
		printf("KX_Scene::MergeScene: converters differ, aborting\n");
		return false;
	}


	GetBucketManager()->MergeBucketManager(other->GetBucketManager(), this);


	/* active + inactive == all ??? - lets hope so */
	for (int i = 0; i < other->GetObjectList()->GetCount(); i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*)other->GetObjectList()->GetValue(i);
		MergeScene_GameObject(gameobj, this, other);

		/* add properties to debug list for LibLoad objects */
		if (KX_GetActiveEngine()->GetAutoAddDebugProperties()) {
			AddObjectDebugProperties(gameobj);
		}
	}

	for (int i = 0; i < other->GetInactiveList()->GetCount(); i++)
	{
		KX_GameObject* gameobj = (KX_GameObject*)other->GetInactiveList()->GetValue(i);
		MergeScene_GameObject(gameobj, this, other);
	}

	if (env) {
		env->MergeEnvironment(env_other);
		CListValue *otherObjects = other->GetObjectList();

		// List of all physics objects to merge (needed by ReplicateConstraints).
		std::vector<KX_GameObject *> physicsObjects;
		for (unsigned int i = 0; i < otherObjects->GetCount(); ++i) {
			KX_GameObject *gameobj = (KX_GameObject *)otherObjects->GetValue(i);
			if (gameobj->GetPhysicsController()) {
				physicsObjects.push_back(gameobj);
			}
		}

		for (unsigned int i = 0; i < physicsObjects.size(); ++i) {
			KX_GameObject *gameobj = physicsObjects[i];
			// Replicate all constraints in the right physics environment.
			gameobj->GetPhysicsController()->ReplicateConstraints(gameobj, physicsObjects);
			gameobj->ClearConstraints();
		}
	}


	GetTempObjectList()->MergeList(other->GetTempObjectList());
	other->GetTempObjectList()->ReleaseAndRemoveAll();

	GetObjectList()->MergeList(other->GetObjectList());
	other->GetObjectList()->ReleaseAndRemoveAll();

	GetInactiveList()->MergeList(other->GetInactiveList());
	other->GetInactiveList()->ReleaseAndRemoveAll();

	GetRootParentList()->MergeList(other->GetRootParentList());
	other->GetRootParentList()->ReleaseAndRemoveAll();

	GetLightList()->MergeList(other->GetLightList());
	other->GetLightList()->ReleaseAndRemoveAll();

	GetCameraList()->MergeList(other->GetCameraList());
	other->GetCameraList()->ReleaseAndRemoveAll();

	GetFontList()->MergeList(other->GetFontList());
	other->GetFontList()->ReleaseAndRemoveAll();

	/* move materials across, assume they both use the same scene-converters
	 * Do this after lights are merged so materials can use the lights in shaders
	 */
	GetSceneConverter()->MergeScene(this, other);

	/* merge logic */
	{
		SCA_LogicManager *logicmgr=			GetLogicManager();
		SCA_LogicManager *logicmgr_other=	other->GetLogicManager();

		vector<class SCA_EventManager*>evtmgrs= logicmgr->GetEventManagers();
		//vector<class SCA_EventManager*>evtmgrs_others= logicmgr_other->GetEventManagers();

		//SCA_EventManager *evtmgr;
		SCA_EventManager *evtmgr_other;

		for (unsigned int i= 0; i < evtmgrs.size(); i++) {
			evtmgr_other= logicmgr_other->FindEventManager(evtmgrs[i]->GetType());

			if (evtmgr_other) /* unlikely but possible one scene has a joystick and not the other */
				evtmgr_other->Replace_LogicManager(logicmgr);

			/* when merging objects sensors are moved across into the new manager, don't need to do this here */
		}

		/* grab any timer properties from the other scene */
		SCA_TimeEventManager *timemgr=		GetTimeEventManager();
		SCA_TimeEventManager *timemgr_other=	other->GetTimeEventManager();
		vector<CValue*> times = timemgr_other->GetTimeValues();

		for (unsigned int i= 0; i < times.size(); i++) {
			timemgr->AddTimeProperty(times[i]);
		}
		
	}
	return true;
}

RAS_2DFilterManager *KX_Scene::Get2DFilterManager() const
{
	return m_filterManager;
}

void KX_Scene::Render2DFilters(RAS_IRasterizer *rasty, RAS_ICanvas *canvas, unsigned short target)
{
	m_filterManager->RenderFilters(rasty, canvas, target);
}

#ifdef WITH_PYTHON

void KX_Scene::RunDrawingCallbacks(PyObject *cb_list)
{
	if (!cb_list || PyList_GET_SIZE(cb_list) == 0)
		return;

	RunPythonCallBackList(cb_list, NULL, 0, 0);
}

//----------------------------------------------------------------------------
//Python

PyTypeObject KX_Scene::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_Scene",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,
	&Sequence,
	&Mapping,
	0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_Scene::Methods[] = {
	KX_PYMETHODTABLE(KX_Scene, addObject),
	KX_PYMETHODTABLE(KX_Scene, end),
	KX_PYMETHODTABLE(KX_Scene, restart),
	KX_PYMETHODTABLE(KX_Scene, replace),
	KX_PYMETHODTABLE(KX_Scene, suspend),
	KX_PYMETHODTABLE(KX_Scene, resume),
	KX_PYMETHODTABLE(KX_Scene, drawObstacleSimulation),

	
	/* dict style access */
	KX_PYMETHODTABLE(KX_Scene, get),
	
	{NULL,NULL} //Sentinel
};
static PyObject *Map_GetItem(PyObject *self_v, PyObject *item)
{
	KX_Scene* self = static_cast<KX_Scene*>BGE_PROXY_REF(self_v);
	const char *attr_str= _PyUnicode_AsString(item);
	PyObject *pyconvert;
	
	if (self == NULL) {
		PyErr_SetString(PyExc_SystemError, "val = scene[key]: KX_Scene, " BGE_PROXY_ERROR_MSG);
		return NULL;
	}

	if (!self->m_attr_dict)
		self->m_attr_dict = PyDict_New();
	
	if (self->m_attr_dict && (pyconvert=PyDict_GetItem(self->m_attr_dict, item))) {
		
		if (attr_str)
			PyErr_Clear();
		Py_INCREF(pyconvert);
		return pyconvert;
	}
	else {
		if (attr_str)	PyErr_Format(PyExc_KeyError, "value = scene[key]: KX_Scene, key \"%s\" does not exist", attr_str);
		else			PyErr_SetString(PyExc_KeyError, "value = scene[key]: KX_Scene, key does not exist");
		return NULL;
	}
		
}

static int Map_SetItem(PyObject *self_v, PyObject *key, PyObject *val)
{
	KX_Scene* self = static_cast<KX_Scene*>BGE_PROXY_REF(self_v);
	const char *attr_str= _PyUnicode_AsString(key);
	if (attr_str==NULL)
		PyErr_Clear();
	
	if (self == NULL) {
		PyErr_SetString(PyExc_SystemError, "scene[key] = value: KX_Scene, " BGE_PROXY_ERROR_MSG);
		return -1;
	}

	if (!self->m_attr_dict)
		self->m_attr_dict = PyDict_New();

	if (val==NULL) { /* del ob["key"] */
		int del= 0;
		
		if (self->m_attr_dict)
			del |= (PyDict_DelItem(self->m_attr_dict, key)==0) ? 1:0;
		
		if (del==0) {
			if (attr_str)	PyErr_Format(PyExc_KeyError, "scene[key] = value: KX_Scene, key \"%s\" could not be set", attr_str);
			else			PyErr_SetString(PyExc_KeyError, "del scene[key]: KX_Scene, key could not be deleted");
			return -1;
		}
		else if (self->m_attr_dict) {
			PyErr_Clear(); /* PyDict_DelItem sets an error when it fails */
		}
	}
	else { /* ob["key"] = value */
		int set = 0;

		if (self->m_attr_dict==NULL) /* lazy init */
			self->m_attr_dict= PyDict_New();
		
		
		if (PyDict_SetItem(self->m_attr_dict, key, val)==0)
			set= 1;
		else
			PyErr_SetString(PyExc_KeyError, "scene[key] = value: KX_Scene, key not be added to internal dictionary");
	
		if (set==0)
			return -1; /* pythons error value */
		
	}
	
	return 0; /* success */
}

static int Seq_Contains(PyObject *self_v, PyObject *value)
{
	KX_Scene* self = static_cast<KX_Scene*>BGE_PROXY_REF(self_v);
	
	if (self == NULL) {
		PyErr_SetString(PyExc_SystemError, "val in scene: KX_Scene, " BGE_PROXY_ERROR_MSG);
		return -1;
	}

	if (!self->m_attr_dict)
		self->m_attr_dict = PyDict_New();

	if (self->m_attr_dict && PyDict_GetItem(self->m_attr_dict, value))
		return 1;
	
	return 0;
}

PyMappingMethods KX_Scene::Mapping = {
	(lenfunc)NULL,                  /* inquiry mp_length */
	(binaryfunc)Map_GetItem,        /* binaryfunc mp_subscript */
	(objobjargproc)Map_SetItem,     /* objobjargproc mp_ass_subscript */
};

PySequenceMethods KX_Scene::Sequence = {
	NULL,                       /* Cant set the len otherwise it can evaluate as false */
	NULL,                       /* sq_concat */
	NULL,                       /* sq_repeat */
	NULL,                       /* sq_item */
	NULL,                       /* sq_slice */
	NULL,                       /* sq_ass_item */
	NULL,                       /* sq_ass_slice */
	(objobjproc)Seq_Contains,   /* sq_contains */
	(binaryfunc) NULL,          /* sq_inplace_concat */
	(ssizeargfunc) NULL,        /* sq_inplace_repeat */
};

PyObject *KX_Scene::pyattr_get_name(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);
	return PyUnicode_From_STR_String(self->GetName());
}

PyObject *KX_Scene::pyattr_get_objects(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);
	return self->GetObjectList()->GetProxy();
}

PyObject *KX_Scene::pyattr_get_objects_inactive(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);
	return self->GetInactiveList()->GetProxy();
}

PyObject *KX_Scene::pyattr_get_lights(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);
	return self->GetLightList()->GetProxy();
}

PyObject *KX_Scene::pyattr_get_filter_manager(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene *self = static_cast<KX_Scene*>(self_v);
	KX_2DFilterManager *filterManager = (KX_2DFilterManager *)self->Get2DFilterManager();

	return filterManager->GetProxy();
}

PyObject *KX_Scene::pyattr_get_world(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);
	KX_WorldInfo *world = self->GetWorldInfo();

	if (world->GetName() != "") {
		return world->GetProxy();
	}
	else {
		Py_RETURN_NONE;
	}
}

PyObject *KX_Scene::pyattr_get_texts(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene *self = static_cast<KX_Scene *>(self_v);
	return self->GetFontList()->GetProxy();
}

PyObject *KX_Scene::pyattr_get_cameras(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene *self = static_cast<KX_Scene *>(self_v);
	return self->GetCameraList()->GetProxy();
}

PyObject *KX_Scene::pyattr_get_active_camera(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);
	KX_Camera* cam= self->GetActiveCamera();
	if (cam)
		return self->GetActiveCamera()->GetProxy();
	else
		Py_RETURN_NONE;
}


int KX_Scene::pyattr_set_active_camera(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);
	KX_Camera *camOb;
	
	if (!ConvertPythonToCamera(self, value, &camOb, false, "scene.active_camera = value: KX_Scene"))
		return PY_SET_ATTR_FAIL;
	
	self->SetActiveCamera(camOb);
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_Scene::pyattr_get_drawing_callback_pre(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if (self->m_draw_call_pre==NULL)
		self->m_draw_call_pre= PyList_New(0);
	Py_INCREF(self->m_draw_call_pre);
	return self->m_draw_call_pre;
}

PyObject *KX_Scene::pyattr_get_drawing_callback_post(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if (self->m_draw_call_post==NULL)
		self->m_draw_call_post= PyList_New(0);
	Py_INCREF(self->m_draw_call_post);
	return self->m_draw_call_post;
}

PyObject *KX_Scene::pyattr_get_drawing_setup_callback_pre(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if (self->m_draw_setup_call_pre == NULL)
		self->m_draw_setup_call_pre = PyList_New(0);

	Py_INCREF(self->m_draw_setup_call_pre);
	return self->m_draw_setup_call_pre;
}

int KX_Scene::pyattr_set_drawing_callback_pre(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if (!PyList_CheckExact(value))
	{
		PyErr_SetString(PyExc_ValueError, "Expected a list");
		return PY_SET_ATTR_FAIL;
	}
	Py_XDECREF(self->m_draw_call_pre);

	Py_INCREF(value);
	self->m_draw_call_pre = value;

	return PY_SET_ATTR_SUCCESS;
}

int KX_Scene::pyattr_set_drawing_callback_post(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if (!PyList_CheckExact(value))
	{
		PyErr_SetString(PyExc_ValueError, "Expected a list");
		return PY_SET_ATTR_FAIL;
	}
	Py_XDECREF(self->m_draw_call_post);

	Py_INCREF(value);
	self->m_draw_call_post = value;

	return PY_SET_ATTR_SUCCESS;
}

int KX_Scene::pyattr_set_drawing_setup_callback_pre(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	if (!PyList_CheckExact(value)) {
		PyErr_SetString(PyExc_ValueError, "Expected a list");
		return PY_SET_ATTR_FAIL;
	}

	Py_XDECREF(self->m_draw_setup_call_pre);
	Py_INCREF(value);

	self->m_draw_setup_call_pre = value;
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_Scene::pyattr_get_gravity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	return PyObjectFrom(self->GetGravity());
}

int KX_Scene::pyattr_set_gravity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_Scene* self = static_cast<KX_Scene*>(self_v);

	MT_Vector3 vec;
	if (!PyVecTo(value, vec))
		return PY_SET_ATTR_FAIL;

	self->SetGravity(vec);
	return PY_SET_ATTR_SUCCESS;
}

PyAttributeDef KX_Scene::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("name",				KX_Scene, pyattr_get_name),
	KX_PYATTRIBUTE_RO_FUNCTION("objects",			KX_Scene, pyattr_get_objects),
	KX_PYATTRIBUTE_RO_FUNCTION("objectsInactive",	KX_Scene, pyattr_get_objects_inactive),
	KX_PYATTRIBUTE_RO_FUNCTION("lights",			KX_Scene, pyattr_get_lights),
	KX_PYATTRIBUTE_RO_FUNCTION("texts",				KX_Scene, pyattr_get_texts),
	KX_PYATTRIBUTE_RO_FUNCTION("cameras",			KX_Scene, pyattr_get_cameras),
	KX_PYATTRIBUTE_RO_FUNCTION("filterManager",		KX_Scene, pyattr_get_filter_manager),
	KX_PYATTRIBUTE_RO_FUNCTION("world",				KX_Scene, pyattr_get_world),
	KX_PYATTRIBUTE_RW_FUNCTION("active_camera",		KX_Scene, pyattr_get_active_camera, pyattr_set_active_camera),
	KX_PYATTRIBUTE_RW_FUNCTION("pre_draw",			KX_Scene, pyattr_get_drawing_callback_pre, pyattr_set_drawing_callback_pre),
	KX_PYATTRIBUTE_RW_FUNCTION("post_draw",			KX_Scene, pyattr_get_drawing_callback_post, pyattr_set_drawing_callback_post),
	KX_PYATTRIBUTE_RW_FUNCTION("pre_draw_setup",	KX_Scene, pyattr_get_drawing_setup_callback_pre, pyattr_set_drawing_setup_callback_pre),
	KX_PYATTRIBUTE_RW_FUNCTION("gravity",			KX_Scene, pyattr_get_gravity, pyattr_set_gravity),
	KX_PYATTRIBUTE_BOOL_RO("suspended",				KX_Scene, m_suspend),
	KX_PYATTRIBUTE_BOOL_RO("activity_culling",		KX_Scene, m_activity_culling),
	KX_PYATTRIBUTE_FLOAT_RW("activity_culling_radius", 0.5f, FLT_MAX, KX_Scene, m_activity_box_radius),
	KX_PYATTRIBUTE_BOOL_RO("dbvt_culling",			KX_Scene, m_dbvt_culling),
	{ NULL }	//Sentinel
};

KX_PYMETHODDEF_DOC(KX_Scene, addObject,
"addObject(object, other, time=0)\n"
"Returns the added object.\n")
{
	PyObject *pyob, *pyreference = Py_None;
	KX_GameObject *ob, *reference;

	float time = 0.0f;

	if (!PyArg_ParseTuple(args, "O|Of:addObject", &pyob, &pyreference, &time))
		return NULL;

	if (!ConvertPythonToGameObject(m_logicmgr, pyob, &ob, false, "scene.addObject(object, reference, time): KX_Scene (first argument)") ||
		!ConvertPythonToGameObject(m_logicmgr, pyreference, &reference, true, "scene.addObject(object, reference, time): KX_Scene (second argument)"))
		return NULL;

	if (!m_inactivelist->SearchValue(ob)) {
		PyErr_Format(PyExc_ValueError, "scene.addObject(object, reference, time): KX_Scene (first argument): object must be in an inactive layer");
		return NULL;
	}
	SCA_IObject *replica = AddReplicaObject((SCA_IObject*)ob, reference, time);
	
	// release here because AddReplicaObject AddRef's
	// the object is added to the scene so we don't want python to own a reference
	replica->Release();
	return replica->GetProxy();
}

KX_PYMETHODDEF_DOC(KX_Scene, end,
"end()\n"
"Removes this scene from the game.\n")
{
	
	KX_GetActiveEngine()->RemoveScene(m_sceneName);
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_Scene, restart,
				   "restart()\n"
				   "Restarts this scene.\n")
{
	KX_GetActiveEngine()->ReplaceScene(m_sceneName, m_sceneName);
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_Scene, replace,
				   "replace(newScene)\n"
                   "Replaces this scene with another one.\n"
                   "Return True if the new scene exists and scheduled for replacement, False otherwise.\n")
{
	char* name;
	
	if (!PyArg_ParseTuple(args, "s:replace", &name))
		return NULL;
	
    if (KX_GetActiveEngine()->ReplaceScene(m_sceneName, name))
        Py_RETURN_TRUE;
	
    Py_RETURN_FALSE;
}

KX_PYMETHODDEF_DOC(KX_Scene, suspend,
					"suspend()\n"
					"Suspends this scene.\n")
{
	Suspend();
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_Scene, resume,
					"resume()\n"
					"Resumes this scene.\n")
{
	Resume();
	
	Py_RETURN_NONE;
}

KX_PYMETHODDEF_DOC(KX_Scene, drawObstacleSimulation,
				   "drawObstacleSimulation()\n"
				   "Draw debug visualization of obstacle simulation.\n")
{
	if (GetObstacleSimulation())
		GetObstacleSimulation()->DrawObstacles();

	Py_RETURN_NONE;
}

/* Matches python dict.get(key, [default]) */
KX_PYMETHODDEF_DOC(KX_Scene, get, "")
{
	PyObject *key;
	PyObject *def = Py_None;
	PyObject *ret;

	if (!PyArg_ParseTuple(args, "O|O:get", &key, &def))
		return NULL;
	
	if (m_attr_dict && (ret=PyDict_GetItem(m_attr_dict, key))) {
		Py_INCREF(ret);
		return ret;
	}
	
	Py_INCREF(def);
	return def;
}

#endif // WITH_PYTHON
