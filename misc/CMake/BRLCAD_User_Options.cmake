#       B R L C A D _ U S E R _ O P T I O N S . C M A K E
# BRL-CAD
#
# Copyright (c) 2020-2021 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###
# Now we define the various options for BRL-CAD - ways to enable and disable
# features, select which parts of the system to build, etc.  As much as
# possible, sane default options are either selected or detected.  Because
# documentation is autogenerated for BRL-CAD options, be sure to initialize the
# file.
set(CONFIG_OPT_STRING "CONFIGURATION OPTIONS\n---------------------\n")
file(WRITE "${CMAKE_BINARY_DIR}/BRLCAD_OPTIONS" "${CONFIG_OPT_STRING}")


# Build shared libs by default.  Mark this as advanced - turning off ALL shared
# library building is unlikely to result in a working build and is not a
# typical configuration.  Note that turning this option off will not disable
# libraries specifically added as SHARED.
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
mark_as_advanced(BUILD_SHARED_LIBS)

# Build static libs by default unless we're debugging. Note: this option will
# not disable libraries specifically added as STATIC even when OFF.  For
# multi-configuration options the build type determination is made at build
# time, so just default to "ON".
cmake_dependent_option(BUILD_STATIC_LIBS "Build static libraries" ON "CMAKE_CONFIGURATION_TYPES OR NOT \"${CMAKE_BUILD_TYPE}\" STREQUAL Debug" OFF)

# Turn off the brlcad.dll build.
# It's an expert's setting at the moment.
option(BRLCAD_ENABLE_BRLCAD_LIBRARY "Build the brlcad.dll" OFF)
mark_as_advanced(BRLCAD_ENABLE_BRLCAD_LIBRARY)

