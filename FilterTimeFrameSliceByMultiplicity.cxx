/**
 * @file FilterTimeFrameSliceByMultiplicity.cxx
 * @brief Slice TimeFrame by Multiplicity for NestDAQ
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-21 02:08:24 JST
 * 
 * author Fumiya Furukawa <fumiya@rcnp.osaka-u.ac.jp>
 * @comment Modify FilterTimeFrameSliceBySomething.cxx for Multiplicity
 * 
 */
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include "FilterTimeFrameSliceByMultiplicity.h"
#include "FilterTimeFrameSliceABC.icxx"
#include "fairmq/runDevice.h"
#include "utility/MessageUtil.h"
#include "UnpackTdc.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"

#define DEBUG 0

using nestdaq::FilterTimeFrameSliceByMultiplicity;
namespace bpo = boost::program_options;

FilterTimeFrameSliceByMultiplicity::FilterTimeFrameSliceByMultiplicity()
// change here !
: minClusterSize(3),           // Number of elements in clustere      
minClusterCountPerPlane(1)     // Minimum number of clusters in each plan
{
}

bool FilterTimeFrameSliceByMultiplicity::ProcessSlice(TTF& tf)
{
    // Initializing Multiplicity 
    GeoIDs.clear();
    int foundID, foundGeo, Geofield;

    auto tfHeader = tf.GetHeader();

    for (auto& SubTimeFrame : tf) {
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
                    //std::cout << "after searching wire ID" << std::endl;
                    GeoIDs[Geofield].push_back(foundID);
                }
            }            
        }
    }
    // End of TimeFrame

    // Multiplicity preprocessing step 2 (sort IDs for each VDC plane)
    for (auto& pair : GeoIDs) {
        std::sort(pair.second.begin(), pair.second.end());
    }

    // Display of sorted results
    #if DEBUG
        for (const auto& pair : GeoIDs) {
            std::cout << "Geofield: " << pair.first << " - GeoIDs: ";
            for (int geoID : pair.second) {
                std::cout << geoID << " ";
            }
            std::cout << std::endl;
            std::cout << "completely sorted" << std::endl;
        }
    #endif  

    auto geofieldClusterMap = analyzeGeofieldClusters(GeoIDs);

    if(allKeysHaveAtLeastOneCluster(geofieldClusterMap)){
        return true;        
    }

    return false;
}

int FilterTimeFrameSliceByMultiplicity::findWirenumber(const std::array<std::array<Wire_map, maxCh + 1>, 8>& wireMapArray, uint64_t geo, int ch, int *foundid, int *foundGeo, int &Geofield) {
    int geoIndex = geoToIndex(geo);
    #if DEBUG
        std::cout << "[findWirenumber] geoToIndex: geo=" << std::hex << geo << " index=" << geoIndex << std::dec << std::endl;
    #endif

    if (geoIndex == -1) {
        #if DEBUG
            std::cout << "[findWirenumber] Invalid geo value: " << std::hex << geo << std::dec << std::endl;
        #endif

        *foundGeo = -1;
        return 0;
    }

    if (ch >= wireMapArray[geoIndex].size()) {
        #if DEBUG
            std::cout << "[findWirenumber] Invalid channel: " << ch << " for geoIndex: " << geoIndex << std::endl;
        #endif

        *foundGeo = -1;
        return 0;
    }

    const Wire_map& wire = wireMapArray[geoIndex][ch];
    #if DEBUG
        std::cout << "[findWirenumber] Checking Wire_map for geoIndex: " << geoIndex << ", channel: " << ch << std::endl;
        std::cout << "[findWirenumber] Wire_map details - catid: " << wire.catid << ", id: " << wire.id << ", geo: " << std::hex << wire.geo << std::dec << std::endl;
    #endif

    if (wire.catid != -1) {
        *foundid = wire.id;
        *foundGeo = wire.geo;

        if ((geo == 0xc0a802a1) || (geo == 0xc0a802a2)) {
            Geofield = 1; // plane1
        } else if ((geo == 0xc0a802a3) || (geo == 0xc0a802a4)) {
            Geofield = 2; // plane2
        } else if ((geo == 0xc0a802a5) || (geo == 0xc0a802a6)) {
            Geofield = 3; // plane3
        } else if ((geo == 0xc0a802a7) || (geo == 0xc0a802a8)) {
            Geofield = 4; // plane4
        }
        #if DEBUG
        std::cout << "[findWirenumber] Found wire ID: " << *foundid << ", foundGeo: " << std::hex << *foundGeo << ", Geofield: " << Geofield << std::dec << std::endl;
        #endif

        return 1;
    } else {
        #if DEBUG
            std::cout << "[findWirenumber] Wire_map catid is -1 for geoIndex: " << geoIndex << ", channel: " << ch << std::endl;
        #endif

        *foundGeo = -1;
    }
    
    return 0;
}

std::vector<std::vector<int>> FilterTimeFrameSliceByMultiplicity::clusterNumbers(const std::vector<int>& numbers) {
    std::vector<std::vector<int>> clusters;
    std::vector<int> currentCluster;

    for (int number : numbers) {
        if (currentCluster.empty() || number == currentCluster.back() + 1) {
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

std::map<int, FilterTimeFrameSliceByMultiplicity::GeofieldClusterInfo> FilterTimeFrameSliceByMultiplicity::analyzeGeofieldClusters(const std::map<int, std::vector<int>>& GeoIDs) {
    std::map<int, GeofieldClusterInfo> geofieldClusterMap;

    for (int geofieldID = 1; geofieldID <= 4; ++geofieldID) {
        geofieldClusterMap[geofieldID] = GeofieldClusterInfo{0, std::vector<int>()};
    }

    for (const auto& pair : GeoIDs) {
        int geofieldID = pair.first;
        const std::vector<int>& wireIDs = pair.second;
        std::vector<std::vector<int>> clusters = clusterNumbers(wireIDs);

        GeofieldClusterInfo& info = geofieldClusterMap[geofieldID];
        info.clusterCount = clusters.size();
        for (const auto& cluster : clusters) {
            info.clusterSizes.push_back(cluster.size());
        }
    }
    #if DEBUG
        for (const auto& pair : geofieldClusterMap) {
            std::cout << "GeoID: " << pair.first << " には " << pair.second.clusterCount << " 個のクラスターがあります。" << std::endl;
            std::cout << "各クラスターの要素数は以下の通りです：" << std::endl;
            for (int size : pair.second.clusterSizes) {
                std::cout << " - クラスターの要素数: " << size << std::endl;
            }
        } 
    #endif
    return geofieldClusterMap;
}

bool FilterTimeFrameSliceByMultiplicity::allKeysHaveAtLeastOneCluster(const std::map<int, GeofieldClusterInfo>& geofieldClusterMap) {
    for (const auto& pair : geofieldClusterMap) {
        if (pair.second.clusterCount < minClusterCountPerPlane) {
            return false;
        }
    }
    return true;
}

void addCustomOptions(bpo::options_description& options)
{
    using opt = FilterTimeFrameSliceByMultiplicity::OptionKey;

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
    return std::make_unique<FilterTimeFrameSliceByMultiplicity>();
}
