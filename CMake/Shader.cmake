

function ( add_sfx_shader_project targetName configJsonFile )
	if(SIMUL_BUILD_SHADERS)
		cmake_parse_arguments(sfx "" "INTERMEDIATE;OUTPUT;FOLDER" "INCLUDES;SOURCES;OPTIONS;DEFINES" ${ARGN} )
		if (NOT TARGET ${targetName})
			if("${sfx_FOLDER}" STREQUAL "")
				set(sfx_FOLDER Shaders)
			endif()
		endif()
		set(SET_DEFINES -E\"PLATFORM=${SIMUL_PLATFORM_DIR}\" )
		foreach(opt_d ${sfx_DEFINES})
			set(SET_DEFINES "${SET_DEFINES} -E\"${opt_d}\"" )
		endforeach() 
		set(EXTRA_OPTS)
		foreach(opt_in ${sfx_OPTIONS})
			set(EXTRA_OPTS "${EXTRA_OPTS} ${opt_in}" )
		endforeach()
		set(INCLUDE_OPTS)
		foreach(incl_path ${sfx_INCLUDES})
			set(INCLUDE_OPTS ${INCLUDE_OPTS} -I"${incl_path}" )
		endforeach()
		if(NOT "${EXTRA_OPTS}" STREQUAL "")
			string(REPLACE "\"" "" EXTRA_OPTS ${EXTRA_OPTS})
		endif()
		set(EXTRA_OPTS "${EXTRA_OPTS} ${SET_DEFINES}")
		separate_arguments(EXTRA_OPTS_S WINDOWS_COMMAND "${EXTRA_OPTS}")
		if(SIMUL_DEBUG_SHADERS)
			set(EXTRA_OPTS_S ${EXTRA_OPTS_S} -v -d)
		endif()
		set(srcs_includes)
		set(srcs_shaders)
		set(srcs)
		set(outputs${targetName})
		set(out_folder ${sfx_OUTPUT})
		get_filename_component(PLATFORM_NAME ${configJsonFile} NAME )
		string(REPLACE ".json" "" PLATFORM_NAME ${PLATFORM_NAME})
		string(REPLACE "$PLATFORM_NAME" ${PLATFORM_NAME} out_folder ${out_folder})
		#message("PLATFORM_NAME ${PLATFORM_NAME}")
		#message("out_folder ${out_folder}")
		if("${sfx_INTERMEDIATE}" STREQUAL "")
			set(intermediate_folder "${CMAKE_CURRENT_BINARY_DIR}/sfx_intermediate")
		else()
			set(intermediate_folder ${sfx_INTERMEDIATE})
		endif()
		foreach(in_f ${sfx_SOURCES})
			list(APPEND srcs ${in_f})
			string(FIND ${in_f} ".sl" slpos REVERSE)
			if(NOT slpos EQUAL -1)
				list(APPEND srcs_includes ${in_f}) 
			endif()
			string(FIND ${in_f} ".sfx" pos REVERSE)
			if(NOT ${pos} EQUAL -1)
				list(APPEND srcs_shaders ${in_f})
				get_filename_component(name ${in_f} NAME )
				string(REPLACE ".sfx" ".sfxo" out_f ${name})
				set(out_f "${out_folder}/${out_f}")
				add_custom_command(OUTPUT ${out_f}
					COMMAND "${PLATFORM_SFX_EXECUTABLE}" ${in_f} ${INCLUDE_OPTS} -O"${out_folder}" -P"${configJsonFile}" ${EXTRA_OPTS_S}
					MAIN_DEPENDENCY ${in_f}
					WORKING_DIRECTORY ${out_folder}
					DEPENDS ${PLATFORM_SFX_EXECUTABLE}
					)
				list(APPEND outputs${targetName} ${out_f})
			else()
				if(MSVC)
					set_source_files_properties( ${in_f} PROPERTIES HEADER_FILE_ONLY ON )
				endif()
			endif()
		endforeach()
		source_group("Shaders" FILES  ${srcs_shaders} )
		source_group("Shader Includes" FILES ${srcs_includes} )
		if (NOT TARGET ${targetName})
			add_custom_target(${targetName} DEPENDS ${outputs${targetName}} SOURCES ${srcs} ${configJsonFile} )
			set_target_properties( ${targetName} PROPERTIES FOLDER ${sfx_FOLDER})
		endif()
		set_target_properties( ${targetName} PROPERTIES EXCLUDE_FROM_ALL FALSE )
		set_target_properties( ${targetName} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG FALSE )
		set_target_properties( ${targetName} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE FALSE )
		if((PLATFORM_WINDOWS OR PLATFORM_LINUX) AND SIMUL_SOURCE_BUILD)
			add_dependencies( ${targetName} Sfx )
		endif()
		set(${out_var} "${outputs${targetName}}" PARENT_SCOPE)
	endif()
endfunction()


function ( add_multiplatform_sfx_shader_project targetName )
	if(SIMUL_BUILD_SHADERS)
		cmake_parse_arguments(sfx "" "INTERMEDIATE;OUTPUT;FOLDER" "INCLUDES;SOURCES;OPTIONS;DEFINES;CONFIG_FILES" ${ARGN} )
		# default include paths
		set(sfx_INCLUDES ${sfx_INCLUDES} "${SIMUL_PLATFORM_DIR}/CrossPlatform/Shaders")
		if (NOT TARGET ${targetName})
			if("${sfx_FOLDER}" STREQUAL "")
				set(sfx_FOLDER Shaders)
			endif()
		endif()
		set(SET_DEFINES -E\"PLATFORM=${SIMUL_PLATFORM_DIR}\" )
		foreach(opt_d ${sfx_DEFINES})
			set(SET_DEFINES "${SET_DEFINES} -E\"${opt_d}\"" )
		endforeach() 
		set(SET_CONFIGS)
		if("${sfx_CONFIG_FILES}" STREQUAL "")
			foreach(GRAPHICS_API ${PLATFORM_GRAPHICS_APIS})
				set( sfx_CONFIG_FILES ${sfx_CONFIG_FILES} "${SIMUL_PLATFORM_DIR}/${GRAPHICS_API}/Sfx/${GRAPHICS_API}.json" )
			endforeach()
		endif()
		#message("sfx_CONFIG_FILES ${sfx_CONFIG_FILES}")
		foreach(opt_cfg ${sfx_CONFIG_FILES})
			set(SET_CONFIGS "${SET_CONFIGS} -P\"${opt_cfg}\"" )
		endforeach() 
		if(NOT "${SET_CONFIGS}" STREQUAL "")
			#message("SET_CONFIGS ${SET_CONFIGS}")
			set(EXTRA_OPTS)
			foreach(opt_in ${sfx_OPTIONS})
				set(EXTRA_OPTS "${EXTRA_OPTS} ${opt_in}" )
			endforeach()
			set(INCLUDE_OPTS)
			foreach(incl_path ${sfx_INCLUDES})
				set(INCLUDE_OPTS ${INCLUDE_OPTS} -I"${incl_path}" )
			endforeach()
			if(NOT "${EXTRA_OPTS}" STREQUAL "")
				string(REPLACE "\"" "" EXTRA_OPTS ${EXTRA_OPTS})
			endif()
			set(EXTRA_OPTS "${EXTRA_OPTS} ${SET_DEFINES}")
			separate_arguments(EXTRA_OPTS_S WINDOWS_COMMAND "${EXTRA_OPTS}")
			if(SIMUL_DEBUG_SHADERS)
				set(EXTRA_OPTS_S ${EXTRA_OPTS_S} -v -d)
			endif()
			set(srcs_includes)
			set(srcs_shaders)
			set(srcs)
			set(outputs${targetName})
			string(REPLACE "$PLATFORM_NAME" "" out_folder ${sfx_OUTPUT})
			if("${sfx_INTERMEDIATE}" STREQUAL "")
				set(intermediate_folder "${CMAKE_CURRENT_BINARY_DIR}/sfx_intermediate")
			else()
				set(intermediate_folder ${sfx_INTERMEDIATE})
			endif()
			foreach(in_f ${sfx_SOURCES})
				list(APPEND srcs ${in_f})
				string(FIND ${in_f} ".sl" slpos REVERSE)
				if(NOT slpos EQUAL -1)
					list(APPEND srcs_includes ${in_f}) 
				endif()
				string(FIND ${in_f} ".sfx" pos REVERSE)
				if(NOT ${pos} EQUAL -1)
					list(APPEND srcs_shaders ${in_f})
					get_filename_component(name ${in_f} NAME )
					string(REPLACE ".sfx" ".sfxo" out_f ${name})
					set(out_f "${out_folder}/${out_f}")
					string(REPLACE ".sfxo" ".sfx_summary" main_output_file ${out_f})
					#message("main_output_file ${main_output_file}")
					add_custom_command(OUTPUT ${main_output_file}
						COMMAND "${PLATFORM_SFX_EXECUTABLE}" ${in_f} ${INCLUDE_OPTS} -O"${sfx_OUTPUT}" ${SET_CONFIGS} ${EXTRA_OPTS_S}
						MAIN_DEPENDENCY ${in_f}
						WORKING_DIRECTORY ${out_folder}
						DEPENDS ${PLATFORM_SFX_EXECUTABLE}
						COMMENT "info: Sfx compiling ${in_f}"
						)
					list(APPEND outputs${targetName} ${out_f})
				else()
					if(MSVC)
						set_source_files_properties( ${in_f} PROPERTIES HEADER_FILE_ONLY ON )
					endif()
				endif()
			endforeach()
			source_group("Shaders" FILES  ${srcs_shaders} )
			source_group("Shader Includes" FILES ${srcs_includes} )
			if (NOT TARGET ${targetName})
				add_custom_target(${targetName} DEPENDS ${outputs${targetName}} SOURCES ${srcs} ${configJsonFile} )
				set_target_properties( ${targetName} PROPERTIES FOLDER ${sfx_FOLDER})
			endif()
			set_target_properties( ${targetName} PROPERTIES EXCLUDE_FROM_ALL FALSE )
			set_target_properties( ${targetName} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_DEBUG FALSE )
			set_target_properties( ${targetName} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD_RELEASE FALSE )
			if((PLATFORM_WINDOWS OR PLATFORM_LINUX) AND SIMUL_SOURCE_BUILD)
				add_dependencies( ${targetName} Sfx )
			endif()
			set(${out_var} "${outputs${targetName}}" PARENT_SCOPE)
		endif()
	endif()
endfunction()

