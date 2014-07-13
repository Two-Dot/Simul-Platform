#define NOMINMAX
#include "OceanRenderer.h"
#include "CreateEffectDX1x.h"
#include "Simul/Sky/Float4.h"
#include "Simul/Sky/SkyInterface.h"
#ifndef SIMUL_WIN8_SDK
#include <D3DX11tex.h>
#endif
#include "CompileShaderDX1x.h"
#include "Simul/Platform/CrossPlatform/RenderPlatform.h"
#include "Simul/Platform/CrossPlatform/DeviceContext.h"
#include "Simul/Platform/DirectX11/CreateEffectDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Math/Matrix4x4.h"
#pragma warning(disable:4995)
#include <vector>
#include <string>
#include <map>
#include <assert.h>
#include "D3dx11effect.h"
using namespace std;
using namespace simul;
using namespace dx11;

#define FRESNEL_TEX_SIZE			256
#define PERLIN_TEX_SIZE				64
#define SAFE_DELETE_ARRAY(c) delete [] c;c=NULL;

struct ocean_vertex
{
	float index_x;
	float index_y;
};

// Mesh properties:

// Shading properties:
// Two colors for waterbody and sky color
D3DXVECTOR3 g_SkyColor= D3DXVECTOR3(0.38f, 0.45f, 0.56f);
D3DXVECTOR3 g_WaterbodyColor = D3DXVECTOR3(0.12f, 0.14f, 0.17f);
// Blending term for sky cubemap
float g_SkyBlending = 16.0f;

// Perlin wave parameters
float g_PerlinSize = 1.0f;
float g_PerlinSpeed = 0.06f;
D3DXVECTOR3 g_PerlinAmplitude = D3DXVECTOR3(35, 42, 57);
D3DXVECTOR3 g_PerlinGradient = D3DXVECTOR3(1.4f, 1.6f, 2.2f);
D3DXVECTOR3 g_PerlinOctave = D3DXVECTOR3(1.12f, 0.59f, 0.23f);

D3DXVECTOR3 g_BendParam = D3DXVECTOR3(0.1f, -0.4f, 0.2f);

// Sunspot parameters
D3DXVECTOR3 g_SunDir = D3DXVECTOR3(0.936016f, -0.343206f, 0.0780013f);
D3DXVECTOR3 g_SunColor = D3DXVECTOR3(1.0f, 1.0f, 0.6f);
float g_Shineness = 400.0f;


// Constant buffer
struct Const_Per_Call
{
	D3DXMATRIX	g_matLocal;
	D3DXMATRIX	g_matWorldViewProj;
	D3DXMATRIX	g_matWorld;
	math::float2	g_UVBase;
	math::float2	g_PerlinMovement;
	vec3			g_LocalEye;
	// Atmospherics
	float		hazeEccentricity;
	vec3			lightDir;
	vec4			mieRayleighRatio;
};

struct Const_Shading
{
	// The color of bottomless water body
	D3DXVECTOR3		g_WaterbodyColor;
	// The strength, direction and color of sun streak
	float			g_Shineness;
	D3DXVECTOR3		g_SunDir;
	float			unused1;
	D3DXVECTOR3		g_SunColor;
	float			unused2;
	// The parameter is used for fixing an artifact
	D3DXVECTOR3		g_BendParam;
	// Perlin noise for distant wave crest
	float			g_PerlinSize;
	D3DXVECTOR3		g_PerlinAmplitude;
	float			unused3;
	D3DXVECTOR3		g_PerlinOctave;
	float			unused4;
	D3DXVECTOR3		g_PerlinGradient;
	// Constants for calculating texcoord from position
	float			g_TexelLength_x2;
	float			g_UVScale;
	float			g_UVOffset;
};


OceanRenderer::OceanRenderer(simul::terrain::SeaKeyframer *s)
	:simul::terrain::BaseSeaRenderer(s)
	,oceanSimulator(NULL)
	,m_pd3dDevice(NULL)
	,effect(NULL)
	,g_pPerCallCB(NULL)
	,g_pPerFrameCB(NULL)
	,g_pShadingCB(NULL)
	// D3D11 buffers and layout
	,g_pMeshVB(NULL)
	,g_pMeshIB(NULL)
	,g_pMeshLayout(NULL)
	// Color look up 1D texture
	,g_pFresnelMap(NULL)
	,g_pSRV_Fresnel(NULL)
	// Distant perlin wave
	,g_pSRV_Perlin(NULL)
	// Environment maps
	,cubemapTexture(NULL)
	,skyLossTexture_SRV(NULL)
	,skyInscatterTexture_SRV(NULL)
{
}

