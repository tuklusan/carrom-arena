#!/usr/bin/env python3
"""Carrom Arena build driver. CI runs exactly this; run it by hand for a manual build.

    python build/build.py                     configure, build and test (Debug) into out/
    python build/build.py --build-type Release --no-test
    python build/build.py --out /tmp/carrom-out --no-tools    (use the system cmake and ninja)
    python build/build.py fetch              only pre-fetch the pinned dependencies into deps/
    python build/build.py admit              CI only: the per-runner queue gate (see below)

What it does, in order:
  1. tools    : CMake and Ninja at the versions in build/tools.txt, installed with pip into <out>/.tools
                (PyPI has wheels for every runner architecture); falls back to the ones on PATH if that fails.
  2. packages : on Linux, the X11/OpenGL development packages and xvfb, only if they are missing.
  3. deps     : the exact commits in build/pins.txt, depth-1, into deps/ (CMake uses deps/<name> when present).
  4. build    : cmake configure + build (Ninja), the compiler found for this platform.
  5. test     : ctest (under xvfb-run on a Linux machine without a display).
  6. dist     : the game executable copied to <out>/dist/ with the build id in its name.

Queue gate (`admit`, used by .github/workflows/ci.yml): at most one job per runner kind may run and at most one may wait;
a request that would be the second waiting one is rejected, and the run fails. RUNNERS is the single list of runner kinds:
the smallest set that covers every hosted architecture (Linux x64/arm64, Windows x64/arm64, macOS arm64/Intel).
"""
import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build"

RUNNERS = [
    "ubuntu-24.04",       # Linux x64
    "ubuntu-24.04-arm",   # Linux arm64
    "windows-2022",       # Windows x64
    "windows-11-arm",     # Windows arm64
    "macos-15",           # macOS arm64
    "macos-15-intel",     # macOS x64
]
MAX_WAITING = 1           # per runner kind: one running plus this many queued

LINUX_PACKAGES = ["libx11-dev", "libxrandr-dev", "libxinerama-dev", "libxcursor-dev", "libxi-dev",
                  "libgl1-mesa-dev", "xvfb"]


def log(msg):
    print(f"[build] {msg}", flush=True)


def run(cmd, **kw):
    log("$ " + " ".join(str(c) for c in cmd))
    subprocess.run([str(c) for c in cmd], check=True, **kw)


def exe(name):
    return name + (".exe" if os.name == "nt" else "")


# ---------------------------------------------------------------- pins
def read_tools():
    out = {}
    for line in (BUILD / "tools.txt").read_text().splitlines():
        if "=" in line and not line.startswith("#"):
            k, v = line.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def read_pins():
    pins = []
    for line in (BUILD / "pins.txt").read_text().splitlines():
        if line.strip() and not line.startswith("#"):
            name, url, sha = line.split()[:3]
            pins.append((name, url, sha))
    return pins


def git(*args, cwd=None):
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def fetch_deps(dest):
    dest.mkdir(parents=True, exist_ok=True)
    for name, url, sha in read_pins():
        d = dest / name
        try:
            if (d / ".git").exists() and git("rev-parse", "HEAD", cwd=d) == sha:
                log(f"deps: {name} already at {sha[:9]}")
                continue
        except subprocess.CalledProcessError:
            pass
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
        git("init", "-q", cwd=d)
        git("remote", "add", "origin", url, cwd=d)
        git("fetch", "-q", "--depth", "1", "origin", sha, cwd=d)
        git("checkout", "-q", "--detach", "FETCH_HEAD", cwd=d)
        if git("rev-parse", "HEAD", cwd=d) != sha:
            sys.exit(f"deps: {name}: the fetched commit does not match the pin")
        log(f"deps: {name} fetched {sha[:9]}")


# ---------------------------------------------------------------- tools
def ensure_tools(out, use_pinned):
    """Put pinned CMake and Ninja first on PATH (pip venv in <out>/.tools); fall back to the system ones."""
    if use_pinned:
        tools = read_tools()
        venv = out / ".tools"
        bindir = venv / ("Scripts" if os.name == "nt" else "bin")
        if not (bindir / exe("cmake")).exists() or not (bindir / exe("ninja")).exists():
            try:
                run([sys.executable, "-m", "venv", venv])
                run([bindir / exe("python"), "-m", "pip", "install", "--quiet", "--disable-pip-version-check",
                     f"cmake=={tools['cmake']}", f"ninja=={tools['ninja']}"])
            except (subprocess.CalledProcessError, OSError) as e:
                log(f"WARNING: pinned tools could not be installed ({e}); using the system cmake and ninja")
                bindir = None
        if bindir is not None:
            os.environ["PATH"] = str(bindir) + os.pathsep + os.environ["PATH"]
    for tool in ("cmake", "ninja"):
        if not shutil.which(tool):
            sys.exit(f"{tool} not found (install it, or drop --no-tools)")
    log(subprocess.run(["cmake", "--version"], capture_output=True, text=True).stdout.splitlines()[0]
        + ", ninja " + subprocess.run(["ninja", "--version"], capture_output=True, text=True).stdout.strip())


