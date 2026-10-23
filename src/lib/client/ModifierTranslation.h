/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "deskflow/IKeyState.h"
#include "deskflow/KeyTypes.h"
#include "deskflow/OptionTypes.h"

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <vector>

namespace deskflow {

// Protocol modifier IDs and KeyIDs share a wire value, but not an index space.
// Modifier sources with equal translated KeyIDs share one output button. Aliases
// between different KeyIDs in the platform KeyMap cannot be detected here.
class ModifierTranslation
{
public:
  enum class OptionResult
  {
    Unhandled,
    Applied,
    Invalid
  };

  enum class Action
  {
    Down,
    Repeat,
    Up
  };

  struct KeyEvent
  {
    KeyID key;
    KeyModifierMask mask;
    KeyButton button;
    bool shouldEmit = true;
  };

  ModifierTranslation()
  {
    resetOptions();
  }

  // Configuration refreshes must not release held keys or change their targets.
  // Only releaseActiveKeys() ends the input session.
  void resetOptions()
  {
    for (KeyModifierID id = 0; id < kKeyModifierIDLast; ++id) {
      m_generic[id] = Target{Target::Kind::Modifier, id};
    }
    m_overrides = {};
  }

  OptionResult setOption(OptionID option, uint32_t value)
  {
    KeyModifierID id;
    std::optional<size_t> side;
    switch (option) {
    case kOptionModifierMapForShift:
      id = kKeyModifierIDShift;
      break;
    case kOptionModifierMapForControl:
      id = kKeyModifierIDControl;
      break;
    case kOptionModifierMapForAlt:
      id = kKeyModifierIDAlt;
      break;
    case kOptionModifierMapForAltGr:
      id = kKeyModifierIDAltGr;
      break;
    case kOptionModifierMapForMeta:
      id = kKeyModifierIDMeta;
      break;
    case kOptionModifierMapForSuper:
      id = kKeyModifierIDSuper;
      break;
    case kOptionModifierMapForMetaLeft:
      id = kKeyModifierIDMeta;
      side = 0;
      break;
    case kOptionModifierMapForMetaRight:
      id = kKeyModifierIDMeta;
      side = 1;
      break;
    case kOptionModifierMapForSuperLeft:
      id = kKeyModifierIDSuper;
      side = 0;
      break;
    case kOptionModifierMapForSuperRight:
      id = kKeyModifierIDSuper;
      side = 1;
      break;
    default:
      return OptionResult::Unhandled;
    }

    const auto target = Target::fromOption(value);
    if (!target) {
      return OptionResult::Invalid;
    }
    if (side) {
      m_overrides[id][*side] = *target;
    } else {
      m_generic[id] = *target;
    }
    return OptionResult::Applied;
  }

  KeyEvent translate(Action action, KeyID key, KeyModifierMask mask, KeyButton button)
  {
    auto source = sourceForKey(key);
    // Some servers send kKeyNone on release. Match the physical button first,
    // and retain the press target even if options changed while it was held.
    if (action != Action::Down && button != 0) {
      for (KeyModifierID id = kKeyModifierIDShift; id < kKeyModifierIDLast; ++id) {
        for (size_t side = 0; side < 2; ++side) {
          if (m_active[id][side] && m_active[id][side]->button == button) {
            source = Source{id, side};
          }
        }
      }
    }
    KeyEvent event{key, 0, button};
    if (source) {
      auto &active = m_active[source->id][source->side];
      const auto target = active ? active->target : targetFor(*source);
      event.key = target.key(source->side);
      if (action == Action::Up) {
        const auto output = active ? active->outputButton : std::nullopt;
        active.reset();
        // KeyState releases by button, ignoring the translated key and mask.
        // Keep the original output button held until its last source is up.
        event.shouldEmit = output && !activeForKey(event.key);
        event.button = output.value_or(button);
      } else {
        bool shared = false;
        if (!active) {
          const auto owner = activeForKey(event.key);
          shared = owner != nullptr;
          std::optional<KeyButton> output;
          if (owner) {
            output = owner->outputButton;
          } else if (event.key != kKeyNone) {
            output = allocateButton(button);
          }
          active = ActiveKey{target, button, output};
        }
        event.button = active->outputButton.value_or(button);
        event.shouldEmit = active->outputButton.has_value() && !(shared && action == Action::Down);
      }
    } else if (button != 0) {
      // A retained owner button can already be physically up and pressed again.
      // Route all other keys around it rather than aliasing KeyState's slot.
      auto binding = m_passthrough.find(button);
      if (binding == m_passthrough.end() && action == Action::Down) {
        if (const auto output = allocateButton(button)) {
          binding = m_passthrough.emplace(button, *output).first;
        }
      }
      event.shouldEmit = binding != m_passthrough.end();
      if (event.shouldEmit) {
        event.button = binding->second;
        if (action == Action::Up) {
          m_passthrough.erase(binding);
        }
      }
    }
    m_lastMask = mask;
    event.mask = translateMask(mask, action == Action::Up ? source : std::nullopt);
    return event;
  }

