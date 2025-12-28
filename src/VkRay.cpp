#include "VkRay/VkRay.h"

namespace vr
{
    VkRayLogCallback LogCallback = nullptr;

    namespace detail
    {
        char _gStringFormatBuffer[1024];
    }

} // namespace vr
