#pragma once
#include "Simul/Camera/BaseOpticsRenderer.h"
#include "Simul/Camera/LensFlare.h"
#include "Simul/Sky/Float4.h"
#include <GL/glew.h>
#include "Simul/Platform/OpenGL/Export.h"
#include <vector>



class SimulOpticsRendererGL: simul::camera::BaseOpticsRenderer
{
public:
	SimulOpticsRendererGL();
	virtual ~SimulOpticsRendererGL();
	virtual void RestoreDeviceObjects(void *device);
	virtual void InvalidateDeviceObjects();
	virtual void RenderFlare(float exposure,const float *dir,const float *light);
	virtual void ReloadShaders();

	GLuint			flare_program;
	GLint			flareColour_param;
	GLuint			flare_texture;
	GLint			flareTexture_param;

	std::vector<GLuint> halo_textures;
};
