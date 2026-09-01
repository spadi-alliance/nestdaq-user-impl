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
#include "FilterTimeFrameSliceByTracking.h"
#include "FilterTimeFrameSliceABC.icxx"
#include "fairmq/runDevice.h"
#include "utility/MessageUtil.h"
#include "UnpackTdc.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"

#define DEBUG 1

using nestdaq::FilterTimeFrameSliceByTracking;
namespace bpo = boost::program_options;

FilterTimeFrameSliceByTracking::FilterTimeFrameSliceByTracking()
: minClusterSize(3), minClusterCountPerPlane(1)
{
    // 初期化コード
    fCos = {1.0, 0.0, -1.0, 0.0};
    fSin = {0.0, 1.0, 0.0, -1.0};
    fZ = {1.0, 2.0, 3.0, 4.0};  // 適切な値に設定
}

bool FilterTimeFrameSliceByTracking::ProcessSlice(TTF& tf)
{
    GeoIDs.clear();
    int foundID, foundGeo, Geofield;

    auto tfHeader = tf.GetHeader();
    std::cout << "TimeFrame" << std::endl;
    for (auto& SubTimeFrame : tf) {
        std::cout << "SubTimeFrame" << std::endl;
        auto header = SubTimeFrame->GetHeader();
        auto& hbf = SubTimeFrame->at(0);
        uint64_t nData = hbf->GetNumData();

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

    if(allKeysHaveAtLeastOneCluster(geofieldClusterMap)){
        std::cout << "start to tracking" << std::endl;
        PerformTracking(geofieldClusterMap);  // Track clusters based on centroid and other parameters
        return true;
    }

    return false;
} // End of main

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
            } else if ((geo == 0xc0a802a7) || (geo == 0xc0a802aa)) {
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
        int clusterIDCounter = 0;  // クラスターIDカウンタをリセット
        for (const auto& cluster : clusters) {
            ClusterInfo clusterInfo;
            clusterInfo.clusterID = clusterIDCounter++;
            clusterInfo.size = cluster.size();
            clusterInfo.wires = cluster;

            // クラスター内の電荷量の合計を計算
            int chargeSum = 0;
            for (const auto& wire : cluster) {
                chargeSum += wire.second;
            }
            clusterInfo.chargeSum = chargeSum;  // 電荷量の合計を設定

            // クラスターの重心を計算
            clusterInfo.centroid = calculateCentroid(cluster);

            info.clusters.push_back(clusterInfo);  // クラスター情報を追加

#if DEBUG
            std::cout << "Geofield: " << geofieldID << " ClusterID: " << clusterInfo.clusterID 
            << " Size: " << clusterInfo.size << " ChargeSum: " << clusterInfo.chargeSum 
            << " Centroid: " << clusterInfo.centroid << std::endl;
#endif
            info.clusterSizes.push_back(cluster.size()); // クラスターのサイズを追加
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

void FilterTimeFrameSliceByTracking::PerformTracking(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap) {
    // トラッキング計算のための行列計算
    for (const auto& pair : geofieldClusterMap) {
        int geofieldID = pair.first;
        const GeofieldClusterInfo& info = pair.second;

        for (const auto& cluster : info.clusters) {
            std::vector<double> pos(3, 0.0);  // 位置の初期化 [x, y, z]

            for (const auto& wire : cluster.wires) {
                int wireID = wire.first;
                int charge = wire.second;

                pos[0] += fCos[geofieldID - 1] * wireID * charge;  // x座標
                pos[1] += fSin[geofieldID - 1] * wireID * charge;  // y座標
                pos[2] += fZ[geofieldID - 1] * charge;             // z座標
            }

            double totalCharge = static_cast<double>(cluster.chargeSum);

            pos[0] /= totalCharge;
            pos[1] /= totalCharge;
            pos[2] /= totalCharge;

#if DEBUG
            std::cout << "Tracking results for GeoField " << geofieldID << ": ";
            std::cout << "x = " << pos[0] << ", y = " << pos[1] << ", z = " << pos[2] << std::endl;
#endif
        }
    }
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
         "STF split method");
}

std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<FilterTimeFrameSliceByTracking>();
}
