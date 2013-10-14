// Copyright (c) 2007-2013 Simul Software Ltd
// All Rights Reserved.
//
// This source code is supplied under the terms of a license agreement or
// nondisclosure agreement with Simul Software Ltd and may not 
// be copied or disclosed except in accordance with the terms of that 
// agreement.

// CreateEffect.cpp Create a DirectX .fx effect and report errors.

#include "CreateEffectDX1x.h"
#include "Simul/Base/StringToWString.h"
#include "Simul/Base/EnvironmentVariables.h"
#include "Simul/Base/RuntimeError.h"
#include "Simul/Base/DefaultFileLoader.h"
#include "Simul/Sky/Float4.h"
#include <tchar.h>
#include "CompileShaderDX1x.h"
#include "Simul/Platform/DirectX11/Utilities.h"
#include "Simul/Platform/DirectX11/CompileShaderDX1x.h"
#include <string>
static const DWORD default_effect_flags=0;

#include <vector>
#include <iostream>
#include <assert.h>
#include <fstream>
#include <d3dx9.h>
#include "MacrosDX1x.h"
#include <dxerr.h>

#pragma comment(lib,"d3dx9.lib")
#pragma comment(lib,"d3d9.lib")
#pragma comment(lib,"d3dx11.lib")
#pragma comment(lib,"Effects11.lib")
#pragma comment(lib,"dxerr.lib")
#pragma comment(lib,"dxguid.lib")
#pragma comment(lib,"d3dcompiler.lib")

// winmm.lib comctl32.lib
static bool pipe_compiler_output=false;

static ID3D1xDevice		*pd3dDevice		=NULL;
using namespace simul;
using namespace dx11;
using namespace base;
ShaderBuildMode shaderBuildMode=ALWAYS_BUILD;
static DefaultFileLoader fl;
static FileLoader *fileLoader=&fl;

HRESULT __stdcall ShaderIncludeHandler::Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileNameUtf8, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes)
{
	try
	{
		std::string finalPathUtf8;
		switch(IncludeType)
		{
		case D3D_INCLUDE_LOCAL:
			finalPathUtf8	=m_ShaderDirUtf8+"\\"+pFileNameUtf8;
			break;
		case D3D_INCLUDE_SYSTEM:
			finalPathUtf8	=m_SystemDirUtf8+"\\"+pFileNameUtf8;
			break;
		default:
			assert(0);
		}
		void *buf=NULL;
		unsigned fileSize=0;
		fileLoader->AcquireFileContents(buf,fileSize,finalPathUtf8.c_str(),false);
		*ppData = buf;
		*pBytes = (UINT)fileSize;
		if(!*ppData)
			return E_FAIL;
		return S_OK;
	}
	catch(std::exception& e)
	{
		std::cerr<<e.what()<<std::endl;
		return E_FAIL;
	}
}

HRESULT __stdcall ShaderIncludeHandler::Close(LPCVOID pData)
{
	fileLoader->ReleaseFileContents((void*)pData);
	return S_OK;
}

namespace simul
{
	namespace dx11
	{
		std::string *shaderPathUtf8;
		std::string *texture_path;
		void SetFileLoader(simul::base::FileLoader *l)
		{
			fileLoader=l;
		}
		void GetCameraPosVector(D3DXMATRIX &view,float *dcam_pos,float *view_dir,bool y_vertical)
		{
			D3DXMATRIX tmp1;
			D3DXMatrixInverse(&tmp1,NULL,&view);
			
			dcam_pos[0]=tmp1._41;
			dcam_pos[1]=tmp1._42;
			dcam_pos[2]=tmp1._43;
			if(view_dir)
			{
				if(y_vertical)
				{
					view_dir[0]=view._13;
					view_dir[1]=view._23;
					view_dir[2]=view._33;
				}
				else
				{
					view_dir[0]=-view._13;
					view_dir[1]=-view._23;
					view_dir[2]=-view._33;
				}
			}
		}

