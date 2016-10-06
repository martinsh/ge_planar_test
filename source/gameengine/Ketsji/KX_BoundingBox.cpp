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


#include "KX_BoundingBox.h"
#include "KX_GameObject.h"
#include "KX_PyMath.h"

#ifdef WITH_PYTHON

KX_BoundingBox::KX_BoundingBox(KX_GameObject *owner)
	:m_owner(owner),
	m_proxy(owner->GetProxy())
{
}

KX_BoundingBox::~KX_BoundingBox()
{
}

bool KX_BoundingBox::IsValidOwner()
{
	if (!BGE_PROXY_REF(m_proxy)) {
		PyErr_SetString(PyExc_SystemError, "KX_BoundingBox, " BGE_PROXY_ERROR_MSG);
		return false;
	}
	return true;
}

const MT_Vector3& KX_BoundingBox::GetMax() const
{
	// Update AABB to make sure we have the last one.
	m_owner->UpdateBounds(false);
	SG_BBox &box = m_owner->GetSGNode()->BBox();
	return box.GetMax();
}

const MT_Vector3& KX_BoundingBox::GetMin() const
{
	// Update AABB to make sure we have the last one.
	m_owner->UpdateBounds(false);
	SG_BBox &box = m_owner->GetSGNode()->BBox();
	return box.GetMin();
}

const MT_Vector3 KX_BoundingBox::GetCenter() const
{
	// Update AABB to make sure we have the last one.
	m_owner->UpdateBounds(false);
	SG_BBox &box = m_owner->GetSGNode()->BBox();
	return box.GetCenter();
}

float KX_BoundingBox::GetRadius() const
{
	// Update AABB to make sure we have the last one.
	m_owner->UpdateBounds(false);
	SG_BBox &box = m_owner->GetSGNode()->BBox();
	return box.GetRadius();
}

bool KX_BoundingBox::SetMax(MT_Vector3 max)
{
	const MT_Vector3& min = GetMin();

	if (min.x() > max.x() || min.y() > max.y() || min.z() > max.z()) {
		return false;
	}

	m_owner->SetBoundsAabb(min, max);
	return true;
}

bool KX_BoundingBox::SetMin(MT_Vector3 min)
{
	const MT_Vector3& max = GetMax();

	if (min.x() > max.x() || min.y() > max.y() || min.z() > max.z()) {
		return false;
	}

	m_owner->SetBoundsAabb(min, max);
	return true;
}


#ifdef USE_MATHUTILS

#define MATHUTILS_VEC_CB_BOX_MIN 1
#define MATHUTILS_VEC_CB_BOX_MAX 2

static unsigned char mathutils_kxboundingbox_vector_cb_index = -1; /* index for our callbacks */

static int mathutils_kxboundingbox_generic_check(BaseMathObject *bmo)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>BGE_PROXY_REF(bmo->cb_user);
	if (!self)
		return -1;

	return 0;
}

static int mathutils_kxboundingbox_vector_get(BaseMathObject *bmo, int subtype)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>BGE_PROXY_REF(bmo->cb_user);
	if (!self)
		return -1;

	switch (subtype) {
		case MATHUTILS_VEC_CB_BOX_MIN:
		{
			self->GetMin().getValue(bmo->data);
			break;
		}
		case MATHUTILS_VEC_CB_BOX_MAX:
		{
			self->GetMax().getValue(bmo->data);
			break;
		}
	}

	return 0;
}

static int mathutils_kxboundingbox_vector_set(BaseMathObject *bmo, int subtype)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>BGE_PROXY_REF(bmo->cb_user);
	if (!self)
		return -1;

	switch (subtype) {
		case MATHUTILS_VEC_CB_BOX_MIN:
		{
			if (!self->SetMin(MT_Vector3(bmo->data))) {
				PyErr_SetString(PyExc_AttributeError, "bounds.min = Vector: KX_BoundingBox, min bigger than max");
				return -1;
			}
			break;
		}
		case MATHUTILS_VEC_CB_BOX_MAX:
		{
			if (!self->SetMax(MT_Vector3(bmo->data))) {
				PyErr_SetString(PyExc_AttributeError, "bounds.max = Vector: KX_BoundingBox, max smaller than min");
				return -1;
			}
			break;
		}
	}

	return 0;
}

static int mathutils_kxboundingbox_vector_get_index(BaseMathObject *bmo, int subtype, int index)
{
	/* lazy, avoid repeteing the case statement */
	if (mathutils_kxboundingbox_vector_get(bmo, subtype) == -1)
		return -1;
	return 0;
}

static int mathutils_kxboundingbox_vector_set_index(BaseMathObject *bmo, int subtype, int index)
{
	float f = bmo->data[index];

	/* lazy, avoid repeateing the case statement */
	if (mathutils_kxboundingbox_vector_get(bmo, subtype) == -1)
		return -1;

	bmo->data[index] = f;
	return mathutils_kxboundingbox_vector_set(bmo, subtype);
}

static Mathutils_Callback mathutils_kxboundingbox_vector_cb = {
	mathutils_kxboundingbox_generic_check,
	mathutils_kxboundingbox_vector_get,
	mathutils_kxboundingbox_vector_set,
	mathutils_kxboundingbox_vector_get_index,
	mathutils_kxboundingbox_vector_set_index
};

