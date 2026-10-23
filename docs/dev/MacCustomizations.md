# Local macOS customizations

## Baseline

Review snapshot: 2026-10-23. `custom/macos-upstream` starts at `upstream/master`,
`c1b19244a3` (`v1.27.0+17`; `git describe`: `v1.27.0-17-gc1b19244a3`). The
customizations are a local reimplementation on that baseline, not a published
release.

- `origin`: <https://github.com/yattmak/deskflow.git> (fork).
- `upstream`: <https://github.com/deskflow/deskflow.git> (original project).
- Preserve `codex/macos-side-modifiers` at `7be3753f1f` as the old custom
  reference; do not replace the newer upstream tree with that older tree.

See also [configuration](../Configuration.md) and [building](build.md).

## Modifier mapping and migration

Per-computer GUI settings (`[computer_<name>]`) retain common `modifierMeta` /
`modifierSuper` mappings and add `modifierMetaLeft`, `modifierMetaRight`,
`modifierSuperLeft`, `modifierSuperRight` source overrides. An absent/empty
side value means **inherit the common mapping**, not identity or `none`.
The GUI labels this `Inherit Meta` / `Inherit Super`; `none` explicitly consumes
the source. Common mappings default to identity. These are Deskflow source
identities: macOS Command is captured as `Super_L` / `Super_R`, not Meta.

Targets include the existing modifiers, `none`, `hangul` and `hanja`.
The server and affected clients must both use the custom build: protocol v1.8
does not negotiate these extensions, and stock clients may ignore side options
or misinterpret raw-key targets.
Configuration also accepts supported key names/printable KeyIDs; the GUI
preserves an existing raw-key target outside its standard choices. Lock-key
targets (`CapsLock`, `NumLock`, `ScrollLock`) are rejected because lock state
is synchronized separately.

Keep value spaces distinct: current GUI `KeyboardModifier` ordinals are
`AltGr=5`, `None=6`, `Hangul=7`, `Hanja=8`; wire modifier IDs differ
(`kKeyModifierIDNull=0`), and key targets use actual KeyIDs such as `kKeyHangul`
/ `kKeyHanja`. Never send combo indices as protocol values or index a modifier
table with a raw KeyID.

Back up settings before migration. `Settings::upgradeSettings()` recognizes
only the old custom **ten-slot** `internalConfig/screens/.../modifierArray`
format as `None=5`, `Hangul=6`. Source slots 6–9 become the four side keys
above; an unset (`-1`) side remains empty/inherited. Old source slot 5 was the
None pseudo-source, not AltGr. Six/seven-slot upstream arrays retain
`AltGr=5`, `None=6`. Migration writes string-valued
`computer_<name>/modifier...` keys and preserves existing new keys, including
during the `screen_` to `computer_` rename.

Implementation: `src/lib/common/{KeyboardModifier.h,Settings.cpp}`,
`src/lib/gui/config/Computer.{h,cpp}`,
`src/lib/gui/dialogs/ComputerSettingsDialog.cpp`,
`src/lib/server/Config.cpp`, `src/lib/deskflow/OptionTypes.h`.

## Held keys and protocol limits

`src/lib/client/ModifierTranslation.h` retains each source's press target and
output button for repeat/up, even if options change while held. A `kKeyNone`
release can be matched by source button. Sources translating to the same KeyID
share an output press until the last source releases; different KeyIDs that
alias to one physical platform key are not detected as shared here.

An options reset restores common defaults/clears overrides **without releasing
held targets**; new mappings apply on subsequent presses. Session cleanup in
`ServerProxy` (leave, enter cleanup, screensaver-on) releases tracked outputs
and clears held identities. Do not substitute session cleanup for an options
refresh.

The enter protocol carries an aggregate mask, **not left/right identity**.
After leave/re-entry, a still-held source cannot be reconstructed reliably.
Mask-only translation keeps only modifier bits common to both possible side
targets: it does not guess the common mapping or invent a Hangul/Hanja press.
A shortcut held across a screen transition may need release/re-press. This is
a deliberate safe limitation, not full side-state synchronization.

## IME, Secure Input and Qt compatibility

`src/lib/deskflow/unix/OSXInputSource.cpp` requires usable Unicode layout data
and prefers: current keyboard layout (when the active source has layout data
or that layout is selectable), current ASCII-capable layout, enabled/selectable
ABC, then US, then another enabled/selectable ASCII-capable layout. Otherwise
it returns no translation source. This preserves layouts such as Dvorak and
avoids using an IME's private non-selectable layout merely because it exists.
The chosen translation language travels with the key event through
`OSXKeyState`, `IKeyState::KeyInfo` and `Server`; it need not be the active IME's
language. This fallback **does not select or change the user's input source**.

On macOS, `Server.cpp` checks Secure Input immediately and every **2 seconds**,
even without connected clients. Each poll resends the `LOG_PRINT` state record,
including unchanged states, so a missed record can recover on the next heartbeat.
Its `secure input status: ` JSON records
feed `gui/SecureInputState.cpp` / `gui/MainWindow.cpp`: warning tray icon, tooltip
and disabled menu status, with the blocking app when available. Recovery clears
the warning even with INFO logs filtered out; stop/disconnect or switching to
client mode also clears stale GUI state. This is notification only, **not a
Secure Input bypass**; capture remains blocked while another app holds it.

