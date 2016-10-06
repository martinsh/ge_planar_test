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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/RAS_Shader.cpp
 *  \ingroup bgerast
 */

#include <iostream>
#include <cstring>
#include "RAS_Shader.h"
#include "RAS_IRasterizer.h"

#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

#include "GPU_shader.h"

#define spit(x) std::cout << x << std::endl;

#define UNIFORM_MAX_LEN (int)sizeof(float) * 16

RAS_Shader::RAS_Uniform::RAS_Uniform(int data_size)
	:m_loc(-1),
	m_count(1),
	m_dirty(true),
	m_type(UNI_NONE),
	m_transpose(0),
	m_dataLen(data_size)
{
#ifdef SORT_UNIFORMS
	BLI_assert((int)m_dataLen <= UNIFORM_MAX_LEN);
	m_data = (void *)MEM_mallocN(m_dataLen, "shader-uniform-alloc");
#endif
}

RAS_Shader::RAS_Uniform::~RAS_Uniform()
{
#ifdef SORT_UNIFORMS
	if (m_data) {
		MEM_freeN(m_data);
		m_data = NULL;
	}
#endif
}

void RAS_Shader::RAS_Uniform::Apply(RAS_Shader *shader)
{
#ifdef SORT_UNIFORMS
	BLI_assert(m_type > UNI_NONE && m_type < UNI_MAX && m_data);

	if (!m_dirty) {
		return;
	}

	GPUShader *gpushader = shader->GetGPUShader();
	switch (m_type) {
		case UNI_FLOAT:
		{
			float *f = (float *)m_data;
			GPU_shader_uniform_vector(gpushader, m_loc, 1, m_count, (float *)f);
			break;
		}
		case UNI_INT:
		{
			int *f = (int *)m_data;
			GPU_shader_uniform_vector_int(gpushader, m_loc, 1, m_count, (int *)f);
			break;
		}
		case UNI_FLOAT2:
		{
			float *f = (float *)m_data;
			GPU_shader_uniform_vector(gpushader, m_loc, 2, m_count, (float *)f);
			break;
		}
		case UNI_FLOAT3:
		{
			float *f = (float *)m_data;
			GPU_shader_uniform_vector(gpushader, m_loc, 3, m_count, (float *)f);
			break;
		}
		case UNI_FLOAT4:
		{
			float *f = (float *)m_data;
			GPU_shader_uniform_vector(gpushader, m_loc, 4, m_count, (float *)f);
			break;
		}
		case UNI_INT2:
		{
			int *f = (int *)m_data;
			GPU_shader_uniform_vector_int(gpushader, m_loc, 2, m_count, (int *)f);
			break;
		}
		case UNI_INT3:
		{
			int *f = (int *)m_data;
			GPU_shader_uniform_vector_int(gpushader, m_loc, 3, m_count, (int *)f);
			break;
		}
		case UNI_INT4:
		{
			int *f = (int *)m_data;
			GPU_shader_uniform_vector_int(gpushader, m_loc, 4, m_count, (int *)f);
			break;
		}
		case UNI_MAT4:
		{
			float *f = (float *)m_data;
			GPU_shader_uniform_vector(gpushader, m_loc, 16, m_count, (float *)f);
			break;
		}
		case UNI_MAT3:
		{
			float *f = (float *)m_data;
			GPU_shader_uniform_vector(gpushader, m_loc, 9, m_count, (float *)f);
			break;
		}
	}
	m_dirty = false;
#endif
}

void RAS_Shader::RAS_Uniform::SetData(int location, int type, unsigned int count, bool transpose)
{
#ifdef SORT_UNIFORMS
	m_type = type;
	m_loc = location;
	m_count = count;
	m_dirty = true;
#endif
}

int RAS_Shader::RAS_Uniform::GetLocation()
{
	return m_loc;
}

void *RAS_Shader::RAS_Uniform::GetData()
{
	return m_data;
}

bool RAS_Shader::Ok() const
{
	return (m_shader && m_use);
}

RAS_Shader::RAS_Shader()
	:m_shader(NULL),
	m_use(0),
	m_attr(0),
	m_error(0),
	m_dirty(true)
{
	for (unsigned short i = 0; i < MAX_PROGRAM; ++i) {
		m_progs[i] = "";
	}
}

RAS_Shader::~RAS_Shader()
{
	ClearUniforms();

	DeleteShader();
}

void RAS_Shader::ClearUniforms()
{
	RAS_UniformVec::iterator it = m_uniforms.begin();
	while (it != m_uniforms.end()) {
		delete *it;
		it++;
	}
	m_uniforms.clear();

	RAS_UniformVecDef::iterator itp = m_preDef.begin();
	while (itp != m_preDef.end()) {
		delete *itp;
		itp++;
	}
	m_preDef.clear();
}