  KeyModifierMask translateMask(KeyModifierMask mask) const
  {
    return translateMask(mask, std::nullopt);
  }

  // End the input session, not an options refresh. Release each shared target
  // once, using its owner button, and clear any ordinary-key button bindings.
  std::vector<KeyEvent> releaseActiveKeys()
  {
    std::vector<KeyEvent> releases;
    for (KeyModifierID id = kKeyModifierIDShift; id < kKeyModifierIDLast; ++id) {
      for (size_t side = 0; side < 2; ++side) {
        if (m_active[id][side]) {
          const auto button = m_active[id][side]->button;
          auto mask = m_lastMask;
          if (!m_active[id][1 - side]) {
            mask &= ~s_masks[id];
          }
          const auto event = translate(Action::Up, s_keys[id][side], mask, button);
          if (event.shouldEmit) {
            releases.push_back(event);
          }
        }
      }
    }
    for (const auto &[source, output] : m_passthrough) {
      releases.push_back({kKeyNone, 0, output});
    }
    m_passthrough.clear();
    m_lastMask = 0;
    return releases;
  }

private:
  struct Source
  {
    KeyModifierID id;
    size_t side;
  };

  inline static constexpr KeyID s_keys[kKeyModifierIDLast][2] = {
      {kKeyNone, kKeyNone},     {kKeyShift_L, kKeyShift_R}, {kKeyControl_L, kKeyControl_R}, {kKeyAlt_L, kKeyAlt_R},
      {kKeyMeta_L, kKeyMeta_R}, {kKeySuper_L, kKeySuper_R}, {kKeyAltGr, kKeyAltGr}
  };
  inline static constexpr KeyModifierMask s_masks[kKeyModifierIDLast] = {
      0, KeyModifierShift, KeyModifierControl, KeyModifierAlt, KeyModifierMeta, KeyModifierSuper, KeyModifierAltGr
  };
  inline static constexpr KeyModifierMask s_translatedMasks =
      KeyModifierShift | KeyModifierControl | KeyModifierAlt | KeyModifierMeta | KeyModifierSuper | KeyModifierAltGr;

  static std::optional<Source> sourceForKey(KeyID key)
  {
    for (KeyModifierID id = kKeyModifierIDShift; id < kKeyModifierIDLast; ++id) {
      // AltGr has only a right side, including when translated to another modifier.
      for (size_t side = (id == kKeyModifierIDAltGr ? 1 : 0); side < 2; ++side) {
        if (s_keys[id][side] == key) {
          return Source{id, side};
        }
      }
    }
    return std::nullopt;
  }

  struct Target
  {
    enum class Kind
    {
      Modifier,
      Key
    };

    Kind kind = Kind::Modifier;
    uint32_t value = kKeyModifierIDNull;

