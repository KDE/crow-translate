#
# SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

# A translation backend for Crow Translate, built entirely outside its source
# tree. Nothing here knows where Crow Translate's sources are; it needs an
# installed prefix and nothing else.
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/crow/prefix \
#                       -DCMAKE_INSTALL_PREFIX=/path/to/crow/prefix
#   cmake --build build
#   cmake --install build
#   crow --tp hello -s en -t en "text"
#
# Installing into Crow's own prefix is the simple case. Anywhere else works
# too - the module records an absolute rpath back to libcrow-core as well as a
# relative one - as long as it lands in a directory Crow searches. See
# PLUGINS.md for the list.

cmake_minimum_required(VERSION 3.16)

project(crow-hello-translation VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Crow REQUIRED)

crow_add_plugin_module(hello-translation
    FAMILY translation
    SOURCES
        hellotranslationplugin.cpp
        hellotranslationprovider.cpp
)