		const float *GetCameraPosVector(D3DXMATRIX &view,bool y_vertical)
		{
			static float cam_pos[4],view_dir[4];
			GetCameraPosVector(view,(float*)cam_pos,(float*)view_dir,y_vertical);
			return cam_pos;
		}
		void PipeCompilerOutput(bool p)
		{
			pipe_compiler_output=p;
		}
		void SetShaderBuildMode( ShaderBuildMode b)
		{
			shaderBuildMode=b;
		}
		void SetShaderPath(const char *path_utf8)
		{
			if(!path_utf8)
				delete shaderPathUtf8;
			else
			{
				if(!shaderPathUtf8)
					shaderPathUtf8=new std::string;
				*shaderPathUtf8=std::string(path_utf8)+"/";
			}
		}
		void SetTexturePath(const char *path)
		{
			if(!path)
				delete texture_path;
			else
			{
				if(!texture_path)
					texture_path=new std::string;
				*texture_path=std::string(path)+"/";
			}
		}
		void MakeWorldViewProjMatrix(D3DXMATRIX *wvp,D3DXMATRIX &world,D3DXMATRIX &view,D3DXMATRIX &proj)
		{
			D3DXMATRIX tmp1, tmp2;
			D3DXMatrixMultiply(&tmp1, &world,&view);
			D3DXMatrixMultiply(&tmp2, &tmp1,&proj);
			D3DXMatrixTranspose(wvp,&tmp2);
		}
		D3DXMATRIX ConvertReversedToRegularProjectionMatrix(const D3DXMATRIX &proj)
		{
			D3DXMATRIX p=proj;
			if(proj._43>0)
			{
				float zF=proj._43/proj._33;
				float zN=proj._43*zF/(zF+proj._43);
				p._33=-zF/(zF-zN);
				p._43=-zN*zF/(zF-zN);
			}
			return p;
		}
	}
}

ID3D1xShaderResourceView* simul::dx11::LoadTexture(ID3D11Device* pd3dDevice,const char *filename)
{
	ID3D11ShaderResourceView* tex=NULL;
	D3DX11_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory( &loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO) );
	loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	loadInfo.MipLevels=0;
	if(!texture_path)
		texture_path=new std::string("media/textures");
	std::wstring wstr=simul::base::Utf8ToWString(*texture_path+filename);
	HRESULT hr=D3DX11CreateShaderResourceViewFromFileW(
										pd3dDevice,
										wstr.c_str(),
										&loadInfo,
										NULL,
										&tex,
										&hr);
	return tex;
}


ID3D11Texture2D* simul::dx11::LoadStagingTexture(ID3D11Device* pd3dDevice,const char *filename)
{
	D3DX11_IMAGE_LOAD_INFO loadInfo;

    ZeroMemory(&loadInfo, sizeof(D3DX11_IMAGE_LOAD_INFO));
	//loadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	//loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	loadInfo.MipLevels=0;

    loadInfo.Width          = D3DX11_FROM_FILE;
    loadInfo.Height         = D3DX11_FROM_FILE;
     loadInfo.Depth          = D3DX11_FROM_FILE;
	loadInfo.Usage          = D3D11_USAGE_STAGING;
    loadInfo.Format         = (DXGI_FORMAT) D3DX11_FROM_FILE;
    loadInfo.CpuAccessFlags = D3D11_CPU_ACCESS_READ;
	loadInfo.FirstMipLevel  = D3DX11_FROM_FILE;
    loadInfo.MipLevels      = D3DX11_FROM_FILE;
    loadInfo.MiscFlags      = 0;
    loadInfo.MipFilter      = D3DX11_FROM_FILE;
    loadInfo.pSrcInfo       = NULL;
    loadInfo.Filter         = D3DX11_FILTER_NONE;

	ID3D11Texture2D *tex=NULL;
	if(!texture_path)
		texture_path=new std::string("media/textures");
	std::wstring wstr=simul::base::Utf8ToWString((*texture_path+filename).c_str());
	HRESULT hr=D3DX11CreateTextureFromFileW(pd3dDevice,wstr.c_str()
		,&loadInfo, NULL, ( ID3D11Resource** )&tex, &hr );
	if(hr!=S_OK)
	{
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateEffect", hr );
#endif
	}
	return tex;
}

ID3D1xTexture1D* simul::dx11::make1DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w
							,DXGI_FORMAT format
							,const float *src)
{
	ID3D1xTexture1D*	tex;
	D3D11_TEXTURE1D_DESC textureDesc=
	{
		w,
		1,
		1,
		format,
		D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		(UINT)(w*ByteSizeOfFormatElement(format)),
		(UINT)(w*ByteSizeOfFormatElement(format))
	};

	m_pd3dDevice->CreateTexture1D(&textureDesc,&init,&tex);
	return tex;
}

ID3D11Texture2D* simul::dx11::make2DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w,int h
							,DXGI_FORMAT format
							,const float *src)
{
	ID3D1xTexture2D*	tex;
	D3D11_TEXTURE2D_DESC textureDesc=
	{
		w,h,
		1,
		1,
		format,
		{1,0}
		,D3D11_USAGE_DYNAMIC,
		D3D11_BIND_SHADER_RESOURCE,
		D3D11_CPU_ACCESS_WRITE,
		0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		(UINT)w*ByteSizeOfFormatElement(format),
		(UINT)w*h*ByteSizeOfFormatElement(format)
	};
	m_pd3dDevice->CreateTexture2D(&textureDesc,&init,&tex);
	return tex;
}


