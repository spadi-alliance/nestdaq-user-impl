/**
 * @file FilterTimeFrameSliceByTracking.h
 * @brief Header file for FilterTimeFrameSliceByTracking class
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-20 01:26:30 JST
 * 
 * author Fumiya Furukawa <fumiya@rcnp.osaka-u.ac.jp>
 * @comment Modify FilterTimeFrameSliceBySomething.h for Tracking
 */
#ifndef NESTDAQ_TIMEFRAMESLICERBYTRACKING_H
#define NESTDAQ_TIMEFRAMESLICERBYTRACKING_H

#include "fairmq/Device.h"
#include "FilterTimeFrameSliceABC.h"
#include <vector>
#include <memory>
#include <map>
#include <array>
#include <tuple>

namespace nestdaq {
   class FilterTimeFrameSliceByTracking;
}

class nestdaq::FilterTimeFrameSliceByTracking : public nestdaq::FilterTimeFrameSliceABC {
public:
   FilterTimeFrameSliceByTracking();
   virtual ~FilterTimeFrameSliceByTracking() override = default;

   virtual bool ProcessSlice(TTF& ) override;

   void SetMinClusterSize(int size) { minClusterSize = size; }
   void SetMinClusterCountPerPlane(int count) { minClusterCountPerPlane = count; }
   void SetSSRThreshold(double threshold) { fSSRThreshold = threshold; }  // Add this line

private:
   struct ClusterInfo {
       int clusterID;
       int size;
       int chargeSum;
       double centroid;
       std::vector<std::pair<int, int>> wires;  // Store wireID and charge
   };

   struct GeofieldClusterInfo {
       int clusterCount;
       std::vector<ClusterInfo> clusters;
       std::vector<int> clusterSizes;  // Number of cluster elements
   };

   int findWirenumber(const std::array<std::array<Wire_map, maxCh + 1>, 8>& wireMapArray, uint64_t geo, int ch, int *foundid, int *foundGeo, int &Geofield);
   std::vector<std::vector<std::pair<int, int>>> clusterNumbers(const std::vector<std::pair<int, int>>& numbers);
   std::map<int, GeofieldClusterInfo> analyzeGeofieldClusters(const std::map<int, std::vector<std::pair<int, int>>>& GeoIDs);
   bool allKeysHaveAtLeastOneCluster(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap);
   double calculateCentroid(const std::vector<std::pair<int, int>>& cluster);
   bool PerformTracking(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap);
   
   void GenerateCombinations(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap, int trackCount, std::vector<std::vector<std::pair<int, int>>>& combinations);
   void CalculateTrackParams(const std::vector<std::pair<int, int>>& combination, std::vector<double>& trackParams);
   double CalculateSSR(const std::vector<std::pair<int, int>>& combination, const std::vector<double>& trackParams);

   std::array<double, 4> fCos;
   std::array<double, 4> fSin;
   std::array<double, 4> fZ;

   std::map<int, std::vector<std::pair<int, int>>> GeoIDs;

   int minClusterSize;
   int minClusterCountPerPlane;
   int fNPlane;
   int fNParameter;
   double fSSRMax;
   double fSSRThreshold;  // Add this line

   // For performance and reduction rate
   int totalCalls;
   int totalAccepted;
};

#endif  // NESTDAQ_TIMEFRAMESLICERBYTRACKING_H
