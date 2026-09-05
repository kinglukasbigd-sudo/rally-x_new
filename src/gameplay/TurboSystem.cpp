#include "gameplay/TurboSystem.h"

namespace rx {
namespace TurboRules {

int countForLevel(int level) {
    if (level <= 4)  return 0;
    if (level <= 7)  return 2;
    if (level <= 10) return 3;
    return 5;
}

} // namespace TurboRules
} // namespace rx