ID3D1xTexture3D* simul::dx11::make3DTexture(
							ID3D1xDevice			*m_pd3dDevice
							,int w,int l,int d
							,DXGI_FORMAT format
							,const void *src)
{
	ID3D1xTexture3D*	tex;
	D3D11_TEXTURE3D_DESC textureDesc=
	{
		w,l,d
		,1
		,format
		,D3D11_USAGE_DYNAMIC
		,D3D11_BIND_SHADER_RESOURCE
		,D3D11_CPU_ACCESS_WRITE
		,0	// was D3D11_RESOURCE_MISC_GENERATE_MIPS
	};
	D3D11_SUBRESOURCE_DATA init=
	{
		src,
		w*ByteSizeOfFormatElement(format),
		w*l*ByteSizeOfFormatElement(format)
	};
	m_pd3dDevice->CreateTexture3D(&textureDesc,src?&init:NULL,&tex);
	return tex;
}
							
void simul::dx11::Ensure3DTextureSizeAndFormat(
							ID3D1xDevice			*m_pd3dDevice
							,ID3D1xTexture3D* &tex
							,ID3D11ShaderResourceView* &srv
							,int w,int l,int d
							,DXGI_FORMAT format)
{
	if(tex)
	{
		D3D11_TEXTURE3D_DESC desc;
		tex->GetDesc(&desc);
		if(desc.Width!=w||desc.Height!=l||desc.Depth!=d||desc.Format!=format)
		{
			SAFE_RELEASE(tex);
			SAFE_RELEASE(srv);
		}
	}

	if(!tex)
	{
		tex=make3DTexture(	m_pd3dDevice,w,l,d,format,NULL);
		m_pd3dDevice->CreateShaderResourceView(tex,NULL,&srv);
		return;
	}
}

void simul::dx11::setDepthState(ID3DX11Effect *effect,const char *name	,ID3D11DepthStencilState * value)
{
	ID3DX11EffectDepthStencilVariable*	var	=effect->GetVariableByName(name)->AsDepthStencil();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetDepthStencilState(0,value);
}

void simul::dx11::setSamplerState(ID3DX11Effect *effect,const char *name	,ID3D11SamplerState * value)
{
	ID3DX11EffectSamplerVariable*	var	=effect->GetVariableByName(name)->AsSampler();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetSampler(0,value);
}

void simul::dx11::setTexture(ID3DX11Effect *effect,const char *name	,ID3D11ShaderResourceView * value)
{
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetResource(value);
}

void simul::dx11::setUnorderedAccessView(ID3DX11Effect *effect,const char *name	,ID3D11UnorderedAccessView * value)
{
	ID3DX11EffectUnorderedAccessViewVariable*	var	=effect->GetVariableByName(name)->AsUnorderedAccessView();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetUnorderedAccessView(value);
}

void simul::dx11::setStructuredBuffer(ID3DX11Effect *effect,const char *name,ID3D11ShaderResourceView * value)
{
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetResource(value);
}

