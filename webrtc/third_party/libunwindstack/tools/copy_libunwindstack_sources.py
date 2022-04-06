#!/usr/bin/env vpython3
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
'''Copies the source code required to compile libunwindstack into the
libunwindstack/src directory.'''

import argparse
import os
import re
import shutil
import subprocess
import sys

_LIBUNWINDSTACK_ROOT = os.path.abspath(
    os.path.join(os.path.dirname(__file__), os.pardir))
_LIBUNWINDSTACK_SRC_ROOT = os.path.join(_LIBUNWINDSTACK_ROOT, 'src')
_DIRS_TO_COPY = [
    'base',
    'liblog',
    'libprocinfo',
    'libunwindstack',
]
_ALTERNATE_PATH_DICT = {'base': 'android-base'}


def GitFetchTag(git_tag):
  'Fetches the specified upstream git tag.'
  # Upstream is a direct mirror of Android's platform/system/core. That git
  # repo's ref/heads/* and refs/tags/* are mirrored to origin's
  # refs/upstream/heads/* and refs/upstream/tags/*, respectively. By default,
  # only refs/heads/* and refs/tags/* are fetched, so we need to explicitly
  # fetch the tag we care about.
  subprocess.check_call([
      'git', 'fetch', 'origin',
      'refs/upstream/tags/{0}'.format(git_tag)
  ])

def GitFetchRevision(revision):
  'Fetches the specified upstream git revision.'
  subprocess.check_call(['git', 'fetch', 'origin', revision])


def CopyDirsFromUpstream(git_rev):
  '''Checks out the required directories from the upstream Android repo and puts
  them into the src/ directory.'''
  os.mkdir(_LIBUNWINDSTACK_SRC_ROOT)
  for path in _DIRS_TO_COPY:
    dir_root_path = os.path.join(_LIBUNWINDSTACK_ROOT, path)
    if os.path.exists(dir_root_path):
      raise IOError('{} should not exist before running this script.'.format(
          dir_root_path))

    subprocess.check_call(
        ['git', 'checkout', git_rev, '--', path])

    # Some directories need to be renamed in order for C++ #includes to work
    # correctly.
    dir_src_path = os.path.join(_LIBUNWINDSTACK_SRC_ROOT,
                                _ALTERNATE_PATH_DICT.get(path, path))
    # The source file directories live in upstream's root directory, so are
    # copied there when we use `git checkout`. In this repo, we move them to
    # the src/ directory.
    shutil.move(dir_root_path, dir_src_path)


def Main():
  parser = argparse.ArgumentParser(description='''Copies the source code
      required to compile libunwindstack into the libunwindstack/src/
      directory.''')
  parser.add_argument(
      'git_rev',
      help='''The git revision or tag for the Android platform/system/core
      commit that you want to copy the source code from.''')
  args = parser.parse_args()

  if os.path.exists(_LIBUNWINDSTACK_SRC_ROOT):
    raise IOError('{} should not exist before running this script.'.format(
        _LIBUNWINDSTACK_SRC_ROOT))

  if re.search(r'^[0-9a-f]+$', args.git_rev):
    GitFetchRevision(args.git_rev)
  else:
    GitFetchTag(args.git_rev)
  CopyDirsFromUpstream('FETCH_HEAD')
  return 0


if __name__ == '__main__':
  sys.exit(Main())
