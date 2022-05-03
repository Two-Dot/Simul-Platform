include_guard()

set(PLATFORM_CPP_VERSION 17)
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS ON )
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
SET( CMAKE_STATIC_LINKER_FLAGS_RELEASE "${CMAKE_STATIC_LINKER_FLAGS_RELEASE} ")
set( CMAKE_EXPORT_COMPILE_COMMANDS "ON" )
set( CMAKE_POSITION_INDEPENDENT_CODE ON )

if(NOT CMAKE_DEBUG_POSTFIX)
	set(CMAKE_DEBUG_POSTFIX d )
endif()

# Get the git branch
find_package (Git)
execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
	RESULT_VARIABLE GIT_BRANCH_RESULT
	OUTPUT_VARIABLE PLATFORM_GIT_BRANCH)

# Set the output folder where program will be created
set( EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/eop/ )
set( LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lop/ )
set( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/ )
set( CMAKE_CACHEFILE_DIR ${CMAKE_BINARY_DIR}/cachefiles/ )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/ )
set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/ )
set( CMAKE_PDB_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/ )

set(STATIC_LINK_SUFFIX "")
set(DYNAMIC_LINK_SUFFIX "") 
set(SIMUL_DYNAMIC_LIBS OFF )
set(PLATFORM_EMSCRIPTEN 0)

if(PLATFORM_WINDOWS)
	set(STATIC_LINK_SUFFIX "_MT")
	set(DYNAMIC_LINK_SUFFIX "_MD")
	set(SIMUL_DYNAMIC_LIBS ON )
	set(MSVC 1)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od /Ob2 /DNDEBUG /DSIMUL_EDITOR=1 /Zi /JMC")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /RTC1 /DSIMUL_EDITOR=1 /JMC")
	set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG")
	link_directories(${VULKAN_SDK_DIR}/Lib)
	#Specify C++ 17 in the CMAKE_CXX_FLAGS after project() and before add_subdirectory().
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++${PLATFORM_CPP_VERSION}")
elseif(PLATFORM_COMMODORE)
	#set(DYNAMIC_LINK_SUFFIX "_Prospero")
	#set(STATIC_LINK_SUFFIX "_Prospero")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std:c++${PLATFORM_CPP_VERSION}")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
elseif(PLATFORM_PS4)
	#set(DYNAMIC_LINK_SUFFIX "_Orbis")
	#set(STATIC_LINK_SUFFIX "_Orbis")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g -O3 -D__ORBIS__=1")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0 -D__ORBIS__=1")
elseif(PLATFORM_XBOXONE)
	if(GDK)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std:c++${PLATFORM_CPP_VERSION}")
		set(DYNAMIC_LINK_SUFFIX "_XboxOneGDK")
		set(STATIC_LINK_SUFFIX "_XboxOneGDK")
		set(MSVC 1)
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od /Ob2 /DNDEBUG /Zi /EHsc")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /DDEBUG /D_DEBUG /RTC1 /EHsc")
	else()
		set(DYNAMIC_LINK_SUFFIX "_XboxOne")
		set(STATIC_LINK_SUFFIX "_XboxOne")
		set(MSVC 1)
		set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od /Ob2 /DNDEBUG /Zi /EHsc")
		set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /DDEBUG /D_DEBUG /RTC1 /EHsc")
		set(CMAKE_VS_GLOBALS "DefaultLanguage=en-US")
	endif()
elseif(PLATFORM_WINGDK)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std:c++${PLATFORM_CPP_VERSION}")
	set(DYNAMIC_LINK_SUFFIX "_WinGDK")
	set(STATIC_LINK_SUFFIX "_WinGDK")
	set(MSVC 1)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od /Ob2 /DNDEBUG /Zi /EHsc")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /DDEBUG /D_DEBUG /RTC1 /EHsc")
