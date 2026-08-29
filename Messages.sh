#!/bin/sh
# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: CC0-1.0

$EXTRACT_TR_STRINGS `find . -name '*.cpp' -o -name '*.h' -o -name '*.ui' | grep -v -e '/3rdparty/' -e '/examples/'` -o $podir/crow-translate_qt.pot
