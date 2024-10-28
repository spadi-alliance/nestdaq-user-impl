/**
 * @file FilterTimeFrameSliceByTOT.h 
 * @brief Class for filtering time frame slice by Time Over Threshold (TOT)
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-23 11:09:34 JST
 *
 * author Fumiya Furukawa <fumiya@rcnp.osaka-u.ac.jp>
 * @comment Add Reduction-rate, Throughput per unit time
 */

#ifndef NESTDAQ_TIMEFRAMESLICERBYTOT_H
#define NESTDAQ_TIMEFRAMESLICERBYTOT_H

#include "fairmq/Device.h"
#include "KTimer.cxx"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "HeartbeatFrameHeader.h"
#include "FrameContainer.h"
#include "FilterTimeFrameSliceABC.h"
#include <vector>
#include <memory>
#include <map>

namespace nestdaq {
   class FilterTimeFrameSliceByTOT;
}

class nestdaq::FilterTimeFrameSliceByTOT : public nestdaq::FilterTimeFrameSliceABC {
public:
   FilterTimeFrameSliceByTOT();
   virtual ~FilterTimeFrameSliceByTOT() override = default;

   virtual bool ProcessSlice(TTF& ) override;

private:
   int totalCalls;
   int totalAccepted;
   int DeterminePlane(uint64_t fem, int ch);
   bool Chargelogic(const std::map<int, std::tuple<int, int>>& chargeSums);
};

#endif  // NESTDAQ_TIMEFRAMESLICERBYTOT_H
