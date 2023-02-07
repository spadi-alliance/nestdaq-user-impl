#ifndef NESTDAQ_UTILITY_SCOPE_GUARD_H_
#define NESTDAQ_UTILITY_SCOPE_GUARD_H_

#include <functional>

namespace nestdaq {
class scope_guard {
public:
   explicit scope_guard(std::function<void()> f) : f(f) {}
   scope_guard(const scope_guard &) = delete;
   scope_guard &operator=(const scope_guard &) = delete;
   ~scope_guard() { f(); }

private:
   std::function<void()> f;
};

} // namespace nestdaq

#endif // NESTDAQ_UTILITY_SCOPE_GUARD_H_
