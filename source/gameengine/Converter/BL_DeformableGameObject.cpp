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

/** \file gameengine/Converter/BL_DeformableGameObject.cpp
 *  \ingroup bgeconv
 */


#include "BL_DeformableGameObject.h"
#include "BL_ShapeDeformer.h"
#include "RAS_MeshUser.h"
#include "RAS_MaterialBucket.h"


BL_DeformableGameObject::~BL_DeformableGameObject()
{
	if (m_pDeformer)
		delete m_pDeformer;		//	__NLA : Temporary until we decide where to put this
}

void BL_DeformableGameObject::ProcessReplica()
{
	KX_GameObject::ProcessReplica();

	if (m_pDeformer)
		m_pDeformer= (BL_MeshDeformer*)m_pDeformer->GetReplica();
}

CValue*		BL_DeformableGameObject::GetReplica()
{

	BL_DeformableGameObject* replica = new BL_DeformableGameObject(*this);//m_float,GetName());
	replica->ProcessReplica();
	return replica;
}

bool BL_DeformableGameObject::SetActiveAction(short priority, double curtime)
{
	if (curtime != m_lastframe) {
		m_activePriority = 9999;
		m_lastframe= curtime;
	}

	if (priority<=m_activePriority)
	{
		m_activePriority = priority;
		m_lastframe = curtime;
	
		return true;
	}
	else {
		return false;
	}
}

bool BL_DeformableGameObject::GetShape(vector<float> &shape)
{
	shape.clear();
	BL_ShapeDeformer* shape_deformer = dynamic_cast<BL_ShapeDeformer*>(m_pDeformer);
	if (shape_deformer)
	{
		// this check is normally superfluous: a shape deformer can only be created if the mesh
		// has relative keys
		Key* key = shape_deformer->GetKey();
		if (key && key->type==KEY_RELATIVE) 
		{
			KeyBlock *kb;
			for (kb = (KeyBlock *)key->block.first; kb; kb = (KeyBlock *)kb->next)
			{
				shape.push_back(kb->curval);
			}
		}
	}
	return !shape.empty();
}

void BL_DeformableGameObject::SetDeformer(class RAS_Deformer* deformer)
{
	m_pDeformer = deformer;

	if (m_meshUser) {
		RAS_MeshSlotList& meshSlots = m_meshUser->GetMeshSlots();
		for (RAS_MeshSlotList::iterator it = meshSlots.begin(), end = meshSlots.end(); it != end; ++it) {
			(*it)->SetDeformer(deformer);
		}
	}
}