void simul::dx11::setTextureArray(ID3DX11Effect *effect	,const char *name	,ID3D11ShaderResourceView *value)
{
	ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByName(name)->AsShaderResource();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetResource(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float value)
{
	ID3DX11EffectScalarVariable*	var	=effect->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloat(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float x,float y)
{
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	float V[]={x,y,0.f,0.f};
	var->SetFloatVector(V);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float x,float y,float z,float w)
{
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	float V[]={x,y,z,w};
	var->SetFloatVector(V);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,int value)
{
	ID3DX11EffectScalarVariable*	var	=effect->GetVariableByName(name)->AsScalar();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetInt(value);
}

void simul::dx11::setParameter(ID3DX11Effect *effect,const char *name	,float *value)
{
	ID3DX11EffectVectorVariable*	var	=effect->GetVariableByName(name)->AsVector();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetFloatVector(value);
}

void simul::dx11::setMatrix(ID3DX11Effect *effect,const char *name	,const float *value)
{
	ID3DX11EffectMatrixVariable*	var	=effect->GetVariableByName(name)->AsMatrix();
	SIMUL_ASSERT(var->IsValid()!=0);
	var->SetMatrix(value);
}

void simul::dx11::setConstantBuffer(ID3DX11Effect *effect	,const char *name	,ID3D11Buffer *b)
{
	ID3DX11EffectConstantBuffer*	pD3DX11EffectConstantBuffer=effect->GetConstantBufferByName(name);
	SIMUL_ASSERT(pD3DX11EffectConstantBuffer->IsValid()!=0);
	pD3DX11EffectConstantBuffer->SetConstantBuffer(b);
}

void simul::dx11::unbindTextures(ID3DX11Effect *effect)
{
	D3DX11_EFFECT_DESC desc;
	effect->GetDesc(&desc);
	for(unsigned i=0;i<desc.GlobalVariables;i++)
	{
		ID3DX11EffectShaderResourceVariable*	var	=effect->GetVariableByIndex(i)->AsShaderResource();
		if(var->IsValid())
			var->SetResource(NULL);
		ID3DX11EffectUnorderedAccessViewVariable*	uav	=effect->GetVariableByIndex(i)->AsUnorderedAccessView();
		if(var->IsValid())
			var->SetResource(NULL);
	}
}

struct d3dMacro
{
	std::string name;
	std::string define;
};

HRESULT WINAPI D3DX11CreateEffectFromBinaryFileUtf8(const char *text_filename_utf8, UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	HRESULT hr=(HRESULT)(-1);
	std::string binary_filename_utf8=std::string(text_filename_utf8)+"o";
	void *pData=NULL;
	unsigned sz=0;
	fileLoader->AcquireFileContents(pData,sz,binary_filename_utf8.c_str(),false);
	if(sz>0)
	{
		hr=D3DX11CreateEffectFromMemory(pData,sz,FXFlags,pDevice,ppEffect);
		if(hr!=S_OK)
			std::cerr<<"D3DX11CreateEffectFromBinaryFile error "<<(int)hr<<std::endl;
	}
	else
		std::cerr<<"D3DX11CreateEffectFromBinaryFile cannot find file "<<binary_filename_utf8.c_str()<<std::endl;
	fileLoader->ReleaseFileContents(pData);
	return hr;
}

HRESULT WINAPI D3DX11CreateEffectFromFileUtf8(std::string text_filename_utf8,D3D10_SHADER_MACRO *macros,UINT FXFlags, ID3D11Device *pDevice, ID3DX11Effect **ppEffect)
{
	HRESULT hr=S_OK;
#if 0
	void *textData=NULL;
	unsigned textSize=0;
	fileLoader->AcquireFileContents(textData,textSize,text_filename_utf8.c_str(),true);
	ID3DBlob *binaryBlob=NULL;
	ID3DBlob *errorMsgs=NULL;
	int pos=text_filename_utf8.find_last_of("/");
	int bpos=text_filename_utf8.find_last_of("\\");
	if(pos<0||bpos>pos)
		pos=bpos;
	std::string path_utf8=text_filename_utf8.substr(0,pos);
	ShaderIncludeHandler shaderIncludeHandler(path_utf8.c_str(),"");
	hr=D3DCompile(
		textData,
		textSize,
		text_filename_utf8.c_str(),		//LPCSTR pSourceName,
		macros,		//const D3D_SHADER_MACRO *pDefines,
		&shaderIncludeHandler,		//ID3DInclude *pInclude,
		NULL,		//LPCSTR pEntrypoint,
		"fx_5_0",	//LPCSTR pTarget,
		0,	//UINT Flags1,
		FXFlags,	//UINT Flags2,
		&binaryBlob,//ID3DBlob **ppCode,
		&errorMsgs	//ID3DBlob **ppErrorMsgs
		);
	fileLoader->ReleaseFileContents(textData);
	if(hr==S_OK)
		hr=D3DX11CreateEffectFromMemory(binaryBlob->GetBufferPointer(),binaryBlob->GetBufferSize(),FXFlags,pDevice,ppEffect);
	else
	{
		char *errs=(char*)errorMsgs->GetBufferPointer();
		std::string err(errs);
		unsigned pos=0;
		while(pos<err.length())
		{
			int last=pos;
			pos=err.find("\n",pos+1);
			std::string line=err.substr(last,pos-last);
			std::cerr<<line.c_str()<<std::endl;
		};//text_filename_utf8.c_str()<<
	}
	if(binaryBlob)
		binaryBlob->Release();
	if(errorMsgs)
		errorMsgs->Release();
#else
	// first try to find an existing text source with this filename, and compile it.
	std::string filename_utf8= text_filename_utf8;
	std::string binary_filename_utf8=text_filename_utf8+"o";
	int pos=(int)text_filename_utf8.find_last_of("/");
	if(pos<0)
		pos=(int)text_filename_utf8.find_last_of("\\");
	std::string temporary_filename_utf8=std::string(text_filename_utf8)+"temp";
	void *textData=NULL;
	unsigned textSize=0;
	fileLoader->AcquireFileContents(textData,textSize,text_filename_utf8.c_str(),true);
	
	void *binaryData=NULL;
	unsigned binarySize=0;
	fileLoader->AcquireFileContents(binaryData,binarySize,binary_filename_utf8.c_str(),false);
	
	fileLoader->ReleaseFileContents(textData);
	fileLoader->ReleaseFileContents(binaryData);
	
	//ALWAYS_BUILD=1,BUILD_IF_NO_BINARY,NEVER_BUILD
	if(textSize>0&&(shaderBuildMode==ALWAYS_BUILD||(shaderBuildMode==BUILD_IF_NO_BINARY&&binarySize==0)))
	{
		//std::cout<<"Create DX11 effect: "<<text_filename.c_str()<<std::endl;
		DeleteFileW(simul::base::Utf8ToWString(binary_filename_utf8).c_str());
		std::wstring wcommand=simul::base::Utf8ToWString(simul::base::EnvironmentVariables::GetSimulEnvironmentVariable("DXSDK_DIR"));
		if(!wcommand.length())
		{
			std::string progfiles=simul::base::EnvironmentVariables::GetSimulEnvironmentVariable("ProgramFiles");
			wcommand=simul::base::Utf8ToWString(progfiles)+L"/Microsoft DirectX SDK (June 2010)/";
			std::cerr<<"Missing DXSDK_DIR environment variable, defaulting to June 2010 "<<std::endl;
		}
		{
//>"fxc.exe" /T fx_2_0 /Fo "..\..\gamma.fx"o "..\..\gamma.fx"
			wcommand=L"\""+wcommand;
			wcommand+=L"Utilities\\Bin\\x86\\fxc.exe\"";
			wcommand+=L" /nologo /Tfx_5_0 /Fo \"";
			wcommand+=simul::base::Utf8ToWString(temporary_filename_utf8)+L"\" \"";
			wcommand+=simul::base::Utf8ToWString(text_filename_utf8)+L"\"";
			if(macros)
			{
				while(macros->Name)
				{
					wcommand+=L" /D";
					wcommand+=simul::base::Utf8ToWString(macros->Name);
					wcommand+=L"=";
					wcommand+=simul::base::Utf8ToWString(macros->Definition);
					macros++;
				}
			}
		//	command+=" > \""+text_filename+".log\"";
			STARTUPINFOW si;
			PROCESS_INFORMATION pi;
			ZeroMemory( &si, sizeof(si) );
			si.cb = sizeof(si);
			ZeroMemory( &pi, sizeof(pi) );
			si.wShowWindow=false;
			wchar_t com[500];
			wcscpy(com,wcommand.c_str());
			si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
			si.wShowWindow = SW_HIDE;

			HANDLE hReadOutPipe = NULL;
			HANDLE hWriteOutPipe = NULL;
			HANDLE hReadErrorPipe = NULL;
			HANDLE hWriteErrorPipe = NULL;
			SECURITY_ATTRIBUTES saAttr; 
// Set the bInheritHandle flag so pipe handles are inherited. 
		 
			if(pipe_compiler_output)
			{
				saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
				saAttr.bInheritHandle = TRUE; 
				saAttr.lpSecurityDescriptor = NULL; 
				CreatePipe( &hReadOutPipe, &hWriteOutPipe, &saAttr, 100 );
				CreatePipe( &hReadErrorPipe, &hWriteErrorPipe, &saAttr, 100 );
			}

			si.hStdOutput = hWriteOutPipe;
			si.hStdError= hWriteErrorPipe;
			CreateProcessW( NULL,		// No module name (use command line)
					com,				// Command line
					NULL,				// Process handle not inheritable
					NULL,				// Thread handle not inheritable
					TRUE,				// Set handle inheritance to FALSE
					0,//CREATE_NO_WINDOW,	// No nasty console windows
					NULL,				// Use parent's environment block
					NULL,				// Use parent's starting directory 
					&si,				// Pointer to STARTUPINFO structure
					&pi )				// Pointer to PROCESS_INFORMATION structure
				;
			// Wait until child process exits.

		HANDLE WaitHandles[] = {
				pi.hProcess, hReadOutPipe, hReadErrorPipe
			};

		const DWORD BUFSIZE = 4096;
		BYTE buff[BUFSIZE];
		bool has_errors=false;
		while (1)
		{
			DWORD dwBytesRead, dwBytesAvailable;
			DWORD dwWaitResult = WaitForMultipleObjects(pipe_compiler_output?3:1, WaitHandles, FALSE, 60000L);

			// Read from the pipes...
			if(pipe_compiler_output)
			{
				while( PeekNamedPipe(hReadOutPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable )
				{
				  ReadFile(hReadOutPipe, buff, BUFSIZE-1, &dwBytesRead, 0);
				  std::cout << std::string((char*)buff, (size_t)dwBytesRead).c_str();
				}
				while( PeekNamedPipe(hReadErrorPipe, NULL, 0, NULL, &dwBytesAvailable, NULL) && dwBytesAvailable )
				{
					ReadFile(hReadErrorPipe, buff, BUFSIZE-1, &dwBytesRead, 0);
					std::string str((char*)buff, (size_t)dwBytesRead);
					size_t pos=str.find("rror");
					if(pos>=str.length())
						pos=str.find("failed");
					if(pos<str.length())
						has_errors=true;
					int bracket_pos=(int)str.find("(");
					if(bracket_pos>0)
					{
						int close_bracket_pos	=(int)str.find(")",bracket_pos);
						int comma_pos			=(int)str.find(",",bracket_pos);
						if(comma_pos>bracket_pos&&comma_pos<close_bracket_pos)
						{
							str.replace(comma_pos,close_bracket_pos-comma_pos,"");
						}
					}
					// IF it's not an absolute path:
					if(shaderPathUtf8&&str.find(":")!=1)
					{
						str=*shaderPathUtf8+str;
					}
					std::cerr<<str.c_str();
				}
			}
			// Process is done, or we timed out:
			if(dwWaitResult == WAIT_OBJECT_0 || dwWaitResult == WAIT_TIMEOUT)
			break;
		  }

			//WaitForSingleObject( pi.hProcess, INFINITE );
			CloseHandle( pi.hProcess );
			CloseHandle( pi.hThread );

			if(has_errors)
				return S_FALSE;
		}
	}
	MoveFileW(simul::base::Utf8ToWString(temporary_filename_utf8).c_str(),simul::base::Utf8ToWString(binary_filename_utf8).c_str());
	hr=D3DX11CreateEffectFromBinaryFileUtf8(filename_utf8.c_str(),FXFlags,pDevice,ppEffect);
#endif
	return hr;
}

HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3DX11Effect **effect,const char *filename)
{
	std::map<std::string,std::string> defines;
	return CreateEffect(d3dDevice,effect,filename,defines);
}

ID3D11ComputeShader *LoadComputeShader(ID3D1xDevice *pd3dDevice,const char *filename_utf8)
{
	if(!shaderPathUtf8)
		shaderPathUtf8=new std::string("media/hlsl/dx11");
	std::string fn=(*shaderPathUtf8+"/")+filename_utf8;
	DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( _DEBUG )
	dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

	LPCSTR pProfile = (pd3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ) ? "cs_5_0" : "cs_4_0";

	ID3DBlob* pErrorBlob = NULL;
	ID3DBlob* pBlob = NULL;
	HRESULT hr = D3DX11CompileFromFileW(simul::base::Utf8ToWString(fn.c_str()).c_str(), NULL, NULL, "main", pProfile, dwShaderFlags, NULL, NULL, &pBlob, &pErrorBlob, NULL );
	if ( FAILED(hr) )
	{
		if ( pErrorBlob )
			OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
		if(pErrorBlob)
			pErrorBlob->Release();
		if(pBlob)
			pBlob->Release();

		return NULL;
	}
	else
	{
		ID3D11ComputeShader *computeShader;
		hr = pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL,&computeShader );
		if(pErrorBlob)
			pErrorBlob->Release();
		if(pBlob)
			pBlob->Release();

		return computeShader;
	}
}

HRESULT CreateEffect(ID3D1xDevice *d3dDevice,ID3DX11Effect **effect,const char *filenameUtf8,const std::map<std::string,std::string>&defines)
{
	SIMUL_ASSERT(d3dDevice!=NULL);
	HRESULT hr=S_OK;
	std::string text_filename=(filenameUtf8);
	if(!shaderPathUtf8)
		shaderPathUtf8=new std::string("media/hlsl/dx11");
	std::string filename_utf8=*shaderPathUtf8+filenameUtf8;
	
	D3D10_SHADER_MACRO *macros=NULL;
	std::vector<std::string> d3dmacros;
	{
		size_t num_defines=defines.size();
		if(num_defines)
		{
			macros=new D3D10_SHADER_MACRO[num_defines+1];
			macros[num_defines].Definition=0;
			macros[num_defines].Name=0;
		}
		size_t def=0;
		for(std::map<std::string,std::string>::const_iterator i=defines.begin();i!=defines.end();i++)
		{
			macros[def].Name=i->first.c_str();
			macros[def].Definition=i->second.c_str();
			def++;
		}
	}
	DWORD flags=default_effect_flags;
	SAFE_RELEASE(*effect);
	hr=1;
	while(hr!=S_OK)
	{
		hr=D3DX11CreateEffectFromFileUtf8(	filename_utf8,
											macros,
											flags,
											d3dDevice,
											effect);
		if(hr==S_OK)
			break;
		std::string err="";
#ifdef DXTRACE_ERR
        hr=DXTRACE_ERR( L"CreateEffect", hr );
#endif
		DebugBreak();
	}
	assert((*effect)->IsValid());
	delete [] macros;
	return hr;
}

ID3DX11Effect *LoadEffect(ID3D1xDevice *d3dDevice,const char *filename_utf8)
{
	std::map<std::string,std::string> defines;
	return LoadEffect(d3dDevice,filename_utf8,defines);
}

ID3DX11Effect *LoadEffect(ID3D1xDevice *d3dDevice,const char *filename_utf8,const std::map<std::string,std::string>&defines)
{
	ID3DX11Effect *effect=NULL;
	CreateEffect(d3dDevice,&effect,filename_utf8,defines);
	return effect;
}

#define D3D10_SHADER_ENABLE_STRICTNESS              (1 << 11)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p)      { if (p) { (p)->Release(); (p)=NULL; } }
#endif


HRESULT Map2D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture2D *tex,D3D1x_MAPPED_TEXTURE2D *mp)
{
	return pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
}

