#!/usr/bin/env python3
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import re
import shutil
import subprocess
import sys

_RSP_RE = re.compile(r' (@(.+?\.rsp)) ')
_GOMA_CMD_LINE_RE = re.compile(
    r'^(?P<gomacc>.*gomacc(\.exe)?"?\s+)?(?P<clang>\S*clang\S*)\s+(?P<args>.*)$'
)
_debugging = False


def _IsTargettingWindows(target_os):
  if target_os is not None:
    # Available choices are based on: gn help target_os
    assert target_os in [
        'android',
        'chromeos',
        'fuchsia',
        'ios',
        'linux',
        'mac',
        'nacl',
        'win',
    ]
    return target_os == 'win'
  return sys.platform == 'win32'


def _ProcessCommand(command, filtered_args, target_os):
  # If the driver mode is not already set then define it. Driver mode is
  # automatically included in the compile db by clang starting with release
  # 9.0.0.
  driver_mode = ''
  # Only specify for Windows. Other platforms do fine without it.
  if _IsTargettingWindows(target_os) and '--driver-mode' not in command:
    driver_mode = '--driver-mode=cl'

  # Removes gomacc(.exe). On Windows inserts --driver-mode=cl as the first arg.
  #
  # Deliberately avoid  shlex.split() here, because it doesn't work predictably
  # for Windows commands (specifically, it doesn't parse args the same way that
  # Clang does on Windows).
  #
  # Instead, use a regex, with the simplifying assumption that the path to
  # clang-cl.exe contains no spaces.
  match = _GOMA_CMD_LINE_RE.search(command)
  if match:
    match_dict = match.groupdict()
    command = ' '.join([match_dict['clang'], driver_mode, match_dict['args']])
  elif _debugging:
    print('Compile command didn\'t match expected regex!')
    print('Command:', command)
    print('Regex:', _GOMA_CMD_LINE_RE.pattern)

  # Remove some blocklisted arguments. These are Visual Studio-specific
  # arguments not recognized or used by some third-party clang tooling. They
  # only suppress or activate graphical output anyway.
  args_to_remove = ['/nologo', '/showIncludes']
  if filtered_args:
    args_to_remove.extend(filtered_args)
  command_parts = filter(lambda arg: arg not in args_to_remove, command.split())

  # Drop frontend-only arguments, which generally aren't needed by clang
  # tooling.
  filtered_command_parts = []
  skip_next = False
  for command_part in command_parts:
    if command_part == '-Xclang':
      skip_next = True
      continue
    if skip_next:
      skip_next = False
      continue
    filtered_command_parts.append(command_part)

  return ' '.join(filtered_command_parts)


def _ProcessEntry(entry, filtered_args, target_os):
  """Transforms one entry in a compile db to be more clang-tool friendly.

  Expands the contents of the response file, if any, and performs any
  transformations needed to make the compile DB easier to use for third-party
  tooling.
  """
  # Expand the contents of the response file, if any.
  # http://llvm.org/bugs/show_bug.cgi?id=21634
  try:
    match = _RSP_RE.search(entry['command'])
    if match:
      rsp_path = os.path.join(entry['directory'], match.group(2))
      rsp_contents = open(rsp_path).read()
      entry['command'] = ''.join([
          entry['command'][:match.start(1)], rsp_contents,
          entry['command'][match.end(1):]
      ])
  except IOError:
    if _debugging:
      print('Couldn\'t read response file for %s' % entry['file'])

  entry['command'] = _ProcessCommand(entry['command'], filtered_args, target_os)

  return entry


def ProcessCompileDatabase(compile_db, filtered_args, target_os=None):
  """Make the compile db generated by ninja more clang-tool friendly.

  Args:
    compile_db: The compile database parsed as a Python dictionary.

  Returns:
    A postprocessed compile db that clang tooling can use.
  """
  compile_db = [_ProcessEntry(e, filtered_args, target_os) for e in compile_db]

  if not _IsTargettingWindows(target_os):
    return compile_db

  if _debugging:
    print('Read in %d entries from the compile db' % len(compile_db))
  original_length = len(compile_db)

  # Filter out NaCl stuff. The clang tooling chokes on them.
  # TODO(dcheng): This doesn't appear to do anything anymore, remove?
  compile_db = [
      e for e in compile_db if '_nacl.cc.pdb' not in e['command']
      and '_nacl_win64.cc.pdb' not in e['command']
  ]
  if _debugging:
    print('Filtered out %d entries...' % (original_length - len(compile_db)))

  # TODO(dcheng): Also filter out multiple commands for the same file. Not sure
  # how that happens, but apparently it's an issue on Windows.
  return compile_db


def GetNinjaPath():
  ninja_executable = 'ninja.exe' if sys.platform == 'win32' else 'ninja'
  return os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..',
                      '..', '..', 'third_party', 'depot_tools',
                      ninja_executable)


# FIXME: This really should be a build target, rather than generated at runtime.
def GenerateWithNinja(path, targets=None):
  """Generates a compile database using ninja.

  Args:
    path: The build directory to generate a compile database for.
    targets: Additional targets to pass to ninja.

  Returns:
    List of the contents of the compile database.
  """
  # TODO(dcheng): Ensure that clang is enabled somehow.

  # First, generate the compile database.
  ninja_path = GetNinjaPath()
  if not os.path.exists(ninja_path):
    ninja_path = shutil.which('ninja')
  if targets is None:
    targets = []
  json_compile_db = subprocess.check_output(
      [ninja_path, '-C', path] + targets +
      ['-t', 'compdb', 'cc', 'cxx', 'objc', 'objcxx'])
  return json.loads(json_compile_db)


def Read(path):
  """Reads a compile database into memory.

  Args:
    path: Directory that contains the compile database.
  """
  with open(os.path.join(path, 'compile_commands.json'), 'rb') as db:
    return json.load(db)
