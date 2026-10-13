#!/usr/bin/env python3
"""JSON-RPC 2.0 provider for vcpkg over stdio (Content-Length framing).

The host runs this script as an external provider. How installations are read
depends on the environment's scope:

- scope=system (classic mode, the environment root is <VCPKG_ROOT>/installed/
  <triplet>): the trusted vcpkg.exe from VCPKG_ROOT answers `list --x-json`,
  which yields name, version, triplet and features authoritatively.
- scope=project (manifest mode, a directory with vcpkg.json): nothing is
  executed; packages are read from files under vcpkg_installed/ using the
  SPDX manifest (share/<pkg>/vcpkg.spdx.json), so foreign project code never
  runs. This mirrors the trust rule for project-local environments.

Only stderr may carry diagnostics; stdout is reserved for the protocol.
"""

import json
import os
import subprocess
import sys


def _read_message():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        line = line.strip()
        if not line:
            break
        key, value = line.split(b":", 1)
        headers[key.lower().strip()] = value.strip()
    length = int(headers[b"content-length"])
    return json.loads(sys.stdin.buffer.read(length))


def _send(payload):
    data = json.dumps(payload, ensure_ascii=False).encode()
    sys.stdout.buffer.write(b"Content-Length: %d\r\n\r\n" % len(data) + data)
    sys.stdout.buffer.flush()


def _error(req, code, message, kind):
    _send({
        "jsonrpc": "2.0",
        "id": req.get("id"),
        "error": {"code": code, "message": message,
                  "data": {"kind": kind}},
    })


def _result(req, result):
    _send({"jsonrpc": "2.0", "id": req.get("id"), "result": result})


def _vcpkg_root():
    return os.environ.get("VCPKG_ROOT", "").strip()


def _vcpkg_exe():
    return os.path.join(_vcpkg_root(), "vcpkg.exe")


def detect():
    root = _vcpkg_root()
    if not root:
        # No VCPKG_ROOT: no tool instance to report.
        return {"tools": []}
    return {"tools": [{"path": _vcpkg_exe(), "version": ""}]}


def locate(tool):
    root = os.path.dirname(tool.get("path", "")) if tool.get("path") else ""
    if not root:
        return {"environments": []}
    installed = os.path.join(root, "installed")
    if os.path.isdir(installed):
        # One system environment per triplet present.
        envs = []
        for triplet in sorted(os.listdir(installed)):
            if os.path.isdir(os.path.join(installed, triplet)):
                envs.append({
                    "root": os.path.join(installed, triplet),
                    "scope": "system",
                })
        return {"environments": envs}
    return {"environments": []}


def locate_at(scan_root):
    # Manifest mode: a project directory is an environment when it has
    # vcpkg.json. Packages are read from files (never executed).
    if os.path.isfile(os.path.join(scan_root, "vcpkg.json")):
        return {"environments": [{
            "root": scan_root,
            "scope": "project",
            "tool_id": "",
        }]}
    return {"environments": []}


def list_packages(env):
    root = env.get("root", "")
    if not root or not os.path.isdir(root):
        # The directory does not exist: nothing to read, but this is a real
        # answer, not a failure.
        return {"installations": [], "complete": True}

    if env.get("scope") == "project":
        installs = _list_from_files(root)
        # File-based scanning cannot distinguish "empty" from "cannot read"
        # robustly; treat a missing install dir as genuinely empty.
        return {"installations": installs, "complete": True}

    installs, ok = _list_from_vcpkg(root)
    return {"installations": installs, "complete": ok}


def _list_from_vcpkg(env_root):
    """Trusted system path: ask vcpkg.exe for the authoritative package list.

    Returns (installations, ok). `ok` is False when the list could not be
    produced (vcpkg.exe missing, crashed, or its output was not parseable), so
    the host can report "I don't know" instead of a misleading empty list.
    """
    triplet = os.path.basename(env_root.rstrip("\\/"))
    exe = _vcpkg_exe()
    if not os.path.isfile(exe):
        return [], False
    try:
        proc = subprocess.run(
            [exe, "list", "--x-json"],
            capture_output=True, text=True, timeout=10,
            check=False, encoding="utf-8", errors="replace")
    except (subprocess.SubprocessError, OSError):
        return [], False

    try:
        data = json.loads(proc.stdout)
        if not isinstance(data, dict):
            return [], False
    except json.JSONDecodeError:
        return [], False

    installs = []
    for key, info in data.items():
        # key is "<name>:<triplet>"; report only this triplet, drop
        # infrastructure/tool ports (vcpkg-cmake, vcpkg-tool-*, ...) so the list
        # shows user libraries.
        if not key.endswith(":" + triplet):
            continue
        if not isinstance(info, dict):
            continue
        name = info.get("package_name")
        if not name or name.startswith("vcpkg-"):
            continue
        installs.append({
            "name": name,
            "version": info.get("version") or "",
            "location": os.path.join(env_root, "share", name),
        })
    installs.sort(key=lambda i: i["name"])
    return installs, True


def _list_from_files(env_root):
    """Trusted-agnostic read of a project's manifest-mode install.

    Packages live under <root>/vcpkg_installed[/<triplet>]/share/<pkg>/ and are
    identified by vcpkg.spdx.json, whose packages[] carries SPDXRef-port with a
    clean name + versionInfo. Nothing is executed.
    """
    installs = []
    installed_dir = os.path.join(env_root, "vcpkg_installed")
    if not os.path.isdir(installed_dir):
        return installs

    # Both <root>/vcpkg_installed/<triplet>/share and /share are observed.
    for share_base in _share_candidates(installed_dir):
        for pkg in sorted(os.listdir(share_base)):
            pkg_dir = os.path.join(share_base, pkg)
            spdx = os.path.join(pkg_dir, "vcpkg.spdx.json")
            if not os.path.isdir(pkg_dir) or not os.path.isfile(spdx):
                continue
            name, version = _parse_spdx(spdx)
            if name:
                installs.append({
                    "name": name,
                    "version": version,
                    "location": pkg_dir,
                })
    return installs


def _share_candidates(installed_dir):
    candidates = []
    flat = os.path.join(installed_dir, "share")
    if os.path.isdir(flat):
        candidates.append(flat)
    try:
        for entry in os.listdir(installed_dir):
            nested = os.path.join(installed_dir, entry, "share")
            if os.path.isdir(nested):
                candidates.append(nested)
    except OSError:
        pass
    return candidates


def _parse_spdx(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            doc = json.load(fh)
    except (OSError, json.JSONDecodeError):
        return "", ""
    # packages[] has both SPDXRef-port (clean name + versionInfo) and
    # SPDXRef-binary (name=name:triplet, versionInfo=abi hash). Take "port".
    for pkg in doc.get("packages", []):
        if pkg.get("SPDXID") == "SPDXRef-port":
            return pkg.get("name", ""), pkg.get("versionInfo", "") or ""
    return "", ""


_HANDLERS = {
    "detect": lambda req: _result(req, detect()),
    "list": lambda req: _result(req, list_packages(req.get("params", {}).get("environment", {}))),
}


def _route(req):
    method = req.get("method")
    params = req.get("params")
    if method == "locate":
        tool = params.get("tool", {}) if params else {}
        _result(req, locate(tool))
    elif method == "locate_at":
        root = params.get("root", "") if params else ""
        _result(req, locate_at(root))
    elif method in _HANDLERS:
        _HANDLERS[method](req)
    else:
        _error(req, -32601, "Method not found", "Unsupported")


def main():
    while True:
        req = _read_message()
        if req is None:
            break
        _route(req)
    sys.exit(0)


if __name__ == "__main__":
    main()