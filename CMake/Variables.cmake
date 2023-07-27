include_guard()
set(PLATFORM_USED_CMAKE_GENERATOR "${CMAKE_GENERATOR}" CACHE STRING "Which CMAKE_GENERATOR was used before?" FORCE)
if(NOT DEFINED SIMUL_SOURCE_BUILD)
	option( SIMUL_SOURCE_BUILD "Build from source?" ON )
endif()
option( SIMUL_INTERNAL_CHECKS "Enable Simul internal debugging checks?" OFF )

option( SIMUL_BUILD_SHADERS "Build shaders? If false, shaders should be already present." ON )
option( SIMUL_DEBUG_SHADERS "Compile shaders with debug info." OFF )
option( SIMUL_BUILD_SAMPLES "Deprecated, use PLATFORM_BUILD_SAMPLES instead." ON )
mark_as_advanced(SIMUL_BUILD_SAMPLES)
option(PLATFORM_BUILD_SAMPLES "Build executable samples?" ${SIMUL_BUILD_SAMPLES})
option(PLATFORM_WARNINGS_AS_ERRORS "Should Platform treat C++ compile warnings as errors. " ON)
mark_as_advanced(PLATFORM_WARNINGS_AS_ERRORS)


set(PLATFORM_STD_FILESYSTEM 1 CACHE STRING "Use std::filesystem?" )
if(XBOXONE)
	set(PLATFORM_STD_FILESYSTEM 2 CACHE STRING "Use std::filesystem?" )
elseif(PLATFORM_PS4)
	set(PLATFORM_STD_FILESYSTEM 0 CACHE STRING "Use std::filesystem?" )
elseif(PLATFORM_LINUX)
	set(PLATFORM_STD_FILESYSTEM 1 CACHE STRING "Use std::filesystem?" )
endif()
set_property(CACHE PLATFORM_STD_FILESYSTEM PROPERTY STRINGS 0 1 2)

find_package(Vulkan REQUIRED)
set( VULKAN_SDK_DIR "{Vulkan_INCLUDE_DIR}/.."  )
set( PLATFORM_EMSCRIPTEN_DIR "$ENV{EMSCRIPTEN}" CACHE STRING "Set the location of the Emscripten SDK if compiling for Emscripten." )

set( PLATFORM_DEBUG_DISABLE 0 CACHE STRING "Set disable-level for debugging. Zero for full functionality." )

if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	set(PLATFORM_LINUX ON)
else()
	set(PLATFORM_LINUX OFF)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
	set(PLATFORM_COMPILE_DEVICE_MANAGER OFF)
else()
	option(PLATFORM_COMPILE_DEVICE_MANAGER "" ON)
endif()

option(PLATFORM_SUPPORT_WEBGPU "Use WebGPU API with Emscripten?" OFF)
option(PLATFORM_IMGUI "" OFF)

option(PLATFORM_LOAD_RENDERDOC "Always load the renderdoc dll?" OFF )
option(PLATFORM_BUILD_DOCS "Whether to build html documentation with Doxygen and Sphinx" OFF )
option(PLATFORM_USE_FMT "Include the fmt formatting library?" ON )
 
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	set( WINDOWS ON )
	set(PLATFORM_WINDOWS ON)
	option(PLATFORM_SUPPORT_WIN7 "" ON )
	option(PLATFORM_SUPPORT_OPENGL "" ON)
	option(PLATFORM_SUPPORT_D3D11 "" ON )
	set(PLATFORM_D3D11_SFX ON )
	option(PLATFORM_SUPPORT_D3D12 "" ON )
	option(PLATFORM_USE_ASSIMP "" OFF )
	option(PLATFORM_BUILD_MD_LIBS "Build dynamically-linked dlls?" OFF)
	set( ON)
else()
	set(PLATFORM_WINDOWS OFF)
	option(PLATFORM_USE_ASSIMP "" OFF )
	option(PLATFORM_SUPPORT_D3D11 "" OFF )
	option(PLATFORM_SUPPORT_OPENGL "" OFF )
	if(XBOXONE)
		option(PLATFORM_SUPPORT_D3D12 "" ON )
	else()
		if(GDK)
			option(PLATFORM_SUPPORT_D3D12 "" ON )
			if(PLATFORM_WINGDK)
				option(PLATFORM_SUPPORT_D3D11 "" ON )
				set(PLATFORM_D3D11_SFX ON )
			endif()
		else()
			option(PLATFORM_SUPPORT_D3D12 "" OFF )
		endif()
	endif()
endif()

if(PLATFORM_WINDOWS OR PLATFORM_LINUX)
	if("${Vulkan_INCLUDE_DIR}" STREQUAL "")
		set(PLATFORM_SUPPORT_VULKAN OFF )
	else()
		option(PLATFORM_SUPPORT_VULKAN "" ON)
	endif()
else()
	set(PLATFORM_SUPPORT_VULKAN OFF )
endif()

set(SIMUL_SUPPORT_D3D11 ${PLATFORM_SUPPORT_D3D11})
set(SIMUL_SUPPORT_D3D12 ${PLATFORM_SUPPORT_D3D12})
set(SIMUL_SUPPORT_OPENGL ${PLATFORM_SUPPORT_OPENGL})
set(SIMUL_SUPPORT_VULKAN ${PLATFORM_SUPPORT_VULKAN})

#Default options for assimp:
set(BUILD_SHARED_LIBS off)
set(ASSIMP_BUILD_TESTS off)

find_program(SIMUL_FX_EXECUTABLE fxc.exe PATHS "C:/Program Files (x86)/Windows Kits/10/bin" "C:/Program Files (x86)/Windows Kits/10/bin/10.0.18362.0" PATH_SUFFIXES x64 )

if(PLATFORM_WINDOWS)
	set( BISON_EXECUTABLE "${SIMUL_PLATFORM_DIR}/External/win_flex_bison/win_bison.exe" CACHE STRING "" )
	set( FLEX_EXECUTABLE "${SIMUL_PLATFORM_DIR}/External/win_flex_bison/win_flex.exe" CACHE STRING "" )
	set( FLEX_INCLUDE_DIR "${SIMUL_PLATFORM_DIR}/External/win_flex_bison/" c STRING "" )
endif()

if(PLATFORM_WINDOWS)
set(PLATFORM_SFX_EXECUTABLE "${CMAKE_BINARY_DIR}/bin/Release/Sfx.exe" CACHE STRING "")
else()
find_program(PLATFORM_SFX_EXECUTABLE sfx PATHS "${CMAKE_BINARY_DIR}/bin/Release" "${CMAKE_BINARY_DIR}/bin/Debug" "${CMAKE_BINARY_DIR}/bin/Sfx") 
endif()

mark_as_advanced(SIMUL_INTERNAL_CHECKS SIMUL_DEBUG_SHADERS )

# Defaults for glfw
set(GLFW_BUILD_TESTS OFF )
set(GLFW_BUILD_DOCS OFF )
set(GLFW_BUILD_EXAMPLES OFF )
