#!/usr/bin/env python3
"""JSON-RPC 2.0 provider for vcpkg over stdio (Content-Length framing).

The host runs this script as an external provider. It answers the IProvider
read operations without ever executing vcpkg itself, so a foreign project
cannot run code: installations are read from the filesystem.

Classic mode: the environment is the vcpkg root (VCPKG_ROOT or the directory
containing this script), and packages live in <root>/installed/<triplet>.
Manifest mode: a project with vcpkg.json is an environment; packages are
located under vcpkg_installed/.

Only stderr may carry diagnostics; stdout is reserved for the protocol.
"""

import json
import os
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


def detect():
    root = _vcpkg_root()
    if not root:
        # No VCPKG_ROOT: no tool instance to report.
        return {"tools": []}
    return {"tools": [{"path": os.path.join(root, "vcpkg.exe"),
                       "version": ""}]}


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
    # vcpkg.json. Packages live in <root>/vcpkg_installed.
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
        return {"installations": [], "complete": True}

    # Both modes keep packages under subdirectories named <name>/info or as
    # <name> directories; the simplest reliable signal is a CONTROL/METADATA
    # file that carries "Package: <name>" and "Version: <version>".
    installs = []
    with os.scandir(root) as it:
        for entry in it:
            if not entry.is_dir():
                continue
            for meta_name in ("CONTROL", "METADATA"):
                meta_path = os.path.join(entry.path, meta_name)
                if os.path.isfile(meta_path):
                    package, version = _parse_control(meta_path)
                    if package:
                        installs.append({
                            "name": package,
                            "version": version,
                            "location": entry.path,
                        })
                    break
    return {"installations": installs, "complete": True}


def _parse_control(path):
    name = version = ""
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for line in fh:
                if line.startswith("Package:") and not name:
                    name = line.split(":", 1)[1].strip()
                elif line.startswith("Version:") and not version:
                    version = line.split(":", 1)[1].strip()
    except OSError:
        pass
    return name, version


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