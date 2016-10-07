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
* Contributor(s): Ulysse Martin, Tristan Porteries.
*
* ***** END GPL LICENSE BLOCK *****
*/

/** \file gameengine/Ketsji/KX_PlanarManager.cpp
*  \ingroup ketsji
*/

#include "KX_PlanarManager.h"
#include "KX_Camera.h"
#include "KX_Scene.h"
#include "KX_Globals.h"
#include "KX_Planar.h"

#include "EXP_ListValue.h"

#include "RAS_IRasterizer.h"
#include "RAS_Texture.h"
#include "RAS_ICanvas.h"

#include "DNA_texture_types.h"

#include "glew-mx.h"

KX_PlanarManager::KX_PlanarManager(KX_Scene *scene)
	:m_scene(scene)
{
	const RAS_CameraData& camdata = RAS_CameraData();
	m_camera = new KX_Camera(m_scene, KX_Scene::m_callbacks, camdata, true, true);
	m_camera->SetName("__planar_cam__");
}

KX_PlanarManager::~KX_PlanarManager()
{
	for (std::vector<KX_Planar *>::iterator it = m_planars.begin(), end = m_planars.end(); it != end; ++it) {
		delete *it;
	}

	m_camera->Release();
}

void KX_PlanarManager::AddPlanar(RAS_Texture *texture, KX_GameObject *gameobj, RAS_IPolyMaterial *polymat, int type, int width, int height)
{
	for (std::vector<KX_Planar *>::iterator it = m_planars.begin(), end = m_planars.end(); it != end; ++it) {
		KX_Planar *planar = *it;
		const std::vector<RAS_Texture *>& textures = planar->GetTextureUsers();
		for (std::vector<RAS_Texture *>::const_iterator it = textures.begin(), end = textures.end(); it != end; ++it) {
			if ((*it)->GetTex() == texture->GetTex()) {
				planar->AddTextureUser(texture);
				return;
			}
		}
	}

	Tex *tex = texture->GetTex();
	KX_Planar *kxplanar = new KX_Planar(tex, gameobj, polymat, type, width, height);
	kxplanar->AddTextureUser(texture);
	texture->SetPlanar(kxplanar);
	m_planars.push_back(kxplanar);
}

