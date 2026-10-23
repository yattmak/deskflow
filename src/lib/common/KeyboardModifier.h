/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "KeyModifierID.h"
#include "deskflow/KeyTypes.h"

#include <QObject>
#include <QString>

inline static const auto kModifierNameAlt = "alt";
inline static const auto kModifierNameAltGr = "altgr";
inline static const auto kModifierNameCtrl = "ctrl";
inline static const auto kModifierNameMeta = "meta";
inline static const auto kModifierNameNone = "none";
inline static const auto kModifierNameShift = "shift";
inline static const auto kModifierNameSuper = "super";
inline static const auto kModifierNameHangul = "hangul";
inline static const auto kModifierNameHanja = "hanja";

enum class KeyboardModifier : int8_t
{
  DefaultMod = -1,
  Shift,
  Ctrl,
  Alt,
  Meta,
  Super,
  AltGr,
  None,
  Hangul,
  Hanja
};
Q_DECLARE_METATYPE(KeyboardModifier);

// Side sources are independent of target values and the upstream modifier indices.
enum class KeyboardModifierSide : int8_t
{
  MetaLeft,
  MetaRight,
  SuperLeft,
  SuperRight
};

static QString keyboardModifierToOption(const KeyboardModifier modifier)
{
  using enum KeyboardModifier;
  switch (modifier) {
  case Alt:
    return kModifierNameAlt;
  case AltGr:
    return kModifierNameAltGr;
  case Ctrl:
    return kModifierNameCtrl;
  case Meta:
    return kModifierNameMeta;
  case None:
    return kModifierNameNone;
  case Shift:
    return kModifierNameShift;
  case Super:
    return kModifierNameSuper;
  case Hangul:
    return kModifierNameHangul;
  case Hanja:
    return kModifierNameHanja;
  default:
    return {};
  }
}

static QString valueToKeyboardModifierOption(int modifier)
{
  return keyboardModifierToOption(static_cast<KeyboardModifier>(modifier));
}

static KeyboardModifier keyboardModifierFromString(const QString &modifier)
{
  using enum KeyboardModifier;
  if (modifier.compare(kModifierNameShift, Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("Shift")) == 0)
    return Shift;
  if (modifier.compare(kModifierNameCtrl, Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("Ctrl")) == 0)
    return Ctrl;
  if (modifier.compare(kModifierNameAlt, Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("Alt")) == 0)
    return Alt;
  if (modifier.compare(kModifierNameMeta, Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("Meta")) == 0)
    return Meta;
  if (modifier.compare(kModifierNameSuper, Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("Super")) == 0)
    return Super;
  if (modifier.compare(kModifierNameAltGr, Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("AltGr")) == 0)
    return AltGr;
  if (modifier.compare(kModifierNameHangul, Qt::CaseInsensitive) == 0 ||
      modifier.compare("haen", Qt::CaseInsensitive) == 0 || modifier.compare("kc_haen", Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("Hangul")) == 0)
    return Hangul;
  if (modifier.compare(kModifierNameHanja, Qt::CaseInsensitive) == 0 ||
      modifier.compare("Hangul_Hanja", Qt::CaseInsensitive) == 0 ||
      modifier.compare("haej", Qt::CaseInsensitive) == 0 || modifier.compare("kc_haej", Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("Hanja")) == 0)
    return Hanja;
  if (modifier.compare(kModifierNameNone, Qt::CaseInsensitive) == 0 ||
      modifier.localeAwareCompare(QObject::tr("None")) == 0)
    return None;
  return DefaultMod;
}

static int modifierValueFromString(const QString &modifier)
{
  return static_cast<int>(keyboardModifierFromString(modifier));
}

// GUI enum values are not wire values: e.g. GUI None=6, wire Null=0.
// Non-modifier targets must be sent as explicit KeyIDs, never enum ordinals.
static int32_t keyboardModifierToProtocolValue(const KeyboardModifier modifier)
{
  auto mod = keyboardModifierToOption(modifier);
  if (mod == kModifierNameShift)
    return static_cast<int32_t>(kKeyModifierIDShift);
  if (mod == kModifierNameCtrl)
    return static_cast<int32_t>(kKeyModifierIDControl);
  if (mod == kModifierNameAlt)
    return static_cast<int32_t>(kKeyModifierIDAlt);
  if (mod == kModifierNameAltGr)
    return static_cast<int32_t>(kKeyModifierIDAltGr);
  if (mod == kModifierNameMeta)
    return static_cast<int32_t>(kKeyModifierIDMeta);
  if (mod == kModifierNameSuper)
    return static_cast<int32_t>(kKeyModifierIDSuper);
  if (mod == kModifierNameNone)
    return static_cast<int32_t>(kKeyModifierIDNull);
  if (mod == kModifierNameHangul)
    return static_cast<int32_t>(kKeyHangul);
  if (mod == kModifierNameHanja)
    return static_cast<int32_t>(kKeyHanja);
  return static_cast<int32_t>(-1);
}

static int32_t modifierIDValueFromString(const QString &modifier)
{
  return keyboardModifierToProtocolValue(keyboardModifierFromString(modifier));
}

static bool isUnsupportedModifierKeyTarget(KeyID key)
{
  // Lock state is synchronized separately; translated key injection cannot toggle it.
  return key == kKeyCapsLock || key == kKeyNumLock || key == kKeyScrollLock;
}
