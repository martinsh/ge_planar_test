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

/** \file RAS_Polygon.h
 *  \ingroup bgerast
 */

#ifndef __RAS_POLYGON_H__
#define __RAS_POLYGON_H__

class RAS_IDisplayArray;
class RAS_MaterialBucket;
class RAS_ITexVert;
class RAS_TexVertInfo;

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class RAS_Polygon
{
	// location
	RAS_MaterialBucket *m_bucket;
	RAS_IDisplayArray *m_darray;
	unsigned int m_offset[4];
	unsigned short m_numvert;
	unsigned short m_polyflags;

public:
	enum {
		VISIBLE = 1,
		COLLIDER = 2,
		TWOSIDE = 4
	};

	RAS_Polygon(RAS_MaterialBucket *bucket, RAS_IDisplayArray *darray, int numvert);
	virtual ~RAS_Polygon()
	{
	}

	int VertexCount();
	RAS_ITexVert *GetVertex(int i);
	RAS_TexVertInfo& GetVertexInfo(unsigned int i);

	void SetVertexOffset(int i, unsigned int offset);
	unsigned int GetVertexOffset(unsigned int i);

	bool IsVisible();
	void SetVisible(bool visible);

	bool IsCollider();
	void SetCollider(bool collider);

	bool IsTwoside();
	void SetTwoside(bool twoside);

	RAS_MaterialBucket *GetMaterial();
	RAS_IDisplayArray *GetDisplayArray();


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_Polygon")
#endif
};

#endif