HRESULT Map3D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture3D *tex,D3D1x_MAPPED_TEXTURE3D *mp)
{
	return pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
}

HRESULT Map1D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture1D *tex,D3D1x_MAPPED_TEXTURE1D *mp)
{
	return pImmediateContext->Map(tex,0,D3D1x_MAP_WRITE_DISCARD,0,mp);
}

void Unmap2D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture2D *tex)
{
	pImmediateContext->Unmap(tex,0);
}

void Unmap3D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture3D *tex)
{
	pImmediateContext->Unmap(tex,0);
}

void Unmap1D(ID3D11DeviceContext *pImmediateContext,ID3D1xTexture1D *tex)
{
	pImmediateContext->Unmap(tex,0);
}

HRESULT MapBuffer(ID3D11DeviceContext *pImmediateContext,ID3D1xBuffer *vertexBuffer,D3D11_MAPPED_SUBRESOURCE*vert)
{
	return pImmediateContext->Map(vertexBuffer,0,D3D1x_MAP_WRITE_DISCARD,0,vert);
}

void UnmapBuffer(ID3D11DeviceContext *pImmediateContext,ID3D1xBuffer *vertexBuffer)
{
	pImmediateContext->Unmap(vertexBuffer,0);
}

HRESULT ApplyPass(ID3D11DeviceContext *pImmediateContext,ID3DX11EffectPass *pass)
{
	return pass->Apply(0,pImmediateContext);
}

