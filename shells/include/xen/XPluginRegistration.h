#pragma once

#include "xen/XPlugin.h"

#include <memory>
#include <vector>

namespace xen {

// Implemented by the generated registration TU. The kernel does not see it.
std::vector<std::unique_ptr<XPlugin>> createXPlugins();

}
