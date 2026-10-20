# Package Browser

Desktop application in **C++/Qt 6 (Widgets)** for browsing packages of different managers (pip, npm, vcpkg), answering "which environment does a package come from: global or project".

## Status

Stages 1–4 of the roadmap are implemented: domain model, scanning with trust, declarative and process (JSON-RPC) providers, dependency edges and cache startup. Architecture and roadmap live in [docs/](docs/).

## Structure

```
app/       — Qt Widgets UI (window, models, Settings tab)
src/       — application core (domain model, services, plugins, providers)
tests/     — tests (Qt Test)
docs/      — architecture documentation
plugins/   — plugin manifests and process providers
```

## Build

Requires CMake >= 3.16 and Qt 6. `toml++` is fetched at configure time via `FetchContent`. Build options: `PACKAGE_BROWSER_BUILD_UI` (default ON) and `PACKAGE_BROWSER_BUILD_TESTS` (default ON).

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Branches

The repository follows git-flow rules: `main` (stable), `develop` (development), `feature/*`, `release/*`, `hotfix/*`. See [AGENTS.md](AGENTS.md) for details.

## License

Licensed under the **GNU General Public License v3.0** (GPLv3). See [LICENSE](LICENSE) for the full license text.

Third-party software (Qt, toml++) and their required notices are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).