void KX_BoundingBox_Mathutils_Callback_Init()
{
	// register mathutils callbacks, ok to run more than once.
	mathutils_kxboundingbox_vector_cb_index = Mathutils_RegisterCallback(&mathutils_kxboundingbox_vector_cb);
}

#endif  // USE_MATHUTILS

PyTypeObject KX_BoundingBox::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_BoundingBox",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0, 0, 0, 0, 0, 0, 0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0, 0, 0, 0, 0, 0,
	py_base_new
};

PyMethodDef KX_BoundingBox::Methods[] = {
	{NULL, NULL} // Sentinel
};

PyAttributeDef KX_BoundingBox::Attributes[] = {
	KX_PYATTRIBUTE_RW_FUNCTION("min", KX_BoundingBox, pyattr_get_min, pyattr_set_min),
	KX_PYATTRIBUTE_RW_FUNCTION("max", KX_BoundingBox, pyattr_get_max, pyattr_set_max),
	KX_PYATTRIBUTE_RO_FUNCTION("center", KX_BoundingBox, pyattr_get_center),
	KX_PYATTRIBUTE_RO_FUNCTION("radius", KX_BoundingBox, pyattr_get_radius),
	KX_PYATTRIBUTE_RW_FUNCTION("autoUpdate", KX_BoundingBox, pyattr_get_auto_update, pyattr_set_auto_update),
	{NULL} // Sentinel
};

PyObject *KX_BoundingBox::py_repr()
{
	if (!IsValidOwner()) {
		return PyUnicode_FromString("KX_BoundingBox of invalid object");
	}
	return PyUnicode_FromFormat("KX_BoundingBox of object %s, min: %R, max: %R", m_owner->GetName().ReadPtr(),
								PyObjectFrom(GetMin()), PyObjectFrom(GetMax()));
}

PyObject *KX_BoundingBox::pyattr_get_min(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>(self_v);
	if (!self->IsValidOwner()) {
		return NULL;
	}

#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxboundingbox_vector_cb_index, MATHUTILS_VEC_CB_BOX_MIN);
#else
	return PyObjectFrom(self->GetMin());
#endif  // USE_MATHUTILS
}

int KX_BoundingBox::pyattr_set_min(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>(self_v);
	if (!self->IsValidOwner()) {
		return PY_SET_ATTR_FAIL;
	}

	MT_Vector3 min;
	if (!PyVecTo(value, min)) {
		return PY_SET_ATTR_FAIL;
	}
	if (!self->SetMin(min)) {
		PyErr_SetString(PyExc_AttributeError, "bounds.min = Vector: KX_BoundingBox, min bigger than max");
		return PY_SET_ATTR_FAIL;
	}

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_BoundingBox::pyattr_get_max(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>(self_v);
	if (!self->IsValidOwner()) {
		return NULL;
	}

#ifdef USE_MATHUTILS
	return Vector_CreatePyObject_cb(
	        BGE_PROXY_FROM_REF_BORROW(self_v), 3,
	        mathutils_kxboundingbox_vector_cb_index, MATHUTILS_VEC_CB_BOX_MAX);
#else
	return PyObjectFrom(self->GetMax());
#endif  // USE_MATHUTILS
}

int KX_BoundingBox::pyattr_set_max(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>(self_v);
	if (!self->IsValidOwner()) {
		return PY_SET_ATTR_FAIL;
	}

	MT_Vector3 max;
	if (!PyVecTo(value, max)) {
		return PY_SET_ATTR_FAIL;
	}
	if (!self->SetMax(max)) {
		PyErr_SetString(PyExc_AttributeError, "bounds.max = Vector: KX_BoundingBox, max smaller than min");
		return PY_SET_ATTR_FAIL;
	}

	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_BoundingBox::pyattr_get_center(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>(self_v);
	if (!self->IsValidOwner()) {
		return NULL;
	}

	return PyObjectFrom(self->GetCenter());
}

PyObject *KX_BoundingBox::pyattr_get_radius(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>(self_v);
	if (!self->IsValidOwner()) {
		return NULL;
	}

	return PyFloat_FromDouble(self->GetRadius());
}

PyObject *KX_BoundingBox::pyattr_get_auto_update(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>(self_v);
	if (!self->IsValidOwner()) {
		return NULL;
	}

	return PyBool_FromLong(self->m_owner->GetAutoUpdateBounds());
}

int KX_BoundingBox::pyattr_set_auto_update(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BoundingBox *self = static_cast<KX_BoundingBox *>(self_v);
	if (!self->IsValidOwner()) {
		return PY_SET_ATTR_FAIL;
	}

	int autoUpdate = PyObject_IsTrue(value);
	if (autoUpdate == -1) {
		PyErr_SetString(PyExc_AttributeError, "bounds.autoUpdate = bool: KX_BoundingBox, expected True or False");
		return PY_SET_ATTR_FAIL;
	}

	self->m_owner->SetAutoUpdateBounds((bool)autoUpdate);

	return PY_SET_ATTR_SUCCESS;
}

#endif  // WITH_PYTHON