RAS_Shader::RAS_Uniform *RAS_Shader::FindUniform(const int location)
{
#ifdef SORT_UNIFORMS
	RAS_UniformVec::iterator it = m_uniforms.begin();
	while (it != m_uniforms.end()) {
		if ((*it)->GetLocation() == location) {
			return *it;
		}
		it++;
	}
#endif
	return NULL;
}

void RAS_Shader::SetUniformfv(int location, int type, float *param, int size, unsigned int count, bool transpose)
{
#ifdef SORT_UNIFORMS
	RAS_Uniform *uni = FindUniform(location);

	if (uni) {
		memcpy(uni->GetData(), param, size);
		uni->SetData(location, type, count, transpose);
	}
	else {
		uni = new RAS_Uniform(size);
		memcpy(uni->GetData(), param, size);
		uni->SetData(location, type, count, transpose);
		m_uniforms.push_back(uni);
	}

	m_dirty = true;
#endif
}

void RAS_Shader::SetUniformiv(int location, int type, int *param, int size, unsigned int count, bool transpose)
{
#ifdef SORT_UNIFORMS
	RAS_Uniform *uni = FindUniform(location);

	if (uni) {
		memcpy(uni->GetData(), param, size);
		uni->SetData(location, type, count, transpose);
	}
	else {
		uni = new RAS_Uniform(size);
		memcpy(uni->GetData(), param, size);
		uni->SetData(location, type, count, transpose);
		m_uniforms.push_back(uni);
	}

	m_dirty = true;
#endif
}

void RAS_Shader::ApplyShader()
{
#ifdef SORT_UNIFORMS
	if (!m_dirty) {
		return;
	}

	for (unsigned int i = 0; i < m_uniforms.size(); i++) {
		m_uniforms[i]->Apply(this);
	}

	m_dirty = false;
#endif
}

void RAS_Shader::UnloadShader()
{
	//
}

void RAS_Shader::DeleteShader()
{
	if (m_shader) {
		GPU_shader_free(m_shader);
		m_shader = NULL;
	}
}

bool RAS_Shader::LinkProgram()
{
	const char *vert;
	const char *frag;
	const char *geom;

	if (m_error) {
		goto program_error;
	}

	if (m_progs[VERTEX_PROGRAM].IsEmpty() || m_progs[FRAGMENT_PROGRAM].IsEmpty()) {
		spit("Invalid GLSL sources");
		return false;
	}

	vert = m_progs[VERTEX_PROGRAM].ReadPtr();
	frag = m_progs[FRAGMENT_PROGRAM].ReadPtr();
	geom = (m_progs[GEOMETRY_PROGRAM] == "") ? NULL : m_progs[GEOMETRY_PROGRAM].ReadPtr();
	m_shader = GPU_shader_create_ex(vert, frag, geom, NULL, NULL, 0, 0, 0, GPU_SHADER_FLAGS_SPECIAL_RESET_LINE);
	if (!m_shader) {
		goto program_error;
	}

	m_error = 0;
	return true;

	program_error:
	{
		m_use = 0;
		m_error = 1;
		return false;
	}
}

void RAS_Shader::ValidateProgram()
{
	char *log = GPU_shader_validate(m_shader);
	if (log) {
		spit("---- GLSL Validation ----");
		spit(log);
		MEM_freeN(log);
	}
}

bool RAS_Shader::GetError()
{
	return m_error;
}

unsigned int RAS_Shader::GetProg()
{
	return GPU_shader_program(m_shader);
}

GPUShader *RAS_Shader::GetGPUShader()
{
	return m_shader;
}

void RAS_Shader::SetSampler(int loc, int unit)
{
	GPU_shader_uniform_int(m_shader, loc, unit);
}

void RAS_Shader::SetProg(bool enable)
{
	if (m_shader && enable) {
		GPU_shader_bind(m_shader);
	}
	else {
		GPU_shader_unbind();
	}
}

void RAS_Shader::SetEnabled(bool enabled)
{
	m_use = enabled;
}

bool RAS_Shader::GetEnabled() const
{
	return m_use;
}

