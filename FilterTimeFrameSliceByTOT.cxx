/**
 * @file FilterTimeFrameSliceByTOT.cxx 
 * @brief Slice TimeFrame by Time Over Threshold (TOT) for NestDAQ
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-15 00:47:34 JST
 * 
 * @comment Modify FilterTimeFrameSliceBySomething.cxx for MultiHit 
 * 
 */

#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <map>
#include <tuple>
#include "FilterTimeFrameSliceByTOT.h"
#include "FilterTimeFrameSliceABC.icxx"
#include "fairmq/runDevice.h"
#include "utility/MessageUtil.h"
#include "UnpackTdc.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"

#define DEBUG 0

using nestdaq::FilterTimeFrameSliceByTOT;
namespace bpo = boost::program_options;

FilterTimeFrameSliceByTOT::FilterTimeFrameSliceByTOT()
{
}

bool FilterTimeFrameSliceByTOT::ProcessSlice(TTF& tf)
{
    bool tofCondition = false;

    std::map<int, std::tuple<int, int>> chargeSums;

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
                int charge = tdc.tot;

                int planeId = DeterminePlane(header->femId, tdc.ch);
                if (planeId != -1) {
                    auto& [sum, count] = chargeSums[planeId];
                    sum += charge;
                    count++;
                }
            }
        }
    }

    if (Chargelogic(chargeSums)) {
        return true;
    }

    return false;
}

int FilterTimeFrameSliceByTOT::DeterminePlane(uint64_t fem, int ch) {
//Define detector face with IP address and channel
    if ((fem == 0xc0a802a1) || (fem == 0xc0a802a2)) {
        return 1; // plane1
    } else if ((fem == 0xc0a802a3) || (fem == 0xc0a802a4)) {
        return 2; // plane2
    } else if ((fem == 0xc0a802a5) || (fem == 0xc0a802a6)) {
        return 3; // plane3
    } else if ((fem == 0xc0a802a7) || (fem == 0xc0a802aa)) {
        return 4; // plane4
    }
    return -1; 
}

// gate condition
bool FilterTimeFrameSliceByTOT::Chargelogic(const std::map<int, std::tuple<int, int>>& chargeSums) {
    try {
        auto [chargeSum1, n1] = chargeSums.at(1); // plane1
        auto [chargeSum2, n2] = chargeSums.at(2); // plane2
        auto [chargeSum3, n3] = chargeSums.at(3); // plane3
        auto [chargeSum4, n4] = chargeSums.at(4); // plane4

        //Change here !
        if ((((double)chargeSum1 / n1) > 35) &&
            (((double)chargeSum2 / n2) > 45) &&
            (((double)chargeSum3 / n3) > 50) &&
            (((double)chargeSum4 / n4) > 55)) {
            return true;
        }
    } catch (const std::out_of_range& e) {
        return false; 
    }
    return false;
}

void addCustomOptions(bpo::options_description& options)
{
    using opt = FilterTimeFrameSliceByTOT::OptionKey;

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
    return std::make_unique<FilterTimeFrameSliceByTOT>();
}
