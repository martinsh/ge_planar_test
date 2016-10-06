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

/** \file RAS_DisplayArrayBucket.h
 *  \ingroup bgerast
 */

#ifndef __RAS_DISPLAY_MATERIAL_BUCKET_H__
#define __RAS_DISPLAY_MATERIAL_BUCKET_H__

#include "RAS_MeshSlot.h" // needed for RAS_MeshSlotList
#include "RAS_IRasterizer.h" // needed for RAS_IRasterizer::StorageType

#include "MT_Transform.h"

#include <vector>

class RAS_MaterialBucket;
class RAS_IDisplayArray;
class RAS_MeshObject;
class RAS_MeshMaterial;
class RAS_Deformer;
class RAS_IStorageInfo;
class RAS_InstancingBuffer;

typedef std::vector<RAS_Deformer *> RAS_DeformerList;

class RAS_DisplayArrayBucket
{
private:
	unsigned int m_vbo;
	/// The number of mesh slot using it.
	unsigned int m_refcount;
	/// The parent bucket.
	RAS_MaterialBucket *m_bucket;
	/// The display array = list of vertexes and indexes.
	RAS_IDisplayArray *m_displayArray;
	/// The parent mesh object, it can be NULL for text objects.
	RAS_MeshObject *m_mesh;
	/// The material mesh.
	RAS_MeshMaterial *m_meshMaterial;
	/// The list fo all visible mesh slots to render this frame.
	RAS_MeshSlotList m_activeMeshSlots;
	/// The list of all deformer usign this display array.
	RAS_DeformerList m_deformerList;

	/// True if the display array is not frequently modified and can use display list.
	bool m_useDisplayList;
	/// As m_useDisplayList but without rasterizer value.
	bool m_useVao;

	/// True if a deformer is dynamic or the mesh i modified this frame.
	bool m_meshModified;

	/** Info created by the storage and freed by this class.
	 * So it's an unique instance by display array bucket.
	 * It contains all infos about special redering e.g
	 * VBO and IBO ID for VBO storage, DL for VA storage.
	 */
	RAS_IStorageInfo *m_storageInfo;

	RAS_InstancingBuffer *m_instancingBuffer;

public:
	RAS_DisplayArrayBucket(RAS_MaterialBucket *bucket, RAS_IDisplayArray *array, RAS_MeshObject *mesh, RAS_MeshMaterial *meshmat);
	~RAS_DisplayArrayBucket();

	/// \section Reference Count Management.
	RAS_DisplayArrayBucket *AddRef();
	RAS_DisplayArrayBucket *Release();
	unsigned int GetRefCount() const;

	/// \section Replication
	RAS_DisplayArrayBucket *GetReplica();
	void ProcessReplica();

	/// \section Accesor
	RAS_IDisplayArray *GetDisplayArray() const;
	RAS_MaterialBucket *GetMaterialBucket() const;
	RAS_MeshObject *GetMesh() const;
	RAS_MeshMaterial *GetMeshMaterial() const;

	/// \section Active Mesh Slots Management.
	void ActivateMesh(RAS_MeshSlot *slot);
	RAS_MeshSlotList& GetActiveMeshSlots();
	unsigned int GetNumActiveMeshSlots() const;
	/// Remove all mesh slots from the list.
	void RemoveActiveMeshSlots();

	/// \section Deformer
	/// Add a deformer user.
	void AddDeformer(RAS_Deformer *deformer);
	/// Remove the given deformer.
	void RemoveDeformer(RAS_Deformer *deformer);

	/// \section Render Infos
	bool UseDisplayList() const;
	bool UseVao() const;
	bool IsMeshModified() const;

	/// Update render infos.
	void UpdateActiveMeshSlots(RAS_IRasterizer *rasty);
	/// Set the mesh object as unmodified flag.
	void SetMeshUnmodified();

	RAS_IStorageInfo *GetStorageInfo() const;
	void SetStorageInfo(RAS_IStorageInfo *info);
	void DestructStorageInfo();
	RAS_IRasterizer::StorageType GetStorageType() const;

	void SetAttribLayers(RAS_IRasterizer *rasty) const;

	/// Render all mesh slots for solid render.
	void RenderMeshSlots(const MT_Transform& cameratrans, RAS_IRasterizer *rasty);
	/// Render all mesh slots with geometry instancing render.
	void RenderMeshSlotsInstancing(const MT_Transform& cameratrans, RAS_IRasterizer *rasty, bool alpha);
};

typedef std::vector<RAS_DisplayArrayBucket *> RAS_DisplayArrayBucketList;

#endif  // __RAS_DISPLAY_MATERIAL_BUCKET_H__