OceanRenderer::~OceanRenderer()
{
	InvalidateDeviceObjects();
}


void OceanRenderer::Update(float dt)
{
	// Update simulation
	app_time += (double)dt;
}

void OceanRenderer::RecompileShaders()
{
	SAFE_DELETE(effect);
	if(!m_pd3dDevice)
		return;
	std::map<std::string,std::string> defines;
	defines["REVERSE_DEPTH"]=ReverseDepth?"1":"0";
	defines["FX"]="1";
	effect=renderPlatform->CreateEffect("ocean.fx",defines);

	// Input layout
	D3D11_INPUT_ELEMENT_DESC mesh_layout_desc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};
	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("ocean");
	if(tech)
	{
		D3DX11_PASS_DESC PassDesc;
		tech->asD3DX11EffectTechnique()->GetPassByIndex(0)->GetDesc(&PassDesc);
		SAFE_RELEASE(g_pMeshLayout);
		m_pd3dDevice->CreateInputLayout(mesh_layout_desc,1, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &g_pMeshLayout);
	}
	if(oceanSimulator)
		oceanSimulator->RecompileShaders();
	shadingConstants		.LinkToEffect(effect,"cbShading");
	changePerCallConstants	.LinkToEffect(effect,"cbChangePerCall");
}

void OceanRenderer::RestoreDeviceObjects(crossplatform::RenderPlatform *r)
{
	InvalidateDeviceObjects();
	BaseSeaRenderer::RestoreDeviceObjects(r);
	if(!renderPlatform)
		return;
	m_pd3dDevice=renderPlatform->AsD3D11Device();
	oceanSimulator=new OceanSimulator(seaKeyframer);
	oceanSimulator->RestoreDeviceObjects(renderPlatform);
	
	// Update the simulation for the first time.
	crossplatform::DeviceContext deviceContext;
	deviceContext.renderPlatform=renderPlatform;
	ID3D11DeviceContext *pImmediateContext;
	m_pd3dDevice->GetImmediateContext(&pImmediateContext);
	deviceContext.platform_context=pImmediateContext;
	oceanSimulator->updateDisplacementMap(deviceContext,0);
	// D3D buffers
	createSurfaceMesh();
	createFresnelMap();
	loadTextures();

	shadingConstants		.RestoreDeviceObjects(renderPlatform);
	changePerCallConstants	.RestoreDeviceObjects(renderPlatform);
	RecompileShaders();
	// Constants
	D3D11_BUFFER_DESC cb_desc;
	cb_desc.Usage = D3D11_USAGE_DYNAMIC;
	cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cb_desc.MiscFlags = 0;    
	cb_desc.ByteWidth = PAD16(sizeof(Const_Per_Call));
	cb_desc.StructureByteStride = 0;
	m_pd3dDevice->CreateBuffer(&cb_desc, NULL, &g_pPerCallCB);
	assert(g_pPerCallCB);

	Const_Shading shading_data;
	// Grid side length * 2
	shading_data.g_TexelLength_x2 = seaKeyframer->patch_length / seaKeyframer->dmap_dim * 2;;
	// Color
	shading_data.g_WaterbodyColor = g_WaterbodyColor;
	// Texcoord
	shading_data.g_UVScale = 1.0f / seaKeyframer->patch_length;
	shading_data.g_UVOffset = 0.5f / seaKeyframer->dmap_dim;
	// Perlin
	shading_data.g_PerlinSize = g_PerlinSize;
	shading_data.g_PerlinAmplitude = g_PerlinAmplitude;
	shading_data.g_PerlinGradient = g_PerlinGradient;
	shading_data.g_PerlinOctave = g_PerlinOctave;
	// Multiple reflection workaround
	shading_data.g_BendParam = g_BendParam;
	// Sun streaks
	shading_data.g_SunColor = g_SunColor;
	shading_data.g_SunDir = g_SunDir;
	shading_data.g_Shineness = g_Shineness;

	D3D11_SUBRESOURCE_DATA cb_init_data;
	cb_init_data.pSysMem = &shading_data;
	cb_init_data.SysMemPitch = 0;
	cb_init_data.SysMemSlicePitch = 0;

	cb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	cb_desc.CPUAccessFlags = 0;
	cb_desc.ByteWidth = PAD16(sizeof(Const_Shading));
	cb_desc.StructureByteStride = 0;
	m_pd3dDevice->CreateBuffer(&cb_desc, &cb_init_data, &g_pShadingCB);
	assert(g_pShadingCB);

	D3D11_BLEND_DESC blend_desc;
	memset(&blend_desc, 0, sizeof(D3D11_BLEND_DESC));
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE;
	blend_desc.RenderTarget[0].BlendEnable = TRUE;
	blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	
	blend_desc.RenderTarget[0].BlendEnable = FALSE;
	blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	SAFE_RELEASE(pImmediateContext);
}

