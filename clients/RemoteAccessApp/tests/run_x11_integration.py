#!/usr/bin/env python3
"""Run native input tests on a temporary X server, never the user's display."""
import argparse
import os
import select
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("test_binary")
parser.add_argument("--xvfb", default="Xvfb")
parser.add_argument("--scale", choices=["1", "2"], default="1")
args = parser.parse_args()
server = subprocess.Popen(
    [args.xvfb, "-displayfd", "1", "-screen", "0", "1280x800x24", "-nolisten", "tcp", "-ac"],
    stdout=subprocess.PIPE, text=True,
)
try:
    if not select.select([server.stdout], [], [], 10)[0]:
        raise RuntimeError("Temporary X server did not start")
    display = server.stdout.readline().strip()
    if not display.isdigit():
        raise RuntimeError("Temporary X server returned no display number")
    env = os.environ.copy()
    env.update(DISPLAY=":" + display, QT_QPA_PLATFORM="xcb", PBL4_ISOLATED_X11="1",
               QT_SCALE_FACTOR=args.scale)
    result = subprocess.run([args.test_binary, "-nocrashhandler"], env=env, timeout=40)
    raise SystemExit(result.returncode)
finally:
    server.terminate()
    try:
        server.wait(timeout=5)
    except subprocess.TimeoutExpired:
        server.kill()
        server.wait()
