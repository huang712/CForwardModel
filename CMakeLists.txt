cmake_minimum_required(VERSION 3.10)
project(CForwardModel)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c forwardmodel.h initialization.c forwardmodel.c gnssr.h surface.c ddm.c antenna.c math.c wind.c geom.c specular.c coord.c grid.c debug.c saveFile.c cygnss.h cygnss.c)

set(INC0_DIR /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include)
set(INC_DIR /usr/local/include/)
set(LINK_DIR /usr/local/lib)

include_directories(${INC0_DIR} ${INC_DIR})
link_directories(${LINK_DIR})

add_executable(CForwardModel ${SOURCE_FILES})

target_link_libraries(CForwardModel fftw3 netcdf)

