/**
 * @file FilterTimeFrameSliceByTracking.cxx
 * @brief Slice TimeFrame by Time of Flight for NestDAQ
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-20 01:26:30 JST
 * 
 * author Fumiya Furukawa <fumiya@rcnp.osaka-u.ac.jp>
 * @comment Modify FilterTimeFrameSliceBySomething.cxx for Tracking without Drift Timing 
 */
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <tuple>
#include <numeric>
#include <cmath>
#include <map>
#include <chrono>
#include "FilterTimeFrameSliceByTracking.h"
#include "FilterTimeFrameSliceABC.icxx"
#include "fairmq/runDevice.h"
#include "utility/MessageUtil.h"
#include "UnpackTdc.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"

#define DEBUG 1
#define OUTPUT 1

using nestdaq::FilterTimeFrameSliceByTracking;
namespace bpo = boost::program_options;

FilterTimeFrameSliceByTracking::FilterTimeFrameSliceByTracking()
: minClusterSize(3), minClusterCountPerPlane(1), fNPlane(4), fNParameter(4), fSSRMax(1e+1),
  totalCalls(0), totalAccepted(0), fSSRThreshold(1.0)  // Initialize fSSRThreshold with default value
{
    // Input the geometry of the detector. 
    // First from left, second from right...
    fCos = {1.0, 0.0, -1.0, 0.0};
    fSin = {0.0, 1.0, 0.0, -1.0};
    fZ = {1.0, 2.0, 3.0, 4.0};
}

bool FilterTimeFrameSliceByTracking::ProcessSlice(TTF& tf)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    size_t total_size = 0;

    GeoIDs.clear();
    int foundID, foundGeo, Geofield;

    auto tfHeader = tf.GetHeader();
    std::cout << "TimeFrame" << std::endl;
    for (auto& SubTimeFrame : tf) {
        std::cout << "SubTimeFrame" << std::endl;
        auto header = SubTimeFrame->GetHeader();
        auto& hbf = SubTimeFrame->at(0);
        uint64_t nData = hbf->GetNumData();
        total_size += nData * sizeof(hbf->UncheckedAt(0));

        for (int i = 0; i < nData; ++i) {
            if (header->femType == SubTimeFrame::TDC64H) {
                TDC64H::tdc64 tdc;
                TDC64H::Unpack(hbf->UncheckedAt(i), &tdc);
            } else if (header->femType == SubTimeFrame::TDC64L) {
                TDC64L::tdc64 tdc;
                TDC64L::Unpack(hbf->UncheckedAt(i), &tdc);
            } else if (header->femType == SubTimeFrame::TDC64H_V3) {
                TDC64H_V3::tdc64 tdc;
                TDC64H_V3::Unpack(hbf->UncheckedAt(i), &tdc);
            } else if (header->femType == SubTimeFrame::TDC64L_V3) {
                TDC64L_V3::tdc64 tdc;
                TDC64L_V3::Unpack(hbf->UncheckedAt(i), &tdc);
                // Multiplicity preprocessing step 1 (search for wire IDs)
                if (findWirenumber(wireMapArray, header->femId, tdc.ch, &foundID, &foundGeo, Geofield)) {
                    GeoIDs[Geofield].emplace_back(foundID, tdc.tot);  // Store wireID and charge
#if DEBUG
                    std::cout << "GeoID: " << Geofield << " WireID: " << foundID << " Charge: " << tdc.tot << std::endl;
#endif
                }
            }            
        }
    }
    // End of TimeFrame

    // Multiplicity preprocessing step 2 (sort IDs for each VDC plane)
    for (auto& pair : GeoIDs) {
        std::sort(pair.second.begin(), pair.second.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return a.first < b.first;
        });
    }

    auto geofieldClusterMap = analyzeGeofieldClusters(GeoIDs);

    totalCalls++; // Increment total calls

    auto end_time = std::chrono::high_resolution_clock::now();
    auto processing_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

#if OUTPUT
    // Output processing performance to file
    std::ofstream performance_file("filter_performance_tot.txt", std::ios_base::app);
    if (performance_file.is_open()) {
        performance_file << "ProcessSlice processed " << total_size << " bytes in " << processing_time.count() << " µs" << std::endl;
        performance_file << "ratio: " << static_cast<double>(total_size) / processing_time.count() << std::endl;
        performance_file.close();
    }
#endif

    bool result = allKeysHaveAtLeastOneCluster(geofieldClusterMap);
    if (result) {
        std::cout << "start to tracking" << std::endl;
        bool trackingSuccess = PerformTracking(geofieldClusterMap);  // Track clusters based on centroid and other parameters
        if (trackingSuccess) {
            totalAccepted++; // Increment total accepted
            return true;
        }
    }

