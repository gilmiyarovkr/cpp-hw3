add_library(cfl_lib INTERFACE
    castom_forward_list.h
)

target_link_libraries(cfl_lib INTERFACE compiler_flags)

target_include_directories(cfl_lib INTERFACE
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
   $<INSTALL_INTERFACE:include>
)