void MakeCubeMatrices(D3DXMATRIX mat[],const float *cam_pos,bool ReverseDepth)
{
    D3DXVECTOR3 vEyePt = D3DXVECTOR3( cam_pos );
    D3DXVECTOR3 vLookDir;
    D3DXVECTOR3 vUpDir;
    ZeroMemory(mat,6*sizeof(D3DXMATRIX) );
	static const D3DVECTOR lookf[6]=
	{
		 {1.f,0.f,0.f}		,{-1.f,0.f,0.f}
		,{0.f,1.f,0.f}		,{0.f,-1.f,0.f}
		,{0.f,0.f,1.f}		,{0.f,0.f,-1.f}
	};
	static const D3DVECTOR upf[6]=
	{
		 {0.f,-1.f,0.f}		,{0.f,-1.f,0.f}
		,{0.f,0.f,-1.f}		,{0.f,0.f,1.f}
		,{0.f,1.f,0.f}		,{0.f,1.f,0.f}
	};
	static const D3DVECTOR lookr[6]=
	{
		 {-1.f,0.f,0.f}		,{1.f,0.f,0.f}
		,{0.f,-1.f,0.f}		,{0.f,1.f,0.f}
		,{0.f,0.f,-1.f}		,{0.f,0.f,1.f}
	};
	static const D3DVECTOR upr[6]=
	{
		 {0.f,-1.f,0.f}		,{0.f,-1.f,0.f}
		,{0.f,0.f,1.f}		,{0.f,0.f,-1.f}
		,{0.f,-1.f,0.f}		,{0.f,-1.f,0.f}
	};
	for(int i=0;i<6;i++)
	{
		vLookDir	=vEyePt+lookf[i];
		vUpDir		=upf[i];
		D3DXMatrixLookAtLH(&mat[i], &vEyePt, &vLookDir, &vUpDir );
		if(ReverseDepth)
		{
			vLookDir	=vEyePt+lookr[i];
			vUpDir		=upr[i];
			D3DXMatrixLookAtRH(&mat[i], &vEyePt, &vLookDir, &vUpDir );
		}
	}
}

