cmake_minimum_required(VERSION 3.16)

# Keep this on one line for release checking
project(dbc VERSION 0.5.0 DESCRIPTION "C++ DBC Parser")

# -- PROJECT OPTIONS  -- #
option(DBC_ENABLE_TESTS "Enable Unittests" ON)
option(DBC_ENABLE_EXAMPLES "Build the example programs" ON)
option(DBC_TEST_LOCALE_INDEPENDENCE "Used to deterime if the libary is locale agnostic when it comes to converting floats. You need `de_DE.UTF-8` locale installed for this testing." OFF)
option(DBC_GENERATE_DOCS "Use doxygen if installed to generated documentation files" OFF)
option(DBC_GENERATE_SINGLE_HEADER "This will run the generator for the single header file version. Default is OFF since we make a static build. Requires cargo installed." OFF)
# ---------------------- #

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Standard install directories (CMAKE_INSTALL_LIBDIR / CMAKE_INSTALL_INCLUDEDIR / ...)
include(GNUInstallDirs)

# package
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE)
set(CPACK_RESOURCE_FILE_README ${CMAKE_CURRENT_SOURCE_DIR}/README.md)
include(CPack)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(FastFloat QUIET)
if (NOT ${FastFloat_FOUND})
	if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../fast_float)
		# Sibling checkout of fast_float (repo-local convention)
		add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../fast_float ${CMAKE_CURRENT_BINARY_DIR}/fast_float)
	else()
		# No local copy: fetch fast_float from upstream. Header-only, so this
		# only costs download time and keeps the build self-contained.
		include(FetchContent)
		FetchContent_Declare(fast_float
			GIT_REPOSITORY https://github.com/fastfloat/fast_float.git
			GIT_TAG v7.0.0
		)
		FetchContent_MakeAvailable(fast_float)
	endif()
endif()

# add where to find the source files
list(APPEND SOURCE_FILES
	${PROJECT_SOURCE_DIR}/src/utils.cpp
	${PROJECT_SOURCE_DIR}/src/message.cpp
	${PROJECT_SOURCE_DIR}/src/signal.cpp
	${PROJECT_SOURCE_DIR}/src/dbc.cpp
)

list(APPEND HEADER_FILES
  ${PROJECT_SOURCE_DIR}/include/libdbc/dbc.hpp
  ${PROJECT_SOURCE_DIR}/include/libdbc/message.hpp
  ${PROJECT_SOURCE_DIR}/include/libdbc/signal.hpp
  ${PROJECT_SOURCE_DIR}/include/libdbc/utils/utils.hpp
  ${PROJECT_SOURCE_DIR}/include/libdbc/exceptions/error.hpp
)

if(DBC_ENABLE_TESTS)
	include(CTest)
	add_subdirectory(test)
endif()

if(DBC_ENABLE_EXAMPLES)
	add_subdirectory(examples)
endif()

if(DBC_GENERATE_DOCS)
	add_subdirectory(doc)
endif()

list(APPEND GCC_CLANG_COMPILE_FLAGS
	-Wall -Wextra -Wpedantic
	-Wconversion -Wint-in-bool-context
	-Wmissing-declarations -Wmissing-field-initializers
	-Werror
	-Wno-unused-const-variable
	-Wno-sign-conversion
)


if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	add_compile_options(/W4 /WX)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	# Clang shadow warnings aren't as sensitive as gcc
	add_compile_options(${GCC_CLANG_COMPILE_FLAGS} -Wshadow)
else()
	add_compile_options(${GCC_CLANG_COMPILE_FLAGS})
endif()

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES})
# fast_float is header-only and only used in dbc's sources (not in its public
# headers), so link it privately to keep the installed target dependency-free.
target_link_libraries(${PROJECT_NAME} PRIVATE FastFloat::fast_float)
# Re-add fast_float's include dir as a SYSTEM include so compiler warnings and
# clang-tidy's static analyzer (which skips system headers by default) don't
# flag issues in third-party code we don't control.
get_target_property(FASTFLOAT_INTERFACE_INCLUDE_DIRS FastFloat::fast_float INTERFACE_INCLUDE_DIRECTORIES)
if(FASTFLOAT_INTERFACE_INCLUDE_DIRS)
	target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE ${FASTFLOAT_INTERFACE_INCLUDE_DIRS})
endif()
target_include_directories(${PROJECT_NAME} PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>
)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)

# List the headers as private sources (for IDEs / clang-tidy). They must not be
# INTERFACE sources: installed targets cannot reference source-dir paths.
target_sources(${PROJECT_NAME} PRIVATE ${HEADER_FILES})

if(DBC_GENERATE_SINGLE_HEADER)
	find_program(BASH_PROGRAM bash)
	add_custom_target(single_header ALL
						WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
						COMMAND ${BASH_PROGRAM} ${CMAKE_SOURCE_DIR}/scripts/create_single_header.sh
	)
endif()

## Installation
include(CMakePackageConfigHelpers)

# install lib + export for find_package(dbc)
install(TARGETS ${PROJECT_NAME}
	EXPORT dbcTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# install headers
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/libdbc DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Generate pkg-config file
configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc @ONLY)
install(
	FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# CMake package config so consumers can use find_package(dbc)
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion)

configure_package_config_file(
	${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
	${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(EXPORT dbcTargets
	NAMESPACE Libdbc::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

# Clang tidy
add_custom_target(clang-tidy-check
	clang-tidy -p ${CMAKE_BINARY_DIR}/compile_commands.json -warnings-as-errors=* ${SOURCE_FILES} ${HEADER_FILES}
	DEPENDS ${SOURCE_FILES} ${HEADER_FILES}
)

add_custom_target(clang-tidy-dump
	clang-tidy -checks=-*,clang-analyzer-*,clang-analyzer-cplusplus*,cert-*,cppcoreguidelines-*,portability-*,readability-*,clang-diagnostic-* -dump-config -p ${CMAKE_BINARY_DIR}/compile_commands.json ${SOURCE_FILES} ${HEADER_FILES} > ../.clang-tidy
	DEPENDS ${SOURCE_FILES} ${HEADER_FILES}
)

add_custom_target(clang-tidy-fix
	clang-tidy -fix-notes -p ${CMAKE_BINARY_DIR}/compile_commands.json ${SOURCE_FILES} ${HEADER_FILES}
	DEPENDS ${SOURCE_FILES} ${HEADER_FILES}
)
