cmake_minimum_required(VERSION 3.1)
project(test_SW)

add_executable(test_SW main.c
                       test_buffer.c
                       test_common.h
                       test_context.c
                       test_framebuffer.c
                       test_pixels.c
                       test_program.c
                       test_texture.c
                       test_zbuffer.c)
target_link_libraries(test_SW ${LIBS} SW)