void BreakIfDebugging()
{
	DebugBreak();
}
 

// Stored states
static ID3D11DepthStencilState* m_pDepthStencilStateStored11=NULL;
static UINT m_StencilRefStored11;
static ID3D11RasterizerState* m_pRasterizerStateStored11=NULL;
static ID3D11BlendState* m_pBlendStateStored11=NULL;
static float m_BlendFactorStored11[4];
static UINT m_SampleMaskStored11;
static ID3D11SamplerState* m_pSamplerStateStored11=NULL;

void StoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMGetDepthStencilState( &m_pDepthStencilStateStored11, &m_StencilRefStored11 );
    pd3dImmediateContext->RSGetState( &m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMGetBlendState( &m_pBlendStateStored11, m_BlendFactorStored11, &m_SampleMaskStored11 );
    pd3dImmediateContext->PSGetSamplers( 0, 1, &m_pSamplerStateStored11 );
}

void RestoreD3D11State( ID3D11DeviceContext* pd3dImmediateContext )
{
    pd3dImmediateContext->OMSetDepthStencilState( m_pDepthStencilStateStored11, m_StencilRefStored11 );
    pd3dImmediateContext->RSSetState( m_pRasterizerStateStored11 );
    pd3dImmediateContext->OMSetBlendState( m_pBlendStateStored11, m_BlendFactorStored11, m_SampleMaskStored11 );
    pd3dImmediateContext->PSSetSamplers( 0, 1, &m_pSamplerStateStored11 );

    SAFE_RELEASE( m_pDepthStencilStateStored11 );
    SAFE_RELEASE( m_pRasterizerStateStored11 );
    SAFE_RELEASE( m_pBlendStateStored11 );
    SAFE_RELEASE( m_pSamplerStateStored11 );
}

