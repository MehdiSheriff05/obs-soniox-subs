# Live Captions (Soniox) — OBS Plugin

An OBS Studio plugin that shows live Urdu -> English translated captions
during mosque livestreams, powered by [Soniox](https://soniox.com)'s
real-time speech-to-text translation API. Built for a non-technical
volunteer operator: a single dock, split into four tabs so the everyday
controls stay uncluttered:

- **Captions** — audio source picker, max caption length, Start/Stop, a
  live caption preview, an audio input level meter, and plain-language
  status/error messages (technical detail on hover). This is the only tab
  you need for day-to-day use.
- **Stats** — elapsed session time, an estimated cost for the session, and
  a reconnect counter (a rising count usually means a shaky network).
- **Settings** — the Soniox API key, and a **Check for Updates** button.
- **Appearance** — font and an outline/border toggle for the on-screen
  caption text (the same settings are applied on both macOS and Windows).

Created by [Mehdi Sheriff](https://github.com/MehdiSheriff05).

See [HANDOFF.md](HANDOFF.md) for the full build history, real issues found
and fixed against the actual OBS SDK/Qt/IXWebSocket sources, and what still
needs manual end-to-end verification.

## Known limits (Soniox API)

- Each WebSocket connection supports **up to 300 minutes of audio**. A
  khutbah running longer than that will need the connection re-established
  (not yet handled automatically — `SonioxClient` will need a "max duration
  reached" case added if this becomes a real problem).
- Auth failures (invalid/expired API key) are **not retried** — the plugin
  surfaces "Invalid API key" and stops rather than reconnect-looping against
  a bad key. Other errors (rate limiting, timeouts, transient network) retry
  with exponential backoff.
- API key and last-used audio source are stored in OBS's user config
  (`obs_frontend_get_user_config()`) as plain text — standard for a
  single-operator local setup, not a secrets vault.
- The Stats tab's session cost is an **estimate**, computed locally from
  elapsed time × Soniox's published flat real-time rate ($0.12/hour,
  translation included at no extra charge, per
  [soniox.com/pricing](https://soniox.com/pricing) as of Aug 2026) — it is
  not a real bill (actual Soniox billing is token-based) and it is not your
  account balance. Check your Soniox account's own usage/quota page
  independently for that.
- The **Appearance** tab's outline toggle sends the same request on both
  platforms, but OBS's two text source implementations render it
  differently: `text_gdiplus` (Windows) draws a real black stroke;
  `text_ft2_source` (macOS/Linux) has no separate outline color, so it just
  softens the text edge instead. That's a difference in OBS's own text
  sources, not something this plugin's settings can paper over. A
  translucent background box was considered but dropped — it only exists
  on Windows' text source at all, and this plugin only ships features that
  behave the same on both platforms.
- The font picker only takes effect if the chosen font is actually
  installed on the machine running OBS — e.g. the default, **Poppins**, is
  a Google font not preinstalled on macOS or Windows.

## Installation

Download the build for your platform from the
[Releases page](https://github.com/MehdiSheriff05/obs-soniox-subs/releases)
(or build it yourself per the **Build** section below).

### macOS

1. Unzip/open the download and copy `obs-soniox-subs.plugin` into
   `~/Library/Application Support/obs-studio/plugins/`.
2. Downloaded files get quarantined by Gatekeeper. Either right-click the
   `.plugin` bundle > **Open** once, or clear the quarantine flag from
   Terminal:
   ```
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/obs-soniox-subs.plugin
   ```
3. Restart OBS.
4. Open the **Docks** menu (or **View > Docks** on older OBS versions) and
   check **Live Captions** to show the dock.
5. Pick your audio source, enter your Soniox API key, and press **Start**.

### Windows

**Option A — Installer (recommended):** run the downloaded
`...-windows-x64-Installer.exe`. It installs to
`%ProgramData%\obs-studio\plugins\obs-soniox-subs\` — OBS's recommended
location for third-party plugins, which doesn't require admin rights — and
appears in Windows' Add/Remove Programs for easy uninstalling.

**Option B — Manual zip:** unzip the `.zip` and copy the `obs-soniox-subs`
folder inside it (containing `bin\64bit\obs-soniox-subs.dll` and `data\`)
into `%ProgramData%\obs-studio\plugins\` (create the `plugins` folder if it
doesn't exist yet), so you end up with
`%ProgramData%\obs-studio\plugins\obs-soniox-subs\bin\64bit\obs-soniox-subs.dll`.

Either way, then:

1. Restart OBS.
2. Open the **Docks** menu (or **View > Docks** on older OBS versions) and
   check **Live Captions** to show the dock.
3. Pick your audio source, enter your Soniox API key, and press **Start**.

## Updating

The **Settings** tab checks GitHub Releases for a newer version automatically
on startup, and has a **Check for Updates** button for on demand checks. When
a newer version is available, an **Install Update** button appears on the
Captions tab too — it downloads the matching installer for your platform and
launches it for you.

This isn't a fully seamless update: neither platform can safely replace its
own plugin file while OBS has it loaded, so **you still need to restart OBS**
once the installer finishes for the new version to take effect. Your saved
API key and settings live in OBS's own config, not in the plugin files, so
they survive the update either way.

To update manually instead (or if you built from source):

- **Built from source:** `git pull`, then re-run the build/install steps
  below, then restart OBS.
- **Installed from a downloaded release:** download the new build from the
  [Releases page](https://github.com/MehdiSheriff05/obs-soniox-subs/releases)
  and run/copy it per the **Installation** section above, then restart OBS.

## Build (from source)

```
cmake --preset macos
cmake --build --preset macos
cmake --install build_macos --config RelWithDebInfo
```

Requires Xcode 16.0+, CMake 3.28+. First build fetches the OBS SDK source,
prebuilt OBS/Qt6 dependencies, and vendors IXWebSocket via `FetchContent` —
see [HANDOFF.md](HANDOFF.md) for what's actually pinned and why.

## Supported Build Environments

| Platform  | Tool   |
|-----------|--------|
| Windows   | Visual Studio 17 2022 |
| macOS     | XCode 16.0 |
| Windows, macOS  | CMake 3.30.5 |
| Ubuntu 24.04 | CMake 3.28.3 |
| Ubuntu 24.04 | `ninja-build` |
| Ubuntu 24.04 | `pkg-config`
| Ubuntu 24.04 | `build-essential` |

## GitHub Actions & CI

Default GitHub Actions workflows (inherited from
[obsproject/obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate))
are available for the following repository actions:

* `push`: Run for commits or tags pushed to `master` or `main` branches.
* `pr-pull`: Run when a Pull Request has been pushed or synchronized.
* `dispatch`: Run when triggered by the workflow dispatch in GitHub's user interface.
* `build-project`: Builds the actual project and is triggered by other workflows.
* `check-format`: Checks CMake and plugin source code formatting and is triggered by other workflows.

The workflows make use of GitHub repository actions (contained in `.github/actions`) and build scripts (contained in `.github/scripts`) which are not needed for local development, but might need to be adjusted if additional/different steps are required to build the plugin.

### Building a Release

To create a release, an appropriately named tag needs to be pushed to the `main`/`master` branch using semantic versioning (e.g., `0.2.0`). A draft release will be created on the associated repository with generated installer packages or installation programs attached as release artifacts.

## Signing and Notarizing on macOS

Basic concepts of codesigning and notarization on macOS are explained in the [obs-plugintemplate Wiki article](https://github.com/obsproject/obs-plugintemplate/wiki/Codesigning-On-macOS), which has a specific section for the [GitHub Actions setup](https://github.com/obsproject/obs-plugintemplate/wiki/Codesigning-On-macOS#setting-up-code-signing-for-github-actions). Not yet done for this plugin — current builds are ad-hoc signed (`CODESIGN_IDENTITY=-`), fine for local testing, not for distribution.
