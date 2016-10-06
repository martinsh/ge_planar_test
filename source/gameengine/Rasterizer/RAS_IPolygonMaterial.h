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

/** \file RAS_IPolygonMaterial.h
 *  \ingroup bgerast
 */

#ifndef __RAS_IPOLYGONMATERIAL_H__
#define __RAS_IPOLYGONMATERIAL_H__

#include "RAS_Texture.h"
#include "RAS_IRasterizer.h"

#include "STR_String.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

#include "MT_Vector4.h"

#include <map>

struct MTexPoly;
struct Material;
struct Image;
struct Scene;
class SCA_IScene;
struct GameSettings;

enum MaterialProps
{
	RAS_MULTILIGHT = (1 << 1),
	RAS_BLENDERGLSL = (1 << 3),
	RAS_CASTSHADOW = (1 << 4),
	RAS_ONLYSHADOW = (1 << 5),
	RAS_OBJECTCOLOR = (1 << 6),
	RAS_DISPLAYLISTS = (1 << 7)
};

enum MaterialRasterizerModes
{
	RAS_COLLIDER = 2,
	RAS_ZSORT = 4,
	RAS_ALPHA = 8,
	RAS_DEPTH_ALPHA = 16,
	RAS_WIRE = 64,
	RAS_TEXT = 128,
	RAS_TWOSIDED = 512,
};

/**
 * Polygon Material on which the material buckets are sorted
 */
class RAS_IPolyMaterial
{
protected:
	STR_String m_name; // also needed for collisionsensor
	int m_drawingmode;
	int m_alphablend;
	int m_rasMode;
	unsigned int m_flag;

	RAS_Texture *m_textures[RAS_Texture::MaxUnits];

	/// The storage type used to render with this material.
	RAS_IRasterizer::StorageType m_storageType;

public:

	// care! these are taken from blender polygonflags, see file DNA_mesh_types.h for #define TF_BILLBOARD etc.
	enum MaterialFlags
	{
		BILLBOARD_SCREENALIGNED = 512, // GEMAT_HALO
		BILLBOARD_AXISALIGNED = 1024, // GEMAT_BILLBOARD
		SHADOW = 2048 // GEMAT_SHADOW
	};

	RAS_IPolyMaterial(const STR_String& name,
	                  GameSettings *game);

	virtual ~RAS_IPolyMaterial();

	virtual void Activate(RAS_IRasterizer *rasty) = 0;
	virtual void Desactivate(RAS_IRasterizer *rasty) = 0;
	virtual void ActivateInstancing(RAS_IRasterizer *rasty, void *matrixoffset, void *positionoffset, void *coloroffset, unsigned int stride) = 0;
	virtual void DesactivateInstancing() = 0;
	virtual void ActivateMeshSlot(RAS_MeshSlot *ms, RAS_IRasterizer *rasty) = 0;

	bool IsAlpha() const;
	bool IsAlphaDepth() const;
	bool IsZSort() const;
	bool IsWire() const;
	bool IsText() const;
	int GetDrawingMode() const;
	virtual STR_String& GetName();
	unsigned int GetFlag() const;
	bool IsAlphaShadow() const;
	bool UsesObjectColor() const;
	bool CastsShadows() const;
	bool OnlyShadow() const;
	RAS_Texture *GetTexture(unsigned int index);
	bool UseDisplayLists() const;
	RAS_IRasterizer::StorageType GetStorageType() const;

	virtual const STR_String& GetTextureName() const = 0;
	virtual Material *GetBlenderMaterial() const = 0;
	virtual Image *GetBlenderImage() const = 0;
	virtual MTexPoly *GetMTexPoly() const = 0;
	virtual Scene *GetBlenderScene() const = 0;
	virtual bool UseInstancing() const = 0;
	virtual void ReleaseMaterial() = 0;
	virtual void GetRGBAColor(unsigned char *rgba) const;
	virtual bool UsesLighting(RAS_IRasterizer *rasty) const;

	virtual void UpdateIPO(MT_Vector4 rgba, MT_Vector3 specrgb, MT_Scalar hard, MT_Scalar spec, MT_Scalar ref,
						   MT_Scalar emit, MT_Scalar ambient, MT_Scalar alpha, MT_Scalar specalpha) = 0;

	virtual const RAS_IRasterizer::AttribLayerList GetAttribLayers(const STR_String uvsname[RAS_Texture::MaxUnits]) const = 0;

	/// Overridden by KX_BlenderMaterial
	virtual void Replace_IScene(SCA_IScene *val) = 0;

	/**
	 * \return the equivalent drawing mode for the material settings (equivalent to old TexFace tface->mode).
	 */
	int ConvertFaceMode(struct GameSettings *game) const;

	/*
	 * PreCalculate texture gen
	 */
	virtual void OnConstruction() = 0;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_IPolyMaterial")
#endif
};

#endif  /* __RAS_IPOLYGONMATERIAL_H__ */
