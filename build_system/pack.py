#!/usr/bin/env python
# -*- coding: UTF-8 -*-

import build
import distutils.dir_util
from genericpath import isdir
import os
import subprocess
from shutil import copyfile
import shutil
import sys
import tarfile
from utils import run_cmd


os.environ['PATH'] = '/usr/libexec' + os.pathsep + os.environ['PATH']
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.join(SCRIPT_DIR, "..", "webrtc", "tools_webrtc", "libs"))
from generate_licenses import LicenseBuilder

SDK_FRAMEWORK_NAME = 'WebRTC.framework'

target_archs = {
    "ios" : ['arm64', 'x64'],
    "mac" : ['arm64', 'x64'],
    "android" : ['arm64', 'arm', 'x86', 'x64'],
    "linux" : ['arm64', 'arm', 'x86', 'x64']
}


def build_gen(config, target_arch):
    print("Build package {0}:{1}.".format(config.target_os, str(target_arch)))
    gen_args = ['gen', config.target_os, target_arch, config.build_type]
    build.exec_build(gen_args)


def build_target(config, target_arch, target=None):
    if target is None:
        build_args = ['build', config.target_os, target_arch, config.build_type]
    else:
        build_args = ['build:' + str(target), config.target_os, target_arch, config.build_type]
    config = build.exec_build(build_args)
    return config.build_dir


def build_library(config, target_arch, target=None):
    build_gen(config, target_arch)
    return build_target(config, target_arch, target)


# Combine the slices.
def merge_framework_slice(output_dir, build_dirs):
    print('Merging framework slices.')
    lib_paths = [
        build_dir for (_, build_dir) in build_dirs.items()
    ]
    dylib_path = os.path.join(SDK_FRAMEWORK_NAME, 'WebRTC')
    # Dylibs will be combined, all other files are the same across archs.
    # Use distutils instead of shutil to support merging folders.
    distutils.dir_util.copy_tree(
        os.path.join(lib_paths[0], SDK_FRAMEWORK_NAME),
        os.path.join(output_dir, SDK_FRAMEWORK_NAME))

    dylib_paths = [os.path.join(path, dylib_path) for path in lib_paths]
    out_dylib_path = os.path.join(output_dir, dylib_path)
    try:
        os.remove(out_dylib_path)
    except OSError:
        pass
    cmd = ['lipo'] + dylib_paths + ['-create', '-output', out_dylib_path]
    cmd_str = " ".join(cmd)
    run_cmd(cmd_str)


def generate_license_file(output_dir, libs_path, gn_target_name):
    output_dir = os.path.join(output_dir, SDK_FRAMEWORK_NAME)
    print('Generate the license file to {0}.'.format(output_dir))
    ninja_dirs = libs_path
    gn_target_full_name = '//sdk:' + gn_target_name
    builder = LicenseBuilder(ninja_dirs, [gn_target_full_name])

    builder.GenerateLicenseText(output_dir)


def modify_the_version_number(output_dir):
    print('Modify the version number.')
    # Modify the version number.
    # Format should be <Branch cut MXX>.<Hotfix #>.<Rev #>.
    # e.g. 55.0.14986 means branch cut 55, no hotfixes, and revision 14986.
    revision = "0"
    infoplist_path = os.path.join(output_dir, SDK_FRAMEWORK_NAME,
                                  'Info.plist')
    cmd = [
        'PlistBuddy', '-c', 'Print :CFBundleShortVersionString',
        infoplist_path
    ]
    major_minor = subprocess.check_output(cmd).strip()
    version_number = "%s.%s" % (major_minor.decode('utf-8'), revision)
    print('Substituting revision number: %s' % version_number)
    cmd = [
        'PlistBuddy', '-c', '\'Set :CFBundleVersion ' + version_number + '\'',
        infoplist_path
    ]
    cmd_str = " ".join(cmd)
    run_cmd(cmd_str)

    cmd_str = " ".join(['plutil', '-convert', 'binary1', infoplist_path])
    run_cmd(cmd_str)


