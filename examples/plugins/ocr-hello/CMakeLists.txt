#
# SPDX-FileCopyrightText: 2026 Mauritius Clemens <gitlab@janitor.chat>
# SPDX-License-Identifier: GPL-3.0-or-later
#

# A text-recognition backend for Crow Translate, built outside its source tree.
# See ../translation-hello for the commentary.
#
# Recognition is the family with no built-in fallback: if no engine is
# installed, Crow says so rather than pretending. A backend here is therefore
# the only thing standing between --image and "no text recognition plugin is
# installed", which is worth knowing before shipping one that throws.
#
#   cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/crow/prefix \
#                       -DCMAKE_INSTALL_PREFIX=/path/to/crow/prefix
#   cmake --build build && cmake --install build
#   crow --image picture.png --tp copy -s en -t en

cmake_minimum_required(VERSION 3.16)

project(crow-hello-ocr VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Crow REQUIRED)

crow_add_plugin_module(hello-ocr
    FAMILY ocr
    SOURCES
        helloocrplugin.cpp
        helloocrprovider.cpp
)