int simul::dx11::ByteSizeOfFormatElement( DXGI_FORMAT format )
{
    switch( format )
    {
        case DXGI_FORMAT_R32G32B32A32_TYPELESS:
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
        case DXGI_FORMAT_R32G32B32A32_UINT:
        case DXGI_FORMAT_R32G32B32A32_SINT:
            return 16;
               
        case DXGI_FORMAT_R32G32B32_TYPELESS:
        case DXGI_FORMAT_R32G32B32_FLOAT:
        case DXGI_FORMAT_R32G32B32_UINT:
        case DXGI_FORMAT_R32G32B32_SINT:
            return 12;
               
        case DXGI_FORMAT_R16G16B16A16_TYPELESS:
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:
        case DXGI_FORMAT_R16G16B16A16_SNORM:
        case DXGI_FORMAT_R16G16B16A16_SINT:
        case DXGI_FORMAT_R32G32_TYPELESS:
        case DXGI_FORMAT_R32G32_FLOAT:
        case DXGI_FORMAT_R32G32_UINT:
        case DXGI_FORMAT_R32G32_SINT:
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
            return 8;
       
        case DXGI_FORMAT_R10G10B10A2_TYPELESS:
        case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R10G10B10A2_UINT:
        case DXGI_FORMAT_R11G11B10_FLOAT:
        case DXGI_FORMAT_R8G8B8A8_TYPELESS:
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_R8G8B8A8_UINT:
        case DXGI_FORMAT_R8G8B8A8_SNORM:
        case DXGI_FORMAT_R8G8B8A8_SINT:
        case DXGI_FORMAT_R16G16_TYPELESS:
        case DXGI_FORMAT_R16G16_FLOAT:
        case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:
        case DXGI_FORMAT_R16G16_SNORM:
        case DXGI_FORMAT_R16G16_SINT:
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R32_UINT:
        case DXGI_FORMAT_R32_SINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            return 4;
               
        case DXGI_FORMAT_R8G8_TYPELESS:
        case DXGI_FORMAT_R8G8_UNORM:
        case DXGI_FORMAT_R8G8_UINT:
        case DXGI_FORMAT_R8G8_SNORM:
        case DXGI_FORMAT_R8G8_SINT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_FLOAT:
        case DXGI_FORMAT_D16_UNORM:
        case DXGI_FORMAT_R16_UNORM:
        case DXGI_FORMAT_R16_UINT:
        case DXGI_FORMAT_R16_SNORM:
        case DXGI_FORMAT_R16_SINT:
        case DXGI_FORMAT_B5G6R5_UNORM:
        case DXGI_FORMAT_B5G5R5A1_UNORM:
            return 2;
               
        case DXGI_FORMAT_R8_TYPELESS:
        case DXGI_FORMAT_R8_UNORM:
        case DXGI_FORMAT_R8_UINT:
        case DXGI_FORMAT_R8_SNORM:
        case DXGI_FORMAT_R8_SINT:
        case DXGI_FORMAT_A8_UNORM:
            return 1;
       
        // Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
        case DXGI_FORMAT_BC2_TYPELESS:
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC2_UNORM_SRGB:
        case DXGI_FORMAT_BC3_TYPELESS:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC3_UNORM_SRGB:
        case DXGI_FORMAT_BC5_TYPELESS:
        case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC5_SNORM:
            return 16;
               
        // Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
        case DXGI_FORMAT_R1_UNORM:
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            return 8;
       
        // Compressed format; http://msdn2.microso.../bb694531(VS.85).aspx
        case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
            return 4;
       
        // These are compressed, but bit-size information is unclear.
        case DXGI_FORMAT_R8G8_B8G8_UNORM:
        case DXGI_FORMAT_G8R8_G8B8_UNORM:
            return 4;

        case DXGI_FORMAT_UNKNOWN:
        default:
            return 0;
    }
}