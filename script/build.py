#!/usr/bin/env python3
import os
import sys
import argparse


def run_cmd(cmd: str):
    print(f"Running: {cmd}")
    os.system(cmd)

def build(debug: bool = False, docker: bool = False):
    dir_build =  "build" + ("_docker" if docker else "")
    files = os.listdir(os.getcwd())
    if dir_build not in files:
        os.mkdir(dir_build)
    if docker:
        os.environ["CC"] = "/usr/local/bin/gcc"
        os.environ["CXX"] = "/usr/local/bin/g++"
    if debug:
        run_cmd(f"cmake -S . -B {dir_build} -GNinja -DCMAKE_BUILD_TYPE=Debug")
        run_cmd(f"cmake --build {dir_build} --config Debug -j 8")
    else:
        run_cmd(f"cmake -S . -B {dir_build} -GNinja")
        run_cmd(f"cmake --build {dir_build} --config Release -j 8")

def clean():
    run_cmd("cmake --build build --target clean")


if __name__ == "__main__":

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("--clean", action="store_true")
    arg_parser.add_argument("--debug", action="store_true")
    arg_parser.add_argument("--docker", action="store_true")
    args = arg_parser.parse_args()

    files = os.listdir(os.getcwd())
    if "LICENSE" not in files:
        print("script should run in the root directory of the project")
        sys.exit(1)


    if args.clean:
        clean()
    else:
        build(args.debug, args.docker)
