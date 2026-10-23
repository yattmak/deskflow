/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */
#pragma once

#include "arch/Arch.h"
#include "base/Log.h"
#include "platform/OSXKeyState.h"

#include <QTest>

class OSXKeyStateTests : public QObject
{
  Q_OBJECT
private Q_SLOTS:
  void initTestCase();
  void mapModifiersFromOSX_OSXMask();
  void mapKeyFromEvent_sidedModifiers();
  void handleModifierKeys_sidedModifiers_data();
  void handleModifierKeys_sidedModifiers();
  void clientModifierState_data();
  void clientModifierState();
  void capsLockAndNonModifierState();
  void chooseTranslationInputSource_data();
  void chooseTranslationInputSource();
  void inputSourceLanguage_missingProperties();
  void keyInfo_translationLanguage();
  void sendKeyEvent_translationLanguage();

private:
  Arch m_arch;
  Log m_log;
};