def ensure_linux_packages():
    if sys.platform != "linux" or not shutil.which("dpkg"):
        return
    missing = [p for p in LINUX_PACKAGES
               if subprocess.run(["dpkg", "-s", p], capture_output=True).returncode != 0]
    if not missing:
        return
    sudo = [] if os.geteuid() == 0 else ["sudo", "-n"]
    log("installing missing packages: " + " ".join(missing))
    try:
        try:
            run(sudo + ["apt-get", "install", "-y", "--no-install-recommends", *missing])
        except subprocess.CalledProcessError:
            run(sudo + ["apt-get", "update"])
            run(sudo + ["apt-get", "install", "-y", "--no-install-recommends", *missing])
    except subprocess.CalledProcessError:
        log("WARNING: could not install the packages above (no sudo?); continuing, the build will say if something is really missing")


def compiler_args(cc):
    """CMake arguments selecting a C compiler where the default is not right."""
    if cc:
        return [f"-DCMAKE_C_COMPILER={cc}"]
    if os.name == "nt":
        for candidate in ("gcc", "clang"):       # MinGW-w64 first, LLVM as the fallback
            if shutil.which(candidate):
                return [f"-DCMAKE_C_COMPILER={candidate}"]
        sys.exit("no gcc or clang on PATH: install MinGW-w64 or LLVM")
    return []


# ---------------------------------------------------------------- build
def build_id():
    try:
        return git("describe", "--tags", "--always", "--dirty", cwd=ROOT)
    except (subprocess.CalledProcessError, OSError):
        return "unknown"


def do_build(args):
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    os.chdir(ROOT)
    log(f"platform: {platform.system()} {platform.machine()}, python {platform.python_version()}, out={out}")
    ensure_tools(out, not args.no_tools)
    ensure_linux_packages()
    fetch_deps(ROOT / "deps")

    cfg = ["cmake", "-S", ROOT, "-B", out, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={args.build_type}",
           *compiler_args(args.cc)]
    run(cfg)
    run(["cmake", "--build", out, "--parallel", str(os.cpu_count() or 2)])

    if not args.no_test:
        ctest = ["ctest", "--test-dir", out, "--output-on-failure", "-j", str(os.cpu_count() or 2)]
        if sys.platform == "linux" and not os.environ.get("DISPLAY") and shutil.which("xvfb-run"):
            ctest = ["xvfb-run", "-a", *ctest]
        run(ctest)

    dist = out / "dist"
    dist.mkdir(exist_ok=True)
    src = out / exe("carrom_arena")
    if src.exists():
        arch = platform.machine().lower().replace("amd64", "x64").replace("x86_64", "x64").replace("aarch64", "arm64")
        name = f"carrom_arena-{sys.platform.replace('win32', 'windows')}-{arch}-{build_id()}" + (".exe" if os.name == "nt" else "")
        shutil.copy2(src, dist / name)
        log(f"dist: {dist / name}")
    log("OK")


# ---------------------------------------------------------------- queue gate
def gh_api(path):
    req = urllib.request.Request("https://api.github.com/" + path, headers={
        "Authorization": "Bearer " + os.environ["GH_TOKEN"],
        "Accept": "application/vnd.github+json",
        "X-GitHub-Api-Version": "2022-11-28"})
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.load(r)


def do_admit(_args):
    """Decide which runner kinds this run may queue for. Writes `runners` (JSON list) and `rejected` to $GITHUB_OUTPUT."""
    repo, me = os.environ["GITHUB_REPOSITORY"], int(os.environ["GITHUB_RUN_ID"])
    wf = gh_api(f"repos/{repo}/actions/runs/{me}")["workflow_id"]
    busy = {k: 0 for k in RUNNERS}                      # unfinished jobs per kind in OTHER runs
    seen = set()
    for status in ("in_progress", "queued", "waiting", "pending", "requested"):
        for r in gh_api(f"repos/{repo}/actions/workflows/{wf}/runs?status={status}&per_page=100")["workflow_runs"]:
            if r["id"] == me or r["id"] in seen:
                continue
            seen.add(r["id"])
            for j in gh_api(f"repos/{repo}/actions/runs/{r['id']}/jobs?per_page=100")["jobs"]:
                m = re.fullmatch(r"build \((.+)\)", j["name"])
                if m and m.group(1) in busy and j["status"] != "completed":
                    busy[m.group(1)] += 1
    admitted = [k for k in RUNNERS if busy[k] < 1 + MAX_WAITING]
    rejected = [k for k in RUNNERS if k not in admitted]
    for k in RUNNERS:
        log(f"{k}: {busy[k]} unfinished job(s) ahead -> " + ("admitted" if k in admitted else "REJECTED, queue full"))
    with open(os.environ.get("GITHUB_OUTPUT", os.devnull), "a") as f:
        f.write(f"runners={json.dumps(admitted)}\nrejected={' '.join(rejected)}\n")


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", nargs="?", default="all", choices=["all", "fetch", "admit"])
    ap.add_argument("--build-type", default="Debug", choices=["Debug", "Release", "RelWithDebInfo"])
    ap.add_argument("--out", default=str(ROOT / "out"), help="build directory (default: out/)")
    ap.add_argument("--no-test", action="store_true", help="skip ctest")
    ap.add_argument("--no-tools", action="store_true", help="use the cmake and ninja already on PATH")
    ap.add_argument("--cc", help="C compiler to pass to CMake (default: platform default, gcc/clang on Windows)")
    args = ap.parse_args()
    if args.command == "fetch":
        fetch_deps(ROOT / "deps")
    elif args.command == "admit":
        do_admit(args)
    else:
        do_build(args)


if __name__ == "__main__":
    main()
