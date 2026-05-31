add_executable(${PROJECT_NAME}
    main.cpp
)

# target_link_libraries(${PROJECT_NAME} PRIVATE
    # allocator_lib compiler_flags
# )

# Include module for GNU standard installation directories
include(GNUInstallDirs)

# Установка
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
