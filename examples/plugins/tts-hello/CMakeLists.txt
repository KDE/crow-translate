#
# SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

# A speech backend for Crow Translate, built outside its source tree. See
# ../translation-hello for the commentary; this one exists to show that the
# same three lines of CMake work for the other two families, and that the
# speech interface is reachable from outside without linking anything Crow
# does not install.
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/crow/prefix \
#                       -DCMAKE_INSTALL_PREFIX=/path/to/crow/prefix
#   cmake --build build && cmake --install build
#   crow --tts hello -o -a -s en -t en "text"

cmake_minimum_required(VERSION 3.16)

project(crow-hello-tts VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Crow REQUIRED)

crow_add_plugin_module(hello-tts
    FAMILY tts
    SOURCES
        hellottsplugin.cpp
        hellottsprovider.cpp
)
