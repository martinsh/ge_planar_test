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

/** \file gameengine/Rasterizer/RAS_ITexVert.cpp
 *  \ingroup bgerast
 */

#include "RAS_ITexVert.h"

RAS_TexVertInfo::RAS_TexVertInfo(unsigned int origindex, bool flat)
	:m_origindex(origindex),
	m_softBodyIndex(-1)
{
	m_flag = (flat) ? FLAT : 0;
}

RAS_TexVertInfo::~RAS_TexVertInfo()
{
}

RAS_ITexVert::RAS_ITexVert(const MT_Vector3& xyz,
						 const MT_Vector4& tangent,
						 const unsigned int rgba,
						 const MT_Vector3& normal)
{
	xyz.getValue(m_localxyz);
	SetRGBA(rgba);
	SetNormal(normal);
	SetTangent(tangent);
}

RAS_ITexVert::~RAS_ITexVert()
{
}
