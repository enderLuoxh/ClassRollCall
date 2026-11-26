"""Build ClassRollCall with zig cc (no MSVC or Windows SDK required).

Usage:
    python scripts/build_zig.py [--config release|debug] [--clean]

The script compiles every translation unit with `zig c++`/`zig cc`
(-target x86-windows-gnu), links the Win32 GUI executable and embeds the
manifest/version resources with a pure-python resource writer. The resulting
EXE is native and uses the system UCRT supplied with Windows 10/11; it does
not require .NET, Python, Qt, or a separate VC++ redistributable installation.
"""
import argparse
import concurrent.futures
import hashlib
import os
import shutil
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZIG = os.environ.get("ZIG", shutil.which("zig") or "zig")
VENDOR = os.environ.get("CRC_VENDOR", os.path.join(ROOT, "third_party", "vendor"))
TARGET = os.environ.get("CRC_TARGET", "x86-windows-gnu")

APP_SOURCES = [
    "src/main.cpp",
    "src/ui/application.cpp",
    "src/ui/paint.cpp",
    "src/ui/preview.cpp",
    "src/ui/skin.cpp",
    "src/ui/dialogs.cpp",
    "src/ui/dialog_window.cpp",
    "src/ui/theme.cpp",
    "src/ui/menu.cpp",
]
CORE_SOURCES = [
    "src/edition.cpp",
    "src/core/random.cpp",
    "src/platform/platform.cpp",
    "src/storage/store.cpp",
]
IMPORT_SOURCES = [
    "src/importer/importer.cpp",
    "src/importer/text_csv.cpp",
    "src/importer/office.cpp",
    "src/importer/xls.cpp",
    "src/importer/xlsx.cpp",
    "src/importer/docx.cpp",
    "src/importer/xls_locale.c",
]
THIRD_PARTY_SOURCES = [
    "miniz/miniz.c",
    "miniz/miniz_tdef.c",
    "miniz/miniz_tinfl.c",
    "miniz/miniz_zip.c",
    "pugixml/src/pugixml.cpp",
    "libxls/src/xls.c",
    "libxls/src/ole.c",
    "libxls/src/xlstool.c",
    "libxls/src/endian.c",
]

DEFINES = [
    "UNICODE", "_UNICODE", "NOMINMAX", "WIN32_LEAN_AND_MEAN",
    "_WIN32_WINNT=0x0601", "WIN32_IE=0x0800", "_CRT_SECURE_NO_WARNINGS",
]
LIBS = ["gdiplus", "comctl32", "comdlg32", "uxtheme", "uuid", "advapi32",
        "shell32", "user32", "gdi32", "ole32", "oleaut32", "shlwapi", "imm32"]

MINIZ_EXPORT_HEADER = """#ifndef MINIZ_EXPORT_H
#define MINIZ_EXPORT_H
#define MINIZ_EXPORT
#define MINIZ_NO_EXPORT
#endif
"""


def run(cmd, **kwargs):
    kwargs.setdefault("encoding", "utf-8")
    kwargs.setdefault("errors", "replace")
    return subprocess.run(cmd, capture_output=True, text=True, **kwargs)


def zig_env():
    """Allow an out-of-tree zig cache (useful on locked-down or sandboxed hosts)."""
    env = dict(os.environ)
    cache = env.get("ZIG_CACHE") or env.get("CRC_ZIG_CACHE")
    if cache:
        env["ZIG_GLOBAL_CACHE_DIR"] = os.path.join(cache, "global")
        env["ZIG_LOCAL_CACHE_DIR"] = os.path.join(cache, "local")
    return env


def zig():
    if not os.path.exists(ZIG) and not shutil.which(ZIG):
        sys.exit("zig not found; set ZIG to the zig executable")
    return ZIG


def gen_dir(build_dir):
    return os.path.join(build_dir, "gen")


def include_flags(build_dir):
    return [
        "-I" + os.path.join(ROOT, "src"),
        "-I" + os.path.join(ROOT, "src", "resources"),
        "-I" + os.path.join(gen_dir(build_dir), "xls_config"),
        "-I" + os.path.join(gen_dir(build_dir), "miniz"),
        "-I" + os.path.join(VENDOR, "miniz"),
        "-I" + os.path.join(VENDOR, "libxls", "include"),
        "-I" + os.path.join(VENDOR, "pugixml", "src"),
    ]


def common_flags(build_dir, config):
    flags = ["-target", TARGET, "-U_WIN32_WINNT", *["-D" + d for d in DEFINES], *include_flags(build_dir)]
    if config == "release":
        flags += ["-O2", "-DNDEBUG", "-fno-strict-aliasing"]
    else:
        flags += ["-O0", "-g", "-DDEBUG"]
    return flags


def object_path(src, build_dir):
    digest = hashlib.sha1(src.encode()).hexdigest()[:8]
    stem = os.path.splitext(os.path.basename(src))[0]
    return os.path.join(build_dir, "obj", "%s-%s.o" % (stem, digest))


def compile_one(src, build_dir, config, log_dir):
    is_cpp = src.endswith((".cpp", ".cxx"))
    cmd = [zig(), "c++" if is_cpp else "cc"] + common_flags(build_dir, config)
    cmd += ["-std=c++17"] if is_cpp else ["-std=c11"]
    if "third_party" in src:
        cmd += ["-w"]
    out = object_path(src, build_dir)
    cmd += ["-c", src, "-o", out]
    started = time.time()
    result = run(cmd, cwd=ROOT, env=zig_env())
    if result.returncode != 0:
        log = os.path.join(log_dir, os.path.basename(out) + ".log")
        with open(log, "w", encoding="utf-8") as handle:
            handle.write(result.stderr + result.stdout)
        return src, False, log
    return src, True, time.time() - started


