#define NOMINMAX
#include "Simul/Base/RuntimeError.h"
#include "Simul/Platform/DirectX11/RenderPlatform.h"
#include "Simul/Platform/DirectX11/Material.h"
#include "Simul/Platform/DirectX11/Mesh.h"
#include "Simul/Platform/DirectX11/Texture.h"
#include "Simul/Platform/DirectX11/ESRAMTexture.h"
#include "Simul/Platform/DirectX11/FramebufferDX1x.h"
#include "Simul/Platform/DirectX11/Light.h"
#include "Simul/Platform/DirectX11/Effect.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Buffer.h"
#include "Simul/Platform/DirectX11/Layout.h"
#include "Simul/Platform/DirectX11/MacrosDX1x.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/CompileShaderDX1x.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Camera/Camera.h"
#include "Simul/Math/Matrix4x4.h"
#include "Simul/Camera/Camera.h"
#include "D3dx11effect.h"
#ifdef _XBOX_ONE
#include "Simul/Platform/DirectX11/ESRAMManager.h"
#include <D3Dcompiler_x.h>
#else
#include <D3Dcompiler.h>
#endif

using namespace simul;
using namespace dx11;

struct Vertex3_t
{
	float x,y,z;
};

static const float size=5.f;
static Vertex3_t box_vertices[36] =
{
	{-size,		-size,	size},
	{size,		size,	size},
	{size,		-size,	size},
	{size,		size,	size},
	{-size,		-size,	size},
	{-size,		size,	size},
	
	{-size,		-size,	-size},
	{ size,		-size,	-size},
	{ size,		 size,	-size},
	{ size,		 size,	-size},
	{-size,		 size,	-size},
	{-size,		-size,	-size},
	
	{-size,		size,	-size},
	{ size,		size,	-size},
	{ size,		size,	 size},
	{ size,		size,	 size},
	{-size,		size,	 size},
	{-size,		size,	-size},
				
	{-size,		-size,  -size},
	{ size,		-size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
	{-size,		-size,  -size},
	{-size,		-size,	 size},
	
	{ size,		-size,	-size},
	{ size,		 size,	 size},
	{ size,		 size,	-size},
	{ size,		 size,	 size},
	{ size,		-size,	-size},
	{ size,		-size,	 size},
				
	{-size,		-size,	-size},
	{-size,		 size,	-size},
	{-size,		 size,	 size},
	{-size,		 size,	 size},
	{-size,		-size,	 size},
	{-size,		-size,	-size},
};

RenderPlatform::StoredState::StoredState()
	:
				m_StencilRefStored11(0)
				,m_SampleMaskStored11(0)
				,m_indexOffset(0)
				, m_indexFormatStored11(DXGI_FORMAT_UNKNOWN)
				, m_previousTopology(D3D_PRIMITIVE_TOPOLOGY_UNDEFINED)
				,m_pDepthStencilStateStored11(NULL)
				,m_pRasterizerStateStored11(NULL)
				,m_pBlendStateStored11(NULL)
				,pIndexBufferStored11(NULL)
				,m_previousInputLayout(NULL)
{
	for(int i=0;i<4;i++)
		m_BlendFactorStored11[i];
	for(int i=0;i<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;i++)
	{
		m_pSamplerStateStored11[i]=NULL;
		m_pVertexSamplerStateStored11[i]=NULL;
	}
	for(int i=0;i<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;i++)
	{
		m_pVertexBuffersStored11[i]=NULL;
		m_VertexStrides[i]=0;
		m_VertexOffsets[i]=0;
	}
}

RenderPlatform::RenderPlatform()
	:device(NULL)
	,m_pDebugEffect(NULL)
	,m_pCubemapVtxDecl(NULL)
	,m_pVertexBuffer(NULL)
	,solidEffect(NULL)
	,reverseDepth(false)
{
}

RenderPlatform::~RenderPlatform()
{
	InvalidateDeviceObjects();
}

#ifdef _XBOX_ONE
ESRAMManager *eSRAMManager=NULL;
#endif
void RenderPlatform::RestoreDeviceObjects(void *d)
{
	device=(ID3D11Device*)d;
	crossplatform::RenderPlatform::RestoreDeviceObjects(d);
	
	ID3D11DeviceContext *pImmediateContext;
	AsD3D11Device()->GetImmediateContext(&pImmediateContext);
	immediateContext.platform_context=pImmediateContext;
#ifdef _XBOX_ONE
	delete eSRAMManager;
	eSRAMManager=new ESRAMManager(device);
#endif
	solidConstants.RestoreDeviceObjects(this);

	RecompileShaders();
	SAFE_RELEASE(m_pVertexBuffer);
	// Vertex declaration
	{
		D3DX11_PASS_DESC PassDesc;
		crossplatform::EffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("vec3_input_signature");
		tech->asD3DX11EffectTechnique()->GetPassByIndex(0)->GetDesc(&PassDesc);
		D3D11_INPUT_ELEMENT_DESC decl[]=
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,	0,	0,	D3D1x_INPUT_PER_VERTEX_DATA, 0 }
		};
		SAFE_RELEASE(m_pCubemapVtxDecl);
		V_CHECK(AsD3D11Device()->CreateInputLayout(decl,1,PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_pCubemapVtxDecl));
	}
	D3D11_BUFFER_DESC desc=
	{
        36*sizeof(vec3),
        D3D1x_USAGE_DEFAULT,
        D3D1x_BIND_VERTEX_BUFFER,
        0,
        0
	};
	
    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D1x_SUBRESOURCE_DATA) );
    InitData.pSysMem		=box_vertices;
    InitData.SysMemPitch	=sizeof(vec3);
	V_CHECK(AsD3D11Device()->CreateBuffer(&desc,&InitData,&m_pVertexBuffer));

	RecompileShaders();
}

void RenderPlatform::InvalidateDeviceObjects()
{
#ifdef _XBOX_ONE
	delete eSRAMManager;
#endif
	crossplatform::RenderPlatform::InvalidateDeviceObjects();
	solidConstants.InvalidateDeviceObjects();
	SAFE_DELETE(solidEffect);
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11::Material *mat=(dx11::Material*)(*i);
		delete mat;
	}
	materials.clear();
	SAFE_RELEASE(m_pCubemapVtxDecl);
	SAFE_RELEASE(m_pVertexBuffer);
	SAFE_DELETE(m_pDebugEffect);
	ID3D11DeviceContext* c=immediateContext.asD3D11DeviceContext();
	SAFE_RELEASE(c);
	immediateContext.platform_context=NULL;
}

void RenderPlatform::RecompileShaders()
{
	SAFE_DELETE(m_pDebugEffect);
	SAFE_DELETE(solidEffect);
	if(!device)
		return;
	std::map<std::string,std::string> defines;
	m_pDebugEffect=CreateEffect("simul_debug",defines);
	if(reverseDepth)
		defines["REVERSE_DEPTH"]="1";
	solidEffect=CreateEffect("solid",defines);
	solidConstants.LinkToEffect(solidEffect,"SolidConstants");
	//solidConstants.LinkToProgram(solid_program,"SolidConstants",1);
	for(std::set<crossplatform::Material*>::iterator i=materials.begin();i!=materials.end();i++)
	{
		dx11::Material *mat=(dx11::Material*)(*i);
		mat->SetEffect(solidEffect);
	}
	crossplatform::RenderPlatform::RecompileShaders();
}

void RenderPlatform::PushTexturePath(const char *pathUtf8)
{
	simul::dx11::PushTexturePath(pathUtf8);
}

void RenderPlatform::PopTexturePath()
{
	simul::dx11::PopTexturePath();
}