# Global third party controls - these options enable and disable ALL local
# copies of libraries in src/other.  Forcing all local libraries off is not
# usually recommended unless attempting to build packages for a distribution.
# If both of these options are on the enabling of local packages is the
# "senior" option and will force the system libs option to off.
set(BRLCAD_BUNDLED_LIBS_DESCRIPTION "
Enables compilation of all 3rd party sources that are provided within a BRL-CAD
source distribution.  If used this option sets all other 3rd party library
build flags to ON by default.  However, that setting can be overridden by
manually setting individual variables. Default is \"AUTO\" - 3rd party sources
are compiled only if they are not detected as being available and functioning
as expected.
")

if(MSVC)
  set(BRLCAD_BUNDLED_LIBS_DEFAULT "BUNDLED")
else(MSVC)
  set(BRLCAD_BUNDLED_LIBS_DEFAULT "AUTO")
endif(MSVC)
BRLCAD_OPTION(BRLCAD_BUNDLED_LIBS ${BRLCAD_BUNDLED_LIBS_DEFAULT}
  TYPE ABS
  ALIASES ENABLE_ALL
  DESCRIPTION BRLCAD_BUNDLED_LIBS_DESCRIPTION)
if(NOT BRLCAD_BUNDLED_LIBS MATCHES "AUTO" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "BUNDLED" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "SYSTEM")
  message(WARNING "Unknown value BRLCAD_BUNDLED_LIBS supplied for BRLCAD_BUNDLED_LIBS (${BRLCAD_BUNDLED_LIBS}) - defaulting to AUTO")
  message(WARNING "Valid options are AUTO, BUNDLED and SYSTEM")
  set(BRLCAD_BUNDLED_LIBS "AUTO" CACHE STRING "Build bundled libraries." FORCE)
endif(NOT BRLCAD_BUNDLED_LIBS MATCHES "AUTO" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "BUNDLED" AND NOT BRLCAD_BUNDLED_LIBS MATCHES "SYSTEM")

# Single flag for disabling GUI instead of 5.
option(BRLCAD_ENABLE_MINIMAL "Skip GUI support and docs. Faster builds." OFF)
mark_as_advanced(BRLCAD_ENABLE_MINIMAL)

if(BRLCAD_ENABLE_MINIMAL)
  set(BRLCAD_ENABLE_OPENGL "OFF")
  set(BRLCAD_ENABLE_X11 "OFF")
  set(BRLCAD_ENABLE_TK "OFF")
  set(BRLCAD_ENABLE_QT "OFF")
  set(BRLCAD_ENABLE_AQUA "OFF")
  set(BRLCAD_EXTRADOCS "OFF")
endif(BRLCAD_ENABLE_MINIMAL)

# Enable Aqua widgets on Mac OSX.  This impacts Tcl/Tk building and OpenGL
# building. Not currently working - needs work in at least Tk CMake logic
# (probably more), and the display manager/framebuffer codes are known to
# depend on either GLX or WGL specifically in their current forms.
option(BRLCAD_ENABLE_AQUA "Use Aqua instead of X11 whenever possible on OSX." OFF)
mark_as_advanced(BRLCAD_ENABLE_AQUA)

# Install example BRL-CAD Geometry Files
option(BRLCAD_INSTALL_EXAMPLE_GEOMETRY "Install the example BRL-CAD geometry files." ON)

# test for X11 on all platforms since we don't know when/where we'll find it, unless
# we've indicated we *don't* want an X11 build
if(NOT BRLCAD_ENABLE_AQUA AND NOT BRLCAD_ENABLE_MINIMAL)
  find_package(X11)
  if (X11_Xrender_FOUND)
    CONFIG_H_APPEND(BRLCAD "#define HAVE_XRENDER 1\n")
  endif (X11_Xrender_FOUND)
endif(NOT BRLCAD_ENABLE_AQUA AND NOT BRLCAD_ENABLE_MINIMAL)

# make sure Xi is included in the list of X11 libs
# (Xext is automatically added during FindX11)
if(X11_Xi_FOUND)
  set(X11_LIBRARIES ${X11_LIBRARIES} ${X11_Xi_LIB})
endif(X11_Xi_FOUND)

# Set whether X11 is enabled or disabled by default
if(WIN32)
  # even if there is x11, we default to native
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
elseif(BRLCAD_ENABLE_AQUA)
  # aqua implies no X11
  option(BRLCAD_ENABLE_X11 "Use X11." OFF)
else(WIN32)
  # make everywhere else depend on whether we found a suitable X11
  # X11_Xext_LIB AND X11_Xi_LIB AND
  if(X11_FOUND AND NOT BRLCAD_ENABLE_MINIMAL)
    option(BRLCAD_ENABLE_X11 "Use X11." ON)
  else(X11_FOUND AND NOT BRLCAD_ENABLE_MINIMAL)
    option(BRLCAD_ENABLE_X11 "Use X11." OFF)
  endif(X11_FOUND AND NOT BRLCAD_ENABLE_MINIMAL)
endif(WIN32)
mark_as_advanced(BRLCAD_ENABLE_X11)

# if X11 is enabled, make sure aqua is off
if(BRLCAD_ENABLE_X11)
  set(BRLCAD_ENABLE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
  set(OPENGL_USE_AQUA OFF CACHE STRING "Don't use Aqua if we're doing X11" FORCE)
endif(BRLCAD_ENABLE_X11)
mark_as_advanced(OPENGL_USE_AQUA)

# Enable/disable features requiring the Tcl/Tk toolkit - usually this should be
# on, as a lot of functionality in BRL-CAD depends on Tcl/Tk
option(BRLCAD_ENABLE_TCL "Enable features requiring the Tcl toolkit" ON)
cmake_dependent_option(BRLCAD_ENABLE_TK "Enable features requiring the Tk toolkit" ON "BRLCAD_ENABLE_TCL" OFF)
mark_as_advanced(BRLCAD_ENABLE_TCL BRLCAD_ENABLE_TK)
if(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  set(BRLCAD_ENABLE_TK OFF)
endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
if(BRLCAD_ENABLE_X11)
  set(TK_X11_GRAPHICS ON CACHE STRING "Need X11 Tk" FORCE)
endif(BRLCAD_ENABLE_X11)

# Enable features requiring OPENGL
# Be smart about this - if we don't have X11 or Aqua and we're not on Windows,
# we're non-graphical and that means OpenGL is a no-go.  The Windows version
# would have to be some sort of option for the WIN32 graphics layer?  Should
# probably think about that... for now, on Win32 don't try OpenGL if Tk is off.
# That'll hold until we get a non-Tk based GUI - then setting non-graphical on
# Windows will take more thought.
if(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  set(OPENGL_FOUND OFF)
  set(BRLCAD_ENABLE_OPENGL OFF CACHE BOOL "Disabled - NOT BRLCAD_ENABLE_X11 and NOT BRLCAD_ENABLE_AQUA" FORCE)
else(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)
  include(FindGL)
endif(NOT BRLCAD_ENABLE_X11 AND NOT BRLCAD_ENABLE_AQUA AND NOT WIN32)

set(BRLCAD_ENABLE_OPENGL_DESCRIPTION "
Enable support for OpenGL based Display Managers in BRL-CAD.
Default depends on whether OpenGL is successfully detected -
if it is, default is to enable.
")
BRLCAD_OPTION(BRLCAD_ENABLE_OPENGL ${OPENGL_FOUND}
  TYPE BOOL
  ALIASES ENABLE_OPENGL
  DESCRIPTION BRLCAD_ENABLE_OPENGL_DESCRIPTION)

if(BRLCAD_ENABLE_AQUA)
  set(OPENGL_USE_AQUA ON CACHE STRING "Aqua enabled - use Aqua OpenGL" FORCE)
endif(BRLCAD_ENABLE_AQUA)

# Enable features requiring Bullet Physics SDK
option(BRLCAD_ENABLE_BULLET "Enable features requiring the Bullet Physics Library" OFF)
if(BRLCAD_ENABLE_BULLET)
  message("Bullet support is not currently working - disabling.")
  set(BRLCAD_ENABLE_BULLET OFF CACHE BOOL "Currently broken" FORCE)
endif(BRLCAD_ENABLE_BULLET)
mark_as_advanced(BRLCAD_ENABLE_BULLET)

# Enable features requiring GDAL geospatial library
option(BRLCAD_ENABLE_GDAL "Enable features requiring the Geospatial Data Abstraction Library" ON)
mark_as_advanced(BRLCAD_ENABLE_GDAL)

# Enable features requiring STEPcode library
option(BRLCAD_ENABLE_STEP "Enable features requiring the STEP support libraries" ON)
mark_as_advanced(BRLCAD_ENABLE_STEP)

# Enable features requiring Qt
option(BRLCAD_ENABLE_QT "Enable features requiring Qt" OFF)
mark_as_advanced(BRLCAD_ENABLE_QT)
if (BRLCAD_ENABLE_QT)

  find_package(Qt5 COMPONENTS Core Widgets Gui OpenGL)

  if(NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)
    message("QT interface requested, but QT5 is not found - disabling")
    set(BRLCAD_ENABLE_QT OFF)
  endif(NOT Qt5Widgets_FOUND AND BRLCAD_ENABLE_QT)

endif (BRLCAD_ENABLE_QT)
mark_as_advanced(Qt5Widgets_DIR)
mark_as_advanced(Qt5Core_DIR)
mark_as_advanced(Qt5Gui_DIR)

# Enable features requiring OpenSceneGraph
option(BRLCAD_ENABLE_OSG "Enable features requiring OpenSceneGraph" OFF)
mark_as_advanced(BRLCAD_ENABLE_OSG)

if(APPLE)
  # On OSX Tk has to be off in this case, since we don't yet work with Aqua
  # Tk, and this won't work with X11 Tk.  However, if we're enabling Qt on
  # OSX we need the native OpenGL and presumably the focus is on testing Qt
  # apps...
  if (BRLCAD_ENABLE_QT)
    set(OPENGL_USE_AQUA ON CACHE STRING "Qt on OSX - need Aqua OpenGL" FORCE)
    set(BRLCAD_ENABLE_TK OFF CACHE STRING "Qt on OSX - need Aqua OpenGL" FORCE)
  endif (BRLCAD_ENABLE_QT)

  if(BRLCAD_ENABLE_OSG AND NOT BRLCAD_ENABLE_AQUA)
    set(OSG_WINDOWING_SYSTEM "X11" CACHE STRING "Use X11" FORCE)
  endif(BRLCAD_ENABLE_OSG AND NOT BRLCAD_ENABLE_AQUA)
endif(APPLE)

# Enable features requiring GCT
option(BRLCAD_ENABLE_GCT "Enable features requiring GCT" ON)
mark_as_advanced(BRLCAD_ENABLE_GCT)
if(NOT BRLCAD_ENABLE_GCT)
  CONFIG_H_APPEND(BRLCAD "#define BRLCAD_DISABLE_GCT 1\n")
endif(NOT BRLCAD_ENABLE_GCT)

# Enable features requiring OpenCL
option(BRLCAD_ENABLE_OPENCL "Enable features requiring OpenCL" OFF)
mark_as_advanced(BRLCAD_ENABLE_OPENCL)
if(BRLCAD_ENABLE_OPENCL)
  find_package(OpenCL)
  if(NOT OPENCL_FOUND)
    message("OpenCL enablement requested, but OpenCL is not found - disabling")
    set(BRLCAD_ENABLE_OPENCL OFF)
  endif(NOT OPENCL_FOUND)
endif(BRLCAD_ENABLE_OPENCL)

# Enable features requiring OpenVDB
option(BRLCAD_ENABLE_OPENVDB "Enable features requiring OpenVDB" OFF)
mark_as_advanced(BRLCAD_ENABLE_OPENVDB)
if(BRLCAD_ENABLE_OPENVDB)
  find_package(OpenVDB)
  if(NOT OpenVDB_FOUND)
    message("OpenVDB enablement requested, but OpenVDB is not found - disabling")
    set(BRLCAD_ENABLE_OPENVDB OFF)
  endif(NOT OpenVDB_FOUND)
endif(BRLCAD_ENABLE_OPENVDB)

# Enable experimental support for binary attributes
option(BRLCAD_ENABLE_BINARY_ATTRIBUTES "Enable support for binary attributes" OFF)
mark_as_advanced(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
if(BRLCAD_ENABLE_BINARY_ATTRIBUTES)
  CONFIG_H_APPEND(BRLCAD "#define USE_BINARY_ATTRIBUTES 1\n")
endif(BRLCAD_ENABLE_BINARY_ATTRIBUTES)


#------------------------------------------------------------------------------
# The following are fine-grained options for enabling/disabling compiler and
# source code definition settings.  Typically these are set to various
# configurations by the toplevel CMAKE_BUILD_TYPE setting, but can also be
# individually set.

# Enable/disable runtime debugging - these are protections for minimizing the
# possibility of corrupted data files.  Generally speaking these should be left
# on.
set(BRLCAD_ENABLE_RUNTIME_DEBUG_ALIASES
  ENABLE_RUNTIME_DEBUG
  ENABLE_RUN_TIME_DEBUG
  ENABLE_RUNTIME_DEBUGGING
  ENABLE_RUN_TIME_DEBUGGING)
set(BRLCAD_ENABLE_RUNTIME_DEBUG_DESCRIPTION "
Enables support for application and library debugging facilities.
Disabling the run-time debugging facilities can provide a significant
(10%-30%) performance boost at the expense of extensive error
checking (that in turn help prevent corruption of your data).
Default is \"ON\", and should only be disabled for read-only render
work where performance is critical.
")
BRLCAD_OPTION(BRLCAD_ENABLE_RUNTIME_DEBUG ON
  TYPE BOOL
  ALIASES ${BRLCAD_ENABLE_RUNTIME_DEBUG_ALIASES}
  DESCRIPTION BRLCAD_ENABLE_RUNTIME_DEBUG_DESCRIPTION)
mark_as_advanced(BRLCAD_ENABLE_RUNTIME_DEBUG)
if(NOT BRLCAD_ENABLE_RUNTIME_DEBUG)
  message("}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}")
  message("While disabling run-time debugging should increase")
  message("performance, it will likewise remove several")
  message("data-protection safeguards that are in place to")
  message("minimize the possibility of corrupted data files")
  message("in the inevitable event of a user encountering a bug.")
  message("You have been warned.  Proceed at your own risk.")
  message("{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{{")
  CONFIG_H_APPEND(BRLCAD "/*Define to not do anything for macros that only bomb on a fatal error. */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_BOMBING_MACROS 1\n")
  CONFIG_H_APPEND(BRLCAD "/*Define to not perform magic number checking */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_MAGIC_CHECKING 1\n")
  CONFIG_H_APPEND(BRLCAD "/*Define to not provide run-time debug facilities via RTG.debug */\n")
  CONFIG_H_APPEND(BRLCAD "#define NO_DEBUG_CHECKING 1\n")
endif(NOT BRLCAD_ENABLE_RUNTIME_DEBUG)

# Enable debug flags during compilation - we always want to use these unless
# explicitly told not to.
set(BRLCAD_FLAGS_DEBUG_DESCRIPTION "
Add compiler flags to aid in program debugging.  Defaults to ON.
")
BRLCAD_OPTION(BRLCAD_FLAGS_DEBUG ON
  TYPE BOOL
  ALIASES ENABLE_DEBUG ENABLE_FLAGS_DEBUG ENABLE_DEBUG_FLAGS
  DESCRIPTION BRLCAD_FLAGS_DEBUG_DESCRIPTION)

# A variety of debugging messages in the code key off of the DEBUG definition -
# set it according to whether we're using debug flags.
if(BRLCAD_FLAGS_DEBUG)
  CONFIG_H_APPEND(BRLCAD "#define DEBUG 1\n")
endif(BRLCAD_FLAGS_DEBUG)

# Build with compiler warning flags
set(BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION "
Use extra compiler warning flags when compiling C/C++ code.  Defaults to ON.
")
BRLCAD_OPTION(BRLCAD_ENABLE_COMPILER_WARNINGS ON
  TYPE BOOL
  ALIASES ENABLE_WARNINGS ENABLE_COMPILER_WARNINGS
  DESCRIPTION BRLCAD_ENABLE_COMPILER_WARNINGS_DESCRIPTION)
mark_as_advanced(BRLCAD_ENABLE_COMPILER_WARNINGS)

# Enable/disable strict compiler settings - these are used for building BRL-CAD
# by default, but not src/other code.  Always used for BRL-CAD code unless the
# NO_STRICT option is specified when defining a target with BRLCAD_ADDEXEC or
# BRLCAD_ADDLIB.  If only C++ files in a target are not compatible with strict,
# the NO_STRICT_CXX option can be used.
set(BRLCAD_ENABLE_STRICT_DESCRIPTION "
Causes all compilation warnings for C code to be treated as errors.  This is now
the default for BRL-CAD source code, and developers should address issues
discovered by these flags whenever possible rather than disabling strict
mode.
")
BRLCAD_OPTION(BRLCAD_ENABLE_STRICT ON
  TYPE BOOL
  ALIASES ENABLE_STRICT ENABLE_STRICT_COMPILE ENABLE_STRICT_COMPILE_FLAGS
  DESCRIPTION BRLCAD_ENABLE_STRICT_DESCRIPTION)
if(BRLCAD_ENABLE_STRICT)
  mark_as_advanced(BRLCAD_ENABLE_STRICT)
endif(BRLCAD_ENABLE_STRICT)

# Build with compiler optimization flags.  This should normally be on for
# release builds and off otherwise, unless the user specifically enables it.
# For multi-config build tools, this is managed on a per-configuration basis.
if(CMAKE_BUILD_TYPE)
  cmake_dependent_option(BRLCAD_OPTIMIZED_BUILD "Enable optimized build flags" ON "${CMAKE_BUILD_TYPE} STREQUAL Release" OFF)
else(CMAKE_BUILD_TYPE)
  # Note: the cmake_dependent_option test doesn't work if CMAKE_BUILD_TYPE isn't set.
  option(BRLCAD_OPTIMIZED_BUILD "Enable optimized build flags" OFF)
endif(CMAKE_BUILD_TYPE)
mark_as_advanced(BRLCAD_OPTIMIZED_BUILD)

# Build with full compiler lines visible by default (won't need make VERBOSE=1)
# on command line
option(BRLCAD_ENABLE_VERBOSE_PROGRESS "verbose output" OFF)
mark_as_advanced(BRLCAD_ENABLE_VERBOSE_PROGRESS)
if(BRLCAD_ENABLE_VERBOSE_PROGRESS)
  set(CMAKE_VERBOSE_MAKEFILE ON)
endif(BRLCAD_ENABLE_VERBOSE_PROGRESS)

# Build with profile-guided optimization support.  this requires a two-pass
# compile, once with BRLCAD_PGO=ON on a location that did not exist beforehand
# (specified via the PGO_PATH environment variable), and again to use profiling
# metrics captured on "typical" operations and data.  By default, path is
# BUILDDIR/profiling

option(BRLCAD_PGO "Enable profile-guided optimization (set PGO_PATH environment variable)")
mark_as_advanced(BRLCAD_PGO)

#====== ALL CXX COMPILE ===================
# Build all C and C++ files with a C++ compiler
set(ENABLE_ALL_CXX_COMPILE_DESCRIPTION "
Build all C and C++ files with a C++ compiler.  Defaults to OFF.

EXPERIMENTAL!
")
BRLCAD_OPTION(ENABLE_ALL_CXX_COMPILE OFF
  TYPE BOOL
  ALIASES ENABLE_ALL_CXX
  DESCRIPTION ENABLE_ALL_CXX_COMPILE_DESCRIPTION)
mark_as_advanced(ENABLE_ALL_CXX_COMPILE)

# Build with coverage enabled
option(BRLCAD_ENABLE_COVERAGE "Build with coverage enabled" OFF)
mark_as_advanced(BRLCAD_ENABLE_COVERAGE)

# Build with dtrace support
option(BRLCAD_ENABLE_DTRACE "Build with dtrace support" OFF)
mark_as_advanced(BRLCAD_ENABLE_DTRACE)
if(BRLCAD_ENABLE_DTRACE)
  BRLCAD_INCLUDE_FILE(sys/sdt.h HAVE_SYS_SDT_H)
  if(NOT HAVE_SYS_SDT_H)
    set(BRLCAD_ENABLE_DTRACE OFF)
  endif(NOT HAVE_SYS_SDT_H)
endif(BRLCAD_ENABLE_DTRACE)

# Take advantage of parallel processors if available - highly recommended
option(BRLCAD_ENABLE_SMP "Enable SMP architecture parallel computation support" ON)
mark_as_advanced(BRLCAD_ENABLE_SMP)
if(BRLCAD_ENABLE_SMP)
  CONFIG_H_APPEND(BRLCAD "#define PARALLEL 1\n")
endif(BRLCAD_ENABLE_SMP)

if(BRLCAD_HEADERS_OLD_COMPAT)
  add_definitions(-DEXPOSE_FB_HEADER)
  add_definitions(-DEXPOSE_DM_HEADER)
endif(BRLCAD_HEADERS_OLD_COMPAT)

#-----------------------------------------------------------------------------
# Some generators in CMake support generating folders in IDEs for organizing
# build targets.  We want to use them if they are there.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#-----------------------------------------------------------------------------
# There are extra documentation files available requiring DocBook They are
# quite useful in graphical interfaces, but also add considerably to the
# overall build time.  If necessary BRL-CAD provides its own xsltproc (see
# src/other/xmltools), so the html and man page outputs are always potentially
# available.  PDF output, on the other hand, needs Apache FOP.  FOP is not a
# candidate for bundling with BRL-CAD for a number of reasons, so we simply
# check to see if it is present and set the options accordingly.

# Do we have the environment variable set locally?
if(NOT "$ENV{APACHE_FOP}" STREQUAL "")
  set(APACHE_FOP "$ENV{APACHE_FOP}")
endif(NOT "$ENV{APACHE_FOP}" STREQUAL "")
if(NOT APACHE_FOP)
  find_program(APACHE_FOP fop DOC "path to the exec script for Apache FOP")
endif(NOT APACHE_FOP)
mark_as_advanced(APACHE_FOP)
# We care about the FOP version, unfortunately - find out what we have.
if(APACHE_FOP)
  execute_process(COMMAND ${APACHE_FOP} -v OUTPUT_VARIABLE APACHE_FOP_INFO ERROR_QUIET)
  string(REGEX REPLACE "FOP Version ([0-9\\.]*)" "\\1" APACHE_FOP_VERSION_REGEX "${APACHE_FOP_INFO}")
  if(APACHE_FOP_VERSION_REGEX)
    string(STRIP ${APACHE_FOP_VERSION_REGEX} APACHE_FOP_VERSION_REGEX)
  endif(APACHE_FOP_VERSION_REGEX)
  if(NOT "${APACHE_FOP_VERSION}" STREQUAL "${APACHE_FOP_VERSION_REGEX}")
    message("-- Found Apache FOP: version ${APACHE_FOP_VERSION_REGEX}")
    set(APACHE_FOP_VERSION ${APACHE_FOP_VERSION_REGEX} CACHE STRING "Apache FOP version" FORCE)
    mark_as_advanced(APACHE_FOP_VERSION)
  endif(NOT "${APACHE_FOP_VERSION}" STREQUAL "${APACHE_FOP_VERSION_REGEX}")
endif(APACHE_FOP)

# Toplevel variable that controls all DocBook based documentation.  Key it off
# of what target level is enabled.
if(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  set(EXTRADOCS_DEFAULT "ON")
else(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
  set(EXTRADOCS_DEFAULT "OFF")
endif(NOT BRLCAD_ENABLE_TARGETS OR "${BRLCAD_ENABLE_TARGETS}" GREATER 2)
set(BRLCAD_EXTRADOCS_DESCRIPTION "
The core option that enables and disables building of BRL-CAD's
DocBook based documentation (includes manuals and man pages for
commands, among other things).  Defaults to ON, but only HTML and MAN
formats are enabled by default - PDF must be enabled separately by use
of this option or one of its aliases.  Note that you may set
environment variable APACHE_FOP to point to your locally installed fop
executable file (which on Linux is usually a shell script with 0755
permissions).
")
BRLCAD_OPTION(BRLCAD_EXTRADOCS ${EXTRADOCS_DEFAULT}
  TYPE BOOL
  ALIASES ENABLE_DOCS ENABLE_EXTRA_DOCS ENABLE_DOCBOOK
  DESCRIPTION BRLCAD_EXTRADOCS_DESCRIPTION)


# The HTML output is used in the graphical help browsers in MGED and Archer, as
# well as being the most likely candidate for external viewers. Turn this on
# unless explicitly instructed otherwise by the user or all extra documentation
# is disabled.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_HTML "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_HTML)

CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PHP "Build MAN page output from DocBook documentation" OFF "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_PHP)

CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PPT "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_PPT)

# Normally, we'll turn on man page output by default, but there is no point in
# doing man page output for a Visual Studio build - the files aren't useful and
# it *seriously* increases the target build count/build time.  Conditionalize
# on the CMake MSVC variable NOT being set.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_MAN "Build MAN page output from DocBook documentation" ON "BRLCAD_EXTRADOCS;NOT MSVC" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_MAN)

# Don't do PDF by default because it's pretty expensive, and hide the option
# unless the tools to do it are present.
set(BRLCAD_EXTRADOCS_PDF_DESCRIPTION "
Option that enables building of BRL-CAD's DocBook PDF-based documentation
(includes manuals and man pages for commands, among
other things.) Defaults to OFF.
Note that you may set environment variable APACHE_FOP
to point to your locally installed fop executable file (which on Linux is
usually a shell script with 0755 permissions).
")
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_PDF "Build PDF output from DocBook documentation" OFF "BRLCAD_EXTRADOCS;APACHE_FOP" OFF)

# Provide an option to enable/disable XML validation as part of the DocBook
# build - sort of a "strict flags" mode for DocBook.  By default, this will be
# enabled when extra docs are built and the toplevel BRLCAD_ENABLE_STRICT
# setting is enabled.  Unfortunately, Visual Studio 2010 seems to have issues
# when we enable validation on top of everything else... not clear why, unless
# build target counts >1800 are beyond MSVC's practical limit.  Until we either
# find a resolution or a way to reduce the target count on MSVC, disable
# validation there.
CMAKE_DEPENDENT_OPTION(BRLCAD_EXTRADOCS_VALIDATE "Perform validation for DocBook documentation" ON "BRLCAD_EXTRADOCS;BRLCAD_ENABLE_STRICT" OFF)
mark_as_advanced(BRLCAD_EXTRADOCS_VALIDATE)



# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
