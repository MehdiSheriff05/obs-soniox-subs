# Live Captions (Soniox) — OBS Plugin

An OBS Studio plugin that shows live Urdu -> English translated captions
during mosque livestreams, powered by [Soniox](https://soniox.com)'s
real-time speech-to-text translation API. Built for a non-technical
volunteer operator: a single dock with an audio source picker, a live
caption preview, an audio input level meter, and plain-language status/error
messages (technical detail on hover).

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
- Check your Soniox account's own usage credits/quota independently — this
  plugin doesn't surface remaining account balance, only per-connection
  errors from the API itself.

## Installation

These are end-user steps for someone who already has a built plugin (either
downloaded from a Release, or built themselves per the **Build** section
below) — not a development setup.

### macOS

1. Copy `obs-soniox-subs.plugin` into
   `~/Library/Application Support/obs-studio/plugins/`.
2. If it was downloaded rather than built locally, macOS Gatekeeper will
   likely quarantine it. Either right-click the `.plugin` bundle > **Open**
   once, or clear the quarantine flag from Terminal:
   ```
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/obs-soniox-subs.plugin
   ```
3. Restart OBS.
4. Open the **Docks** menu (or **View > Docks** on older OBS versions) and
   check **Live Captions** to show the dock.
5. Pick your audio source, enter your Soniox API key, and press **Start**.

### Windows

**Not yet built or tested** — this project has only been built and verified
on macOS so far (see [HANDOFF.md](HANDOFF.md)). Once a Windows build exists,
installation will follow standard OBS plugin conventions: drop the `.dll`
into `obs-plugins\64bit\` and its data files into
`data\obs-plugins\obs-soniox-subs\` under the OBS install directory (or the
portable `obs-studio\` folder root, for a portable install), then restart
OBS. This section will be updated once that build is confirmed working.

## Updating

There's no in-app auto-updater (that would need a separate updater framework
like Sparkle — more scope than this project needs right now). To update:

- **Built from source:** `git pull`, then re-run the build/install steps
  below, then restart OBS.
- **Installed from a downloaded release:** download the new `.plugin`,
  replace the old one at
  `~/Library/Application Support/obs-studio/plugins/obs-soniox-subs.plugin`,
  then restart OBS. Your saved API key and settings live in OBS's own config,
  not in the plugin bundle, so they survive the replacement.

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
