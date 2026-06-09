/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ScreenConfig.h"

const char *ScreenConfig::m_ModifierNames[] = {"shift", "ctrl", "alt", "meta", "super", "none"};

const char *ScreenConfig::m_ModifierConfigNames[] = {
    "shift", "ctrl", "alt", "meta", "super", "none", "metaLeft", "metaRight", "superLeft", "superRight"
};

const int ScreenConfig::m_DefaultModifiers[] = {0, 1, 2, 3, 4, 5, 3, 3, 4, 4};

const char *ScreenConfig::m_FixNames[] = {
    "halfDuplexCapsLock", "halfDuplexNumLock", "halfDuplexScrollLock", "xtestIsXineramaUnaware"
};

const char *ScreenConfig::m_SwitchCornerNames[] = {"top-left", "top-right", "bottom-left", "bottom-right"};
