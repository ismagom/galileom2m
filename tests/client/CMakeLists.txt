cmake_minimum_required (VERSION 2.8.3)

project (galileom2m)

add_definitions(-DLWM2M_CLIENT_MODE)

include_directories (${PROJECT_SOURCE_DIR}/../.. ${PROJECT_SOURCE_DIR}/../utils)

add_subdirectory(${PROJECT_SOURCE_DIR}/../../core ${CMAKE_CURRENT_BINARY_DIR}/core)

# Added gomezi
set(CMAKE_C_FLAGS "${CFDEB} -Wall -Wno-pointer-sign -Winline -Wno-unused-result -Wno-format")

SET(SOURCES lwm2mclient.c 
			../utils/commandline.c 			
			object_device.c 

# Added gomezi
			status.c
			applications.c
			object.c
			read_objects.c)

add_executable(lwm2mclient ${SOURCES} ${CORE_SOURCES})
target_link_libraries(lwm2mclient pthread)
