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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file LA_System.h
 *  \ingroup player
 */

#ifndef __LA_SYSTEM_H__
#define __LA_SYSTEM_H__

#include "KX_ISystem.h"

class LA_System : public KX_ISystem
{
private:
	double m_starttime;

public:
	LA_System();
	virtual ~LA_System();

	virtual double GetTimeInSeconds();
};

#endif  // __LA_SYSTEM_H__
