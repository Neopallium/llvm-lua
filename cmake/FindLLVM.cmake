# - Find libev
# Find the native LLVM includes and library
#
#  LLVM_INCLUDE_DIR - where to find ev.h, etc.
#  LLVM_LIBRARIES   - List of libraries when using libev.
#  LLVM_FOUND       - True if libev found.

find_program(LLVM_CONFIG_EXECUTABLE NAMES llvm-config DOC "llvm-config executable")

execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --cppflags
	OUTPUT_VARIABLE LLVM_CFLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --ldflags
	OUTPUT_VARIABLE LLVM_LFLAGS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --libs core jit native linker bitreader bitwriter ipo
	OUTPUT_VARIABLE LLVM_JIT_LIBS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
	COMMAND ${LLVM_CONFIG_EXECUTABLE} --libs all
	OUTPUT_VARIABLE LLVM_ALL_LIBS
	OUTPUT_STRIP_TRAILING_WHITESPACE
)
message("llvm jit libs: " ${LLVM_JIT_LIBS})
message("llvm all libs: " ${LLVM_ALL_LIBS})