void OceanRenderer::SetCubemapTexture(crossplatform::Texture *c)
{
	cubemapTexture=c;
}

void OceanRenderer::SetLossAndInscatterTextures(crossplatform::Texture *l,crossplatform::Texture *i,crossplatform::Texture *s)
{
	skyLossTexture_SRV		=l->AsD3D11ShaderResourceView();
	skyInscatterTexture_SRV	=i->AsD3D11ShaderResourceView();
	skylightTexture_SRV		=s->AsD3D11ShaderResourceView();
}

void OceanRenderer::InvalidateDeviceObjects()
{
	if(oceanSimulator)
	{
		oceanSimulator->InvalidateDeviceObjects();
    // Ocean object
		delete (oceanSimulator);
		oceanSimulator=NULL;
	}
	SAFE_RELEASE(g_pMeshIB);
	SAFE_RELEASE(g_pMeshVB);
	SAFE_RELEASE(g_pMeshLayout);
	
	SAFE_DELETE(effect);

	SAFE_RELEASE(g_pFresnelMap);
	SAFE_RELEASE(g_pSRV_Fresnel);
	SAFE_RELEASE(g_pSRV_Perlin);

	SAFE_RELEASE(g_pPerCallCB);
	SAFE_RELEASE(g_pPerFrameCB);
	SAFE_RELEASE(g_pShadingCB);
	shadingConstants		.InvalidateDeviceObjects();
	changePerCallConstants	.InvalidateDeviceObjects();

	g_render_list.clear();
}

#define MESH_INDEX_2D(x, y)	(((y) + vert_rect.bottom) * (g_MeshDim + 1) + (x) + vert_rect.left)


void OceanRenderer::createSurfaceMesh()
{
	// --------------------------------- Vertex Buffer -------------------------------
	int num_verts = (g_MeshDim + 1) * (g_MeshDim + 1);
	ocean_vertex* pV = new ocean_vertex[num_verts];
	assert(pV);

	int i, j;
	for (i = 0; i <= g_MeshDim; i++)
	{
		for (j = 0; j <= g_MeshDim; j++)
		{
			pV[i * (g_MeshDim + 1) + j].index_x = (float)j;
			pV[i * (g_MeshDim + 1) + j].index_y = (float)i;
		}
	}

	D3D11_BUFFER_DESC vb_desc;
	vb_desc.ByteWidth = num_verts * sizeof(ocean_vertex);
	vb_desc.Usage = D3D11_USAGE_IMMUTABLE;
	vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vb_desc.CPUAccessFlags = 0;
	vb_desc.MiscFlags = 0;
	vb_desc.StructureByteStride = sizeof(ocean_vertex);

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = pV;
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;
	
	SAFE_RELEASE(g_pMeshVB);
	m_pd3dDevice->CreateBuffer(&vb_desc, &init_data, &g_pMeshVB);

	SAFE_DELETE_ARRAY(pV);

	// --------------------------------- Index Buffer -------------------------------
	// The index numbers for all mesh LODs (up to 256x256)
	const int index_size_lookup[] = {0, 0, 4284, 18828, 69444, 254412, 956916, 3689820, 14464836};

	memset(&g_mesh_patterns[0][0][0][0][0], 0, sizeof(g_mesh_patterns));

	g_Lods = 0;
	for (i = g_MeshDim; i > 1; i >>= 1)
		g_Lods ++;

	// Generate patch meshes. Each patch contains two parts: the inner mesh which is a regular
	// grids in a triangle strip. The boundary mesh is constructed w.r.t. the edge degrees to
	// meet water-tight requirement.
	DWORD* index_array = new DWORD[index_size_lookup[g_Lods]];
	assert(index_array);
	EnumeratePatterns(index_array);
	D3D11_BUFFER_DESC ib_desc;
	ib_desc.ByteWidth = index_size_lookup[g_Lods] * sizeof(DWORD);
	ib_desc.Usage = D3D11_USAGE_IMMUTABLE;
	ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ib_desc.CPUAccessFlags = 0;
	ib_desc.MiscFlags = 0;
	ib_desc.StructureByteStride = sizeof(DWORD);

	init_data.pSysMem = index_array;

	SAFE_RELEASE(g_pMeshIB);
	m_pd3dDevice->CreateBuffer(&ib_desc, &init_data, &g_pMeshIB);

	SAFE_DELETE_ARRAY(index_array);
}

