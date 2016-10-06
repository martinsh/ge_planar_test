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

/** \file gameengine/Ketsji/KX_Globals.cpp
 *  \ingroup ketsji
 */

#include "KX_Globals.h"
#include "KX_KetsjiEngine.h"
#include "RAS_IRasterizer.h"

extern "C" {
#  include "BLI_blenlib.h"
}

static KX_KetsjiEngine *g_engine = NULL;
static KX_Scene *g_scene = NULL;
static STR_String g_mainPath = "";
static STR_String g_origPath = "";

void KX_SetActiveEngine(KX_KetsjiEngine *engine)
{
	g_engine = engine;
}

void KX_SetActiveScene(KX_Scene *scene)
{
	g_scene = scene;
}

void KX_SetMainPath(const STR_String& path)
{
	char cpath[FILE_MAX];
	BLI_strncpy(cpath, path.ReadPtr(), sizeof(cpath));
	BLI_cleanup_file(NULL, cpath);
	g_mainPath = STR_String(cpath);
}

void KX_SetOrigPath(const STR_String& path)
{
	char cpath[FILE_MAX];
	BLI_strncpy(cpath, path.ReadPtr(), sizeof(cpath));
	BLI_cleanup_file(NULL, cpath);
	g_origPath = STR_String(cpath);
}

KX_KetsjiEngine *KX_GetActiveEngine()
{
	return g_engine;
}

KX_Scene *KX_GetActiveScene()
{
	return g_scene;
}

const STR_String& KX_GetMainPath()
{
	return g_mainPath;
}

const STR_String& KX_GetOrigPath()
{
	return g_origPath;
}

void KX_RasterizerDrawDebugLine(const MT_Vector3& from,const MT_Vector3& to,const MT_Vector4& color)
{
	g_engine->GetRasterizer()->DrawDebugLine(g_scene, from, to, color);
}

void KX_RasterizerDrawDebugCircle(const MT_Vector3& center, const MT_Scalar radius, const MT_Vector4& color,
                                  const MT_Vector3& normal, int nsector)
{
	g_engine->GetRasterizer()->DrawDebugCircle(g_scene, center, radius, color, normal, nsector);
}
