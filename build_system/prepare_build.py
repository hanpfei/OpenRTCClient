
import glob
import json
import os
import sys
import tarfile
from utils import run_cmd


def download_repo_version(repo_local_path, repo_url, version):
    if os.path.exists(repo_local_path):
        return

    parent_dir = os.path.dirname(repo_local_path)

    if not os.path.exists(parent_dir):
        os.makedirs(parent_dir)

    cmd = "git clone {0} {1}".format(repo_url, repo_local_path)
    run_cmd(cmd)

    cur_dir = os.getcwd()
    os.chdir(repo_local_path)

    cmd = "git checkout {0}".format(version)
    run_cmd(cmd)

    os.chdir(cur_dir)


def prepare_gen_common(config):
    dep_file_path = os.path.join(config.webrtc_path, ".deps")

    if not os.path.exists(dep_file_path):
        print("No deps file exist")
        return

    with open(dep_file_path, "r") as f:
        deps_desc = json.load(f)
        f.close()

    for path, repo in deps_desc.items():
        if path.startswith("src/"):
            path = path.replace("src/", "")

        repo_local_path = os.path.join(config.webrtc_path, path)

        if repo.startswith("@"):
            continue

        repo_url = repo
        version = None
        if repo.rfind("@") >= 0:
            repo_url = repo[0:repo.rfind("@")]
            version = repo[repo.rfind("@") + 1:]

        download_repo_version(repo_local_path, repo_url, version)


def select_build_tools():
    """
        Select a max one or install 31.0.0
    """
    sdk_path = os.environ.get("ANDROID_SDK", None)
    if not sdk_path or not os.path.exists(sdk_path) or not os.path.isdir(sdk_path):
        print("Please set ANDROID_SDK environment variable point to a valid android sdk directory")
        sys.exit(-1)

    build_tools_dir = os.path.join(sdk_path, "build-tools")
    build_tools_dir = os.path.abspath(build_tools_dir)

    dirs = os.listdir(build_tools_dir)
    if not dirs:
        dirs = []

    dirs.append("30.0.1")
    dirs.sort()

    result = dirs[len(dirs) - 1]

    selected_build_tools_path = os.path.join(build_tools_dir, result)

    # tools/bin/sdkmanager need java 8.
    # Suppose java point to java 8 here
    java_home = os.environ["JAVA_HOME"]
    os.environ["JAVA_HOME"] = os.environ["JAVA_8_HOME"]
    if not os.path.exists(selected_build_tools_path):
        print("Try to install build-tools " + result)
        cmd = "{0}/tools/bin/sdkmanager --install \"build-tools;{1}\"".format(
            sdk_path, result
            )
        run_cmd(cmd)

    if result != "30.0.1":
        result = "30.0.1"
    selected_build_tools_path = os.path.join(build_tools_dir, result)
    if not os.path.exists(selected_build_tools_path):
        print("Try to install build-tools " + result)
        cmd = "{0}/tools/bin/sdkmanager --install \"build-tools;{1}\"".format(
            sdk_path, result
            )
        run_cmd(cmd)

    os.environ["JAVA_HOME"] = java_home

    return result


def reset_android_ndk_env_if_needed():
    ndk_path = os.environ.get("ANDROID_NDK", None)
    if not ndk_path or not os.path.exists(ndk_path) or not os.path.isdir(ndk_path):
        print("Please set ANDROID_NDK environment variable properly")
        sys.exit(-1)

    ndk_build_path = os.path.join(ndk_path, "ndk-build")
    if os.path.exists(ndk_build_path):
        return

    ndks = os.listdir(ndk_path)
    if not ndks or len(ndks) <= 0:
        print("Please set ANDROID_NDK environment variable properly")
        sys.exit(-1)

    ndks.sort()
    selected_ndk_version = ndks[len(ndks) - 1]
    final_ndk_path = os.path.join(ndk_path, selected_ndk_version)
    os.environ["ANDROID_NDK"] = final_ndk_path


def prepare_gen_android(config):
    reset_android_ndk_env_if_needed()

    config.android_build_tools_ver = select_build_tools()