void OceanRenderer::EnumeratePatterns(unsigned long* index_array)
{
	int offset = 0;
	int level_size = g_MeshDim;
	// Enumerate patterns
	for (int level = 0; level <= g_Lods - 2; level ++)
	{
		int left_degree = level_size;
		for (int left_type = 0; left_type < 3; left_type ++)
		{
			int right_degree = level_size;
			for (int right_type = 0; right_type < 3; right_type ++)
			{
				int bottom_degree = level_size;
				for (int bottom_type = 0; bottom_type < 3; bottom_type ++)
				{
					int top_degree = level_size;
					for (int top_type = 0; top_type < 3; top_type ++)
					{
						QuadRenderParam* pattern = &g_mesh_patterns[level][left_type][right_type][bottom_type][top_type];
						// Inner mesh (triangle strip)
						Rect inner_rect;
						inner_rect.left   = (left_degree   == level_size) ? 0 : 1;
						inner_rect.right  = (right_degree  == level_size) ? level_size : level_size - 1;
						inner_rect.bottom = (bottom_degree == level_size) ? 0 : 1;
						inner_rect.top    = (top_degree    == level_size) ? level_size : level_size - 1;
						int num_new_indices = generateInnerMesh(inner_rect, index_array + offset);
						pattern->inner_start_index = offset;
						pattern->num_inner_verts = (level_size + 1) * (level_size + 1);
						pattern->num_inner_faces = num_new_indices - 2;
						offset += num_new_indices;
						// Boundary mesh (triangle list)
						int l_degree = (left_degree   == level_size) ? 0 : left_degree;
						int r_degree = (right_degree  == level_size) ? 0 : right_degree;
						int b_degree = (bottom_degree == level_size) ? 0 : bottom_degree;
						int t_degree = (top_degree    == level_size) ? 0 : top_degree;
						Rect outer_rect = {0, level_size, level_size, 0};
						num_new_indices = generateBoundaryMesh(l_degree, r_degree, b_degree, t_degree, outer_rect, index_array + offset);
						pattern->boundary_start_index = offset;
						pattern->num_boundary_verts = (level_size + 1) * (level_size + 1);
						pattern->num_boundary_faces = num_new_indices / 3;
						offset += num_new_indices;

						top_degree /= 2;
					}
					bottom_degree /= 2;
				}
				right_degree /= 2;
			}
			left_degree /= 2;
		}
		level_size /= 2;
	}
//	assert(offset == index_size_lookup[g_Lods]);
}

