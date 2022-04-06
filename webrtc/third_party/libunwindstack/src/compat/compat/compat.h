// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPAT_COMPAT_H_
#define COMPAT_COMPAT_H_

// Provides functions that are available in libunwindstack's normal build
// environment (platform/system/core, built at Android ToT) but unavailable in
// libunwindstack's build environment within Chrome (currenty built at Android
// NDK 16).

// Replicates the non-modifying GNU basename() function available in <string.h>
// that was introduced in Android NDK 23: https://bit.ly/31Gkyl9. We rename the
// function to avoid colliding with any existing basename() functions defined at
// the global scope.
const char* compat_basename(const char* file);

#endif  // COMPAT_COMPAT_H_