The macOS 27 status/menu-bar `SysDefined` / `clickCount` crash is addressed in
Qt's dependency source, not by a Deskflow source patch. In
[Qt v6.12.0 `qcocoasystemtrayicon.mm`](https://github.com/qt/qtbase/blob/v6.12.0/src/plugins/platforms/cocoa/qcocoasystemtrayicon.mm),
`QCocoaSystemTrayIcon::emitActivated()` emits `QPlatformSystemTrayIcon::Unknown`
and returns when `qt_mac_isMouseEvent(NSApp.currentEvent)` is false, before
accessing `clickCount`. Its comment explains that macOS 27 drives NSControls
through gesture recognizers, so the action callback's current event may not be
a mouse event. The packaged Qt 6.12.0 frameworks and Cocoa plugin passed a local
native regression probe: 20 menu-tracking notifications with `currentEvent`
controlled to be `SysDefined` produced `Unknown` activations without an exception.
This exercises the failing callback, not a physical menu-bar click. The full GUI
exits at its accessibility permission check until the new app is authorized.

## Local build and validation

The local CMake cache uses official Qt 6.12.0 in `deps/Qt/6.12.0/macos`,
`build/macos-qt612`, and Homebrew OpenSSL 3. Required Qt installation archives
for this build are `qtbase`, `qttools`, `qttranslations`, `qtsvg` and
`qtdeclarative`: `lupdate` also depends on QtQml, so do not omit `qtdeclarative`.
`DEPLOYQT` must use the **same Qt installation's** `macdeployqt`, not another Qt
on `PATH`.

The previous Xcode.app SDK path became unavailable during the session. The
build is now using `/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk`,
verified to exist and recorded in `CMAKE_OSX_SYSROOT`. Run from the repository
root; all absolute prefixes below are host-specific. Verify the selected SDK
exists on the build host before configuring; do not assume this CLT version or
an Xcode.app path is present elsewhere.

```sh
test -d /Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk &&
cmake -S . -B build/macos-qt612 -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX26.5.sdk \
  -DCMAKE_PREFIX_PATH=/Users/mini1/Dev/deskflow/deps/Qt/6.12.0/macos \
  -DDEPLOYQT=/Users/mini1/Dev/deskflow/deps/Qt/6.12.0/macos/bin/macdeployqt \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 \
  -DBUILD_TESTS=ON -DSKIP_BUILD_TESTS=ON
cmake --build build/macos-qt612
XDG_CONFIG_HOME=/Users/mini1/Dev/deskflow/build/test-config \
XDG_STATE_HOME=/Users/mini1/Dev/deskflow/build/test-state \
ctest --test-dir build/macos-qt612/src/unittests --output-on-failure --timeout 60
cmake --install build/macos-qt612 --prefix /Users/mini1/Dev/deskflow/build/install-qt612
```

`SKIP_BUILD_TESTS=ON` skips automatic execution during the build, not building
tests. Isolate `XDG_CONFIG_HOME` and `XDG_STATE_HOME` for tests and GUI probes so
the user's real settings are not migrated or overwritten.

Validation on macOS 27.0.1, arm64, Qt 6.12.0: the Release build, all 31 CTest
targets, packaged GUI/core `--version`, and deep/strict code-signature verification
passed. Migrating a copy of the user's settings preserved all eight side overrides
in the two custom ten-slot profiles; the original settings files were unchanged.
The local Homebrew OpenSSL was built for macOS 27 and Qt requires at least 14.4;
linker deployment-target warnings mean this artifact is not validated for
older macOS releases. Rebuild dependencies for the intended deployment target
before distributing it.

`deploy/mac/deploy.cmake` retains hardened runtime and timestamping when a signing
identity is supplied. Local ad-hoc builds omit those options: ad-hoc executables
have no Team ID for hardened runtime's library validation, which otherwise
prevents loading the bundled Qt libraries. This does not grant accessibility
permissions or change the installed application.

Existing regression tests: `ServerProxyTests` (translation/held keys/reset/
re-entry), `SettingsTests` (migration/protocol values), `ComputerTests` (GUI
round-trips), `ServerConfigTests` (parsing), `SecureInputStateTests` (GUI state),
and `OSXKeyStateTests` (side flags/layout policy). Platform tests are selected
by the build host; a macOS run does not validate Windows/Linux backends.
The user subsequently reported resolving macOS permission setup. Physical tray
clicks, actual IME/Secure Input and mixed-platform server/client behavior were not
independently verified by this implementation's automated checks.

## Windows handoff

Check out `custom/macos-upstream` from the fork (`origin`), not the old custom
branch or upstream's unmodified `master`. Follow [building](build.md) for Windows
prerequisites; the macOS `.app`, local Qt installation and settings backups are
build artifacts and are not part of the source transfer.

Windows compilation and actual remote input remain unverified. First run
`ServerProxyTests`, `SettingsTests`, `ComputerTests` and `ServerConfigTests`, then
check left/right mapping, Hangul/Hanja with the Korean IME, both-side release
orders, options refresh while a key is held, and held-key leave/re-entry. Both the
server and affected clients must use this custom build.

## Updating upstream with a minimal patch

Preserve local changes and a settings backup before updating; keep
the old reference branch intact. Review upstream changes and reapply only
still-needed behavior: side options/translation/migration, macOS side tracking
and IME policy, and Secure Input reporting. Keep the Qt dependency guard above
when changing dependency versions; it is not a Deskflow patch to reapply.
Keep the related regression tests and re-check the implementation anchors after
conflict resolution. Do not copy the old tree wholesale, blindly reset the
working tree, or force-push the fork. Record the new baseline and actual
build/test/manual-check results before declaring an update validated.
Publish custom changes to the fork (`origin`), not the original project's
`upstream` remote. Commit and push only with explicit authorization.
