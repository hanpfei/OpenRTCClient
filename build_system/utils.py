
import os
import sys


def use_clang(config):
    if config.target_os != "win":
        # in linux/mac/ios/android: we use clang
        return "true"

    if os.environ.get('GYP_MSVS_VERSION', None) != "2019":
        # if user want to use 2017: I don't recommend him to do so, but why the hell should I stop him to fuck himself?
        return "true"

    return "false"


def get_platform_name(config):
    if config.is_windows:
        return "windows"
    elif config.is_linux:
        return "linux"
    elif config.is_mac:
        return "mac"
    else:
        return ""


def add_feature(arg, config, overwrite=False):
    """
    if arg exist in gn_args_config, return
    """
    feat = str(arg).strip()
    if not feat or feat.startswith("#"):
        return
    if "#" in feat:
        feat = feat.split('#')[0]

    if feat.endswith('\n'):
        feat = feat[:-1]

    if "=" not in feat:
        return

    k, v = feat.split('=')
    k = k.strip()
    v = v.strip()
    if not k or not v:
        return
    if k not in config.gn_args_config:
        config.gn_args_config[k] = v
    elif overwrite:
        config.gn_args_config[k] = v


def run_cmd(cmd, show_output=True):
    if show_output:
        print(cmd)
    FNULL = open(os.devnull, 'w')
    import subprocess
    if show_output:
        pipe = subprocess.Popen(cmd, stderr=sys.stderr, stdout=sys.stdout, shell=True)
    else:
        pipe = subprocess.Popen(cmd, stderr=FNULL, stdout=FNULL, shell=True)
    ret = pipe.wait()
    FNULL.close()
    return ret


def run_cmd_with_args(cmd, show_output=True):
    if show_output:
        print(cmd)
    import subprocess

    subprocess.run(cmd, check=True)


def run_or_die(cmd, show_output=True):
    if run_cmd(cmd, show_output) != 0:
        print("CMD: {} fail".format(cmd))
        sys.exit(-1)
    return 0


def die_with_status(status, additional_msg=None, script_path=None):
    # type: (int) -> None
    print("")
    if additional_msg:
        print(additional_msg)
        print("")
    print(
        "Usage: webrtc_build command[:target] target_os target_cpu [build_type] [build_options_set] [dir] [generator]")
    print("\t command:      gen|build|clean|info")
    print("\t target_os:    win|mac|ios|android|linux")
    print("\t target_cpu:   x86|x64|arm|arm64")
    print("\t build_type:   debug|release, by default is release")
    print('\t options_set:  build_options_set name you want to build, by default the file name is "default.conf"')
    print('\t dir:          output directory, by default "build" dir under current directory')
    print('\t generator     cmake|make|vs|xcode')
    print("")
    print('It is strongly recommended that you put "{}"'
          '\ninto your system environment variable PATH '
          'so that you can run webrtc_build anywhere'.format(script_path))
    print("")
    sys.exit(-1)