void RenderPlatform::StartRender()
{
	/*glPushAttrib(GL_ENABLE_BIT);
	glPushAttrib(GL_LIGHTING_BIT);
	glEnable(GL_DEPTH_TEST);
	// Draw the front face only, except for the texts and lights.
	glEnable(GL_CULL_FACE);
	glDisable(GL_CULL_FACE);
	glUseProgram(solid_program);*/
}

void RenderPlatform::EndRender()
{
	/*glUseProgram(0);
	glPopAttrib();
	glPopAttrib();*/
}
void RenderPlatform::SetReverseDepth(bool r)
{
	reverseDepth=r;
}

namespace
{
    const float DEFAULT_LIGHT_POSITION[]				={0.0f, 0.0f, 0.0f, 1.0f};
    const float DEFAULT_DIRECTION_LIGHT_POSITION[]	={0.0f, 0.0f, 1.0f, 0.0f};
    const float DEFAULT_SPOT_LIGHT_DIRECTION[]		={0.0f, 0.0f, -1.0f};
    const float DEFAULT_LIGHT_COLOR[]					={1.0f, 1.0f, 1.0f, 1.0f};
    const float DEFAULT_LIGHT_SPOT_CUTOFF				=180.0f;
}

void RenderPlatform::IntializeLightingEnvironment(const float pAmbientLight[3])
{
 /*   glLightfv(GL_LIGHT0, GL_POSITION, DEFAULT_DIRECTION_LIGHT_POSITION);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, DEFAULT_LIGHT_COLOR);
    glLightfv(GL_LIGHT0, GL_SPECULAR, DEFAULT_LIGHT_COLOR);
    glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, DEFAULT_LIGHT_SPOT_CUTOFF);
    glEnable(GL_LIGHT0);
    // Set ambient light.
    GLfloat lAmbientLight[] = {static_cast<GLfloat>(pAmbientLight[0]), static_cast<GLfloat>(pAmbientLight[1]),static_cast<GLfloat>(pAmbientLight[2]), 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lAmbientLight);*/
}

void RenderPlatform::DispatchCompute	(crossplatform::DeviceContext &deviceContext,int w,int l,int d)
{
	deviceContext.asD3D11DeviceContext()->Dispatch(w,l,d);
}

void RenderPlatform::ApplyShaderPass(crossplatform::DeviceContext &deviceContext,crossplatform::Effect *effect,crossplatform::EffectTechnique *tech,int index)
{
	tech->asD3DX11EffectTechnique()->GetPassByIndex(index)->Apply(0,deviceContext.asD3D11DeviceContext());
}

void RenderPlatform::Draw			(crossplatform::DeviceContext &deviceContext,int num_verts,int start_vert)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	pContext->Draw(num_verts,start_vert);
}

void RenderPlatform::DrawIndexed(crossplatform::DeviceContext &deviceContext,int num_indices,int start_index,int base_vert)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	pContext->DrawIndexed(num_indices,start_index,base_vert);
}

void RenderPlatform::DrawMarker(crossplatform::DeviceContext &deviceContext,const double *matrix)
{
 /*   glColor3f(0.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((const double*) matrix);

    glBegin(GL_LINE_LOOP);
        glVertex3f(+1.0f, -1.0f, +1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, +1.0f);

        glVertex3f(+1.0f, +1.0f, +1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);

        glVertex3f(+1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(+1.0f, -1.0f, +1.0f);

        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, +1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);

        glVertex3f(-1.0f, -1.0f, +1.0f);
        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, +1.0f);

        glVertex3f(-1.0f, -1.0f, -1.0f);
        glVertex3f(-1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, +1.0f, -1.0f);
        glVertex3f(+1.0f, -1.0f, -1.0f);
    glEnd();
    glPopMatrix();*/
}

void RenderPlatform::DrawLine(crossplatform::DeviceContext &deviceContext,const double *pGlobalBasePosition, const double *pGlobalEndPosition,const float *colour,float width)
{
/*    glColor3f(colour[0],colour[1],colour[2]);
    glLineWidth(width);

    glBegin(GL_LINES);

    glVertex3dv((const GLdouble *)pGlobalBasePosition);
    glVertex3dv((const GLdouble *)pGlobalEndPosition);

    glEnd();*/
}

void RenderPlatform::DrawCrossHair(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition)
{
/*    glColor3f(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((double*) pGlobalPosition);

    double lCrossHair[6][3] = { { -3, 0, 0 }, { 3, 0, 0 },
    { 0, -3, 0 }, { 0, 3, 0 },
    { 0, 0, -3 }, { 0, 0, 3 } };

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[0]);
    glVertex3dv(lCrossHair[1]);

    glEnd();

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[2]);
    glVertex3dv(lCrossHair[3]);

    glEnd();

    glBegin(GL_LINES);

    glVertex3dv(lCrossHair[4]);
    glVertex3dv(lCrossHair[5]);

    glEnd();
	
	glMatrixMode(GL_MODELVIEW);
    glPopMatrix();*/
}

void RenderPlatform::DrawCamera(crossplatform::DeviceContext &deviceContext,const double *pGlobalPosition, double pRoll)
{
 /*   glColor3d(1.0, 1.0, 1.0);
    glLineWidth(1.0);
	
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glMultMatrixd((const double*) pGlobalPosition);
    glRotated(pRoll, 1.0, 0.0, 0.0);

    int i;
    float lCamera[10][2] = {{ 0, 5.5 }, { -3, 4.5 },
    { -3, 7.5 }, { -6, 10.5 }, { -23, 10.5 },
    { -23, -4.5 }, { -20, -7.5 }, { -3, -7.5 },
    { -3, -4.5 }, { 0, -5.5 }   };

    glBegin( GL_LINE_LOOP );
    {
        for (i = 0; i < 10; i++)
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], 4.5);
        }
    }
    glEnd();

    glBegin( GL_LINE_LOOP );
    {
        for (i = 0; i < 10; i++)
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], -4.5);
        }
    }
    glEnd();

    for (i = 0; i < 10; i++)
    {
        glBegin( GL_LINES );
        {
            glVertex3f(lCamera[i][0], lCamera[i][1], -4.5);
            glVertex3f(lCamera[i][0], lCamera[i][1], 4.5);
        }
        glEnd();
    }
    glPopMatrix();*/
}

void RenderPlatform::DrawLineLoop(crossplatform::DeviceContext &deviceContext,const double *mat,int lVerticeCount,const double *vertexArray,const float colr[4])
{
/*    glPushMatrix();
    glMultMatrixd((const double*)mat);
	glColor3f(colr[0],colr[1],colr[2]);
	glBegin(GL_LINE_LOOP);
	for (int lVerticeIndex = 0; lVerticeIndex < lVerticeCount; lVerticeIndex++)
	{
		glVertex3dv((GLdouble *)&vertexArray[lVerticeIndex*3]);
	}
	glEnd();
    glPopMatrix();*/
}

void RenderPlatform::ApplyDefaultMaterial()
{
    const float BLACK_COLOR[] = {0.0f, 0.0f, 0.0f, 1.0f};
    const float GREEN_COLOR[] = {0.0f, 1.0f, 0.0f, 1.0f};
//    const GLfloat WHITE_COLOR[] = {1.0f, 1.0f, 1.0f, 1.0f};
/*    glMaterialfv(GL_FRONT, GL_EMISSION, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_AMBIENT, BLACK_COLOR);
    glMaterialfv(GL_FRONT, GL_DIFFUSE, GREEN_COLOR);
    glMaterialfv(GL_FRONT, GL_SPECULAR, BLACK_COLOR);
    glMaterialf(GL_FRONT, GL_SHININESS, 0);

    glBindTexture(GL_TEXTURE_2D, 0);*/
}

