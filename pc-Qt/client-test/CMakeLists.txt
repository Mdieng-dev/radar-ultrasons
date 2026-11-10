cmake_minimum_required(VERSION 3.16)

project(client-test LANGUAGES C)

add_executable(client-test main.c)

include(GNUInstallDirs)
install(TARGETS client-test
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
