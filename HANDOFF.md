# HANDOFF

OBS Studio plugin (macOS first, Windows later) that shows live Urdu -> English
translated captions during mosque livestreams, powered by Soniox's real-time
speech translation WebSocket API. Scaffolded from
[obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate),
with hand-written source in `src/`:

- `plugin-main.cpp` — module entry point, registers the dock.
- `CaptionsDock.{h,cpp}` — the Qt dock widget: audio source picker, API key
  field, Start/Stop, status line, level meter, live caption preview.
- `AudioBridge.{h,cpp}` — taps an OBS source's raw audio via
  `obs_source_add_audio_capture_callback`, resamples to 16kHz mono s16le, and
  drives the level meter.
- `SonioxClient.{h,cpp}` — WebSocket client (IXWebSocket, vendored via
  FetchContent) implementing Soniox's real-time speech-to-text-translation
  protocol.

## Build

```
cmake --preset macos
cmake --build --preset macos
cmake --install build_macos --config RelWithDebInfo
```

The install step drops the plugin into
`~/Library/Application Support/obs-studio/plugins/obs-soniox-subs.plugin`.
**Known issue:** this same `cmake --install` also installs ixwebsocket's own
dev files (headers, `lib/cmake`, a `.pkg` installer) into that directory,
because it shares our `CMAKE_INSTALL_PREFIX`. `EXCLUDE_FROM_ALL` has been
added to ixwebsocket's `FetchContent_Declare` to stop this, but this hasn't
been re-verified with a clean `.deps`/`build_macos` wipe — if clutter
reappears, `rm -rf` the extra `include/`, `lib/`, `*.pkg` files after install
and it won't affect OBS (it only reads the `.plugin` bundle).

## What broke on first build, and what was actually true

The five things flagged as likely to break, and what was actually found by
checking the real fetched OBS SDK / Qt / IXWebSocket sources directly (not
guessed):

1. **Text source IDs.** Real IDs, confirmed in
   `plugins/text-freetype2/text-freetype2.c` and `plugins/obs-text/gdiplus/obs-text.cpp`:
   - macOS/Linux: `text_ft2_source`
   - Windows: `text_gdiplus` — **not** `text_gdiplus_source` as you'd
     naturally guess. `CaptionsDock.cpp` picks the right one with
     `#if defined(_WIN32)`.

2. **`audio_resampler_resample` call signature.** Confirmed against real call
   sites (`obs-source.c`, `audio-io.c`, `coreaudio-output.c`): the caller
   declares an uninitialized `uint8_t *output[MAX_AV_PLANES]`, and the
   resampler fills in pointers to its *own* internally-owned buffer — the
   caller does not allocate the output buffer. `AudioBridge::handleAudio`
   follows this pattern.

3. **OBS frontend config API.** `obs_frontend_get_global_config()` is
   `OBS_DEPRECATED` in the fetched 31.1.1 header. The plugin uses
   `obs_frontend_get_user_config()` instead (confirmed present, intended for
   exactly this kind of cross-profile user setting like an API key), storing
   under a `SonioxCaptions` section via `config_get_string`/`config_set_string`.
   Note: `<util/config-file.h>` has to be included explicitly for these —
   `obs.h` does not pull it in.

4. **Whether the prebuilt Qt6 includes WebSockets.** Checked the actual
   fetched `obs-deps-qt6-2025-07-11-universal` package: **no
   `QtWebSockets.framework`** — confirmed absent, only Core/Gui/Widgets/
   Network/Multimedia/Svg/etc. `SonioxClient` uses vendored **IXWebSocket**
   (v11.4.6, fetched via `FetchContent`) instead, with `USE_TLS=ON` (on
   Apple this auto-selects Secure Transport — no OpenSSL dependency needed,
   confirmed in the actual configure log: `TLS configured to use secure
   transport`) and `USE_WS=OFF` (that flag builds an unrelated bundled CLI
   tool we don't need — turning it off avoids a msgpack11 dependency
   entirely).

5. **Reconnect loop behavior on auth errors.** Per Soniox's docs, clients
   should branch on `error_type`, not `error_message`. `SonioxClient`
   distinguishes `error_type == "unauthenticated"` (or HTTP 401/403) as
   **non-retryable** — it surfaces "Invalid API key" and stops, rather than
   hammering the API with a bad key in a reconnect loop. All other errors
   (429, 408, transient network) get exponential backoff up to 16s.

## Other real issues found during the build (not in the original five)

These were genuine build/link failures against the real toolchain on this
machine, not hypothetical:

- **`CMakePresets.json`'s hidden `template` preset silently force-set
  `ENABLE_QT: false` and `ENABLE_FRONTEND_API: false`** as cache variables,
  which override `option()` defaults in `CMakeLists.txt` entirely. This
  would have silently produced a plugin with no UI and no frontend API
  at all — no error, just missing functionality. Fixed by flipping both to
  `true` in the preset itself.
- **Xcode's non-interactive first-launch step** (`sudo xcodebuild
  -runFirstLaunch`) was required before CMake's Xcode generator could detect
  a working compiler at all — separate from `xcodebuild -license accept`.
  Pure one-time machine setup, not a code issue, but worth documenting for
  the next fresh machine.
- **`-Werror` leaking into vendored third-party sources.** OBS's build sets
  `CMAKE_COMPILE_WARNING_AS_ERROR`, which CMake's Xcode generator captures
  per-target at creation time (via `CMAKE_XCODE_ATTRIBUTE_
  GCC_TREAT_WARNINGS_AS_ERRORS`). This broke building spdlog and msgpack11
  (pulled in transitively by IXWebSocket), which aren't warning-clean.
  Scoped the attribute off around `FetchContent_MakeAvailable`, explicitly
  disabled it per-target as a belt-and-suspenders measure, and marked
  ixwebsocket's own headers as `SYSTEM` includes on our target so our
  strict flags don't fail on warnings inside vendored headers we `#include`
  directly.
