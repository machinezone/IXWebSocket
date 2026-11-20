#!/usr/bin/env python3
"""Build and exercise installed static packages with and without compression (Unix)."""
import os
from pathlib import Path
import shlex
import subprocess


SOURCE = Path(__file__).resolve().parent.parent
ROOT = SOURCE / "build" / "installed-package"
CONSUMER = SOURCE / "test" / "cmake" / "installed-package"


def run(*args, env=None, capture=False):
    print("+ " + shlex.join(str(arg) for arg in args), flush=True)
    result = subprocess.run([str(arg) for arg in args], check=True, env=env,
                            stdout=subprocess.PIPE if capture else None, text=True)
    return result.stdout.strip() if capture else None


for enabled in ("ON", "OFF"):
    work = ROOT / enabled.lower()
    build, prefix, consumer = work / "library", work / "prefix", work / "consumer"
    run("cmake", "-S", SOURCE, "-B", build, "-DUSE_TLS=OFF", "-DUSE_ZLIB=" + enabled,
        "-DUSE_TEST=OFF", "-DUSE_WS=OFF", "-DBUILD_SHARED_LIBS=OFF",
        "-DCMAKE_UNITY_BUILD=ON", "-DCMAKE_INSTALL_LIBDIR=lib",
        "-DCMAKE_INSTALL_PREFIX=" + str(prefix),
        "-DCMAKE_CXX_FLAGS=" + os.environ.get("CXXFLAGS", ""))
    run("cmake", "--build", build, "--parallel", "4")
    run("cmake", "--install", build)
    run("cmake", "-S", CONSUMER, "-B", consumer, "-DCMAKE_PREFIX_PATH=" + str(prefix),
        "-DCMAKE_CXX_FLAGS=" + os.environ.get("CXXFLAGS", ""))
    run("cmake", "--build", consumer, "--parallel", "4")
    expected = run(consumer / "consumer", capture=True)

    env = dict(os.environ, PKG_CONFIG_PATH=str(prefix / "lib/pkgconfig"))
    cflags = shlex.split(run("pkg-config", "--cflags", "ixwebsocket", env=env, capture=True))
    libs = shlex.split(run("pkg-config", "--libs", "--static", "ixwebsocket", env=env,
                           capture=True))
    requires = run("pkg-config", "--print-requires-private", "ixwebsocket", env=env,
                   capture=True)
    assert ("zlib" in requires.splitlines()) == (enabled == "ON"), requires
    assert ("-lz" in libs) == (enabled == "ON"), libs
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    flags = shlex.split(os.environ.get("CXXFLAGS", ""))
    for mode in ("pkg-config", "direct"):
        exe = work / mode
        # Neither path receives the CMake target's compression definition.
        includes = cflags if mode == "pkg-config" else ["-I" + str(prefix / "include")]
        if mode == "direct" and enabled == "OFF":
            # Even a stale consumer definition must not select another codec layout.
            includes += ["-DIXWEBSOCKET_USE_ZLIB"]
        run(*compiler, "-std=c++11", *flags, *includes, CONSUMER / "main.cpp", *libs,
            "-o", exe)
        assert run(exe, capture=True) == expected, "Consumer codec layouts differ"
    print("Installed " + enabled + " package passed all three consumer modes.", flush=True)