def prepare(build_dir):
    gen = gen_dir(build_dir)
    xls = os.path.join(gen, "xls_config")
    os.makedirs(xls, exist_ok=True)
    shutil.copyfile(os.path.join(ROOT, "third_party", "config.h.in"), os.path.join(xls, "config.h"))
    miniz = os.path.join(gen, "miniz")
    os.makedirs(miniz, exist_ok=True)
    with open(os.path.join(miniz, "miniz_export.h"), "w", encoding="ascii") as handle:
        handle.write(MINIZ_EXPORT_HEADER)
    os.makedirs(os.path.join(build_dir, "logs"), exist_ok=True)
    res = os.path.join(build_dir, "app_res.obj")
    if os.environ.get("CRC_NO_RES"):
        return None
    icon = os.path.join(build_dir, "app.ico")
    subprocess.run([sys.executable, os.path.join(ROOT, "scripts", "make_icon.py"), icon],
                   check=True, capture_output=True, text=True, encoding="utf-8", errors="replace")
    subprocess.run([sys.executable, os.path.join(ROOT, "scripts", "makeres.py"), res,
                    "--manifest", os.path.join(ROOT, "src", "resources", "app.manifest"),
                    "--icon", icon],
                   check=True, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return res


def collect_sources():
    sources = [os.path.join(ROOT, s) for s in APP_SOURCES + CORE_SOURCES + IMPORT_SOURCES]
    for rel in THIRD_PARTY_SOURCES:
        path = os.path.join(VENDOR, rel)
        if not os.path.exists(path):
            sys.exit("missing vendored source: " + path)
        sources.append(path)
    return sources


def compile_all(sources, build_dir, config, jobs):
    failures = []
    objs = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(compile_one, s, build_dir, config, os.path.join(build_dir, "logs")) for s in sources]
        for future in concurrent.futures.as_completed(futures):
            src, ok, info = future.result()
            if ok:
                objs.append(object_path(src, build_dir))
                print("  ok   %-40s %5.1fs" % (os.path.basename(src), info))
            else:
                failures.append((src, info))
                print("  FAIL %s" % os.path.basename(src))
    if failures:
        for src, log in failures[:6]:
            print("\n=== %s ===" % src)
            print(open(log, encoding="utf-8", errors="replace").read()[:3000])
    return objs, failures


def source_object(rel, build_dir):
    return object_path(os.path.join(ROOT, rel), build_dir)


def build_tests(build_dir, config, jobs):
    """Build the console test binaries from the same objects as the app."""
    prepare(build_dir)
    suites = {
        "core": (["tests/core_tests.cpp"], CORE_SOURCES, []),
        "import": (["tests/import_tests.cpp"], CORE_SOURCES + IMPORT_SOURCES, THIRD_PARTY_SOURCES),
    }
    failures = []
    for name, (mains, shared, extra) in suites.items():
        wanted = [os.path.join(ROOT, s) for s in mains] + [os.path.join(ROOT, s) for s in shared]
        wanted += [os.path.join(VENDOR, s) for s in extra]
        objs, bad = compile_all(wanted, build_dir, config, jobs)
        failures += bad
        if bad:
            continue
        deps = objs
        exe = os.path.join(ROOT, "dist", "tests", "%s_tests.exe" % name)
        os.makedirs(os.path.dirname(exe), exist_ok=True)
        entry = ["-municode"] if name == "import" else []
        link = [zig(), "c++"] + common_flags(build_dir, config) + entry + [
            "-Wl,--subsystem,console", "-o", exe, *deps,
            *["-l" + lib for lib in LIBS],
        ]
        result = run(link, cwd=ROOT, env=zig_env())
        if result.returncode != 0:
            print("=== link %s_tests ===\n%s" % (name, result.stderr[-4000:]))
            failures.append((exe, ""))
        else:
            print("built %s" % exe)
    return 1 if failures else 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", choices=["release", "debug"], default="release")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument("--tests", action="store_true", help="build the console test binaries instead of the app")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    parser.add_argument("--edition", help="private local JSON roster; never commit real student names")
    parser.add_argument("--output", help="output EXE path (default: dist/ClassRollCall.exe)")
    args = parser.parse_args()

    build_dir = os.path.join(ROOT, "build", "zig")
    if args.clean and os.path.isdir(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(os.path.join(build_dir, "obj"), exist_ok=True)
    log_dir = os.path.join(build_dir, "logs")
    os.makedirs(log_dir, exist_ok=True)
    os.makedirs(os.path.join(ROOT, "dist"), exist_ok=True)

    if args.tests:
        return build_tests(build_dir, args.config, args.jobs)

    res = prepare(build_dir)
    sources = collect_sources()
    if args.edition:
        from make_edition import generate
        generated = os.path.join(build_dir, "private-edition.cpp")
        generate(args.edition, generated)
        sources = [generated if s == os.path.join(ROOT, "src/edition.cpp") else s for s in sources]
    print("compiling %d files for %s" % (len(sources), TARGET))

    objs, failures = compile_all(sources, build_dir, args.config, args.jobs)
    if failures:
        return 1

    exe = os.path.abspath(args.output) if args.output else os.path.join(ROOT, "dist", "ClassRollCall.exe")
    os.makedirs(os.path.dirname(exe), exist_ok=True)
    link = [zig(), "c++"] + common_flags(build_dir, args.config) + [
        "-municode", "-Wl,--subsystem,windows", "-o", exe, *objs, *([res] if res else []),
        *["-l" + lib for lib in LIBS],
    ]
    print("linking %s" % exe)
    result = run(link, cwd=ROOT, env=zig_env())
    if result.returncode != 0:
        print(result.stderr[-8000:])
        return 1
    print("built %s (%d bytes)" % (exe, os.path.getsize(exe)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())



