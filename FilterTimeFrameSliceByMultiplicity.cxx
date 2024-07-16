/**
 * @file FilterTimeFrameSliceByMultiplicity.cxx
 * @brief Slice TimeFrame by Time of Flight for NestDAQ
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-15 01:13:54 JST
 * 
 * author Fumiya Furukawa <fumiya@rcnp.osaka-u.ac.jp>
 * @comment Modify FilterTimeFrameSliceBySomething.cxx for MultiHit 
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
: minClusterSize(3),                        // Number of elements in clustere      
minClusterCountPerPlane(1),                 // Minimum number of clusters in each plan
wireMapSize(113),                           // Maximum number of wires on each plane                  
wireMapArray(8, std::vector<Wire_map>(113)) // 8 amaneq  
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
    
    auto geofieldClusterMap = analyzeGeofieldClusters(GeoIDs);

    if(allKeysHaveAtLeastOneCluster(geofieldClusterMap)){
        return true;        
    }

    return false;
} // End of main

int FilterTimeFrameSliceByMultiplicity::geoToIndex(uint64_t geo) {
    if (geo >= 0xc0a802a1 && geo <= 0xc0a802a7) {
        return geo - 0xc0a802a1;
    } else if (geo == 0xc0a802aa) {
        return 7;
    }
    return -1; 
}

int FilterTimeFrameSliceByMultiplicity::findWirenumber(const std::vector<std::vector<Wire_map>>& wireMapArray, uint64_t geo, int ch, int *foundid, int *foundGeo, int &Geofield) {
    int geoIndex = geoToIndex(geo);
    if (geoIndex != -1 && ch < wireMapArray[geoIndex].size()) {
        const Wire_map& wire = wireMapArray[geoIndex][ch];
        if (wire.catid != -1) { 
            *foundid = wire.id;
            *foundGeo = wire.geo;

            if ((geo == 0xc0a802a1) || (geo == 0xc0a802a2)) {
                Geofield = 1; // plane1
            } else if ((geo == 0xc0a802a3) || (geo == 0xc0a802a4)) {
                Geofield = 2; // plane2
            } else if ((geo == 0xc0a802a5) || (geo == 0xc0a802a6)) {
                Geofield = 3; // plane3
            } else if ((geo == 0xc0a802a7) || (geo == 0xc0a802aa)) {
                Geofield = 4; // plane4
            }

            return 1; 
        }
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
