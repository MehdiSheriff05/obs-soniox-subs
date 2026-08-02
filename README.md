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

## Build

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