    static std::optional<Target> fromOption(uint32_t value)
    {
      if (value < kKeyModifierIDLast) {
        return Target{Kind::Modifier, value};
      }
      // KeyState deliberately ignores lock-key injection. Accepting these
      // targets would silently consume the source without producing its target.
      if (value == kKeyCapsLock || value == kKeyNumLock || value == kKeyScrollLock) {
        return std::nullopt;
      }
      // Validate named keys directly, without KeyMap's lazy global name maps.
      for (auto entry = kKeyNameMap; entry->m_name != nullptr; ++entry) {
        if (entry->m_id == value) {
          return Target{Kind::Key, value};
        }
      }
      // Also accept printable Unicode KeyIDs, but not controls, surrogates,
      // noncharacters, or undefined keys in Deskflow's reserved control range.
      if ((value >= 0x20 && value <= 0x7e) ||
          (value >= 0xa0 && value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff) &&
           !(value >= 0xe000 && value <= 0xefff) && !(value >= 0xfdd0 && value <= 0xfdef) &&
           (value & 0xffff) < 0xfffe)) {
        return Target{Kind::Key, value};
      }
      return std::nullopt;
    }

    KeyID key(size_t side) const
    {
      return kind == Kind::Modifier ? s_keys[value][side] : value;
    }

    KeyModifierMask mask() const
    {
      if (kind == Kind::Modifier) {
        return s_masks[value];
      }
      const auto source = sourceForKey(value);
      return source ? s_masks[source->id] : 0;
    }
  };

  struct ActiveKey
  {
    Target target;
    KeyButton button;
    std::optional<KeyButton> outputButton;
  };

  const ActiveKey *activeForKey(KeyID key) const
  {
    for (KeyModifierID id = kKeyModifierIDShift; id < kKeyModifierIDLast; ++id) {
      for (size_t side = 0; side < 2; ++side) {
        const auto &active = m_active[id][side];
        if (active && active->target.key(side) == key) {
          return &*active;
        }
      }
    }
    return nullptr;
  }

  bool buttonInUse(KeyButton button) const
  {
    for (const auto &sides : m_active) {
      for (const auto &active : sides) {
        if (active && active->outputButton == button) {
          return true;
        }
      }
    }
    for (const auto &[source, output] : m_passthrough) {
      if (output == button) {
        return true;
      }
    }
    return false;
  }

  std::optional<KeyButton> allocateButton(KeyButton preferred) const
  {
    preferred &= IKeyState::s_numButtons - 1;
    if (preferred != 0 && !buttonInUse(preferred)) {
      return preferred;
    }
    for (KeyButton button = 1; button < IKeyState::s_numButtons; ++button) {
      if (!buttonInUse(button)) {
        return button;
      }
    }
    return std::nullopt;
  }

  Target targetFor(Source source) const
  {
    return m_overrides[source.id][source.side].value_or(m_generic[source.id]);
  }

  KeyModifierMask translateMask(KeyModifierMask mask, std::optional<Source> released) const
  {
    KeyModifierMask translated = mask & ~s_translatedMasks;
    for (KeyModifierID id = kKeyModifierIDShift; id < kKeyModifierIDLast; ++id) {
      const bool releasing = released && released->id == id;
      // Masks normally describe the current state. On release, however, some
      // platforms clear the shared bit even though the other side remains held.
      if ((mask & s_masks[id]) == 0 && !releasing) {
        continue;
      }
      bool knownSide = false;
      for (size_t side = 0; side < 2; ++side) {
        if (m_active[id][side]) {
          knownSide = true;
          translated |= m_active[id][side]->target.mask();
        }
      }
      // Enter carries no side information, and leave discards held identities.
      // Only retain mask bits common to both possible targets: guessing the
      // generic mapping could turn a held Hangul source into Ctrl+A on reentry.
      if (!knownSide && !releasing) {
        translated |= targetFor({id, 0}).mask() & targetFor({id, 1}).mask();
      }
    }
    return translated;
  }

  std::array<Target, kKeyModifierIDLast> m_generic;
  std::array<std::array<std::optional<Target>, 2>, kKeyModifierIDLast> m_overrides;
  std::array<std::array<std::optional<ActiveKey>, 2>, kKeyModifierIDLast> m_active;
  std::map<KeyButton, KeyButton> m_passthrough;
  KeyModifierMask m_lastMask = 0;
};

} // namespace deskflow
