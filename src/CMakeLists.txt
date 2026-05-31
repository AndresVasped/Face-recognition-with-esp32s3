# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS "main.c"
                            "face_model.cpp"
                            "cam_mod.c"
                            "wifi.c"
                            "spiffs_mod.c"
                            "in_situ_mod.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp-dl
)
