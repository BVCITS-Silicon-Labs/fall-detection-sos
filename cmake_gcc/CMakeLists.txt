# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	fall_detection_sos
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_fall_detection_sos target,
# which contains the content of the SLC project
include(fall_detection_sos.cmake)

add_executable(fall_detection_sos
    # Add additional sources here
)

target_include_directories(fall_detection_sos PUBLIC
    # Add additional include paths here
)

target_compile_definitions(fall_detection_sos PUBLIC
    # Add additional macros here
)

target_compile_options(fall_detection_sos PUBLIC
    # Set additional compiler flags here
)

target_link_options(fall_detection_sos PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(fall_detection_sos PRIVATE
    slc
)

# Include managed project content if available
include(fall_detection_sos_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(fall_detection_sos PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET fall_detection_sos
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:fall_detection_sos>" "$<TARGET_FILE_DIR:fall_detection_sos>/$<TARGET_FILE_BASE_NAME:fall_detection_sos>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:fall_detection_sos>" "$<TARGET_FILE_DIR:fall_detection_sos>/$<TARGET_FILE_BASE_NAME:fall_detection_sos>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:fall_detection_sos>" "$<TARGET_FILE_DIR:fall_detection_sos>/$<TARGET_FILE_BASE_NAME:fall_detection_sos>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET fall_detection_sos
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()