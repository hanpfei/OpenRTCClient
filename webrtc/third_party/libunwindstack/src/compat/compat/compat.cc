// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

const char* compat_basename(const char* file) {
  // Implementation based off of the one in <android-base/logging.cpp>.
  const char* last_slash = strrchr(file, '/');
  if (last_slash != nullptr) {
    return last_slash + 1;
  }
  return file;
}