void MakeWorldViewProjMatrix(float *wvp,const double *w,const float *v,const float *p)
{
	simul::math::Matrix4x4 tmp1,view(v),proj(p),model(w);
	simul::math::Multiply4x4(tmp1,model,view);
	simul::math::Multiply4x4(*(simul::math::Matrix4x4*)wvp,tmp1,proj);
}

void RenderPlatform::SetModelMatrix(crossplatform::DeviceContext &deviceContext,const double *m,const crossplatform::PhysicalLightRenderData &physicalLightRenderData)
{
	simul::math::Matrix4x4 wvp;
	simul::math::Matrix4x4 viewproj;
	simul::math::Matrix4x4 modelviewproj;
	simul::math::Multiply4x4(viewproj,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	simul::math::Matrix4x4 model(m);
	simul::math::Multiply4x4(modelviewproj,model,viewproj);
	//math::Vector3 dirToLight;
	//simul::math::Multiply3(dirToLight,(const float*)&physicalLightRenderData.dirToLight,model);
	solidConstants.worldViewProj=modelviewproj;
	solidConstants.world=model;
	
	solidConstants.lightIrradiance	=physicalLightRenderData.lightColour;
	solidConstants.lightDir			=physicalLightRenderData.dirToLight;
	solidConstants.Apply(deviceContext);
	solidConstants.Apply(deviceContext);
	
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext*)deviceContext.asD3D11DeviceContext();
	solidEffect->asD3DX11Effect()->GetTechniqueByName("solid")->GetPassByIndex(0)->Apply(0,pContext);
}

crossplatform::Material *RenderPlatform::CreateMaterial()
{
	dx11::Material *mat=new dx11::Material;
	mat->SetEffect(solidEffect);
	materials.insert(mat);
	return mat;
}

crossplatform::Mesh *RenderPlatform::CreateMesh()
{
	return new dx11::Mesh;
}

crossplatform::Light *RenderPlatform::CreateLight()
{
	return new dx11::Light();
}

crossplatform::Texture *RenderPlatform::CreateTexture(const char *fileNameUtf8)
{
	crossplatform::Texture * tex=NULL;
#ifdef _XBOX_ONE
	if(fileNameUtf8&&strcmp(fileNameUtf8,"ESRAM")==0)
		tex=new dx11::ESRAMTexture();
	else
#endif
	{
		tex=new dx11::Texture();
		if(fileNameUtf8)
			tex->LoadFromFile(this,fileNameUtf8);
	}
	return tex;
}

crossplatform::BaseFramebuffer *RenderPlatform::CreateFramebuffer()
{
	return new dx11::Framebuffer();
}

static D3D11_TEXTURE_ADDRESS_MODE toD3d11TextureAddressMode(crossplatform::SamplerStateDesc::Wrapping f)
{
	if(f==crossplatform::SamplerStateDesc::CLAMP)
		return D3D11_TEXTURE_ADDRESS_CLAMP;
	if(f==crossplatform::SamplerStateDesc::WRAP)
		return D3D11_TEXTURE_ADDRESS_WRAP;
	if(f==crossplatform::SamplerStateDesc::MIRROR)
		return D3D11_TEXTURE_ADDRESS_MIRROR;
	return D3D11_TEXTURE_ADDRESS_WRAP;
}
D3D11_FILTER toD3d11Filter(crossplatform::SamplerStateDesc::Filtering f)
{
	if(f==crossplatform::SamplerStateDesc::LINEAR)
		return D3D11_FILTER_ANISOTROPIC;
	return D3D11_FILTER_MIN_MAG_MIP_POINT;
}
crossplatform::SamplerState *RenderPlatform::CreateSamplerState(crossplatform::SamplerStateDesc *d)
{
	dx11::SamplerState *s=new dx11::SamplerState();
	
	D3D11_SAMPLER_DESC samplerDesc;
	
    ZeroMemory( &samplerDesc, sizeof( D3D11_SAMPLER_DESC ) );
    samplerDesc.Filter = toD3d11Filter (d->filtering) ;
    samplerDesc.AddressU = toD3d11TextureAddressMode(d->x);
    samplerDesc.AddressV = toD3d11TextureAddressMode(d->y);
    samplerDesc.AddressW = toD3d11TextureAddressMode(d->z);
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxAnisotropy = 16;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	
	SAFE_RELEASE(s->m_pd3D11SamplerState);
	AsD3D11Device()->CreateSamplerState(&samplerDesc,&s->m_pd3D11SamplerState);
	return s;
}

crossplatform::Effect *RenderPlatform::CreateEffect(const char *filename_utf8,const std::map<std::string,std::string> &defines)
{
	std::string fn(filename_utf8);
	if(fn.find(".")>=fn.length())
		fn+=".fx";
	crossplatform::Effect *e= new dx11::Effect(this,fn.c_str(),defines);
	e->SetName(filename_utf8);
	return e;
}

crossplatform::PlatformConstantBuffer *RenderPlatform::CreatePlatformConstantBuffer()
{
	crossplatform::PlatformConstantBuffer *b=new dx11::PlatformConstantBuffer();
	return b;
}

crossplatform::PlatformStructuredBuffer *RenderPlatform::CreatePlatformStructuredBuffer()
{
	crossplatform::PlatformStructuredBuffer *b=new dx11::PlatformStructuredBuffer();
	return b;
}

crossplatform::Buffer *RenderPlatform::CreateBuffer()
{
	crossplatform::Buffer *b=new dx11::Buffer();
	return b;
}

DXGI_FORMAT RenderPlatform::ToDxgiFormat(crossplatform::PixelFormat p)
{
	using namespace crossplatform;
	switch(p)
	{
	case R_16_FLOAT:
		return DXGI_FORMAT_R16_FLOAT;
	case RGBA_16_FLOAT:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case RGBA_32_FLOAT:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case RGB_32_FLOAT:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case RG_32_FLOAT:
		return DXGI_FORMAT_R32G32_FLOAT;
	case R_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case LUM_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case INT_32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case RGBA_8_UNORM:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case RGBA_8_SNORM:
		return DXGI_FORMAT_R8G8B8A8_SNORM;
	case R_8_UNORM:
		return DXGI_FORMAT_R8_UNORM;
	case R_8_SNORM:
		return DXGI_FORMAT_R8_SNORM;
	case R_32_UINT:
		return DXGI_FORMAT_R32_UINT;
	case RG_32_UINT:
		return DXGI_FORMAT_R32G32_UINT;
	case RGB_32_UINT:
		return DXGI_FORMAT_R32G32B32_UINT;
	case RGBA_32_UINT:
		return DXGI_FORMAT_R32G32B32A32_UINT;
	case D_32_FLOAT:
		return DXGI_FORMAT_D32_FLOAT;
	case D_16_UNORM:
		return DXGI_FORMAT_D16_UNORM;
	default:
		return DXGI_FORMAT_UNKNOWN;
	};
}

