# AGENTS.md

Instructions for agents working with this repository.

## Project

Desktop application in **C++/Qt 6 (Widgets)**: browsing packages of different managers (pip, npm, vcpkg) answering the question "which environment does a package come from". Architecture is described in [docs/](docs/) (in particular `docs/Package Browser.md` and `docs/Доменная модель.md`).

Stack: CMake, C++17, Qt 6 Widgets, SQLite (Qt SQL), toml++ (via FetchContent), JSON-RPC 2.0 over stdio for process providers. Built with CMake. Source layout: `app/` (Qt Widgets UI), `src/` (core: domain, catalog, plugins, providers, scanner, settings), `tests/`, `plugins/`.

## Gitflow rules (mandatory)

The repository follows **git-flow** (the add-on is not used — branches are created and managed manually):

- **`main`** — stable branch. Releases only. Direct commits to `main` are forbidden.
- **`develop`** — integration branch for development. Finished features are merged here.
- **`feature/<name>`** — feature branches. Created **only from `develop`**, merged back into `develop` (`--no-ff`).
- **`release/<version>`** — release preparation: created from `develop`, merged into both `main` and `develop`.
- **`hotfix/<name>`** — urgent fixes: created from `main`, merged into both `main` and `develop`.

Mandatory agent rules:

1. **Never** commit or make changes directly in `main`.
2. For every task (new feature or fix) create a `feature/<name>` branch from the up-to-date `develop`.
3. Change `develop` and `main` only through merging finished branches, release commits and hotfixes.
4. Before starting a new feature: `git switch develop`, `git pull --ff-only` (if you are confident about freshness), then `git switch -c feature/<name>`.
5. When merging into `develop`, use `--no-ff` only.
6. Do not list your current `feature/<name>` branch in AGENTS.md — it is one-off, created and deleted while working.

## Workflow (mandatory)

Follow this order for every task:

1. **Plan** — state the goal, the roadmap acceptance criterion it must satisfy, the files/layers involved and the risks. For non-trivial or multi-step work, also keep a visible todo list.
2. **Get the plan approved** — if there are open choices that change the direction, ask before implementing; do not guess.
3. **Create a branch** — `git switch develop`, `git pull --ff-only` (when freshness is certain), then `git switch -c feature/<name>`.
4. **Implement** the planned changes layer by layer. Commit per logical unit during the work (feat/fix/docs/test), each with an English message.
5. **Write tests** for the new code **and** add regression tests for any bug fixed. Test data must be realistic (mirror the real file/system format), otherwise a green test can mask a real defect.
6. **Verify** — the project builds cleanly and the full test suite is green, not only the new tests:
   ```sh
   cmake -S . -B build
   cmake --build build
   ctest --test-dir build
   ```
   Treat `-Wall -Wextra` warnings as findings to fix.
7. **Request review** — run the reviewer over the diff of `feature/<name>` against `develop` (there is no remote/PR; the review is a diff review).
8. **Apply** the required fixes from the review.
9. **Commit** the review fixes with an English message.
10. **Merge into `develop`** — `git switch develop`, `git merge --no-ff feature/<name>`, then delete the feature branch.
11. **Update documentation** — refresh `docs/` (and this file) whenever behavior, architecture or the process itself changed.

## Language for commits and code (mandatory)

- Commit messages — **English only**.
- Comments in source code (C++/CMake/scripts) — **English only**.
- String literals in source code — **English only**, including the text inside `tr()` (no exceptions).

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

## Tests

Infrastructure: Qt Test (`Qt6::Test`). Run: `ctest --test-dir build`. Disable tests: `-DPACKAGE_BROWSER_BUILD_TESTS=OFF`.