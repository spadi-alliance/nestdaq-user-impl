/**
 * @file FilterTimeFrameSliceByTOF.cxx
 * @brief Slice TimeFrame by Time of Flight for NestDAQ
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-07-21 11:06:28 JST
 * 
 * @author Fumiya Furukawa <fumiya@rcnp.osaka-u.ac.jp>
 * @comment Modify FilterTimeFrameSliceBySomething.cxx for MultiHit 
 * 
 */
#include <fstream>
#include <iostream>
#include <vector>
#include <memory>
#include <map>
#include <chrono>
#include "FilterTimeFrameSliceByTOF.h"
#include "FilterTimeFrameSliceABC.icxx"
#include "fairmq/runDevice.h"
#include "utility/MessageUtil.h"
#include "UnpackTdc.h"
#include "SubTimeFrameHeader.h"

#define DEBUG 0
#define OUTPUT 1

using nestdaq::FilterTimeFrameSliceByTOF;
namespace bpo = boost::program_options;

FilterTimeFrameSliceByTOF::FilterTimeFrameSliceByTOF()
: totalCalls(0), totalAccepted(0)  // Initialize counters
{
}

bool FilterTimeFrameSliceByTOF::ProcessSlice(TTF& tf)
{
    auto start_time = std::chrono::high_resolution_clock::now();
    size_t total_size = 0;

    bool tofCondition = false;

    auto tfHeader = tf.GetHeader();

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
                AddTOFHit(header->femId, tdc.ch, tdc.tdc);

            } else if (header->femType == SubTimeFrame::TDC64L_V3) {
                TDC64L_V3::tdc64 tdc;
                TDC64L_V3::Unpack(hbf->UncheckedAt(i), &tdc);
                AddTOFHit(header->femId, tdc.ch, tdc.tdc);
            }
        }
    }

    auto tof_start_averages = CalculateAveragePairs(tof_start_r_hits, tof_start_l_hits);
    auto tof_end_averages = CalculateAveragePairs(tof_end_r_hits, tof_end_l_hits);

    CalculateAndPrintTOF(tof_end_averages, tof_start_averages);

    for (const auto& tof_start_avg : tof_start_averages) {
        for (const auto& tof_end_avg : tof_end_averages) {
            // The last two arguments are the minimum and maximum gate conditions
            if (CheckAllTOFConditions(*tof_start_avg, *tof_end_avg, tof_start_r_hits, tof_start_l_hits, tof_end_r_hits, tof_end_l_hits, -130000, -125000)) {
                tofCondition = true;
                break;
            }
        }
        if (tofCondition) break;
    }

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

    if (tofCondition) {
        totalAccepted++; // Increment total accepted
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
        std::cout << "end of function true" << std::endl;
        return true;
    }

    std::cout << "end of function false" << std::endl;
    return false;
}

void addCustomOptions(bpo::options_description& options)
{
    using opt = FilterTimeFrameSliceByTOF::OptionKey;

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
    return std::make_unique<FilterTimeFrameSliceByTOF>();
}

// Function section
// TOF detector settings 
// Define IP addresses and channels for Start and End detectors
void FilterTimeFrameSliceByTOF::AddTOFHit(uint64_t femId, int ch, int tdc) {
    if (femId == 0xc0a802a9) {
        if (ch == 9) {
            tof_start_r_hits.push_back(std::make_unique<int>(tdc));
        } else if (ch == 10) {
            tof_start_l_hits.push_back(std::make_unique<int>(tdc));
        }
    } else if (femId == 0xc0a802aa) {
        if (ch == 10) {
            tof_end_r_hits.push_back(std::make_unique<int>(tdc));
        } else if (ch == 8) {
            tof_end_l_hits.push_back(std::make_unique<int>(tdc));
        }
    }
}

// Calculate average of all R and L combinations in multi-hits for specific TOF detectors.
std::vector<std::unique_ptr<int>> FilterTimeFrameSliceByTOF::CalculateAveragePairs(const std::vector<std::unique_ptr<int>>& r_hits, const std::vector<std::unique_ptr<int>>& l_hits) {
    std::vector<std::unique_ptr<int>> averages;
    for (const auto& r : r_hits) {
        for (const auto& l : l_hits) {
            averages.push_back(std::make_unique<int>((*r + *l) / 2.0));
        }
    }
    return averages;
}

// Calculate TOF between two detectors, calculating all combinations of averages for Detector 1 and Detector 2.
void FilterTimeFrameSliceByTOF::CalculateAndPrintTOF(const std::vector<std::unique_ptr<int>>& tof_end_averages, const std::vector<std::unique_ptr<int>>& tof_start_averages) {
    for (const auto& tof_end_ave : tof_end_averages) {
        for (const auto& tof_start_ave : tof_start_averages) {
            int tof = *tof_end_ave - *tof_start_ave;
#if DEBUG
            std::cout << "TOF End Average: " << *tof_end_ave << ", TOF Start Average: " << *tof_start_ave << ", TOF: " << tof << std::endl;
#endif
        } 
    }
}

// TOF for debug 
void FilterTimeFrameSliceByTOF::PrintList(const std::vector<std::unique_ptr<int>>& list, const std::string& listName) {
    std::cout << listName << " [";
    for (const auto& value : list) {
        std::cout << *value << " ";
    }
    std::cout << "]" << std::endl;
}

// TOF Flag
bool FilterTimeFrameSliceByTOF::CheckAllTOFConditions(int tof_start_ave, int tof_end_ave, const std::vector<std::unique_ptr<int>>& tof_start_r_hits, const std::vector<std::unique_ptr<int>>& tof_start_l_hits,
                                const std::vector<std::unique_ptr<int>>& tof_end_r_hits, const std::vector<std::unique_ptr<int>>& tof_end_l_hits, int min, int max) {
    int avg_tof = tof_end_ave - tof_start_ave;
    return (avg_tof > min && avg_tof < max);
}