crossplatform::PixelFormat RenderPlatform::FromDxgiFormat(DXGI_FORMAT f)
{
	using namespace crossplatform;
	switch(f)
	{
	case DXGI_FORMAT_R16_FLOAT:
		return R_16_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return RGBA_16_FLOAT;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
		return RGBA_32_FLOAT;
	case DXGI_FORMAT_R32G32B32_FLOAT:
		return RGB_32_FLOAT;
	case DXGI_FORMAT_R32G32_FLOAT:
		return RG_32_FLOAT;
	case DXGI_FORMAT_R32_FLOAT:
		return R_32_FLOAT;
	case DXGI_FORMAT_R8G8B8A8_UNORM:
		return RGBA_8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_SNORM:
		return RGBA_8_SNORM;
	case DXGI_FORMAT_R32_UINT:
		return R_32_UINT;
	case DXGI_FORMAT_R32G32_UINT:
		return RG_32_UINT;
	case DXGI_FORMAT_R32G32B32_UINT:
		return RGB_32_UINT;
	case DXGI_FORMAT_R32G32B32A32_UINT:
		return RGBA_32_UINT;
	case DXGI_FORMAT_D32_FLOAT:
		return D_32_FLOAT;
	case DXGI_FORMAT_D16_UNORM:
		return D_16_UNORM;
	default:
		return UNKNOWN;
	};
}

crossplatform::Layout *RenderPlatform::CreateLayout(int num_elements,crossplatform::LayoutDesc *desc,crossplatform::Buffer *buffer)
{
	D3D11_INPUT_ELEMENT_DESC *decl=new D3D11_INPUT_ELEMENT_DESC[num_elements];
	for(int i=0;i<num_elements;i++)
	{
		const crossplatform::LayoutDesc &d=desc[i];
		D3D11_INPUT_ELEMENT_DESC &D=decl[i];
		D.SemanticName=d.semanticName;
		D.SemanticIndex=d.semanticIndex;
		D.Format=ToDxgiFormat(d.format);
		D.AlignedByteOffset=d.alignedByteOffset;
		D.InputSlot=d.inputSlot;
		D.InputSlotClass=d.perInstance?D3D11_INPUT_PER_INSTANCE_DATA:D3D11_INPUT_PER_VERTEX_DATA;
		D.InstanceDataStepRate=d.instanceDataStepRate;
	};
	dx11::Layout *l=new dx11::Layout();
	l->SetDesc(desc,num_elements);
	ID3DBlob *VS;
	ID3DBlob *errorMsgs=NULL;
	std::string dummy_shader;
	dummy_shader="struct vertexInput\n"
				"{\n";
	for(int i=0;i<num_elements;i++)
	{
		D3D11_INPUT_ELEMENT_DESC &dec=decl[i];
		std::string format;
		switch(dec.Format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			format="float4";
			break;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			format="float3";
			break;
		case DXGI_FORMAT_R32G32_FLOAT:
			format="float2";
			break;
		case DXGI_FORMAT_R32_FLOAT:
			format="float";
			break;
		case DXGI_FORMAT_R32_UINT:
			format="uint";
			break;
		default:
			SIMUL_CERR<<"Unhandled type "<<std::endl;
		};
		dummy_shader+="   ";
		dummy_shader+=format+" ";
		std::string name=dec.SemanticName;
		if(strcmp(dec.SemanticName,"POSITION")!=0)
			name+=('0'+dec.SemanticIndex);
		dummy_shader+=name;
		dummy_shader+="_";
		dummy_shader+=" : ";
		dummy_shader+=name;
		dummy_shader+=";\n";
				//"	float3 position		: POSITION;"
				//"	float texCoords		: TEXCOORD0;";
	}
	dummy_shader+="};\n"
				"struct vertexOutput\n"
				"{\n"
				"	float4 hPosition	: SV_POSITION;\n"
				"};\n"
				"vertexOutput VS_Main(vertexInput IN)\n"
				"{\n"
				"	vertexOutput OUT;\n"
				"	OUT.hPosition	=float4(1.0,1.0,1.0,1.0);\n"
				"	return OUT;\n"
				"}\n";
	const char *str=dummy_shader.c_str();
	size_t len=strlen(str);
#if WINVER<0x602
	HRESULT hr=D3DX11CompileFromMemory(str,len,"dummy",NULL,NULL,"VS_Main", "vs_4_0", 0, 0, 0, &VS, &errorMsgs, 0);
#else
	HRESULT hr=D3DCompile(str,len,"dummy",NULL,NULL,"VS_Main", "vs_4_0", 0, 0, &VS, &errorMsgs);
#endif
	if(hr!=S_OK)
	{
		const char *e=(const char*)errorMsgs->GetBufferPointer();
		std::cerr<<e<<std::endl;
	}
	SAFE_RELEASE(l->d3d11InputLayout);
	AsD3D11Device()->CreateInputLayout(decl, num_elements, VS->GetBufferPointer(), VS->GetBufferSize(), &l->d3d11InputLayout);
	
	if(VS)
		VS->Release();
	if(errorMsgs)
		errorMsgs->Release();
	delete [] decl;
	return l;
}
static D3D11_COMPARISON_FUNC toD3dComparison(crossplatform::DepthComparison d)
{
	switch(d)
	{
	case crossplatform::DEPTH_LESS:
		return D3D11_COMPARISON_LESS;
	case crossplatform::DEPTH_EQUAL:
		return D3D11_COMPARISON_EQUAL;
	case crossplatform::DEPTH_LESS_EQUAL:
		return D3D11_COMPARISON_LESS_EQUAL;
	case crossplatform::DEPTH_GREATER:
		return D3D11_COMPARISON_GREATER;
	case crossplatform::DEPTH_NOT_EQUAL:
		return D3D11_COMPARISON_NOT_EQUAL;
	case crossplatform::DEPTH_GREATER_EQUAL:
		return D3D11_COMPARISON_GREATER_EQUAL;
	default:
		break;
	};
	return D3D11_COMPARISON_LESS;
}
static D3D11_BLEND toD3dBlendOp(crossplatform::BlendOption o)
{
	switch(o)
	{
	case crossplatform::BLEND_ZERO:
		return D3D11_BLEND_ZERO;
	case crossplatform::BLEND_ONE:
		return D3D11_BLEND_ONE;
	case crossplatform::BLEND_SRC_COLOR:
		return D3D11_BLEND_SRC_COLOR;
	case crossplatform::BLEND_INV_SRC_COLOR:
		return D3D11_BLEND_INV_SRC_COLOR;
	case crossplatform::BLEND_SRC_ALPHA:
		return D3D11_BLEND_SRC_ALPHA;
	case crossplatform::BLEND_INV_SRC_ALPHA:
		return D3D11_BLEND_INV_SRC_ALPHA;
	case crossplatform::BLEND_DEST_ALPHA:
		return D3D11_BLEND_DEST_ALPHA;
	case crossplatform::BLEND_INV_DEST_ALPHA:
		return D3D11_BLEND_INV_DEST_ALPHA;
	case crossplatform::BLEND_DEST_COLOR:
		return D3D11_BLEND_DEST_COLOR;
	case crossplatform::BLEND_INV_DEST_COLOR:
		return D3D11_BLEND_INV_DEST_COLOR;
	case crossplatform::BLEND_SRC_ALPHA_SAT:
		return D3D11_BLEND_SRC_ALPHA_SAT;
	case crossplatform::BLEND_BLEND_FACTOR:
		return D3D11_BLEND_BLEND_FACTOR;
	case crossplatform::BLEND_INV_BLEND_FACTOR:
		return D3D11_BLEND_INV_BLEND_FACTOR;
	case crossplatform::BLEND_SRC1_COLOR:
		return D3D11_BLEND_SRC1_COLOR;
	case crossplatform::BLEND_INV_SRC1_COLOR:
		return D3D11_BLEND_INV_SRC1_COLOR;
	case crossplatform::BLEND_SRC1_ALPHA:
		return D3D11_BLEND_SRC1_ALPHA;
	case crossplatform::BLEND_INV_SRC1_ALPHA:
		return D3D11_BLEND_INV_SRC1_ALPHA;
	default:
		break;
	};
	return D3D11_BLEND_ONE;
}

