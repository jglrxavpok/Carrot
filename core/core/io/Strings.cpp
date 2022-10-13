#include "Strings.h"
#include <array>
#include <vector>
#include "core/utils/stringmanip.h"
#include "core/utils/Assert.h"

namespace Carrot::IO {
    std::string toReadableFormat(std::uint64_t size) {
        struct SizeUnit {
            std::size_t smallest = 1;
            std::string unit;
        };

        const std::array unitsDescending {
                SizeUnit { 1024llu * 1024llu * 1024llu * 1024llu * 1024llu, "PiB" },
                SizeUnit { 1024llu * 1024llu * 1024llu * 1024llu, "TiB" },
                SizeUnit { 1024llu * 1024llu * 1024llu, "GiB" },
                SizeUnit { 1024llu * 1024llu, "MiB" },
                SizeUnit { 1024llu, "kiB" },
                SizeUnit { 1, "B" }
        };

        for (const auto& sizeUnit : unitsDescending) {
            if(size >= sizeUnit.smallest) {
                return Carrot::sprintf("%.5f %s", size / (double)sizeUnit.smallest, sizeUnit.unit.c_str());
            }
        }

        return Carrot::sprintf("%llu B", size);
    }
}