
import os
import shlex
from configuration import Configuration
from functools import partial
from utils import die_with_status

die_with_status = partial(die_with_status, script_path=os.path.dirname(os.path.realpath(__file__)))

ARGS_HANDLER = []

def build_args_handler(accept_range=[], is_mandatory=True):
    def decorator(func):
        ARGS_HANDLER.append(func.__name__)
        def wrapper(**kwargs):
            argv = kwargs.get('argv')
            config = kwargs.get('config')
            arg = argv[0] if argv else None
            if is_mandatory and not argv:
                die_with_status(-1, "Must provide {} in command line.".format(func.__name__[7:]))

            if accept_range and not any((arg.lower().startswith(v) if arg else False) for v in accept_range):
                die_with_status(-1, "'{}' is not a valid value, valid: {}".format(arg, ",".join(accept_range)))

            func(config=config, value=argv.pop(0)) if argv else func(config=config)
        return wrapper
    return decorator


@build_args_handler(accept_range=["gen", "build", "clean", "info", "deps_tree"])
def handle_build_command(**kwargs):
    argv = kwargs.get('value')
    config = kwargs.get('config')

    if ':' in argv:
        _tmp_cmd = argv.replace(":", " ")
        arr = [p.replace(" ", ":") for p in shlex.split(_tmp_cmd)]
        config.build_target = " ".join(arr[1:])
        config.build_command = arr[0]
    else:
        config.build_target = ""
        config.build_command = argv

    if config.build_command == "gen":
        config.build_target = ""


@build_args_handler(accept_range=["win", "mac", "linux", "ios", "android"])
def handle_target_os(**kwargs):
    config = kwargs.get('config')
    if config.build_command.startswith("cloud"):
       return
    argv = kwargs.get('value').lower()

    config.target_os = argv


@build_args_handler()
def handle_target_cpu(**kwargs):
    config = kwargs.get('config')
    if config.build_command.startswith("cloud"):
       return
    argv = kwargs.get('value').lower()

    # windows sdk 16299 can build for arm, but cann't work with our source code
    # windows sdk 17134 can build for arm64 and our source code, but cann't build with windows arm
    # so we just keep arm64 for now
    valid_target_cpu = {
        "win": ["x86", "x64", "arm64"],
        "mac": ["x64", "arm64"],
        "linux": ["x64", "arm", "arm64", "mipsel", "mips64el"],
        "ios": ["x86", "x64", "arm", "arm64"],
        "android": ["x86", "x64", "arm", "arm64"],
    }
    config.target_cpu = argv

    if argv not in valid_target_cpu.get(config.target_os):
        die_with_status(-1, "Not a valid cpu type. Available types for os {0} is [{1}]".format(
                config.target_os, ', '.join(valid_target_cpu[config.target_os])))


@build_args_handler(accept_range=["debug", "release"])
def handle_build_config(**kwargs):
    config = kwargs.get('config')
    if config.build_command.startswith("cloud"):
        return
    argv = kwargs.get('value').lower()

    config.is_debug = "true" if argv == "debug" else "false"


@build_args_handler(is_mandatory=False)
def handle_build_options_set(**kwargs):
    argv = kwargs.get('value', 'default').lower()
    config = kwargs.get('config')

    config.build_options_set = argv


@build_args_handler(is_mandatory=False)
def handle_build_dir(**kwargs):
    argv = kwargs.get('value', 'build')
    config = kwargs.get('config')

     # append os and cpu into build_dir
    config.build_dir = argv + "/" + config.target_os + "/" + config.target_cpu
    config.build_dir = os.path.abspath(config.build_dir)
    if config.is_debug == "true":
        config.build_dir = os.path.join(config.build_dir, "debug")
    else:
        config.build_dir = os.path.join(config.build_dir, "release")

    if not os.path.exists(config.build_dir):
        os.makedirs(config.build_dir)


@build_args_handler(is_mandatory=False)
def handle_generator(**kwargs):
    argv = kwargs.get('value', 'default').lower()
    config = kwargs.get('config')

    config.generator = argv