crossplatform::RenderState *RenderPlatform::CreateRenderState(const crossplatform::RenderStateDesc &desc)
{
	dx11::RenderState *s=new dx11::RenderState();
	s->type=desc.type;
	if(desc.type==crossplatform::BLEND)
	{
		D3D11_BLEND_DESC omDesc;
		ZeroMemory( &omDesc, sizeof( D3D11_BLEND_DESC ) );
		for(int i=0;i<desc.blend.numRTs;i++)
		{
			omDesc.RenderTarget[i].BlendEnable				=desc.blend.RenderTarget[i].BlendEnable;
			omDesc.RenderTarget[i].BlendOp					=D3D11_BLEND_OP_ADD;
			omDesc.RenderTarget[i].SrcBlend					=toD3dBlendOp(desc.blend.RenderTarget[i].SrcBlend);
			omDesc.RenderTarget[i].DestBlend				=toD3dBlendOp(desc.blend.RenderTarget[i].DestBlend);
			omDesc.RenderTarget[i].BlendOpAlpha				=D3D11_BLEND_OP_ADD;
			omDesc.RenderTarget[i].SrcBlendAlpha			=toD3dBlendOp(desc.blend.RenderTarget[i].SrcBlendAlpha);
			omDesc.RenderTarget[i].DestBlendAlpha			=toD3dBlendOp(desc.blend.RenderTarget[i].DestBlendAlpha);
			omDesc.RenderTarget[i].RenderTargetWriteMask	=desc.blend.RenderTarget[i].RenderTargetWriteMask;
		}
		omDesc.IndependentBlendEnable			=desc.blend.IndependentBlendEnable;
		omDesc.AlphaToCoverageEnable			=desc.blend.AlphaToCoverageEnable;
		AsD3D11Device()->CreateBlendState(&omDesc,&s->m_blendState);
	}
	else
	{
		D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
		//Initialize the description of the stencil state.
		ZeroMemory(&depthStencilDesc, sizeof(depthStencilDesc));

		// Set up the description of the stencil state.
		depthStencilDesc.DepthEnable		=desc.depth.test;
		depthStencilDesc.DepthWriteMask		=desc.depth.write? D3D11_DEPTH_WRITE_MASK_ALL:D3D11_DEPTH_WRITE_MASK_ZERO;
		depthStencilDesc.DepthFunc			= toD3dComparison(desc.depth.comparison);

		depthStencilDesc.StencilEnable = false;
		depthStencilDesc.StencilReadMask = 0xFF;
		depthStencilDesc.StencilWriteMask = 0xFF;

		// Stencil operations if pixel is front-facing.
		depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
		depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

		// Stencil operations if pixel is back-facing.
		depthStencilDesc.BackFace.StencilFailOp		= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilDepthFailOp= D3D11_STENCIL_OP_DECR;
		depthStencilDesc.BackFace.StencilPassOp		= D3D11_STENCIL_OP_KEEP;
		depthStencilDesc.BackFace.StencilFunc		= D3D11_COMPARISON_ALWAYS;
		
		AsD3D11Device()->CreateDepthStencilState(&depthStencilDesc, &s->m_depthStencilState);
	}
	return s;
}

crossplatform::Query *RenderPlatform::CreateQuery(crossplatform::QueryType type)
{
	dx11::Query *q=new dx11::Query(type);
	return q;
}

void *RenderPlatform::GetDevice()
{
	return device;
}

void RenderPlatform::SetVertexBuffers(crossplatform::DeviceContext &deviceContext,int slot,int num_buffers,crossplatform::Buffer **buffers,const crossplatform::Layout *layout)
{
	UINT offset = 0;
	ID3D11Buffer *buf[10];
	UINT strides[10];
	UINT offsets[10];
	for(int i=0;i<num_buffers;i++)
	{
		strides[i]=buffers[i]->stride;
		buf[i]=buffers[i]->AsD3D11Buffer();
		offsets[i]=0;
	}
	deviceContext.asD3D11DeviceContext()->IASetVertexBuffers(	0,	// the first input slot for binding
									num_buffers,					// the number of buffers in the array
									buf,							// the array of vertex buffers
									strides,						// array of stride values, one for each buffer
									offsets );						// array of offset values, one for each buffer
};

void RenderPlatform::SetStreamOutTarget(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *vertexBuffer)
{
	ID3D11Buffer *b=NULL;
	if(vertexBuffer)
		b=vertexBuffer->AsD3D11Buffer();
	UINT offset = 0;
	deviceContext.asD3D11DeviceContext()->SOSetTargets(1,&b,&offset );
}

void RenderPlatform::ActivateRenderTargets(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Texture **targs,crossplatform::Texture *depth)
{
	ID3D11RenderTargetView *rt[4];
	SIMUL_ASSERT(num<=4);
	for(int i=0;i<num;i++)
		rt[i]=targs[i]->AsD3D11RenderTargetView();
	ID3D11DepthStencilView *d=NULL;
	if(depth)
		d=depth->AsD3D11DepthStencilView();
	deviceContext.asD3D11DeviceContext()->OMSetRenderTargets(num,rt,d);
}

void RenderPlatform::SetViewports(crossplatform::DeviceContext &deviceContext,int num,crossplatform::Viewport *vps)
{
	D3D11_VIEWPORT viewports[4];
	SIMUL_ASSERT(num<=4);
	for(int i=0;i<num;i++)
	{
		viewports[i].Width		=(float)vps[i].w;
		viewports[i].Height		=(float)vps[i].h;
		viewports[i].TopLeftX	=(float)vps[i].x;
		viewports[i].TopLeftY	=(float)vps[i].y;
		viewports[i].MinDepth	=vps[i].znear;
		viewports[i].MaxDepth	=vps[i].zfar;
	}
	deviceContext.asD3D11DeviceContext()->RSSetViewports(num,viewports);
}

crossplatform::Viewport	RenderPlatform::GetViewport(crossplatform::DeviceContext &deviceContext,int index)
{
	D3D11_VIEWPORT viewports[4];
	unsigned num=0;
	deviceContext.asD3D11DeviceContext()->RSGetViewports(&num,NULL);
	deviceContext.asD3D11DeviceContext()->RSGetViewports(&num,viewports);
	crossplatform::Viewport v;
	v.x=(int)viewports[index].TopLeftX;
	v.y=(int)viewports[index].TopLeftY;
	v.w=(int)viewports[index].Width;
	v.h=(int)viewports[index].Height;
	v.znear	=viewports[index].MinDepth;
	v.zfar	=viewports[index].MaxDepth;
	return v;
}

void RenderPlatform::SetIndexBuffer(crossplatform::DeviceContext &deviceContext,crossplatform::Buffer *buffer)
{
	if(!buffer)
	{
		deviceContext.asD3D11DeviceContext()->IASetIndexBuffer(NULL, DXGI_FORMAT_R32_UINT, 0);
		return;
	}
	DXGI_FORMAT f;
	int index_byte_size=buffer->stride;
	switch(index_byte_size)
	{
	case 4:
		f=DXGI_FORMAT_R32_UINT;
		break;
	case 2:
		f=DXGI_FORMAT_R16_UINT;
		break;
	default:
		f=DXGI_FORMAT_UNKNOWN;
		SIMUL_BREAK("Can't use DXGI_FORMAT_UNKNOWN for an index buffer.")
		break;
	};
	deviceContext.asD3D11DeviceContext()->IASetIndexBuffer(buffer->AsD3D11Buffer(), f, 0);
}

