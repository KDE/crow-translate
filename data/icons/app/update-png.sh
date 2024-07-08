#!/usr/bin/env bash
# SPDX-FileCopyrightText: none
# SPDX-License-Identifier: CC0-1.0

for i in 16 22 32 44 48 64 128 150 256 310 512
do
   inkscape sc-apps-org.kde.CrowTranslate.svg -o $i-apps-org.kde.CrowTranslate.png -w $i -h $i
done

for i in 16 22 24
do
    inkscape sc-status-org.kde.CrowTranslate-tray-dark.svg -o $i-status-org.kde.CrowTranslate-tray-dark.png -w $i -h $i
    inkscape sc-status-org.kde.CrowTranslate-tray-light.svg -o $i-status-org.kde.CrowTranslate-tray-light.png -w $i -h $i
done
