# Package Browser

Desktop application in **C++/Qt 6** for browsing packages of different managers (pip, npm, vcpkg), answering "which environment does a package come from: global or project".

## Status

The project is at the initial stage: the skeleton (CMake + core scaffold) is created. Architecture and roadmap live in [docs/](docs/).

## Structure

```
src/       — application core (domain model, services)
tests/     — tests (Qt Test)
docs/      — architecture documentation
```

## Build

Requires CMake >= 3.16 and Qt 6.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Branches

The repository follows git-flow rules: `main` (stable), `develop` (development), `feature/*`, `release/*`, `hotfix/*`. See [AGENTS.md](AGENTS.md) for details.