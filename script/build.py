#!/usr/bin/env python3
import os
import sys
import argparse


def run_cmd(cmd: str):
    print(f"Running: {cmd}")
    os.system(cmd)


def build(docker: bool = False):
    dir_build = "build" + ("_docker" if docker else "")
    dir_build_debug = dir_build + "_debug"
    files = os.listdir(os.getcwd())
    if dir_build not in files:
        os.mkdir(dir_build)
    if docker:
        if os.path.exists(f"tools.tar.gz"):
            os.remove(f"tools.tar.gz")
        os.environ["CC"] = "/usr/local/bin/gcc"
        os.environ["CXX"] = "/usr/local/bin/g++"
        run_cmd(
            r'sed -i "s/# set(Boost_USE_STATIC_LIBS ON)/set(Boost_USE_STATIC_LIBS ON)/g" CMakeLists.txt'
        )
        run_cmd(f"cmake -S . -B {dir_build} -GNinja")
        run_cmd(f"cmake --build {dir_build} --config Release -j 8")
        run_cmd(f"cmake -S . -B {dir_build_debug} -GNinja -DCMAKE_BUILD_TYPE=Debug")
        run_cmd(f"cmake --build {dir_build_debug} --config Debug -j 8")
        run_cmd(
            f"tar -zcvf tools.tar.gz {dir_build}/slack_tool {dir_build_debug}/slack_tool-debug yml/design_period.yml"
        )
        run_cmd(
            r'sed -i "s/set(Boost_USE_STATIC_LIBS ON)/# set(Boost_USE_STATIC_LIBS ON)/g" CMakeLists.txt'
        )
        return

    else:
        run_cmd(f"cmake -S . -B {dir_build} -GNinja")
        run_cmd(f"cmake --build {dir_build} --config Release -j 8")
        run_cmd(f"cmake -S . -B {dir_build_debug} -GNinja -DCMAKE_BUILD_TYPE=Debug")
        run_cmd(f"cmake --build {dir_build_debug} --config Debug -j 8")


def clean():
    run_cmd("cmake --build build --target clean")


if __name__ == "__main__":

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("--clean", action="store_true")
    arg_parser.add_argument("--docker", action="store_true")
    args = arg_parser.parse_args()

    files = os.listdir(os.getcwd())
    if "LICENSE" not in files:
        print("script should run in the root directory of the project")
        sys.exit(1)

    if args.clean:
        clean()
    else:
        build(args.docker)