#if OUTPUT
    // Calculate and output reduction rate every 100 calls
    if (totalCalls % 100 == 0) {
        double reductionRate = 100.0 * (1.0 - static_cast<double>(totalAccepted) / static_cast<double>(totalCalls));

        std::ofstream reduction_file("data_reduction_rate_tot.txt", std::ios_base::app);
        if (reduction_file.is_open()) {
            reduction_file << "Data reduction rate: " << reductionRate << "% (Accepted: " << totalAccepted << ", Total: " << totalCalls << ")" << std::endl;
            reduction_file.close();
        }
    }
#endif

    return false;
}

int FilterTimeFrameSliceByTracking::findWirenumber(const std::array<std::array<Wire_map, maxCh + 1>, 8>& wireMapArray, uint64_t geo, int ch, int *foundid, int *foundGeo, int &Geofield) {
    int geoIndex = geoToIndex(geo);
    if (geoIndex != -1 && ch < wireMapArray[geoIndex].size()) {
        const Wire_map& wire = wireMapArray[geoIndex][ch];
        if (wire.catid != -1) {
            *foundid = wire.id;
            *foundGeo = wire.geo;

            if ((geo == 0xc0a802a1) || (geo == 0xc0a802a2)) {
                Geofield = 1;
            } else if ((geo == 0xc0a802a3) || (geo == 0xc0a802a4)) {
                Geofield = 2;
            } else if ((geo == 0xc0a802a5) || (geo == 0xc0a802a6)) {
                Geofield = 3;
            } else if ((geo == 0xc0a802a7) || (geo == 0xc0a802a8)) {
                Geofield = 4;
            }

            return 1;
        }
    }
    return 0;
}

std::vector<std::vector<std::pair<int, int>>> FilterTimeFrameSliceByTracking::clusterNumbers(const std::vector<std::pair<int, int>>& numbers) {
    std::vector<std::vector<std::pair<int, int>>> clusters;
    std::vector<std::pair<int, int>> currentCluster;

    for (const auto& number : numbers) {
        if (currentCluster.empty() || number.first == currentCluster.back().first + 1) {
            currentCluster.push_back(number);
        } else {
            if (currentCluster.size() >= minClusterSize) {
                clusters.push_back(currentCluster);
            }
            currentCluster.clear();
            currentCluster.push_back(number);
        }
    }

    if (currentCluster.size() >= minClusterSize) {
        clusters.push_back(currentCluster);
    }

    return clusters;
}

std::map<int, FilterTimeFrameSliceByTracking::GeofieldClusterInfo> FilterTimeFrameSliceByTracking::analyzeGeofieldClusters(const std::map<int, std::vector<std::pair<int, int>>>& GeoIDs) {
    std::map<int, GeofieldClusterInfo> geofieldClusterMap;

    for (int geofieldID = 1; geofieldID <= 4; ++geofieldID) {
        geofieldClusterMap[geofieldID] = GeofieldClusterInfo{0, std::vector<ClusterInfo>(), std::vector<int>()};
    }

    for (const auto& pair : GeoIDs) {
        int geofieldID = pair.first;
        const std::vector<std::pair<int, int>>& wireIDs = pair.second;
        std::vector<std::vector<std::pair<int, int>>> clusters = clusterNumbers(wireIDs);

        GeofieldClusterInfo& info = geofieldClusterMap[geofieldID];
        info.clusterCount = clusters.size();
        int clusterIDCounter = 0;  
        for (const auto& cluster : clusters) {
            ClusterInfo clusterInfo;
            clusterInfo.clusterID = clusterIDCounter++;
            clusterInfo.size = cluster.size();
            clusterInfo.wires = cluster;

            // VDC wire ID and charge weighted average
            int chargeSum = 0;
            for (const auto& wire : cluster) {
                chargeSum += wire.second;
            }
            clusterInfo.chargeSum = chargeSum;

            // calculate cluster 
            clusterInfo.centroid = calculateCentroid(cluster);

            info.clusters.push_back(clusterInfo); 

#if DEBUG
            std::cout << "Geofield: " << geofieldID << " ClusterID: " << clusterInfo.clusterID 
            << " Size: " << clusterInfo.size << " ChargeSum: " << clusterInfo.chargeSum 
            << " Centroid: " << clusterInfo.centroid << std::endl;
#endif
            info.clusterSizes.push_back(cluster.size()); 
        }
    }

    return geofieldClusterMap;
}

bool FilterTimeFrameSliceByTracking::allKeysHaveAtLeastOneCluster(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap) {
    for (const auto& pair : geofieldClusterMap) {
        if (pair.second.clusterCount < minClusterCountPerPlane) {
            return false;
        }
    }
    return true;
}

double FilterTimeFrameSliceByTracking::calculateCentroid(const std::vector<std::pair<int, int>>& cluster) {
    double weightedSum = 0.0;
    double totalCharge = 0.0;

    for (const auto& wire : cluster) {
        int wireID = wire.first;
        int charge = wire.second;
        weightedSum += wireID * charge;
        totalCharge += charge;
    }

    if (totalCharge == 0) return 0;  // Avoid division by zero
    return weightedSum / totalCharge;
}