def merge_dSYM_slices(config, output_dir, build_dirs, gn_target_name):
    print('Merging dSYM slices.')
    libs_path = [path for (_, path) in build_dirs.items()]
    lib_dsym_dir_path = os.path.join(libs_path[0], 'WebRTC.dSYM')
    distutils.dir_util.copy_tree(lib_dsym_dir_path, os.path.join(output_dir, 'WebRTC.dSYM'))
    dsym_path = os.path.join('WebRTC.dSYM', 'Contents', 'Resources',
                             'DWARF', 'WebRTC')
    lib_dsym_paths = [os.path.join(path, dsym_path) for (_, path) in build_dirs.items()]
    out_dsym_path = os.path.join(output_dir, dsym_path)
    try:
        os.remove(out_dsym_path)
    except OSError:
        pass
    cmd = ['lipo'] + lib_dsym_paths + ['-create', '-output', out_dsym_path]
    cmd_str = " ".join(cmd)
    run_cmd(cmd_str)
    print("")

    generate_license_file(output_dir, libs_path, gn_target_name)
    print("")

    modify_the_version_number(output_dir)


def find_file_path(dir_path, target_file_name):
    dirs_list = []
    dirs_list.append(dir_path)
    while len(dirs_list) > 0:
        cur_dir = dirs_list[0]
        dirs_list = dirs_list[1:]
        dir_items = os.listdir(cur_dir)
        for item in dir_items:
            absolute_path = os.path.join(cur_dir, item)
            if item == target_file_name:
                return absolute_path
            elif isdir(absolute_path):
                dirs_list.append(absolute_path)

    return ""


def merge_static_library(output_dir, build_dirs, target_file_name):
    print('Merge static libs.')

    output_files = []
    for (_, build_dir) in build_dirs.items():
        static_lib_path = find_file_path(os.path.join(build_dir, "obj"), target_file_name)
        output_files.append(static_lib_path)
    print("")
    
    return output_files


def create_fat_static_library(output_dir, output_files, target_file_name):
    print("Create a FAT libwebrtc static library.")
    cmd = [
        'xcodebuild', '-find-executable', 'lipo'
    ]
    lipo_path = subprocess.check_output(cmd).strip().decode('utf-8')

    output_file = os.path.join(output_dir, target_file_name)
    cmd = [lipo_path] + output_files + ['-create', '-output'] + [output_file]
    cmd_str = " ".join(cmd)
    run_cmd(cmd_str)
    print("")


def pack_ios(config):
    output_dir = os.path.join(os.path.abspath("."), "build", config.target_os, config.build_type)
    archs = target_archs.get(config.target_os)
    gn_target_name = "framework_objc"
    build_dirs = {}
    for arch in archs:
        build_dir = build_library(config, arch, gn_target_name)
        build_dirs[arch] = build_dir
        print("")

    merge_framework_slice(output_dir, build_dirs)
    print("")

    build_dir = build_dirs.get(archs[0])
    lib_dsym_dir_path = os.path.join(build_dir, 'WebRTC.dSYM')

    if os.path.isdir(lib_dsym_dir_path):
        merge_dSYM_slices(config, output_dir, build_dirs, gn_target_name)
        print("")

    gn_target_names = ["webrtc", "crash_catch_system"]
    for arch in archs:
        for target_name in gn_target_names:
            build_target(config, arch, target_name)
            print("")

    gn_target_name = "webrtc"
    build_dirs = {}
    for arch in archs:
        build_dir = build_target(config, arch, gn_target_name)
        build_dirs[arch] = build_dir
        print("")

    target_files = ["libwebrtc.a", "libbreakpad_client.a"]
    for target_file in target_files:
        output_files = merge_static_library(output_dir, build_dirs, target_file)
        create_fat_static_library(output_dir, output_files, target_file)


def pack_mac(config):
    output_dir = os.path.join(os.path.abspath("."), "build", config.target_os, config.build_type)
    archs = target_archs.get(config.target_os)
    gn_target_names = ["webrtc", "mac_framework_objc", "crash_catch_system"]
    build_dirs = {}
    for arch in archs:
        build_dir = None
        for target in gn_target_names:
            if not build_dir:
                build_dir = build_library(config, arch, target)
                build_dirs[arch] = build_dir
            else:
                build_library(config, arch, target)
        print("")

    merge_framework_slice(output_dir, build_dirs)
    print("")

    build_dir = build_dirs.get(archs[0])
    # lib_dsym_dir_path = os.path.join(build_dir, 'WebRTC.dSYM')

    #if os.path.isdir(lib_dsym_dir_path):
    #    merge_dSYM_slices(config, output_dir, build_dirs, gn_target_name)
    #    print("")

    gn_target_name = "webrtc"
    build_dirs = {}
    for arch in archs:
        build_dir = build_target(config, arch, gn_target_name)
        build_dirs[arch] = build_dir
        print("")

    target_files = ["libwebrtc.a", "libbreakpad_client.a"]

    for target_file in target_files:
        output_files = merge_static_library(output_dir, build_dirs, target_file)
        create_fat_static_library(output_dir, output_files, target_file)


