/**
 * @file FilterTimeFrameSliceByTOF.h 
 * @brief Abstract base class for filtering time frame slice
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-15 00:34:28 JST
 * 
 * @author Fumiya Furukawa <fumiya@rcnp.osaka-u.ac.jp>
 * @comment Modify FilterTimeFrameSliceBySomething.h for MultiHit 
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
   class FilterTimeFrameSliceByTOF;
}


class nestdaq::FilterTimeFrameSliceByTOF : public nestdaq::FilterTimeFrameSliceABC {
public:
   FilterTimeFrameSliceByTOF();
   virtual ~FilterTimeFrameSliceByTOF() override = default;

   virtual bool ProcessSlice(TTF& ) override;

   void AddTOFHit(uint64_t femId, int ch, int tdc);
   std::vector<std::unique_ptr<int>> CalculateAveragePairs(const std::vector<std::unique_ptr<int>>& r_hits, const std::vector<std::unique_ptr<int>>& l_hits);
   void CalculateAndPrintTOF(const std::vector<std::unique_ptr<int>>& tof_end_averages, const std::vector<std::unique_ptr<int>>& tof_start_averages);
   void PrintList(const std::vector<std::unique_ptr<int>>& list, const std::string& listName);
   bool CheckAllTOFConditions(int tof_start_ave, int tof_end_ave, const std::vector<std::unique_ptr<int>>& tof_start_r_hits, const std::vector<std::unique_ptr<int>>& tof_start_l_hits,
                              const std::vector<std::unique_ptr<int>>& tof_end_r_hits, const std::vector<std::unique_ptr<int>>& tof_end_l_hits, int min, int max);

private:
   std::vector<std::unique_ptr<int>> tof_start_r_hits;
   std::vector<std::unique_ptr<int>> tof_start_l_hits;
   std::vector<std::unique_ptr<int>> tof_end_r_hits;
   std::vector<std::unique_ptr<int>> tof_end_l_hits;

   // For performance and reduction rate
   int totalCalls;
   int totalAccepted;
};

#endif  // NESTDAQ_TIMEFRAMESLICERBYLOGICTIMING_H