static D3D11_PRIMITIVE_TOPOLOGY toD3dTopology(crossplatform::Topology t)
{
	using namespace crossplatform;
	switch(t)
	{			
	case POINTLIST:
		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case LINELIST:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case LINESTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
	case TRIANGLELIST:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	case TRIANGLESTRIP:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
	case LINELIST_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
	case LINESTRIP_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
	case TRIANGLELIST_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
	case TRIANGLESTRIP_ADJ:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
	default:
		return D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
	};
}

void RenderPlatform::SetTopology(crossplatform::DeviceContext &deviceContext,crossplatform::Topology t)
{
	D3D11_PRIMITIVE_TOPOLOGY T=toD3dTopology(t);
	//(D3D11_PRIMITIVE_TOPOLOGY)((int)t-(int)crossplatform::POINTLIST+(int)D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	deviceContext.asD3D11DeviceContext()->IASetPrimitiveTopology(T);
}

void RenderPlatform::SetRenderState(crossplatform::DeviceContext &deviceContext,const crossplatform::RenderState *s)
{
	if(!s)
		return;
	dx11::RenderState *ds=(dx11::RenderState *)(s);
	if(ds->type==crossplatform::BLEND)
	{
		float blendFactor[]		={0,0,0,0};
		UINT sampleMask			=0xffffffff;
		deviceContext.asD3D11DeviceContext()->OMSetBlendState(ds->m_blendState,blendFactor,sampleMask);
	}
	if(ds->type==crossplatform::DEPTH)
	{
		deviceContext.asD3D11DeviceContext()->OMSetDepthStencilState(ds->m_depthStencilState,0);
	}
}

void RenderPlatform::StoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	storedStates.push_back(StoredState());
	StoredState &s=storedStates.back();
    pContext->OMGetDepthStencilState( &s.m_pDepthStencilStateStored11, &s.m_StencilRefStored11 );
    pContext->RSGetState(&s.m_pRasterizerStateStored11 );
    pContext->OMGetBlendState(&s.m_pBlendStateStored11,s.m_BlendFactorStored11, &s.m_SampleMaskStored11 );
    pContext->PSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT, s.m_pSamplerStateStored11 );
    pContext->VSGetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT , s.m_pVertexSamplerStateStored11 );

	pContext->IAGetInputLayout( &s.m_previousInputLayout );
	pContext->IAGetPrimitiveTopology(&s.m_previousTopology);

	pContext->IAGetVertexBuffers(	0,
									D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
									s.m_pVertexBuffersStored11,
									s.m_VertexStrides,
									s.m_VertexOffsets	);

	pContext->IAGetIndexBuffer(&s.pIndexBufferStored11,
								&s.m_indexFormatStored11,
								&s.m_indexOffset);
}

void RenderPlatform::RestoreRenderState( crossplatform::DeviceContext &deviceContext )
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	StoredState &s=storedStates.back();
    pContext->OMSetDepthStencilState(s.m_pDepthStencilStateStored11,s.m_StencilRefStored11 );
    pContext->RSSetState(s.m_pRasterizerStateStored11 );
    pContext->OMSetBlendState(s.m_pBlendStateStored11,s.m_BlendFactorStored11,s.m_SampleMaskStored11 );
    pContext->PSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pSamplerStateStored11 );
    pContext->VSSetSamplers(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT,s.m_pVertexSamplerStateStored11 );

    SAFE_RELEASE(s.m_pDepthStencilStateStored11 );
    SAFE_RELEASE(s.m_pRasterizerStateStored11 );
    SAFE_RELEASE(s.m_pBlendStateStored11 );
	for(int i=0;i<D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;i++)
	{
	    SAFE_RELEASE(s.m_pSamplerStateStored11[i]);
	    SAFE_RELEASE(s.m_pVertexSamplerStateStored11[i]);
	}
	pContext->IASetPrimitiveTopology(s.m_previousTopology);
	pContext->IASetInputLayout(s.m_previousInputLayout );
	SAFE_RELEASE(s.m_previousInputLayout);

	
	pContext->IASetVertexBuffers(	0,
									D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT,
									s.m_pVertexBuffersStored11,
									s.m_VertexStrides,
									s.m_VertexOffsets	);
	for(int i=0;i<D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;i++)
	{
	    SAFE_RELEASE(s.m_pVertexBuffersStored11[i]);
	}

	pContext->IASetIndexBuffer(s.pIndexBufferStored11,
								s.m_indexFormatStored11,
								s.m_indexOffset);
	SAFE_RELEASE(s.pIndexBufferStored11);
	storedStates.pop_back();
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,ID3D11ShaderResourceView *srv,float mult,bool blend)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTexture",srv);
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"multiplier",mult);
	crossplatform::EffectTechnique *tech=m_pDebugEffect->GetTechniqueByName("textured");
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	int pass=(blend==true)?1:0;
	if(srv)
	{
		srv->GetDesc(&desc);
		bool msaa=(desc.ViewDimension==D3D11_SRV_DIMENSION_TEXTURE2DMS);
		if(msaa)
		{
			tech=m_pDebugEffect->GetTechniqueByName("textured");
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureMS",srv);
		}
		else if(desc.Format==DXGI_FORMAT_R32_UINT||desc.Format==DXGI_FORMAT_R32G32_UINT||desc.Format==DXGI_FORMAT_R32G32B32_UINT||desc.Format==DXGI_FORMAT_R32G32B32A32_UINT)
		{
			tech=m_pDebugEffect->GetTechniqueByName("compacted_texture");
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureUint",srv);
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureUint2",srv);
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureUint3",srv);
			simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTextureUint4",srv);
		}
	}
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	if(mirrorY2)
		y1=(int)viewport.Height-y1-dy;
	{
		UtilityRenderer::DrawQuad2(deviceContext
			,2.f*(float)x1/(float)viewport.Width-1.f
			,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
			,2.f*(float)dx/(float)viewport.Width
			,2.f*(float)dy/(float)viewport.Height
			,m_pDebugEffect->asD3DX11Effect(),tech->asD3DX11EffectTechnique(),pass);
	}
	simul::dx11::setTexture(m_pDebugEffect->asD3DX11Effect(),"imageTexture",NULL);
}

void RenderPlatform::DrawTexture(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,float mult,bool blend)
{
	DrawTexture(deviceContext,x1,y1,dx,dy,tex->AsD3D11ShaderResourceView(),mult,blend);
}

void RenderPlatform::DrawDepth(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Texture *tex,const crossplatform::Viewport *v)
{
	crossplatform::EffectTechnique *tech	=m_pDebugEffect->GetTechniqueByName("show_depth");
	if(tex->GetSampleCount()>0)
	{
		tech=m_pDebugEffect->GetTechniqueByName("show_depth_ms");
		m_pDebugEffect->SetTexture(deviceContext,"imageTextureMS",tex);
	}
	else
	{
		m_pDebugEffect->SetTexture(deviceContext,"imageTexture",tex);
	}
	simul::camera::Frustum frustum=simul::camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
	m_pDebugEffect->SetParameter("tanHalfFov",vec2(frustum.tanHalfHorizontalFov,frustum.tanHalfVerticalFov));
	static float cc=300000.f;
	vec4 depthToLinFadeDistParams=camera::GetDepthToDistanceParameters(deviceContext.viewStruct,cc);//(deviceContext.viewStruct.proj[3*4+2],cc,deviceContext.viewStruct.proj[2*4+2]*cc);
	m_pDebugEffect->SetParameter("depthToLinFadeDistParams",depthToLinFadeDistParams);
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	if(mirrorY2)
	{
		y1=(int)viewport.Height-y1;
		dy*=-1;
	}
	{
		if(v)
			setParameter(m_pDebugEffect->asD3DX11Effect(),"viewport",(float)v->x/(float)tex->width,(float)v->y/(float)tex->length,(float)v->w/(float)tex->width,(float)v->h/(float)tex->length);
		else
			setParameter(m_pDebugEffect->asD3DX11Effect(),"viewport",0.f,0.f,1.f,1.f);
		UtilityRenderer::DrawQuad2(deviceContext
			,2.f*(float)x1/(float)viewport.Width-1.f
			,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
			,2.f*(float)dx/(float)viewport.Width
			,2.f*(float)dy/(float)viewport.Height
			,m_pDebugEffect->asD3DX11Effect(),tech->asD3DX11EffectTechnique());
	}
}

