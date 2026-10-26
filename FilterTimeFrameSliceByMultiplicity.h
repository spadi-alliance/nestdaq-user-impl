/**
 * @file FilterTimeFrameSliceByMultiplicity.h 
 * @brief Header file for FilterTimeFrameSliceByMultiplicity class
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-20 01:26:30 JST
 * 
 * author Fumiya Furukawa <fumiya@rcnp.osaka-u.ac.jp>
 * @comment Modify FilterTimeFrameSliceBySomething.h for Multiplicity
 */
#ifndef NESTDAQ_TIMEFRAMESLICERBYMULTIPLICITY_H
#define NESTDAQ_TIMEFRAMESLICERBYMULTIPLICITY_H

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
#include <array>

namespace nestdaq {
   class FilterTimeFrameSliceByMultiplicity;
}

class nestdaq::FilterTimeFrameSliceByMultiplicity : public nestdaq::FilterTimeFrameSliceABC {
public:
   FilterTimeFrameSliceByMultiplicity();
   virtual ~FilterTimeFrameSliceByMultiplicity() override = default;

   virtual bool ProcessSlice(TTF& ) override;

   void SetMinClusterSize(int size) { minClusterSize = size; }
   void SetMinClusterCountPerPlane(int count) { minClusterCountPerPlane = count; }

private:
   struct GeofieldClusterInfo {
       int clusterCount;
       std::vector<int> clusterSizes;
   };

   int findWirenumber(const std::array<std::array<Wire_map, maxCh + 1>, 8>& wireMapArray, uint64_t geo, int ch, int *foundid, int *foundGeo, int &Geofield);
   std::vector<std::vector<int>> clusterNumbers(const std::vector<int>& numbers);
   std::map<int, GeofieldClusterInfo> analyzeGeofieldClusters(const std::map<int, std::vector<int>>& GeoIDs);
   bool allKeysHaveAtLeastOneCluster(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap);

   std::map<int, std::vector<int>> GeoIDs;

   int minClusterSize;
   int minClusterCountPerPlane;
   int totalCalls;
   int totalAccepted;
};

#endif  // NESTDAQ_TIMEFRAMESLICERBYMULTIPLICITY_H
