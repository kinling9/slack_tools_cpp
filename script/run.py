#!/usr/bin/env python3
from build import run_cmd, build
import os
import sys
import argparse

if __name__ == "__main__":

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("--debug", action="store_true")

    args = arg_parser.parse_args()
    files = os.listdir(os.getcwd())
    if "LICENSE" not in files:
        print("script should run in the root directory of the project")
        sys.exit(1)

    if "build" not in files:
        os.mkdir("build")

    build(args.debug)
    if not args.debug:
        run_cmd("./build/src/parser ./rpt/B005_allpath.rpt.gz")
    else:
        run_cmd("gdb ./build/src/parser")
