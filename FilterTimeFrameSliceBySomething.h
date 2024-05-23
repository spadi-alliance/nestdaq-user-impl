/**
 * @file FilterTimeFrameSliceBySomething.h
 * @brief Abstract base class for filtering time frame slice
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-05-23 07:16:28 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */


#ifndef NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H
#define NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H

#include "fairmq/Device.h"
#include "KTimer.cxx"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "HeartbeatFrameHeader.h"
#include "FrameContainer.h"
#include "FilterTimeFrameSliceABC.h"

namespace nestdaq {
   class FilterTimeFrameSliceBySomething;
}


class nestdaq::FilterTimeFrameSliceBySomething : public nestdaq::FilterTimeFrameSliceABC {
public:
   FilterTimeFrameSliceBySomething();
   virtual ~FilterTimeFrameSliceBySomething() override = default;

   virtual bool ProcessSlice(TTF& ) override;

};


#endif  // NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H
