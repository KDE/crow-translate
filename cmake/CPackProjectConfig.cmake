#
# SPDX-FileCopyrightText: 2018 Hennadii Chernyshchyk <genaloner@gmail.com>
# SPDX-FileCopyrightText: 2022 Volk Milit <javirrdar@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

if(CPACK_GENERATOR STREQUAL External)
    set(CPACK_PACKAGING_INSTALL_PREFIX /usr)
endif()