void RenderPlatform::DrawQuad		(crossplatform::DeviceContext &deviceContext,int x1,int y1,int dx,int dy,crossplatform::Effect *effect,crossplatform::EffectTechnique *technique)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	ID3DX11Effect			*eff		=effect->asD3DX11Effect();
	ID3DX11EffectTechnique	*tech		=technique->asD3DX11EffectTechnique();
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	if(mirrorY)
		y1=(int)viewport.Height-y1-dy;
	{
		UtilityRenderer::DrawQuad2(deviceContext
			,2.f*(float)x1/(float)viewport.Width-1.f
			,1.f-2.f*(float)(y1+dy)/(float)viewport.Height
			,2.f*(float)dx/(float)viewport.Width
			,2.f*(float)dy/(float)viewport.Height
			,eff,tech);
	}
}

void RenderPlatform::DrawQuad(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext		*pContext	=deviceContext.asD3D11DeviceContext();
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->IASetInputLayout(NULL);
	pContext->Draw(4,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void RenderPlatform::DrawLines(crossplatform::DeviceContext &deviceContext,Vertext *line_vertices,int vertex_count,bool strip,bool test_depth,bool view_centred)
{
	if(!vertex_count)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	{
		HRESULT hr=S_OK;
		D3DXMATRIX  tmp1, tmp2;
		crossplatform::EffectTechniqueGroup *g=m_pDebugEffect->GetTechniqueGroupByName(test_depth?"lines_3d_depth":"lines_3d");
		camera::Frustum f=camera::GetFrustumFromProjectionMatrix(deviceContext.viewStruct.proj);
			crossplatform::EffectTechnique *tech=g->GetTechniqueByIndex(0);
		if(test_depth)
			tech=g->GetTechniqueByName(f.reverseDepth?"depth_reverse":"depth_forward");
		D3DXMATRIX wvp;
		if(view_centred)
			camera::MakeCentredViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
		else
			camera::MakeViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
			
		m_pDebugEffect->SetMatrix("worldViewProj",&wvp._11);
	
		ID3D11Buffer *					vertexBuffer=NULL;
		// Create the vertex buffer:
		D3D11_BUFFER_DESC desc=
		{
			vertex_count*sizeof(Vertext),
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_VERTEX_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
			0
		};
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
		InitData.pSysMem = line_vertices;
		InitData.SysMemPitch = sizeof(Vertext);
		hr=AsD3D11Device()->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectPass *pass=tech->asD3DX11EffectTechnique()->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		ID3D11InputLayout*				m_pVtxDecl=NULL;
		SAFE_RELEASE(m_pVtxDecl);
		hr=AsD3D11Device()->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		ID3D11InputLayout* previousInputLayout;
		pContext->IAGetInputLayout( &previousInputLayout );
		pContext->IASetInputLayout(m_pVtxDecl);
		D3D_PRIMITIVE_TOPOLOGY previousTopology;
		pContext->IAGetPrimitiveTopology(&previousTopology);
		pContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(Vertext);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		pContext->IASetVertexBuffers(	0,							// the first input slot for binding
													1,				// the number of buffers in the array
													&vertexBuffer,	// the array of vertex buffers
													&stride,		// array of stride values, one for each buffer
													&offset);		// array of 
		hr=ApplyPass(pContext,tech->asD3DX11EffectTechnique()->GetPassByIndex(0));
		pContext->Draw(vertex_count,0);
		pContext->IASetPrimitiveTopology(previousTopology);
		pContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
		SAFE_RELEASE(m_pVtxDecl);
	}
}

void RenderPlatform::Draw2dLines(crossplatform::DeviceContext &deviceContext,Vertext *lines,int vertex_count,bool strip)
{
	if(!vertex_count)
		return;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	{
		HRESULT hr=S_OK;
		D3DXMATRIX world, tmp1, tmp2;
		D3DXMatrixIdentity(&world);
		ID3DX11EffectTechnique *tech			=m_pDebugEffect->asD3DX11Effect()->GetTechniqueByName("lines_2d");
		
		unsigned int num_v=1;
		D3D11_VIEWPORT								viewport;
		pContext->RSGetViewports(&num_v,&viewport);
		dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"rect",vec4(-1.0,-1.0,2.0f/viewport.Width,2.0f/viewport.Height));

		ID3D11Buffer *vertexBuffer=NULL;
		// Create the vertex buffer:
		D3D11_BUFFER_DESC desc=
		{
			vertex_count*sizeof(VertexXyzRgba),
			D3D11_USAGE_DYNAMIC,
			D3D11_BIND_VERTEX_BUFFER,
			D3D11_CPU_ACCESS_WRITE,
			0
		};
		D3D11_SUBRESOURCE_DATA InitData;
		ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
		InitData.pSysMem = lines;
		InitData.SysMemPitch = sizeof(VertexXyzRgba);
		hr=AsD3D11Device()->CreateBuffer(&desc,&InitData,&vertexBuffer);

		const D3D11_INPUT_ELEMENT_DESC decl[] =
		{
			{ "POSITION",	0, DXGI_FORMAT_R32G32B32_FLOAT,		0,	0,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD",	0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0,	12,	D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		D3DX11_PASS_DESC PassDesc;
		ID3DX11EffectPass *pass=tech->GetPassByIndex(0);
		hr=pass->GetDesc(&PassDesc);

		ID3D11InputLayout*				m_pVtxDecl=NULL;
		SAFE_RELEASE(m_pVtxDecl);
		hr=AsD3D11Device()->CreateInputLayout( decl,2,PassDesc.pIAInputSignature,PassDesc.IAInputSignatureSize,&m_pVtxDecl);
	
		pContext->IASetInputLayout(m_pVtxDecl);
		ID3D11InputLayout* previousInputLayout;
		pContext->IAGetInputLayout( &previousInputLayout );
		D3D_PRIMITIVE_TOPOLOGY previousTopology;
		pContext->IAGetPrimitiveTopology(&previousTopology);
		pContext->IASetPrimitiveTopology(strip?D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		UINT stride = sizeof(VertexXyzRgba);
		UINT offset = 0;
		UINT Strides[1];
		UINT Offsets[1];
		Strides[0] = stride;
		Offsets[0] = 0;
		pContext->IASetVertexBuffers(	0,				// the first input slot for binding
										1,				// the number of buffers in the array
										&vertexBuffer,	// the array of vertex buffers
										&stride,		// array of stride values, one for each buffer
										&offset);		// array of 
		hr=ApplyPass(pContext,tech->GetPassByIndex(0));
		pContext->Draw(vertex_count,0);
		pContext->IASetPrimitiveTopology(previousTopology);
		pContext->IASetInputLayout( previousInputLayout );
		SAFE_RELEASE(previousInputLayout);
		SAFE_RELEASE(vertexBuffer);
		SAFE_RELEASE(m_pVtxDecl);
	}
}

void RenderPlatform::DrawCircle(crossplatform::DeviceContext &deviceContext,const float *dir,float rads,const float *colr,bool fill)
{
	ID3D11DeviceContext *pContext	=deviceContext.asD3D11DeviceContext();
	D3D11_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);
	pContext->IASetPrimitiveTopology(fill?D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	pContext->IASetInputLayout(NULL);
	{
		ID3DX11EffectGroup *g=m_pDebugEffect->asD3DX11Effect()->GetGroupByName("circle");
		ID3DX11EffectTechnique *tech=fill?g->GetTechniqueByName("filled"):g->GetTechniqueByName("outline");
		
		simul::math::Vector3 d(dir);
		d.Normalize();
		float Yaw=atan2(d.x,d.y);
		float Pitch=-asin(d.z);
		simul::math::Matrix4x4 world, tmp1, tmp2;
	
		simul::geometry::SimulOrientation ori;
		ori.Rotate(3.14159f-Yaw,simul::math::Vector3(0,0,1.f));
		ori.LocalRotate(3.14159f/2.f+Pitch,simul::math::Vector3(1.f,0,0));
		world	=ori.T4;
		//set up matrices
		math::Matrix4x4 view=deviceContext.viewStruct.view;
		view._41=0.f;
		view._42=0.f;
		view._43=0.f;
		simul::math::Multiply4x4(tmp1,world,view);
		simul::math::Multiply4x4(tmp2,tmp1,deviceContext.viewStruct.proj);
		camera::MakeWorldViewProjMatrix(tmp2,world,view,deviceContext.viewStruct.proj);
		m_pDebugEffect->SetMatrix("worldViewProj",&tmp2._11);
		m_pDebugEffect->SetParameter("radius",rads);
		m_pDebugEffect->SetVector("colour",colr);
		ApplyPass(pContext,tech->GetPassByIndex(0));
	}
	pContext->Draw(fill?64:32,0);
	pContext->IASetPrimitiveTopology(previousTopology);
}

void RenderPlatform::PrintAt3dPos(crossplatform::DeviceContext &deviceContext,const float *p,const char *text,const float* colr,int offsetx,int offsety,bool centred)
{
	ID3D11DeviceContext *pContext=(ID3D11DeviceContext *)deviceContext.asD3D11DeviceContext();
	unsigned int num_v=1;
	D3D11_VIEWPORT viewport;
	pContext->RSGetViewports(&num_v,&viewport);
	
	//simul::math::Matrix4x4 v(deviceContext.viewStruct.view);
/*	v._41=0;
	v._42=0;
	v._43=0;*/
	mat4 wvp;
	if(centred)
		camera::MakeCentredViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	else
	camera::MakeViewProjMatrix((float*)&wvp,deviceContext.viewStruct.view,deviceContext.viewStruct.proj);
	static bool tr=false;
	if(tr)
		wvp.transpose();
	vec4 clip_pos;
	clip_pos=wvp*vec4(p[0],p[1],p[2],1.0f);
	if(clip_pos.w<0)
		return;
	clip_pos.x/=clip_pos.w;
	clip_pos.y/=clip_pos.w;
	static bool ml=true;
	vec2 pos;
	pos.x=(1.0f+clip_pos.x)/2.0f;
	if(mirrorY)
		pos.y=(1.0f+clip_pos.y)/2.0f;
	else
		pos.y=(1.0f-clip_pos.y)/2.0f;
	if(ml)
	{
		pos.x*=viewport.Width;
		pos.y*=viewport.Height;
	}

	Print(deviceContext,(int)pos.x+offsetx,(int)pos.y+offsety,text,colr,NULL);
}

void RenderPlatform::DrawCube(crossplatform::DeviceContext &deviceContext)
{
	UINT stride = sizeof(vec3);
	UINT offset = 0;
    UINT Strides[1];
    UINT Offsets[1];
    Strides[0] = 0;
    Offsets[0] = 0;
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	ID3D11InputLayout* previousInputLayout;
	pContext->IAGetInputLayout( &previousInputLayout );

	pContext->IASetVertexBuffers(	0,					// the first input slot for binding
									1,					// the number of buffers in the array
									&m_pVertexBuffer,	// the array of vertex buffers
									&stride,			// array of stride values, one for each buffer
									&offset );			// array of offset values, one for each buffer

	// Set the input layout
	pContext->IASetInputLayout(m_pCubemapVtxDecl);

	D3D_PRIMITIVE_TOPOLOGY previousTopology;
	pContext->IAGetPrimitiveTopology(&previousTopology);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pContext->Draw(36,0);

	pContext->IASetPrimitiveTopology(previousTopology);
	pContext->IASetInputLayout( previousInputLayout );
	SAFE_RELEASE(previousInputLayout);
}

void RenderPlatform::DrawCubemap(crossplatform::DeviceContext &deviceContext,crossplatform::Texture *cubemap,float offsetx,float offsety,float exposure,float gamma)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();
	unsigned int num_v=0;
	D3D11_VIEWPORT								m_OldViewports[4];
	pContext->RSGetViewports(&num_v,NULL);
	if(num_v<=4)
		pContext->RSGetViewports(&num_v,m_OldViewports);
	D3D11_VIEWPORT viewport;
		// Setup the viewport for rendering.
	viewport.Width		=m_OldViewports[0].Width*0.3f;
	viewport.Height		=m_OldViewports[0].Height*0.3f;
	viewport.MinDepth	=0.0f;
	viewport.MaxDepth	=1.0f;
	viewport.TopLeftX	=0.5f*(1.f+offsetx)*m_OldViewports[0].Width-viewport.Width/2;
	viewport.TopLeftY	=0.5f*(1.f-offsety)*m_OldViewports[0].Height-viewport.Height/2;
	pContext->RSSetViewports(1,&viewport);
	
	math::Matrix4x4 view=deviceContext.viewStruct.view;
	math::Matrix4x4 proj=camera::Camera::MakeProjectionMatrix(1.f,(float)viewport.Height/(float)viewport.Width,1.f,100.f);
	// Create the viewport.
	math::Matrix4x4 wvp,world;
	world.Identity();
	float tan_x=1.0f/proj(0, 0);
	float tan_y=1.0f/proj(1, 1);
	float size_req=tan_x*.5f;
	static float size=3.f;
	float d=2.0f*size/size_req;
	simul::math::Vector3 offs0(0,0,-d);
	view._41=0;
	view._42=0;
	view._43=0;
	simul::math::Vector3 offs;
	Multiply3(offs,view,offs0);
	world._41=offs.x;
	world._42=offs.y;
	world._43=offs.z;
	camera::MakeWorldViewProjMatrix(wvp,world,view,proj);
	simul::dx11::setMatrix(m_pDebugEffect->asD3DX11Effect(),"worldViewProj",&wvp._11);
	//ID3DX11EffectTechnique*			tech		=m_pDebugEffect->GetTechniqueByName("draw_cubemap");
	ID3DX11EffectTechnique*				tech		=m_pDebugEffect->asD3DX11Effect()->GetTechniqueByName("draw_cubemap_sphere");
	ID3D1xEffectShaderResourceVariable*	cubeTexture	=m_pDebugEffect->asD3DX11Effect()->GetVariableByName("cubeTexture")->AsShaderResource();
	cubeTexture->SetResource(cubemap->AsD3D11ShaderResourceView());
	HRESULT hr=ApplyPass(pContext,tech->GetPassByIndex(0));
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"latitudes",16);
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"longitudes",32);
	static float rr=6.f;
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"radius",rr);
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"exposure",exposure);
	simul::dx11::setParameter(m_pDebugEffect->asD3DX11Effect(),"gamma",gamma);
	UtilityRenderer::DrawSphere(deviceContext,16,32);
	pContext->RSSetViewports(num_v,m_OldViewports);
}