void KX_PlanarManager::RenderPlanar(RAS_IRasterizer *rasty, KX_Planar *planar)
{
	KX_GameObject *mirror = planar->GetMirrorObject();
	KX_GameObject *observer = m_scene->GetActiveCamera();



	//// Doesn't need (or can) update.
	//if (!planar->NeedUpdate() || !planar->GetEnabled() || !mirror) {
	//	return;
	//}

	//RAS_FrameFrustum frustum;

	// mirror mode, compute camera frustum, position and orientation
	// convert mirror position and normal in world space
	const MT_Matrix3x3 & mirrorObjWorldOri = mirror->GetSGNode()->GetWorldOrientation();
	const MT_Vector3 & mirrorObjWorldPos = mirror->GetSGNode()->GetWorldPosition();
	const MT_Vector3 & mirrorObjWorldScale = mirror->GetSGNode()->GetWorldScaling();
	MT_Vector3 mirrorWorldPos =
		mirrorObjWorldPos + mirrorObjWorldScale * (mirrorObjWorldOri * planar->GetMirrorPos());
	MT_Vector3 mirrorWorldZ = mirrorObjWorldOri * planar->GetMirrorZ();
	// get observer world position
	const MT_Vector3 & observerWorldPos = observer->GetSGNode()->GetWorldPosition();
	// get plane D term = mirrorPos . normal
	MT_Scalar mirrorPlaneDTerm = mirrorWorldPos.dot(mirrorWorldZ);
	// compute distance of observer to mirror = D - observerPos . normal
	MT_Scalar observerDistance = mirrorPlaneDTerm - observerWorldPos.dot(mirrorWorldZ);
	// if distance < 0.01 => observer is on wrong side of mirror, don't render
	if (observerDistance < 0.01) {
		std::cout << "don't render, object on the wrong side of the mirror" << std::endl;
		return;
	}

	// set camera orientation: z=normal, y=mirror_up in world space, x= y x z
	MT_Vector3 mirrorWorldY = mirrorObjWorldOri * planar->GetMirrorY();
	MT_Vector3 mirrorWorldX = mirrorObjWorldOri * planar->GetMirrorX();

	MT_Matrix3x3 m1 = mirror->NodeGetWorldOrientation();
	MT_Matrix3x3 m2 = m1;
	m2.invert();

	MT_Matrix3x3 r180;
	r180[0][0] = -1;
	r180[0][1] = 0;
	r180[0][2] = 0;
	r180[1][0] = 0;
	r180[1][1] = 1;
	r180[1][2] = 0;
	r180[2][0] = 0;
	r180[2][1] = 0;
	r180[2][2] = -1;

	MT_Matrix3x3 unmir;
	unmir[0][0] = -1;
	unmir[0][1] = 0;
	unmir[0][2] = 0;
	unmir[1][0] = 0;
	unmir[1][1] = 1;
	unmir[1][2] = 0;
	unmir[2][0] = 0;
	unmir[2][1] = 0;
	unmir[2][2] = 1;


	MT_Matrix3x3 ori = observer->NodeGetWorldOrientation();
	MT_Vector3 cameraWorldPos = observerWorldPos;
	
	int reflection = 0; // temporary solution, to be removed

	if (planar->GetPlanarType() & TEX_PLANAR_REFLECTION) {
	//if (reflection == 1) {
		cameraWorldPos = (observerWorldPos - mirror->GetSGNode()->GetWorldPosition())*m1;
		cameraWorldPos = mirror->GetSGNode()->GetWorldPosition() + cameraWorldPos*r180*unmir*m2;
		ori.transpose();
		ori = ori*m1*r180*unmir*m2;
		ori.transpose();
	}

	m_camera->GetSGNode()->SetLocalPosition(cameraWorldPos);
	m_camera->GetSGNode()->SetLocalOrientation(ori);

	m_camera->GetSGNode()->UpdateWorldData(0.0);
	

	// Store settings to be restored later
	const RAS_IRasterizer::StereoMode stereomode = rasty->GetStereoMode();
	RAS_Rect area = KX_GetActiveEngine()->GetCanvas()->GetWindowArea();

	// The screen area that ImageViewport will copy is also the rendering zone
	// bind the fbo and set the viewport to full size

	// initializing clipping planes for reflection and refraction
	float offset = 0.1; //geometry clipping offset
	double plane1[4] = { mirrorWorldZ[0], mirrorWorldZ[1], mirrorWorldZ[2], mirrorPlaneDTerm + offset };
	double plane2[4] = { mirrorWorldZ[0], mirrorWorldZ[1], mirrorWorldZ[2], -mirrorPlaneDTerm + offset };


	mirror->SetVisible(false, true);

	if (planar->GetPlanarType() & TEX_PLANAR_REFLECTION) {
	//if (reflection == 1) {
		glClipPlane(GL_CLIP_PLANE0, plane2);
		glEnable(GL_CLIP_PLANE0);
		glFrontFace(GL_CW);
	}
	else{
		glClipPlane(GL_CLIP_PLANE0, plane1);
		glEnable(GL_CLIP_PLANE0);
	}


	planar->BeginRender();
	planar->BindFace(rasty);

	rasty->BeginFrame(KX_GetActiveEngine()->GetClockTime());

	rasty->SetViewport(0, 0, planar->GetWidth(), planar->GetHeight()); /////////////////////////DO SOMETHING HERE
	rasty->SetScissor(0, 0, planar->GetWidth(), planar->GetHeight());

	rasty->Clear(RAS_IRasterizer::RAS_DEPTH_BUFFER_BIT);

	m_scene->GetWorldInfo()->UpdateWorldSettings(rasty);
	rasty->SetAuxilaryClientInfo(m_scene);
	rasty->DisplayFog();

	// frustum was computed above
	// get frustum matrix and set projection matrix
	//MT_Matrix4x4 projmat = rasty->GetFrustumMatrix(frustum.x1, frustum.x2, frustum.y1, frustum.y2, frustum.camnear, frustum.camfar);


	MT_Matrix4x4 projmat = m_scene->GetActiveCamera()->GetProjectionMatrix();
	//MT_Matrix4x4 viewmat = m_scene->GetActiveCamera()->GetModelviewMatrix();

	m_camera->SetProjectionMatrix(projmat);


	MT_Transform camtrans(m_camera->GetWorldToCamera());
	MT_Matrix4x4 viewmat(camtrans);

	rasty->SetViewMatrix(viewmat, m_camera->NodeGetWorldOrientation(), m_camera->NodeGetWorldPosition(), m_camera->NodeGetLocalScaling(), m_camera->GetCameraData()->m_perspective);
	m_camera->SetModelviewMatrix(viewmat);



	m_scene->CalculateVisibleMeshes(rasty, m_camera);

	KX_GetActiveEngine()->UpdateAnimations(m_scene);

	// Now the objects are culled and we can render the scene.
	m_scene->GetWorldInfo()->RenderBackground(rasty);

	m_scene->RenderBuckets(camtrans, rasty);

	// restore the canvas area now that the render is completed
	KX_GetActiveEngine()->GetCanvas()->GetWindowArea() = area;
	KX_GetActiveEngine()->GetCanvas()->EndFrame();

	planar->EndRender();

	//glCullFace(GL_BACK);
	
	mirror->SetVisible(true, true);
	glDisable(GL_CLIP_PLANE0);
	if (planar->GetPlanarType() & TEX_PLANAR_REFLECTION) {
	//if (reflection == 1) {
		glFrontFace(GL_CCW);
		
	}

	
}

void KX_PlanarManager::Render(RAS_IRasterizer *rasty)
{
	if (m_planars.size() == 0 || rasty->GetDrawingMode() != RAS_IRasterizer::RAS_TEXTURED) {
		return;
	}

	// Disable scissor to not bother with scissor box.
	rasty->Disable(RAS_IRasterizer::RAS_SCISSOR_TEST);

	// Copy current stereo mode.
	const RAS_IRasterizer::StereoMode steremode = rasty->GetStereoMode();
	// Disable stereo for realtime planar.
	rasty->SetStereoMode(RAS_IRasterizer::RAS_STEREO_NOSTEREO);

	for (std::vector<KX_Planar *>::iterator it = m_planars.begin(), end = m_planars.end(); it != end; ++it) {
		RenderPlanar(rasty, *it);
	}

	// Restore previous stereo mode.
	rasty->SetStereoMode(steremode);

	rasty->Enable(RAS_IRasterizer::RAS_SCISSOR_TEST);
}