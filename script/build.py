#!/usr/bin/env python3
import argparse
import os
import sys


def run_cmd(cmd: str):
    print(f"Running: {cmd}")
    os.system(cmd)


def build(docker: bool = False, build_mode: str = "all"):
    dir_build = "build" + ("_docker" if docker else "")
    dir_build_debug = dir_build + "_debug"
    build_release = build_mode in ("all", "release")
    build_debug = build_mode in ("all", "debug")
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
        if build_release:
            run_cmd(f"cmake -S . -B {dir_build} -GNinja -DCMAKE_BUILD_TYPE=Release")
            run_cmd(f"cmake --build {dir_build} --config Release -j 8")
        if build_debug:
            run_cmd(f"cmake -S . -B {dir_build_debug} -GNinja -DCMAKE_BUILD_TYPE=Debug")
            run_cmd(f"cmake --build {dir_build_debug} --config Debug -j 8")
        tar_inputs = []
        if build_release:
            tar_inputs.append(f"{dir_build}/slack_tool")
        if build_debug:
            tar_inputs.append(f"{dir_build_debug}/slack_tool-debug")
        tar_inputs.append("configs/design_period.yml")
        run_cmd(
            f"tar -zcvf tools.tar.gz {' '.join(tar_inputs)}"
        )
        run_cmd(
            r'sed -i "s/set(Boost_USE_STATIC_LIBS ON)/# set(Boost_USE_STATIC_LIBS ON)/g" CMakeLists.txt'
        )
        return

    else:
        cmake_defs = ""
        import platform
        from pathlib import Path

        release = platform.release()
        if "1160" in release and "el7.x86_64" in release:
            home = Path.home()
            print(
                f"Detected Linux version {release}, setting specific GCC paths and CMake definitions."
            )
            gcc_bin = home / "packages/gcc-13.2.0/bin"
            if (gcc_bin / "gcc").exists() and (gcc_bin / "g++").exists():
                os.environ["CC"] = str((gcc_bin / "gcc").resolve())
                os.environ["CXX"] = str((gcc_bin / "g++").resolve())
                cmake_defs += "-DENV_EL7=ON "
        if build_release:
            run_cmd(
                f"cmake -S . -B {dir_build} -GNinja -DCMAKE_BUILD_TYPE=Release {cmake_defs}"
            )
            run_cmd(f"cmake --build {dir_build} --config Release -j 8")
        # run_cmd(
        #     f"cmake -S . -B {dir_build_debug} -GNinja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON"
        # )
        if build_debug:
            run_cmd(
                f"cmake -S . -B {dir_build_debug} -GNinja -DCMAKE_BUILD_TYPE=Debug {cmake_defs}"
            )
            run_cmd(f"cmake --build {dir_build_debug} --config Debug -j 8")


def clean():
    run_cmd("cmake --build build --target clean")


if __name__ == "__main__":

    arg_parser = argparse.ArgumentParser()
    arg_parser.add_argument("--clean", action="store_true")
    arg_parser.add_argument("--docker", action="store_true")
    arg_parser.add_argument(
        "--build-mode",
        choices=["all", "release", "debug"],
        default="all",
        help="Choose which build variants to compile.",
    )
    args = arg_parser.parse_args()

    files = os.listdir(os.getcwd())
    if "LICENSE" not in files:
        print("script should run in the root directory of the project")
        sys.exit(1)

    if args.clean:
        clean()
    else:
        build(args.docker, args.build_mode)