- **`AGL.framework` link failure.** Qt's static macOS build declares a
  transitive link dependency on `AGL.framework` (legacy pre-Metal OpenGL,
  via Qt's `.prl` metadata) for backward compat. On this machine's SDK
  (macOS 26.2 SDK / Xcode 26.3), `AGL.framework` has **no linkable Mach-O
  left anywhere** — not in the SDK, not at the live system path (confirmed:
  `/System/Library/Frameworks/AGL.framework` is an empty shell, same as
  every other system framework on modern macOS since real binaries only
  live in the dyld shared cache now). Nothing in Qt/libobs's actual
  Cocoa/Metal rendering path calls into AGL at runtime, so `CMakeLists.txt`
  synthesizes an empty stub `AGL.framework` at configure time and adds it
  to the link search path — satisfies the linker, no real symbols needed.
- **`QJsonArray` range-for bug.** `for (const QJsonValue &v : tokens)`
  triggered `-Wrange-loop-bind-reference` — binding a `const &` to a
  temporary `QJsonValueRef`. Real, not just a style nit. Fixed to iterate
  by value.
- **Caption buffer never reset (functional bug, not a build error).** The
  first draft of `CaptionsDock::onCaptionReady` accumulated finalized
  translation text into one string for the entire session, so both the
  preview pane and the on-screen text source would have grown unbounded
  over a 45+ minute khutbah. Fixed with a `kLineCommitChars` (100 char)
  threshold: once a line gets long, it's committed to the preview
  scrollback and a fresh line starts, both in the dock preview and the
  on-screen caption text source.
- **OBS SDK version vs. installed OBS.app version mismatch — the big one.**
  The template pins OBS SDK 31.1.1 / Qt 6.8.3 in `buildspec.json`. The
  machine's installed OBS.app was **31.0.4 / Qt 6.6.3**. Qt embeds a
  version-tag symbol (`_qt_version_tag_6_8`) that a plugin requires at
  `dlopen` time; Qt 6.6.3 doesn't have it (it predates 6.8), so the plugin
  failed to load with `Symbol not found: _qt_version_tag_6_8` — silent as
  far as OBS's UI is concerned, only visible in the log
  (`~/Library/Application Support/obs-studio/logs/`). **The fix was
  updating OBS**, not downgrading the build: Qt's version tags are
  cumulative (a Qt 6.11 build embeds tags for every earlier minor it
  maintains ABI compatibility with, back through the baseline), so **any
  OBS bundling Qt 6.8 or newer works** — it doesn't need to exactly match
  31.1.1. Confirmed working after updating to OBS 32.2.1 (bundles Qt
  6.11.1). If you ever see a `dlopen`/`Symbol not found: _qt_version_tag_*`
  error in the OBS log, this is almost certainly what's wrong, and the fix
  is to check the installed OBS's bundled Qt version against
  `buildspec.json`'s pinned `qt6` release.

## Verified so far

- Clean build (`cmake --build --preset macos`) — no errors, only expected
  warnings in vendored code.
- Universal binary (arm64 + x86_64) confirmed via `lipo -info`.
- Installed into the real OBS plugins directory and loaded in a real,
  currently-running OBS 32.2.1: log shows
  `[obs-soniox-subs] plugin loaded successfully (version 0.1.0)`, listed in
  `Loaded Modules`, no crash.
- "Live Captions" dock entry confirmed present under the **Docks** menu
  (this OBS version has `Docks` as its own top-level menu, not nested under
  `View`) via accessibility scripting; toggling it on didn't crash the
  process.
- Minor cosmetic issue: log shows `Failed to load 'en-GB' text for module:
  'obs-soniox-subs'` — only `data/locale/en-US.ini` exists, no `en-GB.ini`.
  Harmless (falls back), but worth adding an `en-GB.ini` (or more locales)
  later if you want non-US-English users to see translated UI strings.

## Not yet verified — needs your hands

This session's shell has no Screen Recording or Accessibility window-content
permission, so I could confirm the dock *registers* and doesn't crash the
process, but couldn't drive clicks/typing inside it (menu-bar scripting
worked; window-content scripting and `screencapture` both failed). Still
needs a real manual pass:

- Selecting an audio source in the dropdown and clicking **Start** — does
  `AudioBridge::start()` actually attach to the source and start the level
  meter moving?
- Does `ensureCaptionTextSource()` successfully create/find the
  `text_ft2_source` source named "Soniox Live Captions" and add it to the
  current scene without crashing?
- A real Soniox API key: does the WebSocket connect, does the config
  message get accepted, do translated captions actually start appearing in
  the preview and on-screen text source?
- The **"No audio detected"** watchdog and **"Invalid API key"** /
  **"Connection lost, retrying..."** plain-language error paths — these are
  implemented but never triggered against the real service.
- End-to-end with real Urdu audio (e.g. a YouTube khutbah played through the
  selected input device) — translation quality/latency, whether captions
  read naturally, whether the line-commit threshold (100 chars) produces
  reasonably-sized on-screen caption lines for this specific use case.

## Repo / remote

Pushed to `https://github.com/MehdiSheriff05/obs-soniox-subs` on `main`.
