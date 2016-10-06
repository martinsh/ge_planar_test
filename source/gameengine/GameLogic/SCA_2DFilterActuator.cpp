/*
 * SCA_2DFilterActuator.cpp
 *
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/GameLogic/SCA_2DFilterActuator.cpp
 *  \ingroup gamelogic
 */


#include <stddef.h>

#include "SCA_IActuator.h"
#include "SCA_2DFilterActuator.h"
#include "RAS_2DFilterData.h"
#include "RAS_2DFilterManager.h"
#include "RAS_2DFilter.h"

#include <iostream>

SCA_2DFilterActuator::SCA_2DFilterActuator(
        SCA_IObject *gameobj,
        int type,
        short flag,
        float float_arg,
        int int_arg,
        RAS_IRasterizer* rasterizer,
		RAS_2DFilterManager *filterManager,
        SCA_IScene* scene)
    : SCA_IActuator(gameobj, KX_ACT_2DFILTER),
      m_type(type),
      m_disableMotionBlur(flag),
      m_float_arg(float_arg),
      m_int_arg(int_arg),
      m_rasterizer(rasterizer),
      m_filterManager(filterManager),
      m_scene(scene)
{
	m_propNames = m_gameobj->GetPropertyNames();
}

SCA_2DFilterActuator::~SCA_2DFilterActuator()
{
}

CValue* SCA_2DFilterActuator::GetReplica()
{
	SCA_2DFilterActuator* replica = new SCA_2DFilterActuator(*this);
	replica->ProcessReplica();
	return replica;
}


bool SCA_2DFilterActuator::Update()
{
	bool bNegativeEvent = IsNegativeEvent();
	RemoveAllEvents();


	if (bNegativeEvent)
		return false; // do nothing on negative events

	RAS_2DFilter *filter = m_filterManager->GetFilterPass(m_int_arg);
	switch (m_type) {
		case RAS_2DFilterManager::FILTER_ENABLED:
		{
			if (filter) {
				filter->SetEnabled(true);
			}
			break;
		}
		case RAS_2DFilterManager::FILTER_DISABLED:
		{
			if (filter) {
				filter->SetEnabled(false);
			}
			break;
		}
		case RAS_2DFilterManager::FILTER_NOFILTER:
		{
			m_filterManager->RemoveFilterPass(m_int_arg);
			break;
		}
		case RAS_2DFilterManager::FILTER_MOTIONBLUR:
		{
			if (m_disableMotionBlur) {
				m_rasterizer->DisableMotionBlur();
			}
			else {
				m_rasterizer->EnableMotionBlur(m_float_arg);
			}
			break;
		}
		default:
		{
			if (!filter) {
				RAS_2DFilterData info;
				info.filterPassIndex = m_int_arg;
				info.gameObject = m_gameobj;
				info.filterMode = m_type;
				info.propertyNames = m_propNames;
				info.shaderText = m_shaderText;

				m_filterManager->AddFilter(info);
			}
			else {
				std::cout << "Object: " << m_gameobj->GetName() << ", actuator: " << m_name <<
					". Warning: 2D Filter for pass index: " <<  m_int_arg << 
					" already exists, do nothing." << std::endl;
			}
			break;
		}
	}

	// once the filter is in place, no need to update it again => disable the actuator
	return false;
}


void SCA_2DFilterActuator::SetScene(SCA_IScene *scene)
{
	m_scene = scene;
}

void SCA_2DFilterActuator::SetShaderText(const char *text)
{
	m_shaderText = text;
}

#ifdef WITH_PYTHON

/* ------------------------------------------------------------------------- */
/* Python functions                                                          */
/* ------------------------------------------------------------------------- */

/* Integration hooks ------------------------------------------------------- */
PyTypeObject SCA_2DFilterActuator::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"SCA_2DFilterActuator",
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
	&SCA_IActuator::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyMethodDef SCA_2DFilterActuator::Methods[] = {
	/* add python functions to deal with m_msg... */
	{NULL,NULL}
};

PyAttributeDef SCA_2DFilterActuator::Attributes[] = {
	KX_PYATTRIBUTE_STRING_RW("shaderText", 0, 64000, false, SCA_2DFilterActuator, m_shaderText),
	KX_PYATTRIBUTE_SHORT_RW("disableMotionBlur", 0, 1, true, SCA_2DFilterActuator, m_disableMotionBlur),
	KX_PYATTRIBUTE_ENUM_RW("mode", RAS_2DFilterManager::FILTER_ENABLED, RAS_2DFilterManager::FILTER_NUMBER_OF_FILTERS, false, SCA_2DFilterActuator, m_type),
KX_PYATTRIBUTE_INT_RW("passNumber", 0, 100, true, SCA_2DFilterActuator, m_int_arg),
	KX_PYATTRIBUTE_FLOAT_RW("value", 0.0, 100.0, SCA_2DFilterActuator, m_float_arg),
	{ NULL }	//Sentinel
};

#endif
