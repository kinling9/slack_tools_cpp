#!/usr/bin/env python3
from build import run_cmd, build, clean
import os

if __name__ == "__main__":
    files = os.listdir(os.getcwd())
    if "LICENSE" not in files:
        print("script should run in the root directory of the project")
        sys.exit(1)

    if "build" not in files:
        os.mkdir("build")

    build()
    run_cmd("./build/parser ./rpt/B005_allpath.rpt.gz")