void FilterTimeFrameSliceByTracking::GenerateCombinations(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap, int trackCount, std::vector<std::vector<std::pair<int, int>>>& combinations) {
    combinations.clear();
    std::vector<std::pair<int, int>> currentCombination(fNPlane);

    std::function<void(int)> generate = [&](int planeIndex) {
        if (planeIndex == fNPlane) {
            combinations.push_back(currentCombination);
            return;
        }

        const GeofieldClusterInfo& info = geofieldClusterMap.at(planeIndex + 1);
        for (int i = 0; i < trackCount; ++i) {
            if (i < info.clusters.size()) {
                currentCombination[planeIndex] = info.clusters[i].wires[0]; // Use the first wire in the cluster as a representative
            } else {
                currentCombination[planeIndex] = {0, 0}; // Fallback to the first cluster if out of bounds
            }
            generate(planeIndex + 1);
        }
    };

    generate(0);
}

void FilterTimeFrameSliceByTracking::CalculateTrackParams(const std::vector<std::pair<int, int>>& combination, std::vector<double>& trackParams) {
    std::vector<double> pos(fNPlane);
    for (int iPlane = 0; iPlane < fNPlane; ++iPlane) {
        const double cos = fCos[iPlane];
        const double sin = fSin[iPlane];
        const double z = fZ[iPlane];
        const auto& wire = combination[iPlane];
        const double wireID = wire.first;
        const double dl = wire.second;
        pos[iPlane] = wireID + dl;
    }

    for (int iParam = 0; iParam < fNParameter; ++iParam) {
        trackParams[iParam] = std::accumulate(pos.begin(), pos.end(), 0.0) / pos.size();
    }
}

double FilterTimeFrameSliceByTracking::CalculateSSR(const std::vector<std::pair<int, int>>& combination, const std::vector<double>& trackParams) {
    double SSR = 0.0;
    for (int iPlane = 0; iPlane < fNPlane; ++iPlane) {
        const double cos = fCos[iPlane];
        const double sin = fSin[iPlane];
        const double z = fZ[iPlane];
        const double trackedPos = (fNParameter == 2) ?
            cos * trackParams[0] + sin * trackParams[1] :
            cos * (trackParams[0] + trackParams[2] * z) + sin * (trackParams[1] + trackParams[3] * z);
        const double residual = combination[iPlane].first - trackedPos;
        SSR += residual * residual;
        if (SSR > fSSRMax) return fSSRMax;
    }
    return SSR;
}

bool FilterTimeFrameSliceByTracking::PerformTracking(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap) {
    std::vector<std::vector<std::pair<int, int>>> combinations;
    int minTrackCount = std::numeric_limits<int>::max();

    // Determine the minimum number of tracks based on the number of clusters on each surface
    for (const auto& pair : geofieldClusterMap) {
        minTrackCount = std::min(minTrackCount, pair.second.clusterCount);
    }

    //  Adopt minimum number of clusters as number of tracks
    int trackCount = minTrackCount;

    GenerateCombinations(geofieldClusterMap, trackCount, combinations);

    double minSSR = fSSRMax;
    std::vector<double> bestTrackParams(fNParameter, 0.0);
    bool success = false;  // Add a flag to indicate successful tracking

    for (const auto& combination : combinations) {
        std::vector<double> trackParams(fNParameter);
        CalculateTrackParams(combination, trackParams);

        double SSR = CalculateSSR(combination, trackParams);

#if DEBUG
        std::cout << "Combination: ";
        for (const auto& wire : combination) {
            std::cout << "(" << wire.first << ", " << wire.second << ") ";
        }
        std::cout << "SSR: " << SSR << std::endl;
#endif

        if (SSR < minSSR) {
            minSSR = SSR;
            bestTrackParams = trackParams;
        }

        if (SSR <= fSSRThreshold) {  // Compare with the threshold
            success = true;
        }
    }

#if DEBUG
    std::cout << "Best Track Params: ";
    for (const auto& param : bestTrackParams) {
        std::cout << param << " ";
    }
    std::cout << "with SSR: " << minSSR << std::endl;
#endif

    return success;  // Return the success flag
}

void addCustomOptions(bpo::options_description& options)
{
    using opt = FilterTimeFrameSliceByTracking::OptionKey;

    options.add_options()
        (opt::InputChannelName.data(),
         bpo::value<std::string>()->default_value("in"),
         "Name of the input channel")
        (opt::OutputChannelName.data(),
         bpo::value<std::string>()->default_value("out"),
         "Name of the output channel")
        (opt::DQMChannelName.data(),
         bpo::value<std::string>()->default_value("dqm"),
         "Name of the data quality monitoring channel")
        (opt::PollTimeout.data(),
         bpo::value<std::string>()->default_value("1"),
         "Timeout of polling (in msec)")
        (opt::SplitMethod.data(),
         bpo::value<std::string>()->default_value("1"),
         "STF split method")
        ("ssr-threshold", bpo::value<double>()->default_value(1.0), "SSR threshold for tracking");  // Add this line
}

std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<FilterTimeFrameSliceByTracking>();
}