def _merge_sysroot_packages(sysroot_path):
    debian_sysroot_package = os.path.join(sysroot_path, "debian_sid_amd64-sysroot.tgz0*")
    sysroot_packages = glob.glob(debian_sysroot_package)
    sysroot_packages.sort()

    total_size = 0
    for pkg in sysroot_packages:
        filesize = os.path.getsize(pkg)
        total_size = total_size + filesize
        print(filesize, ", ", total_size)

    debian_sysroot_package = os.path.join(sysroot_path, "debian_sid_amd64-sysroot.tgz")
    if os.path.exists(debian_sysroot_package) and os.path.getsize(debian_sysroot_package) == total_size:
        return debian_sysroot_package

    if os.path.exists(debian_sysroot_package):
        os.remove(debian_sysroot_package)

    debian_sysroot_package = os.path.join(sysroot_path, "debian_sid_amd64-sysroot.tgz")
    with open(debian_sysroot_package, "wb") as final_pkg_file:
        for pkg in sysroot_packages:
            filesize = os.path.getsize(pkg)
            with open(pkg, "rb") as pkg_file:
                write_file_size = 0
                while write_file_size < filesize:
                    #print(filesize, " ", write_file_size)
                    write_size = 0
                    if write_file_size + 1024 * 256 < filesize:
                        write_size = 1024 * 256
                    else:
                        write_size = filesize - write_file_size

                    data = pkg_file.read(write_size)
                    final_pkg_file.write(data)
                    write_file_size = write_file_size + write_size

    if os.path.exists(debian_sysroot_package) and os.path.getsize(debian_sysroot_package) == total_size:
        return debian_sysroot_package

    print(os.path.getsize(debian_sysroot_package), ", ", total_size)

    return "invalid_file"


def prepare_linux_sysroot(config):
    sysroot_path = os.path.join(config.root_path, "build_system/sysroot/linux")
    debian_sysroot_path = os.path.join(sysroot_path, "debian_sid_amd64-sysroot")

    if os.path.exists(debian_sysroot_path):
        return

    debian_sysroot_package = _merge_sysroot_packages(sysroot_path)

    print("debian_sysroot_package " + debian_sysroot_package)
    if not os.path.exists(debian_sysroot_package):
        print("No debina sysroot package available")
        sys.exit(-1)

    file = tarfile.open(debian_sysroot_package)
    file.extractall(path=sysroot_path)


def prepare_gen_linux(config):
    prepare_linux_sysroot(config)


def prepare_build_android(config):
    reset_android_ndk_env_if_needed()
    sdk_path = os.environ.get("ANDROID_SDK", None)
    if not sdk_path or not os.path.exists(sdk_path) or not os.path.isdir(sdk_path):
        print("Please set ANDROID_SDK environment variable point to a valid android sdk directory")
        sys.exit(-1)

    java_home = os.environ["JAVA_HOME"]
    os.environ["JAVA_HOME"] = os.environ["JAVA_8_HOME"]

    cmdline_tools_path = os.path.join(sdk_path, "cmdline-tools")
    cmdline_tools_version = "3.0"
    selected_cmdline_tools_path = os.path.join(cmdline_tools_path, cmdline_tools_version)
    if not os.path.exists(selected_cmdline_tools_path):
        print("Try to install cmdline-tools " + cmdline_tools_version)
        cmd = "{0}/tools/bin/sdkmanager --install \"cmdline-tools;{1}\"".format(
            sdk_path, cmdline_tools_version
            )
        run_cmd(cmd)

    os.environ["JAVA_HOME"] = java_home

    check_candidates = [
        os.path.join(config.webrtc_path, "third_party/android_deps/libs/android_arch_core_common/*.jar"),
        os.path.join(config.webrtc_path, "third_party/android_deps/libs/androidx_media_media/*.aar"),
        os.path.join(config.webrtc_path, "third_party/android_deps/libs/androidx_core_core/*.aar"),
        os.path.join(config.webrtc_path, "third_party/android_deps/libs/com_android_support_loader/*.aar"),
        os.path.join(config.webrtc_path, "third_party/android_deps/libs/com_google_guava_guava/*.jar"),
    ]

    for check_item in check_candidates:
        file_paths = glob.glob(check_item)
        if file_paths and len(file_paths) > 0:
            print("All android dependencies has been fetched.")
            return

    fetch_all_deps_script_path = os.path.join(config.webrtc_path, "third_party/android_deps/fetch_all.py")
    run_cmd(fetch_all_deps_script_path)


