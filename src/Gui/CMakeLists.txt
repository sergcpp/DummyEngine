cmake_minimum_required(VERSION 2.8.4)
project(Gui)

IF(WIN32)
    
ELSE(WIN32)
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    set(LIBS -pthread)
ENDIF(WIN32)

set(SOURCE_FILES    BaseElement.h
                    BaseElement.cpp
                    BitmapFont.h
                    BitmapFont.cpp
                    ButtonBase.h
                    ButtonBase.cpp
                    ButtonImage.h
                    ButtonImage.cpp
                    ButtonText.h
                    ButtonText.cpp
                    Cursor.h
                    Cursor.cpp
                    EditBox.h
                    EditBox.cpp
                    Frame.h
                    Frame.cpp
                    Image.h
                    Image.cpp
                    LinearLayout.h
                    LinearLayout.cpp
                    Renderer.h
                    TypeMesh.h
                    TypeMesh.cpp
                    Utils.h
                    Utils.cpp
         )

if(${RENDERER} STREQUAL "GL")
    set(SOURCE_FILES    ${SOURCE_FILES}
                        RendererGL.cpp
            )
elseif(${RENDERER} STREQUAL "SW")
    set(SOURCE_FILES    ${SOURCE_FILES}
                        RendererSW.cpp
            )
elseif(${RENDERER} STREQUAL "VK")
    set(SOURCE_FILES    ${SOURCE_FILES}

            )
endif()

list(APPEND ALL_SOURCE_FILES ${SOURCE_FILES})
set_source_files_properties(${SOURCE_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
source_group("src" FILES ${SOURCE_FILES})

list(APPEND ALL_SOURCE_FILES _Gui.cpp)
source_group("src" FILES _Gui.cpp)

add_library(Gui STATIC ${ALL_SOURCE_FILES})
target_link_libraries(Gui Ren Sys ${LIBS})

add_subdirectory(tests)
