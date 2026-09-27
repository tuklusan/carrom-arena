#!/usr/bin/env python3
"""Carrom Arena build driver. CI runs exactly this; run it by hand for a manual build.

    python build/build.py                     configure, build and test (Debug) into out/
    python build/build.py --build-type Release --no-test
    python build/build.py --build-type Debug,Release      (both, into out/Debug and out/Release)
    python build/build.py --out /tmp/carrom-out --no-tools    (use the system cmake and ninja)
    python build/build.py fetch              only pre-fetch the pinned dependencies into deps/
    python build/build.py admit              CI only: take a queue ticket per runner kind (see below)
    python build/build.py release --kind K   CI only: give the ticket for runner kind K back

What it does, in order:
  1. tools    : CMake and Ninja at the versions in build/tools.txt, installed with pip into <out>/.tools
                (PyPI has wheels for every runner architecture); falls back to the ones on PATH if that fails.
  2. packages : on Linux, the X11/OpenGL development packages and xvfb, only if they are missing.
  3. deps     : the exact commits in build/pins.txt, depth-1, into deps/ (CMake uses deps/<name> when present).
  4. build    : cmake configure + build (Ninja), the compiler found for this platform.
  5. test     : ctest (under xvfb-run on a Linux machine without a display).
  6. dist     : the game executable copied to <out>/dist/ with the build id and build type in its name.

Queue gate (`admit`/`release`, used by .github/workflows/ci.yml): at most one job per runner kind may run and at most one may wait;
a request that would be the second waiting one is rejected, and the run fails. The tickets are git refs created atomically, so
simultaneous requests cannot both get the last one. RUNNERS is the single list of runner kinds:
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
import urllib.error
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
    root_out = Path(args.out).resolve()
    root_out.mkdir(parents=True, exist_ok=True)
    os.chdir(ROOT)
    log(f"platform: {platform.system()} {platform.machine()}, python {platform.python_version()}, out={root_out}")
    ensure_tools(root_out, not args.no_tools)
    ensure_linux_packages()
    fetch_deps(ROOT / "deps")

    configs = [c.strip() for c in args.build_type.split(",") if c.strip()]
    for cfg in configs:
        if cfg not in ("Debug", "Release", "RelWithDebInfo"):
            sys.exit(f"unknown build type {cfg}")
    dist = root_out / "dist"
    dist.mkdir(exist_ok=True)
    for cfg in configs:
        out = root_out if len(configs) == 1 else root_out / cfg
        log(f"===== {cfg} -> {out}")
        run(["cmake", "-S", ROOT, "-B", out, "-G", "Ninja", f"-DCMAKE_BUILD_TYPE={cfg}", *compiler_args(args.cc)])
        run(["cmake", "--build", out, "--parallel", str(os.cpu_count() or 2)])
        if not args.no_test:
            ctest = ["ctest", "--test-dir", out, "--output-on-failure", "-j", str(os.cpu_count() or 2)]
            if sys.platform == "linux" and not os.environ.get("DISPLAY") and shutil.which("xvfb-run"):
                ctest = ["xvfb-run", "-a", *ctest]
            run(ctest)
        src = out / exe("carrom_arena")
        if src.exists():
            arch = platform.machine().lower().replace("amd64", "x64").replace("x86_64", "x64").replace("aarch64", "arm64")
            name = f"carrom_arena-{sys.platform.replace('win32', 'windows')}-{arch}-{build_id()}-{cfg.lower()}" + (".exe" if os.name == "nt" else "")
            shutil.copy2(src, dist / name)
            log(f"dist: {dist / name}")
    log("OK")


# ---------------------------------------------------------------- queue gate
# Per runner kind there are exactly 1 + MAX_WAITING tickets (git refs refs/ci-lock/<kind>/<n>). A run takes a ticket by CREATING
# its ref, which GitHub does atomically: only one creator can win, so two runs arriving in the same instant can never both get the
# last ticket. No ticket means the queue for that kind is full and the run is rejected. GitHub's concurrency group (ci.yml) makes the
# ticket holders run one at a time; the ticket is given back at the end of the build job. A ticket whose run has already finished
# (a killed job never released it) is stale and is taken over by the next request.
LOCK_NS = "ci-lock"


def gh_call(method, path, body=None):
    """(status, json) for a GitHub REST call; HTTP errors come back as their status, not as exceptions."""
    req = urllib.request.Request(
        "https://api.github.com/" + path, method=method,
        data=None if body is None else json.dumps(body).encode(),
        headers={"Authorization": "Bearer " + os.environ["GH_TOKEN"], "Accept": "application/vnd.github+json",
                 "X-GitHub-Api-Version": "2022-11-28", "Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=30) as r:
            raw = r.read()
            return r.status, (json.loads(raw) if raw else None)
    except urllib.error.HTTPError as e:
        raw = e.read()
        try:
            return e.code, json.loads(raw) if raw else None
        except ValueError:
            return e.code, None


def ticket_ref(kind, slot):
    return f"{LOCK_NS}/{kind}/{slot}"


def ticket_holder(repo, kind, slot):
    """Run id holding the ticket, or None when the ticket is free."""
    st, ref = gh_call("GET", f"repos/{repo}/git/ref/{ticket_ref(kind, slot)}")
    if st != 200:
        return None
    st, commit = gh_call("GET", f"repos/{repo}/git/commits/{ref['object']['sha']}")
    m = re.match(r"run=(\d+)", (commit or {}).get("message", "")) if st == 200 else None
    return int(m.group(1)) if m else 0            # 0: a ticket nobody can identify counts as stale


def run_finished(repo, run_id):
    st, run = gh_call("GET", f"repos/{repo}/actions/runs/{run_id}")
    return st == 404 or (st == 200 and run.get("status") == "completed")


def make_ticket_commit(repo):
    """The object every ticket ref points at; its message names this run."""
    sha = os.environ["GITHUB_SHA"]
    st, c = gh_call("GET", f"repos/{repo}/git/commits/{sha}")
    if st != 200:                                  # a pull-request merge commit may not be readable: use the default branch
        st, c = gh_call("GET", f"repos/{repo}/commits/HEAD")
        c = (c or {}).get("commit")
    st, made = gh_call("POST", f"repos/{repo}/git/commits", {
        "message": f"run={os.environ['GITHUB_RUN_ID']} attempt={os.environ.get('GITHUB_RUN_ATTEMPT', '1')}",
        "tree": c["tree"]["sha"], "parents": []})
    if st != 201:
        sys.exit(f"cannot create the ticket object: HTTP {st} {made}")
    return made["sha"]


def take_ticket(repo, kind, sha):
    """Slot number of the ticket taken, or None when every ticket of this kind is held by a live run."""
    for attempt in (1, 2):
        for slot in range(1, MAX_WAITING + 2):
            st, _ = gh_call("POST", f"repos/{repo}/git/refs", {"ref": "refs/" + ticket_ref(kind, slot), "sha": sha})
            if st == 201:
                return slot
            if st not in (422, 409):
                sys.exit(f"ticket request for {kind} failed: HTTP {st}")
        if attempt == 2:
            break
        freed = False                              # everything taken: free the tickets whose run is over, then try once more
        for slot in range(1, MAX_WAITING + 2):
            holder = ticket_holder(repo, kind, slot)
            if holder is not None and (holder == 0 or run_finished(repo, holder)):
                log(f"{kind}: ticket {slot} belonged to finished run {holder}, taking it over")
                gh_call("DELETE", f"repos/{repo}/git/refs/{ticket_ref(kind, slot)}")
                freed = True
        if not freed:
            break
    return None


def do_admit(_args):
    """Take a ticket for every runner kind; write `runners` (the kinds admitted, JSON) and `rejected` to $GITHUB_OUTPUT."""
    repo = os.environ["GITHUB_REPOSITORY"]
    sha = make_ticket_commit(repo)
    admitted, rejected = [], []
    for kind in RUNNERS:
        slot = take_ticket(repo, kind, sha)
        (admitted if slot else rejected).append(kind)
        log(f"{kind}: " + (f"admitted (ticket {slot})" if slot else "REJECTED, the queue is full (one running, one waiting)"))
    with open(os.environ.get("GITHUB_OUTPUT", os.devnull), "a") as f:
        f.write(f"runners={json.dumps(admitted)}\nrejected={' '.join(rejected)}\n")


def do_release(args):
    """Give back this run's ticket for --kind (the last step of the build job, whatever the outcome)."""
    repo, me = os.environ["GITHUB_REPOSITORY"], int(os.environ["GITHUB_RUN_ID"])
    for slot in range(1, MAX_WAITING + 2):
        if ticket_holder(repo, args.kind, slot) == me:
            st, _ = gh_call("DELETE", f"repos/{repo}/git/refs/{ticket_ref(args.kind, slot)}")
            log(f"{args.kind}: ticket {slot} released (HTTP {st})")


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", nargs="?", default="all", choices=["all", "fetch", "admit", "release"])
    ap.add_argument("--build-type", default="Debug", help="Debug (default), Release, RelWithDebInfo, or a comma list such as Debug,Release (each goes to <out>/<type>)")
    ap.add_argument("--out", default=str(ROOT / "out"), help="build directory (default: out/)")
    ap.add_argument("--no-test", action="store_true", help="skip ctest")
    ap.add_argument("--no-tools", action="store_true", help="use the cmake and ninja already on PATH")
    ap.add_argument("--cc", help="C compiler to pass to CMake (default: platform default, gcc/clang on Windows)")
    ap.add_argument("--kind", help="release: the runner kind whose ticket to give back")
    args = ap.parse_args()
    if args.command == "fetch":
        fetch_deps(ROOT / "deps")
    elif args.command == "admit":
        do_admit(args)
    elif args.command == "release":
        do_release(args)
    else:
        do_build(args)


if __name__ == "__main__":
    main()
