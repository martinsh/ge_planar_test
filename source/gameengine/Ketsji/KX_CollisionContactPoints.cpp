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
 * Contributor(s): Tristan Porteries.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_CollisionContactPoints.cpp
 *  \ingroup ketsji
 */

#include "KX_CollisionContactPoints.h"
#include "PHY_DynamicTypes.h"
#include "KX_PyMath.h"

KX_CollisionContactPoint::KX_CollisionContactPoint(const PHY_CollData *collData, unsigned int index, bool firstObject)
	:m_collData(collData),
	m_index(index),
	m_firstObject(firstObject)
{
}

KX_CollisionContactPoint::~KX_CollisionContactPoint()
{
}

static STR_String sCollisionContactPointName = "CollisionContactPoint";

STR_String& KX_CollisionContactPoint::GetName()
{
	return sCollisionContactPointName;
}

#ifdef WITH_PYTHON

PyTypeObject KX_CollisionContactPoint::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_CollisionContactPoint",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&CValue::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef KX_CollisionContactPoint::Methods[] = {
	{NULL, NULL} //Sentinel
};

PyAttributeDef KX_CollisionContactPoint::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("localPointA", KX_CollisionContactPoint, pyattr_get_local_point_a),
	KX_PYATTRIBUTE_RO_FUNCTION("localPointB", KX_CollisionContactPoint, pyattr_get_local_point_b),
	KX_PYATTRIBUTE_RO_FUNCTION("worldPoint", KX_CollisionContactPoint, pyattr_get_world_point),
	KX_PYATTRIBUTE_RO_FUNCTION("normal", KX_CollisionContactPoint, pyattr_get_normal),
	KX_PYATTRIBUTE_RO_FUNCTION("combinedFriction", KX_CollisionContactPoint, pyattr_get_combined_friction),
	KX_PYATTRIBUTE_RO_FUNCTION("combinedRestitution", KX_CollisionContactPoint, pyattr_get_combined_restitution),
	KX_PYATTRIBUTE_RO_FUNCTION("appliedImpulse", KX_CollisionContactPoint, pyattr_get_applied_impulse),
	{NULL} //Sentinel
};

PyObject *KX_CollisionContactPoint::pyattr_get_local_point_a(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CollisionContactPoint *self = static_cast<KX_CollisionContactPoint *>(self_v);
	return PyObjectFrom(self->m_collData->GetLocalPointA(self->m_index, self->m_firstObject));
}

PyObject *KX_CollisionContactPoint::pyattr_get_local_point_b(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CollisionContactPoint *self = static_cast<KX_CollisionContactPoint *>(self_v);
	return PyObjectFrom(self->m_collData->GetLocalPointB(self->m_index, self->m_firstObject));
}

PyObject *KX_CollisionContactPoint::pyattr_get_world_point(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CollisionContactPoint *self = static_cast<KX_CollisionContactPoint *>(self_v);
	return PyObjectFrom(self->m_collData->GetWorldPoint(self->m_index, self->m_firstObject));
}

PyObject *KX_CollisionContactPoint::pyattr_get_normal(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CollisionContactPoint *self = static_cast<KX_CollisionContactPoint *>(self_v);
	return PyObjectFrom(self->m_collData->GetNormal(self->m_index, self->m_firstObject));
}

PyObject *KX_CollisionContactPoint::pyattr_get_combined_friction(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CollisionContactPoint *self = static_cast<KX_CollisionContactPoint *>(self_v);
	return PyFloat_FromDouble(self->m_collData->GetCombinedFriction(self->m_index, self->m_firstObject));
}

PyObject *KX_CollisionContactPoint::pyattr_get_combined_restitution(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CollisionContactPoint *self = static_cast<KX_CollisionContactPoint *>(self_v);
	return PyFloat_FromDouble(self->m_collData->GetCombinedRestitution(self->m_index, self->m_firstObject));
}

PyObject *KX_CollisionContactPoint::pyattr_get_applied_impulse(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_CollisionContactPoint *self = static_cast<KX_CollisionContactPoint *>(self_v);
	return PyFloat_FromDouble(self->m_collData->GetAppliedImpulse(self->m_index, self->m_firstObject));
}

#endif  // WITH_PYTHON

KX_CollisionContactPointList::KX_CollisionContactPointList(const PHY_CollData *collData, bool firstObject)
	:m_collData(collData),
	m_firstObject(firstObject)
{
}

KX_CollisionContactPointList::~KX_CollisionContactPointList()
{
}

#ifdef WITH_PYTHON

static int kx_collision_contact_point_list_get_sensors_size_cb(void *self_v)
{
	return ((KX_CollisionContactPointList *)self_v)->GetNumCollisionContactPoint();
}

static PyObject *kx_collision_contact_point_list_get_sensors_item_cb(void *self_v, int index)
{
	return ((KX_CollisionContactPointList *)self_v)->GetCollisionContactPoint(index)->NewProxy(true);
}

CListWrapper *KX_CollisionContactPointList::GetListWrapper()
{
	return (new CListWrapper(this,
							 NULL, // No base python proxy.
							 NULL,
							 kx_collision_contact_point_list_get_sensors_size_cb,
							 kx_collision_contact_point_list_get_sensors_item_cb,
							 NULL,
							 NULL));
}

#endif  // WITH_PYTHON

KX_CollisionContactPoint *KX_CollisionContactPointList::GetCollisionContactPoint(unsigned int index)
{
	// All contact point infos.
	return (new KX_CollisionContactPoint(m_collData, index, m_firstObject));
}

unsigned int KX_CollisionContactPointList::GetNumCollisionContactPoint()
{
	return m_collData->GetNumContacts();
}

const PHY_CollData *KX_CollisionContactPointList::GetCollData()
{
	return m_collData;
}

bool KX_CollisionContactPointList::GetFirstObject()
{
	return m_firstObject;
}