void RAS_Shader::Update(RAS_IRasterizer *rasty, const MT_Matrix4x4 model)
{
	if (!Ok() || !m_preDef.size()) {
		return;
	}

	const MT_Matrix4x4 &view = rasty->GetViewMatrix();

	RAS_UniformVecDef::iterator it;
	for (it = m_preDef.begin(); it != m_preDef.end(); it++) {
		RAS_DefUniform *uni = (*it);

		if (uni->m_loc == -1) {
			continue;
		}

		switch (uni->m_type) {
			case MODELMATRIX:
			{
				SetUniform(uni->m_loc, model);
				break;
			}
			case MODELMATRIX_TRANSPOSE:
			{
				SetUniform(uni->m_loc, model, true);
				break;
			}
			case MODELMATRIX_INVERSE:
			{
				SetUniform(uni->m_loc, model.inverse());
				break;
			}
			case MODELMATRIX_INVERSETRANSPOSE:
			{
				SetUniform(uni->m_loc, model.inverse(), true);
				break;
			}
			case MODELVIEWMATRIX:
			{
				SetUniform(uni->m_loc, view * model);
				break;
			}
			case MODELVIEWMATRIX_TRANSPOSE:
			{
				MT_Matrix4x4 mat(view * model);
				SetUniform(uni->m_loc, mat, true);
				break;
			}
			case MODELVIEWMATRIX_INVERSE:
			{
				MT_Matrix4x4 mat(view * model);
				SetUniform(uni->m_loc, mat.inverse());
				break;
			}
			case MODELVIEWMATRIX_INVERSETRANSPOSE:
			{
				MT_Matrix4x4 mat(view * model);
				SetUniform(uni->m_loc, mat.inverse(), true);
				break;
			}
			case CAM_POS:
			{
				MT_Vector3 pos(rasty->GetCameraPosition());
				SetUniform(uni->m_loc, pos);
				break;
			}
			case VIEWMATRIX:
			{
				SetUniform(uni->m_loc, view);
				break;
			}
			case VIEWMATRIX_TRANSPOSE:
			{
				SetUniform(uni->m_loc, view, true);
				break;
			}
			case VIEWMATRIX_INVERSE:
			{
				SetUniform(uni->m_loc, view.inverse());
				break;
			}
			case VIEWMATRIX_INVERSETRANSPOSE:
			{
				SetUniform(uni->m_loc, view.inverse(), true);
				break;
			}
			case CONSTANT_TIMER:
			{
				SetUniform(uni->m_loc, (float)rasty->GetTime());
				break;
			}
			case EYE:
			{
				SetUniform(uni->m_loc, (rasty->GetEye() == RAS_IRasterizer::RAS_STEREO_LEFTEYE) ? 0.0f : 0.5f);
			}
			default:
				break;
		}
	}
}

int RAS_Shader::GetAttribute()
{
	return m_attr;
}

int RAS_Shader::GetAttribLocation(const char *name)
{
	return GPU_shader_get_attribute(m_shader, name);
}

void RAS_Shader::BindAttribute(const char *attr, int loc)
{
	GPU_shader_bind_attribute(m_shader, loc, attr);
}

int RAS_Shader::GetUniformLocation(const char *name, bool debug)
{
	BLI_assert(m_shader != NULL);
	int location = GPU_shader_get_uniform(m_shader, name);

	if (location == -1 && debug) {
		spit("Invalid uniform value: " << name << ".");
	}

	return location;
}

void RAS_Shader::SetUniform(int uniform, const MT_Vector2 &vec)
{
	float value[2];
	vec.getValue(value);
	GPU_shader_uniform_vector(m_shader, uniform, 2, 1, value);
}

void RAS_Shader::SetUniform(int uniform, const MT_Vector3 &vec)
{
	float value[3];
	vec.getValue(value);
	GPU_shader_uniform_vector(m_shader, uniform, 3, 1, value);
}

void RAS_Shader::SetUniform(int uniform, const MT_Vector4 &vec)
{
	float value[4];
	vec.getValue(value);
	GPU_shader_uniform_vector(m_shader, uniform, 4, 1, value);
}

void RAS_Shader::SetUniform(int uniform, const unsigned int &val)
{
	GPU_shader_uniform_int(m_shader, uniform, val);
}

void RAS_Shader::SetUniform(int uniform, const int val)
{
	GPU_shader_uniform_int(m_shader, uniform, val);
}

void RAS_Shader::SetUniform(int uniform, const float &val)
{
	GPU_shader_uniform_float(m_shader, uniform, val);
}

void RAS_Shader::SetUniform(int uniform, const MT_Matrix4x4 &vec, bool transpose)
{
	float value[16];
	// note: getValue gives back column major as needed by OpenGL
	vec.getValue(value);
	GPU_shader_uniform_vector(m_shader, uniform, 16, 1, value);
}

void RAS_Shader::SetUniform(int uniform, const MT_Matrix3x3 &vec, bool transpose)
{
	float value[9];
	value[0] = (float)vec[0][0];
	value[1] = (float)vec[1][0];
	value[2] = (float)vec[2][0];
	value[3] = (float)vec[0][1];
	value[4] = (float)vec[1][1];
	value[5] = (float)vec[2][1];
	value[6] = (float)vec[0][2];
	value[7] = (float)vec[1][2];
	value[8] = (float)vec[2][2];
	GPU_shader_uniform_vector(m_shader, uniform, 9, 1, value);
}

void RAS_Shader::SetUniform(int uniform, const float *val, int len)
{
	if (len >= 2 && len <= 4) {
		GPU_shader_uniform_vector(m_shader, uniform, len, 1, (float *)val);
	}
	else {
		BLI_assert(0);
	}
}

void RAS_Shader::SetUniform(int uniform, const int *val, int len)
{
	if (len >= 2 && len <= 4) {
		GPU_shader_uniform_vector_int(m_shader, uniform, len, 1, (int *)val);
	}
	else {
		BLI_assert(0);
	}
}
