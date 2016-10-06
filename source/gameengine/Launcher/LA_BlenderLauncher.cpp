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

/** \file gameengine/Launcher/LA_BlenderLauncher.cpp
 *  \ingroup launcher
 */

#include "LA_BlenderLauncher.h"

#include "KX_BlenderCanvas.h"

#include "KX_PythonInit.h"

extern "C" {
#  include "BKE_context.h"

// avoid c++ conflict with 'new'
#  define new _new
#  include "BKE_screen.h"
#  undef new

#  include "DNA_scene_types.h"
#  include "DNA_screen_types.h"
#  include "DNA_object_types.h"
#  include "DNA_view3d_types.h"

#  include "WM_types.h"
#  include "WM_api.h"
#  include "wm_event_system.h"
#  include "wm_window.h"

#  include "BLI_rect.h"
}

LA_BlenderLauncher::LA_BlenderLauncher(GHOST_ISystem *system, Main *maggie, Scene *scene, GlobalSettings *gs, RAS_IRasterizer::StereoMode stereoMode, 
									   int argc, char **argv, bContext *context, rcti *camframe, ARegion *ar, int alwaysUseExpandFraming)
	:LA_Launcher(system, maggie, scene, gs, stereoMode, scene->gm.aasamples, argc, argv),
	m_context(context),
	m_ar(ar),
	m_camFrame(camframe),
	m_alwaysUseExpandFraming(alwaysUseExpandFraming),
	m_drawLetterBox(false)
{
	m_windowManager = CTX_wm_manager(m_context);
	m_window = CTX_wm_window(m_context);
	m_view3d = CTX_wm_view3d(m_context);

	m_areaRect.SetLeft(m_camFrame->xmin);
	m_areaRect.SetBottom(m_camFrame->ymin);
	m_areaRect.SetRight(m_camFrame->xmax);
	m_areaRect.SetTop(m_camFrame->ymax);
}

LA_BlenderLauncher::~LA_BlenderLauncher()
{
}

RAS_ICanvas *LA_BlenderLauncher::CreateCanvas(RAS_IRasterizer *rasty)
{
	return (new KX_BlenderCanvas(rasty, m_windowManager, m_window, m_areaRect, m_ar));
}

RAS_IRasterizer::DrawType LA_BlenderLauncher::GetRasterizerDrawMode()
{
	View3D *v3d = CTX_wm_view3d(m_context);

	RAS_IRasterizer::DrawType drawmode = RAS_IRasterizer::RAS_TEXTURED;
	switch(v3d->drawtype) {
		case OB_BOUNDBOX:
		case OB_WIRE:
		{
			drawmode = RAS_IRasterizer::RAS_WIREFRAME;
			break;
		}
		case OB_SOLID:
		{
			drawmode = RAS_IRasterizer::RAS_SOLID;
			break;
		}
		case OB_MATERIAL:
		{
			drawmode = RAS_IRasterizer::RAS_TEXTURED;
			break;
		}
	}
	return drawmode;
}

bool LA_BlenderLauncher::GetUseAlwaysExpandFraming()
{
	return m_alwaysUseExpandFraming;
}

void LA_BlenderLauncher::InitCamera()
{
	RegionView3D *rv3d = CTX_wm_region_view3d(m_context);

	// Some blender stuff.
	float camzoom = 1.0f;

	if (rv3d->persp == RV3D_CAMOB) {
		if (m_startScene->gm.framing.type == SCE_GAMEFRAMING_BARS) { /* Letterbox */
			m_drawLetterBox = true;
		}
		else {
			camzoom = 1.0f / BKE_screen_view3d_zoom_to_fac(rv3d->camzoom);
		}
	}

	m_ketsjiEngine->SetCameraZoom(camzoom);
	m_ketsjiEngine->SetCameraOverrideZoom(2.0f);

	if (rv3d->persp != RV3D_CAMOB) {
		m_ketsjiEngine->EnableCameraOverride(m_startSceneName);
		m_ketsjiEngine->SetCameraOverrideUseOrtho((rv3d->persp == RV3D_ORTHO));
		m_ketsjiEngine->SetCameraOverrideProjectionMatrix(MT_CmMatrix4x4(rv3d->winmat));
		m_ketsjiEngine->SetCameraOverrideViewMatrix(MT_CmMatrix4x4(rv3d->viewmat));
		m_ketsjiEngine->SetCameraOverrideClipping(m_view3d->near, m_view3d->far);
		m_ketsjiEngine->SetCameraOverrideLens(m_view3d->lens);
	}
}


void LA_BlenderLauncher::InitPython()
{
#ifdef WITH_PYTHON

#endif  // WITH_PYTHON
}
void LA_BlenderLauncher::ExitPython()
{
#ifdef WITH_PYTHON

	exitGamePythonScripting();

#endif  // WITH_PYTHON
}

void LA_BlenderLauncher::SetWindowOrder(short order)
{
	wm_window_set_order(m_window, order);
}

void LA_BlenderLauncher::InitEngine()
{
	// Lock frame and camera enabled - storing global values.
	m_savedBlenderData.sceneLayer = m_startScene->lay;
	m_savedBlenderData.camera = m_startScene->camera;

	if (m_view3d->scenelock == 0) {
		m_startScene->lay = m_view3d->lay;
		m_startScene->camera = m_view3d->camera;
	}

	LA_Launcher::InitEngine();
}

void LA_BlenderLauncher::ExitEngine()
{
	LA_Launcher::ExitEngine();

	// Lock frame and camera enabled - restoring global values.
	if (m_view3d->scenelock == 0) {
		m_startScene->lay = m_savedBlenderData.sceneLayer;
		m_startScene->camera= m_savedBlenderData.camera;
	}

	// Free all window manager events unused.
	wm_event_free_all(m_window);
}

void LA_BlenderLauncher::RenderEngine()
{
	if (m_drawLetterBox) {
		// Clear screen to border color
		// We do this here since we set the canvas to be within the frames. This means the engine
		// itself is unaware of the extra space, so we clear the whole region for it.
		m_rasterizer->SetClearColor(m_startScene->gm.framing.col[0], m_startScene->gm.framing.col[1], m_startScene->gm.framing.col[2]);
		m_rasterizer->SetViewport(m_ar->winrct.xmin, m_ar->winrct.ymin,
		           BLI_rcti_size_x(&m_ar->winrct) + 1, BLI_rcti_size_y(&m_ar->winrct) + 1);
		m_rasterizer->SetScissor(m_ar->winrct.xmin, m_ar->winrct.ymin,
		           BLI_rcti_size_x(&m_ar->winrct) + 1, BLI_rcti_size_y(&m_ar->winrct) + 1);
		m_rasterizer->Clear(RAS_IRasterizer::RAS_COLOR_BUFFER_BIT);
	}
	LA_Launcher::RenderEngine();
}

bool LA_BlenderLauncher::EngineNextFrame()
{
	// Free all window manager events unused.
	wm_event_free_all(m_window);

	return LA_Launcher::EngineNextFrame();
}
