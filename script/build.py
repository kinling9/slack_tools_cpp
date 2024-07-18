#!/usr/bin/env python3
import os
import sys
import argparse


def run_cmd(cmd: str):
    print(f"Running: {cmd}")
    os.system(cmd)

def build(debug: bool = False):
    if debug:
        run_cmd("cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Debug")
        run_cmd("cmake --build build --config Debug")
    else:
        run_cmd("cmake -S . -B build -GNinja")
        run_cmd("cmake --build build --config Release")

def clean():
    run_cmd("cmake --build build --target clean")


if __name__ == "__main__":

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("--clean", action="store_true")
    arg_parser.add_argument("--debug", action="store_true")
    args = arg_parser.parse_args()

    files = os.listdir(os.getcwd())
    if "LICENSE" not in files:
        print("script should run in the root directory of the project")
        sys.exit(1)

    if "build" not in files:
        os.mkdir("build")

    if args.clean:
        clean()
    else:
        build(args.debug)
