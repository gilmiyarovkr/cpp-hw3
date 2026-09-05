add_library(allocator_lib STATIC
    include/castom_allocators.h
    castom_allocators.cpp
)

target_link_libraries(allocator_lib PRIVATE compiler_flags)

target_include_directories(allocator_lib PUBLIC
   $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
   $<INSTALL_INTERFACE:include>
)
