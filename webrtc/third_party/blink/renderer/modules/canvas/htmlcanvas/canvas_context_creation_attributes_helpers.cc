// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/htmlcanvas/canvas_context_creation_attributes_helpers.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_canvas_context_creation_attributes_module.h"
#include "third_party/blink/renderer/core/html/canvas/canvas_context_creation_attributes_core.h"
#include "third_party/blink/renderer/core/html/canvas/predefined_color_space.h"

namespace blink {

bool ToCanvasContextCreationAttributes(
    const CanvasContextCreationAttributesModule* attrs,
    CanvasContextCreationAttributesCore& result,
    ExceptionState& exception_state) {
  if (!ColorSpaceNameIsValid(attrs->colorSpace(), exception_state))
    return false;
  result.alpha = attrs->alpha();
  result.antialias = attrs->antialias();
  result.color_space = attrs->colorSpace();
  result.depth = attrs->depth();
  result.fail_if_major_performance_caveat =
      attrs->failIfMajorPerformanceCaveat();
#if defined(OS_MAC)
  // TODO(crbug.com/945835): enable desynchronized on Mac.
  result.desynchronized = false;
#else
  result.desynchronized = attrs->desynchronized();
#endif
  result.pixel_format = attrs->pixelFormat();
  result.premultiplied_alpha = attrs->premultipliedAlpha();
  result.preserve_drawing_buffer = attrs->preserveDrawingBuffer();
  result.power_preference = attrs->powerPreference();
  result.stencil = attrs->stencil();
  result.will_read_frequently = attrs->willReadFrequently();
  result.xr_compatible = attrs->xrCompatible();
  return true;
}

}  // namespace blink
