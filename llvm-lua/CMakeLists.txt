
#
# llvm-lua source
#
set(NON_LLVM_SRC
	lua_core.c
	lua_vm_ops_static.c
	load_jit_proto.c
)
set(LLVM_COMMON_SRC
	LLVMCompiler.cpp
	llvm_compiler.cpp
	load_embedded_bc.cpp
	load_vm_ops.cpp
	lua_vm_ops_bc.h
	${NON_LLVM_SRC}
)
set(NO_JIT_SRC
	no_jit.c
	${NON_LLVM_SRC}
)
set(EMB_BCS
	lua_vm_ops_bc.h
	liblua_main_bc.h
)
set(LLVM_LUA_SRC
	llvm-lua.cpp
	lua_interpreter.c
)
set(LLVM_LUAC_SRC
	llvm-luac.cpp
	lua_compiler.c
	LLVMDumper.cpp
	llvm_dumper.cpp
	load_liblua_main.cpp
	liblua_main_bc.h
)
set(LLVM_LUA_LIB_SRC
	lua_interpreter.c
	${LLVM_COMMON_SRC}
)
set(LLVM_LUA_BINS)
set(LUA_COMPILER)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
				${PROJECT_SOURCE_DIR}/src
				${CMAKE_CURRENT_BINARY_DIR})

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/llvm_lua_config.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/llvm_lua_config.h"
	@ONLY
)

add_executable(bin2c bin2c.c)

# cflags for LLVM bitcode files.
string(REPLACE " " ";" BC_CFLAGS ${COMMON_CFLAGS})
# setup cross-compiler flags.
if(CROSS_COMPILE)
	if(NOT CROSS_ISYSTEM STREQUAL "")
		set(BC_CFLAGS ${BC_CFLAGS} -nostdinc -isystem=${CROSS_ISYSTEM})
	endif(NOT CROSS_ISYSTEM STREQUAL "")
	set(BC_CFLAGS ${BC_CFLAGS} -O2 -DFORCE_ALIGNMENT=${FORCE_ALIGNMENT})
	set(BC_CFLAGS ${BC_CFLAGS} -ccc-host-triple ${CROSS_TRIPLE} -march=${CROSS_CPU} -emit-llvm -I../src)
	# don't need shared library support for cross-compiler
	set(WANT_SHARED_LIBRARY FALSE)
else(CROSS_COMPILE)
	set(BC_CFLAGS ${BC_CFLAGS} -std=gnu89 -c -emit-llvm -I../src)
endif(CROSS_COMPILE)

#
# Embedded LLVM bitcode files
#
add_llvm_bc_library(lua_vm_ops lua_vm_ops.c)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lua_vm_ops_bc.h
	COMMAND bin2c -z -c ${CMAKE_CURRENT_BINARY_DIR}/lua_vm_ops.bc ${CMAKE_CURRENT_BINARY_DIR}/lua_vm_ops_bc.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS bin2c ${CMAKE_CURRENT_BINARY_DIR}/lua_vm_ops.bc
)
add_llvm_bc_library(liblua_main lua_main.c ${NO_JIT_SRC})
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/liblua_main_bc.h
	COMMAND bin2c -z -c ${CMAKE_CURRENT_BINARY_DIR}/liblua_main.bc ${CMAKE_CURRENT_BINARY_DIR}/liblua_main_bc.h
	WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
	DEPENDS bin2c ${CMAKE_CURRENT_BINARY_DIR}/liblua_main.bc
)

#
# Lua library.
#
set(LLVM_LUA_LIBS)