void OceanRenderer::createFresnelMap()
{
	DWORD* buffer = new DWORD[FRESNEL_TEX_SIZE];
	for (int i = 0; i < FRESNEL_TEX_SIZE; i++)
	{
		float cos_a = i / (FLOAT)FRESNEL_TEX_SIZE;
		// Using water's refraction index 1.33
		DWORD fresnel = (DWORD)(D3DXFresnelTerm(cos_a, 1.33f) * 255);
		DWORD sky_blend = (DWORD)(powf(1 / (1 + cos_a), g_SkyBlending) * 255);
		buffer[i] = (sky_blend << 8) | fresnel;
	}
	D3D11_TEXTURE1D_DESC tex_desc;
	tex_desc.Width = FRESNEL_TEX_SIZE;
	tex_desc.MipLevels = 1;
	tex_desc.ArraySize = 1;
	tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
	tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	tex_desc.CPUAccessFlags = 0;
	tex_desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA init_data;
	init_data.pSysMem = buffer;
	init_data.SysMemPitch = 0;
	init_data.SysMemSlicePitch = 0;

	m_pd3dDevice->CreateTexture1D(&tex_desc, &init_data, &g_pFresnelMap);
	assert(g_pFresnelMap);

	SAFE_DELETE_ARRAY(buffer);

	// Create shader resource
	D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
	srv_desc.Texture1D.MipLevels = 1;
	srv_desc.Texture1D.MostDetailedMip = 0;

	m_pd3dDevice->CreateShaderResourceView(g_pFresnelMap, &srv_desc, &g_pSRV_Fresnel);
	assert(g_pSRV_Fresnel);
}

void OceanRenderer::loadTextures()
{
//    WCHAR strPath[MAX_PATH];
	SAFE_RELEASE(g_pSRV_Perlin);
    //swprintf_s(strPath, MAX_PATH, L"../../Platform/DirectX11/Textures/perlin_noise.dds");
	g_pSRV_Perlin=simul::dx11::LoadTexture(m_pd3dDevice,"perlin_noise.dds");
	//D3DX11CreateShaderResourceViewFromFile(m_pd3dDevice, strPath, NULL, NULL, &g_pSRV_Perlin, NULL);
	//assert(g_pSRV_Perlin);
}

void OceanRenderer::SetMatrices(const float *v,const float *p)
{
}

