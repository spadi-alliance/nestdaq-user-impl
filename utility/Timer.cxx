#include <chrono>
#include <iostream>

#include "utility/Timer.h"

//______________________________________________________________________________
nestdaq::Timer::~Timer()
{
    fTimer->cancel();
}

//______________________________________________________________________________
void nestdaq::Timer::Start(const std::shared_ptr<net::io_context> &ctx, const std::shared_ptr<strand_t> &strand,
                           unsigned int timeoutMS, std::function<bool(const std::error_code &)> f)
{
    // std::cout << " timer start " << timeoutMS << std::endl;
    fContext = ctx;
    fStrand = strand;
    fTimer = std::make_unique<net::steady_timer>(*fContext);
    fTimeoutMS = timeoutMS;
    fHandle = f;
    Start();
}

//______________________________________________________________________________
void nestdaq::Timer::Start()
{
    fTimer->expires_after(std::chrono::milliseconds(fTimeoutMS));
    fTimer->async_wait( // net::bind_executor(*fStrand,
    [this](const auto &ec) {
        if (ec == std::errc::operation_canceled) {
            std::cout << " timer canceled : " << ec.message() << std::endl;
        } else if (ec) {
            std::cout << " error. timer stopped: " << ec.message() << std::endl;
        } else {
            if (!fHandle(ec)) {
                // std::cout << " restart timer" << std::endl;
                fTimer->cancel();
                Start();
            }
        }
        //}));
    });
}
