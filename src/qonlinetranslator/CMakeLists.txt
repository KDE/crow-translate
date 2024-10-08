#
# SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
# SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

cmake_minimum_required(VERSION 3.15.0)

project(QOnlineTranslator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AUTOMOC ON)

find_package(Qt5 COMPONENTS Multimedia Network REQUIRED)
find_package(Doxygen)

add_library(${PROJECT_NAME} STATIC
    src/qonlinetranslator.cpp
    src/qonlinetts.cpp
    src/qexample.cpp
    src/qoption.cpp
)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt5::Network)
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Multimedia)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(DOXYGEN_FOUND)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE README.md)

    doxygen_add_docs(${PROJECT_NAME}Documentation
        src/qonlinetranslator.h
        src/qonlinetts.h
        src/qexample.h
        src/qoption.h
        README.md
    )
endif()
