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

/** \file gameengine/Launcher/LA_PlayerLauncher.cpp
 *  \ingroup launcher
 */


#ifdef WIN32
#  pragma warning (disable:4786) // suppress stl-MSVC debug info warning
#  include <windows.h>
#endif

#include "GPU_init_exit.h"

#include "LA_PlayerLauncher.h"
#include "LA_SystemCommandLine.h"

extern "C" {
#  include "BKE_sound.h"

#  include "BLI_fileops.h"
}

#include "KX_PythonInit.h"

#include "GPG_Canvas.h" 

#include "GHOST_ISystem.h"

#include "DEV_InputDevice.h"

LA_PlayerLauncher::LA_PlayerLauncher(GHOST_ISystem *system, GHOST_IWindow *window, Main *maggie, Scene *scene, GlobalSettings *gs,
								 RAS_IRasterizer::StereoMode stereoMode, int samples, int argc, char **argv, char *pythonMainLoop)
	:LA_Launcher(system, maggie, scene, gs, stereoMode, samples, argc, argv),
	m_mainWindow(window),
	m_pythonMainLoop(pythonMainLoop)
{
}

LA_PlayerLauncher::~LA_PlayerLauncher()
{
}

bool LA_PlayerLauncher::GetMainLoopPythonCode(char **pythonCode, char **pythonFileName)
{
#ifndef WITH_GAMEENGINE_SECURITY
	if (m_pythonMainLoop) {
		if (BLI_is_file(m_pythonMainLoop)) {
			size_t filesize = 0;
			*pythonCode = (char *)BLI_file_read_text_as_mem(m_pythonMainLoop, 1, &filesize);
			(*pythonCode)[filesize] = '\0';
			*pythonFileName = m_pythonMainLoop;
			return true;
		}
		else {
			std::cout << "Error: cannot yield control to Python: no file named '" << m_pythonMainLoop << "'" << std::endl;
			return false;
		}
	}
#endif
	return LA_Launcher::GetMainLoopPythonCode(pythonCode, pythonFileName);
}

RAS_IRasterizer::DrawType LA_PlayerLauncher::GetRasterizerDrawMode()
{
	const SYS_SystemHandle& syshandle = SYS_GetSystem();
	const bool wireframe = SYS_GetCommandLineInt(syshandle, "wireframe", 0);

	if (wireframe) {
		return RAS_IRasterizer::RAS_WIREFRAME;
	}

	return RAS_IRasterizer::RAS_TEXTURED;
}

bool LA_PlayerLauncher::GetUseAlwaysExpandFraming()
{
	return false;
}

void LA_PlayerLauncher::InitCamera()
{
}

void LA_PlayerLauncher::InitPython()
{
}

void LA_PlayerLauncher::ExitPython()
{
#ifdef WITH_PYTHON
	exitGamePlayerPythonScripting();
#endif  // WITH_PYTHON
}

void LA_PlayerLauncher::SetWindowOrder(short order)
{
	m_mainWindow->setOrder((order == 0) ? GHOST_kWindowOrderBottom : GHOST_kWindowOrderTop);
}

void LA_PlayerLauncher::InitEngine()
{
	GPU_init();
	BKE_sound_init(m_maggie);
	LA_Launcher::InitEngine();

	m_rasterizer->PrintHardwareInfo();
}

void LA_PlayerLauncher::ExitEngine()
{
	LA_Launcher::ExitEngine();
	BKE_sound_exit();
	GPU_exit();
}

bool LA_PlayerLauncher::EngineNextFrame()
{
	if (m_inputDevice->GetInput(SCA_IInputDevice::WINRESIZE).Find(SCA_InputEvent::ACTIVE)) {
		GHOST_Rect bnds;
		m_mainWindow->getClientBounds(bnds);
		m_canvas->Resize(bnds.getWidth(), bnds.getHeight());
		m_ketsjiEngine->Resize();
		m_inputDevice->ConvertEvent(SCA_IInputDevice::WINRESIZE, 0, 0);
	}

	return LA_Launcher::EngineNextFrame();
}

RAS_ICanvas *LA_PlayerLauncher::CreateCanvas(RAS_IRasterizer *rasty)
{
	return (new GPG_Canvas(rasty, m_mainWindow));
}