def pack_android(config):
    archs = target_archs.get(config.target_os)
    gn_target_names = ["webrtc"]
    build_dirs = {}
    for arch in archs:
        build_dir = None
        for target in gn_target_names:
            if not build_dir:
                build_dir = build_library(config, arch, target)
                build_dirs[arch] = build_dir
            else:
                build_library(config, arch, target)

    arch2abi = {
      'arm64' : "arm64-v8a",
      'arm' : "armeabi-v7a",
      'x86' : "x86",
      'x64' : "x86_64",
    }

    output_dir = os.path.join(os.path.abspath("."), "build", config.target_os, config.build_type)

    if os.path.exists(output_dir):
        shutil.rmtree(output_dir)

    os.mkdir(output_dir)
    output_dir = os.path.join(output_dir, 'WebRTC_SDK')
    os.mkdir(output_dir)

    webrtc_jar_path = None

    for arch, abi in arch2abi.items():
        build_dir = build_dirs[arch]
        webrtc_lib_path = os.path.join(build_dir, "obj", "libwebrtc.a")

        target_dir = os.path.join(output_dir, abi)
        os.mkdir(target_dir)
        target_file_path = os.path.join(target_dir, "libwebrtc.a")

        copyfile(webrtc_lib_path, target_file_path)

        if not webrtc_jar_path:
            webrtc_jar_path = os.path.join(build_dir, "lib.java", "sdk", "android", "libwebrtc.jar")

    target_webrtc_jar_path = os.path.join(output_dir, "libwebrtc.jar")
    copyfile(webrtc_jar_path, target_webrtc_jar_path)

    api_dir_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'webrtc', 'api')
    target_header_path = os.path.join(output_dir, "include")
    os.mkdir(target_header_path)
    target_header_path = os.path.join(target_header_path, "api")

    distutils.dir_util.copy_tree(api_dir_path, target_header_path)

    manifest_file_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'webrtc',
                                      'sdk', 'android', 'AndroidManifest.xml')
    target_manifest_file_path = os.path.join(output_dir, os.path.basename(manifest_file_path))
    copyfile(manifest_file_path, target_manifest_file_path)

    target_package_file = os.path.join(output_dir, '..', 'WebRTC_SDK-{}.tar.gz'.format(config.build_type))

    if os.path.exists(target_package_file):
        os.remove(target_package_file)

    with tarfile.open(target_package_file, "w:gz") as tar:
        tar.add(output_dir, arcname=os.path.basename(output_dir))

    print("Create android webrtc package completely: ", os.path.abspath(target_package_file))


def pack_linux(config):
    print()


COMMANDS = {
    "ios": lambda x: pack_ios(x),
    "mac": lambda x: pack_mac(x),
    "android": lambda x: pack_android(x),
    "linux": lambda x: pack_linux(x),
}


class PackConfig(object):
    def __init__(self):
        self.target_os = ""
        self.build_type = ""


def die_with_print_usage(status, additional_msg=None, script_path=None):
    print("")
    if additional_msg:
        print(additional_msg)
        print("")
    print(
        "Usage: webrtc_pack target_os build_type")
    print("\t target_os:    win|mac|ios|android|linux")
    print("\t build_type:   debug|release, by default is release")
    print('\t options_set:  build_options_set name you want to build, by default the file name is "default.conf"')
    print('\t dir:          output directory, by default "build" dir under current directory')
    print("")
    print('It is strongly recommended that you put "{}"'
          '\ninto your system environment variable PATH '
          'so that you can run webrtc_pack anywhere'.format(script_path))
    print("")
    sys.exit(-1)


def main(args):
  if len(args) < 2:
      die_with_print_usage(-1)
  config = PackConfig()
  script_path = os.path.dirname(os.path.realpath(__file__))

  config.target_os = args[0].strip()
  config.build_type = args[1].strip()
  func = COMMANDS.get(config.target_os)
  if not func:
      print("Invalid target os " + str(config.target_os))
      die_with_print_usage(-1)

  func(config)


if __name__ == '__main__':
  try:
    sys.exit(main(sys.argv[1:]))
  except KeyboardInterrupt:
    sys.stderr.write('interrupted\n')
    sys.exit(1)
