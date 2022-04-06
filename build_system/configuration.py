import os
import sys
import re


class Configuration(object):
    def __init__(self):
        self.is_windows = (sys.platform == "win32")
        self.is_mac = (sys.platform == "darwin")
        self.is_linux = re.search('^linux', sys.platform, re.IGNORECASE)

        self.target_os = ""
        self.target_cpu = ""
        self.is_debug = "false"
        self.build_dir = ""
        self.build_command = ""
        self.build_target = ""
        self.generator = ""
        self.android_build_tools_ver = ""
        self.sysroot = ""
        self.gn_args_config = {}

        self.root_path = ""
        self.webrtc_path= ""

        self.gn_bin_path = ""
        self.ninja_bin_path = ""

        self.build_target = ""
        self.verbos_build = False
        self.limit_num_process = 0

        self.build_options_set = ""

    def init_path_config(self, script_path):
        self.root_path = os.path.abspath(os.path.join(script_path, ".."))
        self.webrtc_path = os.path.abspath(os.path.join(
            script_path, "..", "webrtc"))

    def init_command_config(self, script_path):
        self.gn_bin_path = os.path.join(script_path, "tools", "bin")
        self.ninja_bin_path = os.path.join(script_path, "tools", "bin")
        if self.is_windows:
            self.gn_bin_path = os.path.join(self.gn_bin_path, "win", "gn.exe")
            self.ninja_bin_path = os.path.join(
                self.ninja_bin_path, "win", "ninja.exe")
        elif self.is_mac:
            self.gn_bin_path = os.path.join(self.gn_bin_path, "mac", "gn")
            self.ninja_bin_path = os.path.join(
                self.ninja_bin_path, "mac", "ninja")
        else:
            self.gn_bin_path = os.path.join(self.gn_bin_path, "linux", "gn")
            self.ninja_bin_path = os.path.join(
                self.ninja_bin_path, "linux", "ninja")
        self.gn_bin_path = os.path.abspath(self.gn_bin_path).replace("\\", "/")
        self.ninja_bin_path = os.path.abspath(
            self.ninja_bin_path).replace("\\", "/")