elseif(PLATFORM_SPECTRUM)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std:c++${PLATFORM_CPP_VERSION}")
	set(DYNAMIC_LINK_SUFFIX "_GDK")
	set(STATIC_LINK_SUFFIX "_GDK")
	set(MSVC 1)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Od /Ob2 /DNDEBUG /Zi /EHsc")
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Zi /Ob0 /Od /DDEBUG /D_DEBUG /RTC1 /EHsc")
elseif(PLATFORM_LINUX)
	include_directories( ${SIMUL_PLATFORM_DIR}/Linux )
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG")
	set(STATIC_LINK_SUFFIX "_MT")
	set(DYNAMIC_LINK_SUFFIX "_MD")
	set(SIMUL_DYNAMIC_LIBS ON )
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${PLATFORM_CPP_VERSION}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -L/usr/lib -Wl,-rpath,/usr/lib")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
	set( PLATFORM_CPP_VERSION 17 )
	include_directories( ${SIMUL_PLATFORM_DIR}/Linux )
	include_directories( ${PLATFORM_EMSCRIPTEN_DIR}/system/include ${PLATFORM_EMSCRIPTEN_DIR}/system/include/libcxx ${PLATFORM_EMSCRIPTEN_DIR}/system/include/libc )
	set(PLATFORM_EMSCRIPTEN 1)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG -DDEBUG -DUNIX")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -DUNIX")
	set(STATIC_LINK_SUFFIX "_MT")
	set(DYNAMIC_LINK_SUFFIX "_MD")
	set(SIMUL_DYNAMIC_LIBS OFF )
else()
	set(MSVC 0)
	message("Warning: Platform not found")
endif()

function (deploy_target_assets target targetDir )
	cmake_parse_arguments(deploy_target_assets "" "" "ASSETS" ${ARGN} )
	set(asset_list)
	foreach(a ${deploy_target_assets_ASSETS})
		list(APPEND asset_list \"${a}\")
	endforeach()
	add_custom_command(TARGET ${target} BYPRODUCTS none.txt 
			POST_BUILD 
			COMMAND set TARG_DIR=${targetDir}\n
			set ASSETS=${asset_list}\n
			for %%I in (%ASSETS:/=\\%) do copy %%I \"%TARG_DIR:/=\\%\"\n )
endfunction()

function( deploy_to_directory targetName destDir )
	#message( STATUS "deploy_to_directory ${targetName} ${destDir}" )
	get_target_property(target_type ${targetName} TYPE)
	if (target_type STREQUAL "SHARED_LIBRARY")
		set( LIBFILE $<TARGET_LINKER_FILE:${targetName}>)
	else()
		set(LIBFILE "")
	endif ()
	set(no_copy $<NOT:$<CONFIG:Release>>)
	add_custom_command(TARGET ${targetName} BYPRODUCTS none.txt 
		POST_BUILD 
		COMMAND if \"$(Configuration)\" == \"Release\" (\n
		set TARG_DLL=$<TARGET_FILE:${targetName}>\n
		set TARG_DLL=%TARG_DLL:/=\\%\n
		set TARG_DIR=${destDir}\n
		set TARG_DIR=%TARG_DIR:/=\\%\n
		copy \"%TARG_DLL%\" \"%TARG_DIR%\"\n
		set TARG_LIB=${LIBFILE}\n
		if not \"%TARG_LIB%\"==\"\" (\n
			copy \"%TARG_LIB:/=\\%\" \"%TARG_DIR%\"\n
		)
		)
		\n
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )
endfunction()

function(set_target_runtime targname rt)
	# Default to statically-linked runtime.
	#message("set_target_runtime ${targname} ${rt}")
	if("${rt}" STREQUAL "")
		set(rt "static")
	endif()
	if(${MSVC})
		if(${rt} STREQUAL "static")
			SET_TARGET_PROPERTIES(${targname} PROPERTIES COMPILE_OPTIONS "/MT$<$<CONFIG:Debug>:d>")
		else()
			SET_TARGET_PROPERTIES(${targname} PROPERTIES COMPILE_OPTIONS "/MD$<$<CONFIG:Debug>:d>")
		endif()
	endif()
endfunction()