void OceanRenderer::Render(crossplatform::DeviceContext &deviceContext,float exposure)
{
	if(skyInterface)
		app_time=skyInterface->GetTime();
	oceanSimulator->updateDisplacementMap(deviceContext,(float)app_time);

	ID3D11DeviceContext*	pContext		=deviceContext.asD3D11DeviceContext();
	crossplatform::Texture* displacement_map=oceanSimulator->getDisplacementMap();
	crossplatform::Texture* gradient_map	=oceanSimulator->getGradientMap();

	// Build rendering list
	g_render_list.clear();
	float ocean_extent =seaKeyframer->patch_length * (1 << g_FurthestCover);
	QuadNode root_node ={math::float2(-ocean_extent * 0.5f, -ocean_extent * 0.5f), ocean_extent, 0, {-1,-1,-1,-1}};
	buildNodeList(root_node,seaKeyframer->patch_length,(const float *)&deviceContext.viewStruct.view,(const float *)&deviceContext.viewStruct.proj);

	// Matrices
	D3DXMATRIX matView = deviceContext.viewStruct.view;
	D3DXMATRIX matProj = deviceContext.viewStruct.proj;

	// VS & PS
	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("ocean");

	//effect->Apply(deviceContext,tech,0);

	// Textures
	effect->SetTexture(deviceContext	,"g_texDisplacement"	,displacement_map);
	effect->SetTexture(deviceContext	,"g_texGradient"		,gradient_map);
	effect->SetTexture(deviceContext	,"g_texReflectCube"		,cubemapTexture);
	setTexture(effect->asD3DX11Effect()	,"g_texPerlin"			,g_pSRV_Perlin);
	setTexture(effect->asD3DX11Effect()	,"g_texFresnel"			,g_pSRV_Fresnel);
	setTexture(effect->asD3DX11Effect()	,"g_skyLossTexture"		,skyLossTexture_SRV);
	setTexture(effect->asD3DX11Effect()	,"g_skyInscatterTexture",skyInscatterTexture_SRV);

	// IA setup
	pContext->IASetIndexBuffer(g_pMeshIB, DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* vbs[1]	={g_pMeshVB};
	UINT strides[1]			={sizeof(ocean_vertex)};
	UINT offsets[1]			={0};
	pContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);
	pContext->IASetInputLayout(g_pMeshLayout);

	// Constants
	ID3D11Buffer* cbs[1] = {g_pShadingCB};
	setConstantBuffer(effect->asD3DX11Effect(),"cbShading"	,g_pShadingCB);
	effect->Apply(deviceContext,tech,0);
	static int ct=1;
	// We assume the center of the ocean surface at (0, 0, 0).
	for (int i = 0; i <min(ct,(int)g_render_list.size()); i++)
	{
		QuadNode& node = g_render_list[i];
		if (!node.isLeaf())
			continue;
		// Check adjacent patches and select mesh pattern
		QuadRenderParam& render_param = selectMeshPattern(node,seaKeyframer->patch_length);
		// Find the right LOD to render
		int level_size = g_MeshDim;
		for (int lod = 0; lod < node.lod; lod++)
			level_size >>= 1;
		// Matrices and constants
		Const_Per_Call call_consts;
		// Expand of the local coordinate to world space patch size
		D3DXMATRIX matScale;
		D3DXMatrixScaling(&matScale, node.length / level_size, node.length / level_size, 0);
		D3DXMatrixTranspose(&call_consts.g_matLocal, &matScale);
		// WVP matrix
		D3DXMATRIX matWorld;
		D3DXMatrixTranslation(&matWorld, node.bottom_left.x, node.bottom_left.y, 0);
		D3DXMatrixTranspose(&call_consts.g_matWorld, &matWorld);
		D3DXMATRIX matWVP =D3DXMatrixMultiply( matWorld,D3DXMatrixMultiply(matView,matProj));
		D3DXMatrixTranspose(&call_consts.g_matWorldViewProj, &matWVP);
		// Texcoord for perlin noise
		math::float2 uv_base = node.bottom_left / seaKeyframer->patch_length * g_PerlinSize;
		call_consts.g_UVBase = uv_base;
		// Constant g_PerlinSpeed need to be adjusted mannually
		math::float2 perlin_move =math::float2(seaKeyframer->wind_dir)*(-(float)app_time)* g_PerlinSpeed;
		call_consts.g_PerlinMovement = perlin_move;
		// Eye point
		D3DXMATRIX matInvWV = D3DXMatrixMultiply(matWorld ,matView);
		D3DXMatrixInverse(&matInvWV, NULL, &matInvWV);
		D3DXVECTOR3 vLocalEye(0, 0, 0);
		D3DXVec3TransformCoord(&vLocalEye, &vLocalEye, &matInvWV);
		call_consts.g_LocalEye = (const float*)&vLocalEye;

		// Atmospherics
		if(skyInterface)
		{
			call_consts.hazeEccentricity=skyInterface->GetMieEccentricity();
			call_consts.lightDir=vec3((const float *)(skyInterface->GetDirectionToLight(0.f)));
			call_consts.mieRayleighRatio=vec4((const float *)(skyInterface->GetMieRayleighRatio()));
		}
		else
		{
			call_consts.hazeEccentricity=0.85f;
			call_consts.lightDir=vec3(0,0,1.f);
			call_consts.mieRayleighRatio=vec4(0,0,0,0);
		}

		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mapped_res;            
		pContext->Map(g_pPerCallCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		assert(mapped_res.pData);
		*(Const_Per_Call*)mapped_res.pData = call_consts;
		pContext->Unmap(g_pPerCallCB, 0);

		cbs[0]=g_pPerCallCB;
		//pContext->VSSetConstantBuffers(4, 1, cbs);
		//pContext->PSSetConstantBuffers(4, 1, cbs);
		setConstantBuffer(effect->asD3DX11Effect(),"cbChangePerCall"	,g_pPerCallCB);
		effect->Reapply(deviceContext);
		// Perform draw call
		if (render_param.num_inner_faces > 0)
		{
			// Inner mesh of the patch
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pContext->DrawIndexed(render_param.num_inner_faces + 2, render_param.inner_start_index, 0);
		}

		if (render_param.num_boundary_faces > 0)
		{
			// Boundary mesh of the patch
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pContext->DrawIndexed(render_param.num_boundary_faces * 3, render_param.boundary_start_index, 0);
		}
	}
	effect->UnbindTextures(deviceContext);
	effect->Unapply(deviceContext);
}

void OceanRenderer::RenderWireframe(crossplatform::DeviceContext &deviceContext)
{
	ID3D11DeviceContext*	pContext			=deviceContext.asD3D11DeviceContext();
	crossplatform::Texture* displacement_map	=oceanSimulator->getDisplacementMap();
	// Build rendering list
	g_render_list.clear();
	float ocean_extent = seaKeyframer->patch_length * (1 << g_FurthestCover);
	QuadNode root_node = {math::float2(-ocean_extent * 0.5f, -ocean_extent * 0.5f), ocean_extent, 0, {-1,-1,-1,-1}};
	buildNodeList(root_node,seaKeyframer->patch_length,(const float*)&deviceContext.viewStruct.view,(const float*)&deviceContext.viewStruct.proj);

	// Matrices
	D3DXMATRIX matView = deviceContext.viewStruct.view;
	D3DXMATRIX matProj = deviceContext.viewStruct.proj;

	// VS & PS
	crossplatform::EffectTechnique *tech=effect->GetTechniqueByName("wireframe");
	
	effect->SetTexture(deviceContext,"g_texDisplacement",displacement_map);
	setTexture(effect->asD3DX11Effect(),"g_texPerlin",g_pSRV_Perlin);

	// IA setup
	pContext->IASetIndexBuffer(g_pMeshIB, DXGI_FORMAT_R32_UINT, 0);

	ID3D11Buffer* vbs[1] = {g_pMeshVB};
	UINT strides[1] = {sizeof(ocean_vertex)};
	UINT offsets[1] = {0};
	pContext->IASetVertexBuffers(0, 1, &vbs[0], &strides[0], &offsets[0]);

	pContext->IASetInputLayout(g_pMeshLayout);

	// Constants
	ID3D11Buffer* cbs[1] = {g_pShadingCB};
	setConstantBuffer(effect->asD3DX11Effect(),"cbShading",g_pShadingCB);
	effect->Apply(deviceContext,tech,0);

	// We assume the center of the ocean surface is at (0, 0, 0).
	for (int i = 0; i < (int)g_render_list.size(); i++)
	{
		QuadNode& node = g_render_list[i];
		
		if (!node.isLeaf())
			continue;

		// Check adjacent patches and select mesh pattern
		QuadRenderParam& render_param = selectMeshPattern(node,seaKeyframer->patch_length);

		// Find the right LOD to render
		int level_size = g_MeshDim;
		for (int lod = 0; lod < node.lod; lod++)
			level_size >>= 1;

		// Matrices and constants
		Const_Per_Call call_consts;

		// Expand of the local coordinate to world space patch size
		D3DXMATRIX matScale;
		D3DXMatrixScaling(&matScale, node.length / level_size, node.length / level_size, 0);
		D3DXMatrixTranspose(&call_consts.g_matLocal, &matScale);

		// WVP matrix
		D3DXMATRIX matWorld;
		D3DXMatrixTranslation(&matWorld, node.bottom_left.x, node.bottom_left.y, 0);
		D3DXMatrixTranspose(&call_consts.g_matWorld, &matWorld);
		D3DXMATRIX matWVP = D3DXMatrixMultiply(matWorld,D3DXMatrixMultiply(matView ,matProj));
		D3DXMatrixTranspose(&call_consts.g_matWorldViewProj, &matWVP);

		// Texcoord for perlin noise
		math::float2 uv_base = node.bottom_left / seaKeyframer->patch_length * g_PerlinSize;
		call_consts.g_UVBase = uv_base;

		// Constant g_PerlinSpeed need to be adjusted mannually
		math::float2 perlin_move =math::float2(seaKeyframer->wind_dir) *(-(float)app_time)* g_PerlinSpeed;
		call_consts.g_PerlinMovement = perlin_move;

		// Eye point
		D3DXMATRIX matInvWV = D3DXMatrixMultiply(matWorld ,matView);
		D3DXMatrixInverse(&matInvWV, NULL, &matInvWV);
		D3DXVECTOR3 vLocalEye(0, 0, 0);
		D3DXVec3TransformCoord(&vLocalEye, &vLocalEye, &matInvWV);
		call_consts.g_LocalEye =(const float*)&vLocalEye;

		// Update constant buffer
		D3D11_MAPPED_SUBRESOURCE mapped_res;            
		pContext->Map(g_pPerCallCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_res);
		assert(mapped_res.pData);
		*(Const_Per_Call*)mapped_res.pData = call_consts;
		pContext->Unmap(g_pPerCallCB, 0);

		cbs[0] = g_pPerCallCB;
		setConstantBuffer(effect->asD3DX11Effect(),"cbChangePerCall",g_pPerCallCB);
		effect->Reapply(deviceContext);
		// Perform draw call
		if (render_param.num_inner_faces > 0)
		{
			// Inner mesh of the patch
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			pContext->DrawIndexed(render_param.num_inner_faces + 2, render_param.inner_start_index, 0);
		}

		if (render_param.num_boundary_faces > 0)
		{
			// Boundary mesh of the patch
			pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			pContext->DrawIndexed(render_param.num_boundary_faces * 3, render_param.boundary_start_index, 0);
		}
	}
	effect->UnbindTextures(deviceContext);
	effect->Unapply(deviceContext);
}

void OceanRenderer::RenderTextures(crossplatform::DeviceContext &deviceContext,int width,int height)
{
	ID3D11DeviceContext *pContext=deviceContext.asD3D11DeviceContext();

	HRESULT hr=S_OK;
	static int u=8;
	int w=(width-8)/u;
	if(w>height/3)
		w=height/3;
	UtilityRenderer::SetScreenSize(width,height);
	int x=8;
	int y=height-w;
	simul::dx11::setTexture(effect->asD3DX11Effect(),"showTexture",g_pSRV_Perlin);
	simul::dx11::setParameter(effect->asD3DX11Effect(),"showMultiplier",10.f);
	//renderPlatform->DrawTexture(deviceContext,x,y,w,w,g_pSRV_Perlin);
	x+=w+2;
	static float spectrum_multiplier=10000.0f;
	simul::dx11::setTexture(effect->asD3DX11Effect(),"g_InputDxyz",oceanSimulator->GetSpectrum());
	simul::dx11::setParameter(effect->asD3DX11Effect(),"showMultiplier",spectrum_multiplier);
	UtilityRenderer::DrawQuad2(deviceContext,x,y,w,w,effect->asD3DX11Effect(),effect->asD3DX11Effect()->GetTechniqueByName("show_structured_buffer"));
	x+=w+2;
	//simul::dx11::setTexture(effect,"showTexture",oceanSimulator->getDisplacementMap());
	//simul::dx11::setParameter(effect,"showMultiplier",0.01f);
	renderPlatform->DrawTexture(deviceContext,x,y,w,w,oceanSimulator->getDisplacementMap(),0.01f);
	x+=w+2;
	static float gradient_multiplier=1000.0f;
	//simul::dx11::setTexture(effect,"showTexture",oceanSimulator->getGradientMap());
	//simul::dx11::setParameter(effect,"showMultiplier",gradient_multiplier);
	renderPlatform->DrawTexture(deviceContext,x,y,w,w,oceanSimulator->getGradientMap(),gradient_multiplier);
	x+=w+2;
//	simul::dx11::setParameter(effect,"showTexture",g_pSRV_Fresnel);
	//UtilityRenderer::DrawQuad2(pContext,x,y,w,w,effect,effect->GetTechniqueByName("show_texture"));
//	x+=w+2;
	// structured buffer
	/*simul::dx11::setTexture(effect,"g_InputDxyz",oceanSimulator->GetFftInput());
	UtilityRenderer::DrawQuad2(pContext,x,y,w,w,effect,effect->GetTechniqueByName("show_structured_buffer"));
	x+=w+2;*/
	simul::dx11::setTexture(effect->asD3DX11Effect(),"g_InputDxyz",oceanSimulator->GetFftOutput());
	simul::dx11::setParameter(effect->asD3DX11Effect(),"showMultiplier",0.01f);
	UtilityRenderer::DrawQuad2(deviceContext,x,y,w,w,effect->asD3DX11Effect(),effect->asD3DX11Effect()->GetTechniqueByName("show_structured_buffer"));

	simul::dx11::setTexture(effect->asD3DX11Effect(),"g_InputDxyz",NULL);
	simul::dx11::unbindTextures(effect->asD3DX11Effect());
	effect->asD3DX11Effect()->GetTechniqueByName("show_structured_buffer")->GetPassByIndex(0)->Apply(0,pContext);
}