if(WANT_STATIC_LIBRARY)
	# llvm-lua static library.
	add_library(llvm-lua_static STATIC ${LLVM_LUA_LIB_SRC})
	add_target_properties(llvm-lua_static COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
	add_target_properties(llvm-lua_static LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS} ${LLVM_LFLAGS}")
	target_link_libraries(llvm-lua_static ${LIBS} ${LLVM_ALL_LIBS})

	# link lua interpreter with static library
	set(LLVM_LUA_MAIN_LIB llvm-lua_static)
	set(LLVM_LUA_LIBS ${LLVM_LUA_LIBS} llvm-lua_static)

	if (WANT_NOJIT_LIBRARIES)
		# lua static library without LLVM JIT support.
		add_library(llvm-lua_nojit_static STATIC ${NO_JIT_SRC})
		add_target_properties(llvm-lua_nojit_static COMPILE_FLAGS "${COMMON_CFLAGS}")
		add_target_properties(llvm-lua_nojit_static LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS}")
		target_link_libraries(llvm-lua_nojit_static ${LIBS})

		set(LLVM_LUA_LIBS ${LLVM_LUA_LIBS} llvm-lua_nojit_static)
	endif (WANT_NOJIT_LIBRARIES)
endif(WANT_STATIC_LIBRARY)

if(WANT_SHARED_LIBRARY)
	# llvm-lua dynamic library
	add_library(llvm-lua_dynamic SHARED ${LLVM_LUA_LIB_SRC})
	add_target_properties(llvm-lua_dynamic COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
	add_target_properties(llvm-lua_dynamic LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS} ${LLVM_LFLAGS}")
	target_link_libraries(llvm-lua_dynamic ${LIBS} ${LLVM_ALL_LIBS})
	set_target_properties(llvm-lua_dynamic PROPERTIES OUTPUT_NAME llvm-lua)

	# link lua interpreter with dynamic library
	set(LLVM_LUA_MAIN_LIB llvm-lua_dynamic)
	set(LLVM_LUA_LIBS ${LLVM_LUA_LIBS} llvm-lua_dynamic)

	if (WANT_NOJIT_LIBRARIES)
		# lua dynamic library without LLVM JIT support.
		add_library(llvm-lua_nojit_dynamic SHARED ${NO_JIT_SRC})
		add_target_properties(llvm-lua_nojit_dynamic COMPILE_FLAGS "${COMMON_CFLAGS}")
		add_target_properties(llvm-lua_nojit_dynamic LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS}")
		target_link_libraries(llvm-lua_nojit_dynamic ${LIBS})
		set_target_properties(llvm-lua_nojit_dynamic PROPERTIES OUTPUT_NAME llvm-lua_nojit)

		set(LLVM_LUA_LIBS ${LLVM_LUA_LIBS} llvm-lua_nojit_dynamic)
	endif (WANT_NOJIT_LIBRARIES)
endif(WANT_SHARED_LIBRARY)

#
# static library liblua_main.a
#
if(NOT CROSS_COMPILE)
	add_library(llvm-lua_main STATIC lua_main.c ${NO_JIT_SRC})
	add_target_properties(llvm-lua_main COMPILE_FLAGS "${COMMON_CFLAGS}")
	add_target_properties(llvm-lua_main LINK_FLAGS "${LD_FLAGS} ${COMMON_LDFLAGS}")

	set(LLVM_LUA_LIBS ${LLVM_LUA_LIBS} llvm-lua_main)
endif(NOT CROSS_COMPILE)

# set library version & soversion
if(LLVM_LUA_LIBS)
	set_target_properties(${LLVM_LUA_LIBS} PROPERTIES
		VERSION ${LLVM_LUA_VERSION}
		SOVERSION ${LLVM_LUA_SOVERSION}
		CLEAN_DIRECT_OUTPUT 1
	)
endif(LLVM_LUA_LIBS)

# set rpath & handle framework
if(APPLE)
	set(INSTALL_NAME_DIR)
	# Set Dynamic Library and Framework properties
	if(WANT_FRAMEWORK)
		set_target_properties(llvm-lua_dynamic PROPERTIES
			FRAMEWORK TRUE
			FRAMEWORK_VERSION "${LUA_FRAMEWORK_VERSION_NUMBER}"
			PUBLIC_HEADER "${LUA_LIB_HEADERS}"
			RESOURCE "${RESOURCE_FILES}"
		)
		set(INSTALL_NAME_DIR ${CMAKE_FRAMEWORK_INSTALL_NAME_DIR})

		# Short Version is the "marketing version". It is the version
		# the user sees in an information panel.
		set(MACOSX_FRAMEWORK_SHORT_VERSION_STRING
						"${LLVM_LUA_VERSION_MAJOR}.${LLVM_LUA_VERSION_MINOR}.${LLVM_LUA_VERSION_PATCH}")
		# Bundle version is the version the OS looks at.
		set(MACOSX_FRAMEWORK_BUNDLE_VERSION
						"${LLVM_LUA_VERSION_MAJOR}.${LLVM_LUA_VERSION_MINOR}.${LLVM_LUA_VERSION_PATCH}")
		set(MACOSX_FRAMEWORK_IDENTIFIER "org.lua.llvm-lua")
	elseif(WANT_SHARED_LIBRARY)
		set_target_properties(llvm-lua_dynamic PROPERTIES
			FRAMEWORK FALSE
		)
		set(INSTALL_NAME_DIR ${CMAKE_DYLIB_INSTALL_NAME_DIR})
	endif(WANT_FRAMEWORK)

	if(INSTALL_NAME_DIR)
		set_target_properties(llvm-lua_dynamic PROPERTIES
			INSTALL_NAME_DIR ${INSTALL_NAME_DIR}
			BUILD_WITH_INSTALL_RPATH ${CMAKE_BUILD_WITH_INSTALL_RPATH}
		)
	else(INSTALL_NAME_DIR)
		# no install_name folder force BUILD_WITH_INSTALL_RPATH to OFF
		set_property(TARGET llvm-lua_dynamic PROPERTY BUILD_WITH_INSTALL_RPATH OFF)
	endif(INSTALL_NAME_DIR)

	set(CMAKE_OSX_ARCHITECTURES ${OSX_ARCHITECTURES})
elseif(WANT_SHARED_LIBRARY)
	set_target_properties(llvm-lua_dynamic PROPERTIES
		INSTALL_RPATH ${CMAKE_INSTALL_RPATH}
		BUILD_WITH_INSTALL_RPATH ${CMAKE_BUILD_WITH_INSTALL_RPATH}
	)
endif(APPLE)

#
# Lua compiler
#
add_executable(llvm-luac ${LLVM_LUAC_SRC})
add_target_properties(llvm-luac COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
add_target_properties(llvm-luac LINK_FLAGS
	"${LD_FLAGS} ${COMMON_LDFLAGS} ${LLVM_LFLAGS}")
target_link_libraries(llvm-luac ${LLVM_LUA_MAIN_LIB} ${LLVM_ALL_LIBS})
set(LLVM_LUA_BINS llvm-luac)

#
# build as a cross-compiler
#
if(CROSS_COMPILE)
	set(LUA_COMPILER "${CMAKE_CURRENT_BINARY_DIR}/lua-cross-compiler")
	set_target_properties(llvm-luac PROPERTIES OUTPUT_NAME ${CROSS_TRIPLE}-llvm-luac)
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lua-cross-compiler.in" "${LUA_COMPILER}" @ONLY)
else(CROSS_COMPILE)
	set(LUA_COMPILER "${CMAKE_CURRENT_BINARY_DIR}/lua-compiler")
	configure_file("${CMAKE_CURRENT_SOURCE_DIR}/lua-compiler.in" "${LUA_COMPILER}" @ONLY)
endif(CROSS_COMPILE)

#
# Lua stand-alone interpreter
#
if(NOT CROSS_COMPILE)
	add_executable(llvm-lua_binary ${LLVM_LUA_SRC})
	add_target_properties(llvm-lua_binary COMPILE_FLAGS "${COMMON_CFLAGS} ${LLVM_CFLAGS}")
	add_target_properties(llvm-lua_binary LINK_FLAGS
		"${LD_FLAGS} ${COMMON_LDFLAGS} ${LLVM_LFLAGS}")
	target_link_libraries(llvm-lua_binary ${LLVM_LUA_MAIN_LIB} ${LLVM_JIT_LIBS})
	# rename llvm-lua.bin to llvm-lua
	set_target_properties(llvm-lua_binary PROPERTIES OUTPUT_NAME llvm-lua)
	set(LLVM_LUA_BINS ${LLVM_LUA_BINS} llvm-lua_binary)
endif(NOT CROSS_COMPILE)

#
# install llvm-lua/llvm-luac & library.
#
if(APPLE AND WANT_SHARED_LIBRARY AND WANT_FRAMEWORK)
	# Make directory: llvm-lua.framework/Versions/5.1/MacOS
	add_custom_command(
		TARGET llvm-lua_dynamic
		POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		ARGS -E make_directory "${llvm_lua_BINARY_DIR}/${LUA_FRAMEWORK_NAME}/${LUA_FRAMEWORK_VERSIONED_EXECUTABLE_DIR}"
		COMMENT "Command line tools")

	# Make directory: llvm-lua.framework/Versions/5.1/lib
	add_custom_command(
		TARGET llvm-lua_dynamic
		POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		ARGS -E make_directory "${llvm_lua_BINARY_DIR}/${LUA_FRAMEWORK_NAME}/${LUA_FRAMEWORK_VERSIONED_LIB_DIR}"
		COMMENT "Static lib")

	# Copy llvm-lua to: llvm-lua.framework/Versions/5.1/MacOS/llvm-lua
	add_custom_command(
		TARGET llvm-lua_dynamic llvm-lua_binary
		POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		ARGS -E copy "${llvm_lua_BINARY_DIR}/llvm-lua" "${llvm_lua_BINARY_DIR}/${LUA_FRAMEWORK_NAME}/${LUA_FRAMEWORK_VERSIONED_EXECUTABLE_DIR}/llvm-lua"
		COMMENT "Command line tools")

	# Copy llvm-luac to: llvm-lua.framework/Versions/5.1/MacOS/llvm-luac
	add_custom_command(
		TARGET llvm-lua_dynamic llvm-luac
		POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		ARGS -E copy ${llvm_lua_BINARY_DIR}/llvm-luac ${llvm_lua_BINARY_DIR}/${LUA_FRAMEWORK_NAME}/${LUA_FRAMEWORK_VERSIONED_EXECUTABLE_DIR}/llvm-luac
		COMMENT "Command line tools")

	# Copy liblua.a to: llvm-lua.framework/Versions/5.1/lib/liblua.a
	add_custom_command(
		TARGET llvm-lua_dynamic llvm-lua_static
		POST_BUILD
		COMMAND ${CMAKE_COMMAND}
		ARGS -E copy "${llvm_lua_BINARY_DIR}/liblua.a" "${llvm_lua_BINARY_DIR}/${LUA_FRAMEWORK_NAME}/${LUA_FRAMEWORK_VERSIONED_LIB_DIR}/liblua.a"
		COMMENT "Static lib")

	# Will install framework to /Library/Frameworks directory or user specified
	install(TARGETS llvm-lua_dynamic
		FRAMEWORK DESTINATION ${CMAKE_FRAMEWORK_INSTALL_DIR}
	)

	if(LUA_FRAMEWORK_SYMLINKS)
		install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E make_directory \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin\")")

		# create a symbolic link for llvm-lua
		# and add it to the install manifest
		install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E create_symlink \"${CMAKE_FRAMEWORK_INSTALL_DIR}/${LUA_FRAMEWORK_NAME}/${LUA_FRAMEWORK_CURRENT_EXECUTABLE_DIR}/llvm-lua\" \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/lua\")
		list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"${CMAKE_INSTALL_PREFIX}/bin/llvm-lua\")
		")

		# create a symbolic link for llvm-luac
		# and add it to the install manifest
		install(CODE "EXEC_PROGRAM(${CMAKE_COMMAND} ARGS -E create_symlink \"${CMAKE_FRAMEWORK_INSTALL_DIR}/${LUA_FRAMEWORK_NAME}/${LUA_FRAMEWORK_CURRENT_EXECUTABLE_DIR}/llvm-luac\" \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/luac\")
		list(APPEND CMAKE_INSTALL_MANIFEST_FILES \"${CMAKE_INSTALL_PREFIX}/bin/llvm-luac\")
		")
	endif(LUA_FRAMEWORK_SYMLINKS)

else(APPLE AND WANT_SHARED_LIBRARY AND WANT_FRAMEWORK)
	install(TARGETS ${LLVM_LUA_BINS}
		RUNTIME DESTINATION bin
	)
	install(FILES ${LUA_COMPILER}
		DESTINATION bin
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
		WORLD_READ WORLD_EXECUTE
	)

	if(LLVM_LUA_LIBS)
		install(TARGETS ${LLVM_LUA_LIBS}
			LIBRARY DESTINATION lib
			ARCHIVE DESTINATION lib
		)
	endif(LLVM_LUA_LIBS)

endif(APPLE AND WANT_SHARED_LIBRARY AND WANT_FRAMEWORK)