function(LibraryDefaults targname)
	# Need to see why this is not defined.
	if(NOT DEFINED PLATFORM_CPP_VERSION)
		set(PLATFORM_CPP_VERSION 17)
	endif()
	if(SIMUL_INTERNAL_CHECKS)
		target_compile_definitions(${targname} PRIVATE SIMUL_INTERNAL_CHECKS=1 )
	else()
		target_compile_definitions(${targname} PRIVATE SIMUL_INTERNAL_CHECKS=0 )
	endif()
	if(PLATFORM_DEBUG_MEMORY)
		target_compile_definitions(${targname} PRIVATE PLATFORM_DEBUG_MEMORY=1 )
	endif()
	target_compile_definitions(${targname} PRIVATE PLATFORM_DEBUG_DISABLE=${PLATFORM_DEBUG_DISABLE} SWAP_NOEXCEPT= )
	target_compile_definitions(${targname} PRIVATE CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR} )
	target_compile_definitions(${targname} PRIVATE CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR} )
	target_compile_definitions(${targname} PRIVATE PLATFORM_SOURCE_DIR=${PLATFORM_SOURCE_DIR} )
	target_compile_definitions(${targname} PRIVATE PLATFORM_BUILD_DIR=${PLATFORM_BUILD_DIR} )
	if(PLATFORM_EMSCRIPTEN)
		target_compile_definitions(${targname} PRIVATE UNIX=1)
	endif()
	if(PLATFORM_SUPPORT_D3D11)
		target_compile_definitions(${targname} PRIVATE SIMUL_USE_D3D11=1 PLATFORM_SUPPORT_D3D11=1)
	else()
		target_compile_definitions(${targname} PRIVATE SIMUL_USE_D3D11=0 PLATFORM_SUPPORT_D3D11=0)
	endif()
	if(PLATFORM_SUPPORT_D3D12)
		target_compile_definitions(${targname} PRIVATE SIMUL_USE_D3D12=1 PLATFORM_SUPPORT_D3D12=1)
	else()
		target_compile_definitions(${targname} PRIVATE SIMUL_USE_D3D12=0 PLATFORM_SUPPORT_D3D12=0)
	endif()

	if(PLATFORM_SUPPORT_OPENGL)
		target_compile_definitions(${targname} PRIVATE SIMUL_USE_OPENGL=1 PLATFORM_SUPPORT_OPENGL=1)
	else()
		target_compile_definitions(${targname} PRIVATE SIMUL_USE_OPENGL=0 PLATFORM_SUPPORT_OPENGL=0)
	endif()

	if(PLATFORM_SUPPORT_VULKAN)
		target_compile_definitions(${targname} PRIVATE SIMUL_USE_VULKAN=1 PLATFORM_SUPPORT_VULKAN=1)
	else()
		target_compile_definitions(${targname} PRIVATE SIMUL_USE_VULKAN=0 PLATFORM_SUPPORT_VULKAN=0)
	endif()
	target_include_directories(${targname} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
	target_include_directories(${targname} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
	target_include_directories(${targname} PRIVATE "${CMAKE_SOURCE_DIR}")
	target_include_directories(${targname} PRIVATE "${CMAKE_SOURCE_DIR}/..")
	target_include_directories(${targname} PRIVATE "${SIMUL_PLATFORM_DIR}/..")
	set_target_properties(${targname} PROPERTIES CXX_STANDARD_REQUIRED ON)

	# The prebuilt libraries live here if not a source build:
	if(NOT SIMUL_SOURCE_BUILD)
		target_link_directories(${targname} PRIVATE ${SIMUL_BUILD_LOCATION}/lib )
	endif()
	if(XBOXONE)
		target_include_directories(${targname} PRIVATE "${SIMUL_PLATFORM_DIR}/XboxOne")
		add_definitions( -DNOMINMAX )
		target_compile_options(${targname} PRIVATE "/AI$ENV{ProgramFiles\(x86\)}/Microsoft SDKs/Durango.${XDK_TOOLCHAIN_VERSION}/v8.0/references/commonconfiguration/neutral" ) 
		target_compile_options(${targname} PRIVATE "/AI${XDK_ROOT}/${XDK_TOOLCHAIN_VERSION}/xdk/VS2017/vc/platform/amd64" ) 
		target_compile_options(${targname} PRIVATE /diagnostics:classic /GS /TP /W3 /wd4011 /wd4514 /wd5040 /Zc:wchar_t /Zi /Gm- /Od /Ob0 /Zc:inline)
		target_compile_options(${targname} PRIVATE /fp:fast /DWINAPI_FAMILY=WINAPI_FAMILY_TV_TITLE /D_DURANGO /D_XBOX_ONE /DWIN32_LEAN_AND_MEAN)
		target_compile_options(${targname} PRIVATE /errorReport:prompt /WX- /Zc:forScope /RTC1 /arch:AVX /Gd /nologo) 
		target_link_options(${targname} PRIVATE /DEBUG)
		add_definitions( -D_UNICODE -DUNICODE )
		target_compile_definitions(${targname} PRIVATE PLATFORM_NO_OPTIMIZATION )
	elseif(GDK)
		if(PLATFORM_SPECTRUM)
		else()
			target_include_directories(${targname} PRIVATE "${SIMUL_PLATFORM_DIR}/GDK")
		endif()
		#target_compile_options(${targname} PRIVATE /diagnostics:classic /GS /TP /W3 /wd4011 /wd4514 /Zc:wchar_t /Zi /Gm- /Od /Ob0 /Zc:inline)
		target_compile_options(${targname} PRIVATE /fp:fast /EHsc /D_GDK /DWIN32_LEAN_AND_MEAN)
		#target_compile_options(${targname} PRIVATE /errorReport:prompt /WX- /Zc:forScope /RTC1 /arch:AVX /Gd /nologo) 
		target_link_options(${targname} PRIVATE /DEBUG)
		add_definitions( -D_UNICODE -DUNICODE )
		target_compile_definitions(${targname} PRIVATE PLATFORM_NO_OPTIMIZATION )
		add_definitions( -DNOMINMAX )
	elseif(MSVC)
		target_include_directories(${targname} PRIVATE "${SIMUL_PLATFORM_DIR}/Windows")
		add_definitions( -DNOMINMAX )
		add_definitions( -DWIN64 )
		# Maybe an arg could be passed to use /WX? Or maybe we just fix our damn warnings.
		target_compile_options( ${targname} PRIVATE /W3 /wd4011 /wd4514 /wd4251 /w15038 )
		if(PLATFORM_WARNINGS_AS_ERRORS)
			target_compile_options( ${targname} PRIVATE /WX )
		endif()
		target_link_options( ${targname} PRIVATE /DEBUG )
		add_definitions( -DUNICODE -D_UNICODE -DSIMUL_BUILD_NUMBER=$ENV{SIMUL_BUILD_NUMBER} )
		target_compile_definitions(${targname} PRIVATE PLATFORM_NO_OPTIMIZATION )
	elseif(PLATFORM_LINUX)
		target_compile_definitions(${targname} PRIVATE "PLATFORM_NO_OPTIMIZATION=__attribute__((optnone))" )
		target_compile_options(${targname} PRIVATE -Wall -Wextra -pedantic -Wno-keyword-macro -Wno-ignored-qualifiers -Wno-unused-function -Wno-sign-compare -Wno-gnu-anonymous-struct -Wno-gnu-redeclared-enum -Wno-nested-anon-types -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wno-unused-variable -Wno-unused-value -Wno-missing-braces -Wno-keyword-macro -Wno-language-extension-token -Wno-missing-field-initializers)
		target_compile_options(${targname} PUBLIC -Wall -Wextra -pedantic -Wno-keyword-macro -Wno-ignored-qualifiers -Wno-unused-function -Wno-sign-compare -Wno-gnu-anonymous-struct -Wno-gnu-redeclared-enum -Wno-nested-anon-types -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wno-unused-variable -Wno-unused-value -Wno-missing-braces -Wno-inconsistent-missing-override -Wno-deprecated-declarations)
	elseif(PLATFORM_EMSCRIPTEN)
		target_compile_definitions(${targname} PRIVATE "PLATFORM_NO_OPTIMIZATION=__attribute__((optnone))" )
		set_target_properties(${targname} PROPERTIES LINK_FLAGS "-s USE_WEBGPU -s WASM=1 -s ASSERTIONS=1 -s SAFE_HEAP=1 -s DEMANGLE_SUPPORT=1 -s EXPORTED_RUNTIME_METHODS=['ccall'] ")
		set(PLATFORM_CPP_VERSION 17)
		target_compile_options(${targname} PRIVATE -Wall -DUSE_WEBGPU=1)
		target_compile_options(${targname} PUBLIC -Wall )
		target_compile_options(${targname} PRIVATE -Wall -Wextra -pedantic -Wno-format-pedantic -Wno-keyword-macro -Wno-ignored-qualifiers -Wno-unused-function -Wno-sign-compare -Wno-gnu-anonymous-struct -Wno-gnu-redeclared-enum -Wno-nested-anon-types -Wno-gnu-zero-variadic-macro-arguments -Wno-unused-parameter -Wno-unused-variable -Wno-unused-value -Wno-missing-braces -Wno-keyword-macro -Wno-language-extension-token -Wno-missing-field-initializers)
	else()
		target_compile_options(${targname} PRIVATE -Wall -DUSE_WEBGPU=1 -std:c++${PLATFORM_CPP_VERSION})
		target_compile_options(${targname} PUBLIC -Wall )
	endif()
	set_target_properties(${targname} PROPERTIES CXX_STANDARD ${PLATFORM_CPP_VERSION})
endfunction()

function(ImportedLibraryDefaults target)
	set_target_properties(${target} PROPERTIES IMPORTED_LOCATION_DEBUG "${CMAKE_SOURCE_DIR}/build/lib/Debug/${target}${CMAKE_DEBUG_POSTFIX}${CMAKE_LINK_LIBRARY_SUFFIX}")
	set_target_properties(${target} PROPERTIES IMPORTED_LOCATION_RELEASE "${CMAKE_SOURCE_DIR}/build/lib/Release/${target}${CMAKE_LINK_LIBRARY_SUFFIX}")
	set_target_properties(${target} PROPERTIES IMPORTED_IMPLIB_DEBUG "${target}${CMAKE_DEBUG_POSTFIX}${CMAKE_LINK_LIBRARY_SUFFIX}")
	set_target_properties(${target} PROPERTIES IMPORTED_IMPLIB_RELEASE "${target}${CMAKE_LINK_LIBRARY_SUFFIX}")
	get_target_property( LOD ${target} IMPORTED_LOCATION_DEBUG )
endfunction()

function(add_static_library libname)
	cmake_parse_arguments(lib "" "RUNTIME;FOLDER" "SOURCES;DEFINITIONS;PROPERTIES;INCLUDES;PUBLICINCLUDES;DEPENDENCIES" ${ARGN} )
	if( SIMUL_DYNAMIC_LIBS )
		if(NOT "${lib_RUNTIME}" STREQUAL "MD")
			set( StatDyn Static )
		else()
			set( StatDyn Dynamic )
		endif()
		if("${lib_FOLDER}" STREQUAL "")
			set(lib_FOLDER ${StatDyn})
		else()
			set(lib_FOLDER ${lib_FOLDER}/${StatDyn})
		endif()
	endif()
	if(NOT SIMUL_SOURCE_BUILD)
		set(IMP_LIB IMPORTED GLOBAL )
	else()
		set(IMP_LIB ${lib_SOURCES})
	endif()
	set(LINK_SUFFIX ${STATIC_LINK_SUFFIX})
	if(XBOXONE)
		set(LINK_SUFFIX "_MD" )
	elseif(GDK)
		set(LINK_SUFFIX "_MD" )
	else()
		if("${lib_RUNTIME}" STREQUAL "MD")
			set(LINK_SUFFIX "_MD" )
		endif()
	endif()

	set(target "${libname}${LINK_SUFFIX}")
	add_library(${target} STATIC ${IMP_LIB} )
	if(SIMUL_SOURCE_BUILD)
		if(XBOXONE OR GDK)
			set_target_runtime(${target} dynamic)
			set_target_properties( ${target} PROPERTIES SUFFIX ".lib" )
		endif()
		if(XBOXONE )
			set_property(TARGET ${target} PROPERTY PROJECT_LABEL ${libname}_XboxOne )
			set_target_properties(${target} PROPERTIES VS_GLOBAL_XdkEditionTarget ${XDK_TOOLCHAIN_VERSION})
		endif()
		if(GDK)
			set_property(TARGET ${target} PROPERTY PROJECT_LABEL ${libname}_GDK )
			set_target_properties(${target} PROPERTIES VS_GLOBAL_XdkEditionTarget ${GDK_TOOLCHAIN_VERSION})
		endif()
		LibraryDefaults(${target})
		set(srcs_includes)
		set(srcs_shaders)
		set(srcs_shader_includes)
		foreach(in_f ${lib_SOURCES})
			string(FIND ${in_f} ".h" hpos REVERSE)
			if(NOT hpos EQUAL -1)
				list(APPEND srcs_includes ${in_f})
			endif()
			string(FIND ${in_f} ".sfx" sfxpos REVERSE)
			if(NOT sfxpos EQUAL -1)
				list(APPEND srcs_shaders ${in_f})
			endif()
			string(FIND ${in_f} ".sl" slpos REVERSE)
			if(NOT slpos EQUAL -1)
				list(APPEND srcs_shader_includes ${in_f})
			endif()
		endforeach()
		#foreach(in_f ${srcs_shaders})
		#endforeach()
		set_source_files_properties(${srcs_includes} PROPERTIES HEADER_FILE_ONLY TRUE)
		if( SIMUL_DYNAMIC_LIBS )
			if(NOT "${lib_RUNTIME}" STREQUAL "MD")
	#message("add_static_library ${target} runtime static ")
				set_target_runtime( ${target} static )
				set_target_properties( ${target} PROPERTIES FOLDER ${lib_FOLDER} )
			else()
				set_target_runtime( ${target} dynamic )
				set_target_properties( ${target} PROPERTIES FOLDER ${lib_FOLDER} )
			endif()
		endif()
		if(NOT "${lib_PROPERTIES}" STREQUAL "")
			set_target_properties( ${target} PROPERTIES ${lib_PROPERTIES})
		endif()
		if(NOT "${lib_INCLUDES}" STREQUAL "")
			target_include_directories(${target} PRIVATE ${lib_INCLUDES}
			)
		endif()
		if(NOT "${lib_PUBLICINCLUDES}" STREQUAL "")
			target_include_directories(${target} PUBLIC ${lib_PUBLICINCLUDES}
			)
		endif()
		target_compile_definitions(${target} PRIVATE ${lib_DEFINITIONS})
		install( TARGETS ${target} DESTINATION ${LIBRARY_OUTPUT_PATH} )
		set_target_properties(${target} PROPERTIES PREFIX "" )
		target_link_directories( ${target} PUBLIC ${lib_LINK_DIRS})
		if(NOT "${lib_DEPENDENCIES}" STREQUAL "")
			add_dependencies( ${target} ${lib_DEPENDENCIES} )
			target_link_libraries( ${target} ${lib_DEPENDENCIES} )
		endif()
	else()
		ImportedLibraryDefaults(${target})
	endif()
endfunction()

function(add_dynamic_library libname)
	if( SIMUL_DYNAMIC_LIBS )
		add_dll( ${libname} ${ARGN} )
	endif()
endfunction()

function(add_dll libname)
	set(target "${libname}${DYNAMIC_LINK_SUFFIX}")
	cmake_parse_arguments(dll "" "RUNTIME;FOLDER" "SOURCES;DEFINITIONS;LINK;LINK_DIRS;PROPERTIES;INCLUDES;PUBLICINCLUDES" ${ARGN} )
	if("${dll_FOLDER}" STREQUAL "")
		set(dll_FOLDER Dynamic)
	else()
		set(dll_FOLDER ${dll_FOLDER}/Dynamic)
	endif()
	if(NOT SIMUL_SOURCE_BUILD)
		set(IMP_LIB IMPORTED GLOBAL)
	else()
		set(IMP_LIB ${dll_SOURCES} )
	endif()
	add_library(${target} SHARED ${IMP_LIB})
	if(PLATFORM_WINDOWS)
		set_target_runtime(${target} dynamic)
	endif()
	if(XBOXONE)
		set_target_runtime(${target} dynamic)
		#set_target_properties(${target} PROPERTIES VS_USER_PROPS "${SIMUL_PLATFORM_DIR}/XboxOne/SimulXboxOne.props")
		set_target_properties( ${target} PROPERTIES SUFFIX ".dll" )
		set_property(TARGET ${target} PROPERTY PROJECT_LABEL ${libname}_XboxOne )
		set_target_properties(${target} PROPERTIES ARCHIVE_OUTPUT_NAME "${target}arch" )
		set(DYNLINK "")
	elseif(PLATFORM_COMMODORE)
		string(TOLOWER ${libname} lowercase_libname)
		set_target_properties( ${target} PROPERTIES OUTPUT_NAME "${lowercase_libname}" SUFFIX ".prx" )
	elseif(PLATFORM_PS4)
		set_target_properties( ${target} PROPERTIES SUFFIX ".prx" )
	elseif(PLATFORM_XBOXONE AND GDK)
		set_target_runtime(${target} dynamic)
		set_target_properties(${target} PROPERTIES SUFFIX ".dll" )
		set(DYNLINK "")
	elseif(PLATFORM_SPECTRUM)
		set_target_runtime(${target} dynamic)
		set_target_properties(${target} PROPERTIES SUFFIX ".dll" )
		set(DYNLINK "")
	elseif(PLATFORM_WINGDK)
		set_target_runtime(${target} dynamic)
		set_target_properties(${target} PROPERTIES SUFFIX ".dll" )
		set(DYNLINK "")
	else()
		set(DYNLINK SIMUL_DYNAMIC_LINK=1)
	endif()

	set(srcs_includes)
	foreach(in_f ${dll_SOURCES})
		string(FIND ${in_f} ".h" slpos REVERSE)
		if(NOT slpos EQUAL -1)
			list(APPEND srcs_includes ${in_f})
		endif()
	endforeach()
	set_source_files_properties(${srcs_includes} PROPERTIES HEADER_FILE_ONLY TRUE)
	
	if(SIMUL_SOURCE_BUILD)
		LibraryDefaults(${target})
		#message( "${target} PROPERTIES FOLDER ${dll_FOLDER}/Dynamic")
		set_target_properties( ${target} PROPERTIES FOLDER ${dll_FOLDER})
		if(NOT "${dll_PROPERTIES}" STREQUAL "")
			set_target_properties( ${target} PROPERTIES  ${dll_PROPERTIES})
		endif()
		if(NOT "${dll_INCLUDES}" STREQUAL "")
			target_include_directories(${target} PRIVATE ${dll_INCLUDES} )
		endif()
		if(NOT "${dll_PUBLICINCLUDES}" STREQUAL "")
			target_include_directories(${target} PUBLIC ${dll_PUBLICINCLUDES} )
		endif()
		target_compile_definitions(${target} PRIVATE ${dll_DEFINITIONS} ${DYNLINK} )
		install( TARGETS ${target} DESTINATION ${LIBRARY_OUTPUT_PATH} )
		target_link_libraries( ${target} ${dll_LINK} )
		target_link_directories( ${target} PUBLIC ${dll_LINK_DIRS})
		set_target_properties( ${target} PROPERTIES PREFIX "" )
	else()
		ImportedLibraryDefaults(${target})
	endif()
endfunction()

function(ExecutableDefaults target)
	set_target_properties(${target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                                               VS_DEBUGGER_ENVIRONMENT       "PATH=${fx_path};${SIMUL_DEFAULT_BUILD_PATH}/bin/${CONFIG_NAME};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${CMAKE_BINARY_DIR}\n${VS_DEBUGGER_ENVIRONMENT}")
endfunction()

function(add_static_executable target)
	cmake_parse_arguments(exe "WIN32;CONSOLE" "FOLDER" "SOURCES;DEFINITIONS;DEPENDENCIES;INCLUDES;PUBLICINCLUDES" ${ARGN} )
	if("${exe_FOLDER}" STREQUAL "")
		set(exe_FOLDER Static/Applications)
	else()
		set(exe_FOLDER ${exe_FOLDER}/Static/Applications)
	endif()
	if(${exe_WIN32})
		set(exe_SYSTEM WIN32)
	else()
		set(exe_SYSTEM "")
	endif()
	add_executable(${target} ${exe_SYSTEM} ${exe_SOURCES})
	target_compile_definitions(${target} PRIVATE ${exe_DEFINITIONS})
	if(PLATFORM_WINDOWS)
		set_target_properties( ${target} PROPERTIES SUFFIX ".exe" )
		set_target_properties( ${target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
			VS_DEBUGGER_ENVIRONMENT       "Path=${SIMUL_BUILD_LOCATION}/bin/${CONFIG_NAME};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${SIMUL_BUILD_LOCATION}")
	endif()
	set_target_properties(${target} PROPERTIES FOLDER "${exe_FOLDER}" )
	if(NOT "${exe_DEPENDENCIES}" STREQUAL "")
		add_dependencies( ${target} ${exe_DEPENDENCIES} )
	endif()
	if(NOT "${exe_INCLUDES}" STREQUAL "")
		target_include_directories(${target} PRIVATE ${exe_INCLUDES} )
	endif()
	if(NOT "${exe_PUBLICINCLUDES}" STREQUAL "")
		target_include_directories(${target} PUBLIC ${exe_PUBLICINCLUDES} )
	endif()
	set_target_runtime(${target} static)
	LibraryDefaults(${target})
	ExecutableDefaults(${target})
endfunction()

function(add_dynamic_executable target)
	cmake_parse_arguments(exe "WIN32;CONSOLE" "FOLDER" "SOURCES;DEFINITIONS;LINK;INCLUDES;PUBLICINCLUDES" ${ARGN} )
	if("${exe_FOLDER}" STREQUAL "")
		set(exe_FOLDER Dynamic/Applications)
	else()
		set(exe_FOLDER ${exe_FOLDER}/Dynamic/Applications)
	endif()
	if(${exe_WIN32})
		set(exe_SYSTEM WIN32)
	else()
		set(exe_SYSTEM "")
	endif()
	#message("add_executable(${target} ${WIN32} ${exe_SOURCES})")
	add_executable(${target} ${exe_SYSTEM} ${exe_SOURCES})
	
	target_compile_definitions(${target} PRIVATE ${exe_DEFINITIONS})
	if(PLATFORM_WINDOWS)
		set_target_properties( ${target} PROPERTIES SUFFIX ".exe" )
	endif()
	set_target_runtime(${target} dynamic)
	LibraryDefaults(${target})
	ExecutableDefaults(${target})
	if(NOT "${exe_INCLUDES}" STREQUAL "")
		target_include_directories(${target} PRIVATE ${exe_INCLUDES} )
	endif()
	if(NOT "${exe_PUBLICINCLUDES}" STREQUAL "")
		target_include_directories(${target} PUBLIC ${exe_PUBLICINCLUDES} )
	endif()
	set_target_properties( ${target} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                                               VS_DEBUGGER_ENVIRONMENT       "Path=${SIMUL_BUILD_LOCATION}/bin/${CONFIG_NAME};$(PATH)\nSIMUL=${SIMUL_DIR}\nSIMUL_BUILD=${SIMUL_BUILD_LOCATION}")
	set_target_properties(${target} PROPERTIES FOLDER "${exe_FOLDER}" )
	if(NOT "${exe_LINK}" STREQUAL "")
		target_link_libraries( ${target} ${exe_LINK} )
	endif()
endfunction()