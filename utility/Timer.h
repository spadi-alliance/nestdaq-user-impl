#ifndef NESTDAQ_UTILITY_TIMER_H_
#define NESTDAQ_UTILITY_TIMER_H_

#include <functional>
#include <memory>
#include <boost/asio.hpp>

namespace nestdaq {
namespace net = boost::asio;
using strand_t = net::strand<net::io_context::executor_type>;

class Timer {
public:
   Timer() = default;
   ~Timer();

   void Start(const std::shared_ptr<net::io_context> &ctx, const std::shared_ptr<strand_t> &strand,
              unsigned int timeoutMS, std::function<bool(const std::error_code &)> f);

protected:
   void Start();

   std::shared_ptr<net::io_context> fContext;
   std::shared_ptr<strand_t> fStrand;
   std::unique_ptr<net::steady_timer> fTimer;
   unsigned int fTimeoutMS{0};
   std::function<bool(const std::error_code &)> fHandle;
};

} // namespace nestdaq

#endif // NESTDAQ_UTILITY_TIMER_H_
