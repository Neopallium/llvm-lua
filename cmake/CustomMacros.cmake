
macro(add_llvm_bc_library _target)
	if(${ARGC} GREATER 2)
		set(_bc_files)
		foreach(_file ${ARGN})
			set(_bc_file "${CMAKE_CURRENT_BINARY_DIR}/${_file}.bc")
			set(_bc_files ${_bc_files} ${_bc_file})
			add_custom_command(OUTPUT ${_bc_file}
				COMMAND ${LLVM_CC} ARGS ${BC_CFLAGS} -o ${_bc_file} ${_file}
				WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
				DEPENDS ${_file}
			)
		endforeach(_file)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_target}.bc
			COMMAND ${LLVM_LINK} ARGS -f -o ${CMAKE_CURRENT_BINARY_DIR}/${_target}.bc ${_bc_files}
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			DEPENDS ${_bc_files}
		)
	else(${ARGC} GREATER 2)
		add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_target}.bc
			COMMAND ${LLVM_CC} ARGS ${BC_CFLAGS} -o ${CMAKE_CURRENT_BINARY_DIR}/${_target}.bc ${ARGV1}
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
			DEPENDS ${ARGV1}
		)
	endif(${ARGC} GREATER 2)
endmacro(add_llvm_bc_library)

macro(add_target_properties _target _name)
	set(_properties)
	foreach(_prop ${ARGN})
		set(_properties "${_properties} ${_prop}")
	endforeach(_prop)
	get_target_property(_old_properties ${_target} ${_name})
	if(NOT _old_properties)
		# in case it's NOTFOUND
		set(_old_properties)
	endif(NOT _old_properties)
	set_target_properties(${_target} PROPERTIES ${_name} "${_old_properties} ${_properties}")
endmacro(add_target_properties)

