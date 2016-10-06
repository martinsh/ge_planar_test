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

/** \file gameengine/Ketsji/KX_TextMaterial.cpp
 *  \ingroup ketsji
 */

#include "KX_TextMaterial.h"

#include "DNA_material_types.h"

static KX_TextMaterial textMaterial = KX_TextMaterial();

KX_TextMaterial *GetTextMaterial()
{
	return &textMaterial;
}

KX_TextMaterial::KX_TextMaterial()
	:RAS_IPolyMaterial("__TextMaterial__", NULL)
{
	m_rasMode |= (RAS_ALPHA | RAS_TEXT);
	m_flag |= RAS_BLENDERGLSL;
	m_alphablend = GEMAT_ALPHA;
}

KX_TextMaterial::~KX_TextMaterial()
{
}

void KX_TextMaterial::Activate(RAS_IRasterizer *rasty)
{
}

void KX_TextMaterial::Desactivate(RAS_IRasterizer *rasty)
{
}

void KX_TextMaterial::ActivateInstancing(RAS_IRasterizer *rasty, void *matrixoffset, void *positionoffset, void *coloroffset, unsigned int stride)
{
}

void KX_TextMaterial::DesactivateInstancing()
{
}

void KX_TextMaterial::ActivateMeshSlot(RAS_MeshSlot *ms, RAS_IRasterizer *rasty)
{
}

const STR_String& KX_TextMaterial::GetTextureName() const
{
	static STR_String empty = "";
	return empty;
}

Material *KX_TextMaterial::GetBlenderMaterial() const
{
	return NULL;
}

Image *KX_TextMaterial::GetBlenderImage() const
{
	return NULL;
}

MTexPoly *KX_TextMaterial::GetMTexPoly() const
{
	return NULL;
}

Scene *KX_TextMaterial::GetBlenderScene() const
{
	return NULL;
}

bool KX_TextMaterial::UseInstancing() const
{
	return false;
}

void KX_TextMaterial::ReleaseMaterial()
{
}

void KX_TextMaterial::UpdateIPO(MT_Vector4 rgba, MT_Vector3 specrgb, MT_Scalar hard, MT_Scalar spec, MT_Scalar ref,
			   MT_Scalar emit, MT_Scalar ambient, MT_Scalar alpha, MT_Scalar specalpha)
{
}

const RAS_IRasterizer::AttribLayerList KX_TextMaterial::GetAttribLayers(const STR_String uvsname[RAS_Texture::MaxUnits]) const
{
	static const RAS_IRasterizer::AttribLayerList uvLayers;
	return uvLayers;
}

void KX_TextMaterial::Replace_IScene(SCA_IScene *val)
{
}

void KX_TextMaterial::OnConstruction()
{
}
