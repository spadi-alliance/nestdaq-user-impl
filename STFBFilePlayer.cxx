#include <algorithm>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <stdexcept>
#include <functional>
#include <thread>
#include <cassert>
#include <numeric>
#include <unordered_map>
#include <sstream>
#include <sys/time.h>

#include <fmt/core.h>

#include <fairmq/runDevice.h>

#include "utility/HexDump.h"
#include "utility/MessageUtil.h"
#include "SubTimeFrameHeader.h"
#include "AmQStrTdcData.h"
#include "FileSinkHeader.h"
#include "FileSinkTrailer.h"

#include "STFBFilePlayer.h"


//______________________________________________________________________________
STFBFilePlayer::STFBFilePlayer()
    : fair::mq::Device()
{
}


//______________________________________________________________________________
void STFBFilePlayer::InitTask()
{

    using opt = OptionKey;
    fInputChannelName  = fConfig->GetProperty<std::string>(opt::InputChannelName.data());
    fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());
    fDQMChannelName    = fConfig->GetProperty<std::string>(opt::DQMChannelName.data());

    fMaxIterations     = std::stoll(
                             fConfig->GetProperty<std::string>(opt::MaxIterations.data()));
    fPollTimeoutMS     = std::stoi(
                             fConfig->GetProperty<std::string>(opt::PollTimeout.data()));
    fTimeFrameIdType   = static_cast<TimeFrameIdType>(
                             std::stoi(fConfig->GetProperty<std::string>(opt::TimeFrameIdType.data())));
    fInputFileName     = fConfig->GetProperty<std::string>(opt::InputFileName.data());
    fSplitMethod       = std::stoi(
                             fConfig->GetProperty<std::string>(opt::SplitMethod.data()));
    fWait              = std::stoi(
                             fConfig->GetProperty<std::string>(opt::IterationWait.data()));

    fSTFSequenceNumber = 0;
    fHBFCounter = 0;

    LOG(debug) << " output channels: name = " << fOutputChannelName
               << " num = " << GetNumSubChannels(fOutputChannelName);
    fNumDestination = GetNumSubChannels(fOutputChannelName);
    LOG(debug) << " number of desntination = " << fNumDestination;
    if (fNumDestination < 1) {
        LOG(warn) << " number of destination is non-positive";
    }

    LOG(debug) << "DQM channels: name = " << fDQMChannelName;

    //	       << " num = " << fChannels.count(fDQMChannelName);
    //    if (fChannels.count(fDQMChannelName)) {
    //        LOG(debug) << " data quality monitoring channels: name = " << fDQMChannelName
    //                   << " num = " << fChannels.at(fDQMChannelName).size();
    //    }

#if 0
    OnData(fInputChannelName, &STFBFilePlayer::HandleData);
#endif

}

// ----------------------------------------------------------------------------
void STFBFilePlayer::PreRun()
{
    fNumIteration = 0;
    fDirection = 0;
    fInputFile.open(fInputFileName.data(), std::ios::binary);
    if (!fInputFile) {
        LOG(error) << "PreRun: failed to open file = " << fInputFileName;

        assert(!fInputFile);

        return;
    }

    LOG(info) << "PreRun: Input file: " << fInputFileName;

    // check FileSinkHeader
    uint64_t buf;
    fInputFile.read(reinterpret_cast<char*>(&buf), sizeof(buf));
    if (fInputFile.gcount() != sizeof(buf)) {
        LOG(warn) << "Failed to read the first 8 bytes";
        return;
    }
    LOG(info) << "check FS header : " << buf;

    if (buf == SubTimeFrame::Magic) {
        //if ((buf[0] != nestdaq::FileSinkHeaderBlock::kMagic)
        //    || (buf[1] != nestdaq::FileSinkHeaderBlock::kMagi)) {
        fInputFile.seekg(0, std::ios_base::beg);
        fInputFile.clear();
        LOG(debug) << "No FS header";
    } else if (buf == FileSinkHeader::Magic) { /* For new FileSinkHeader after 2023.06.15 */
        uint64_t hsize{0};
        fInputFile.read(reinterpret_cast<char*>(&hsize), sizeof(hsize));
	LOG(debug) << "New FS header (Order: Magic + FS header size)";
	fInputFile.seekg(hsize - 2*sizeof(uint64_t), std::ios_base::cur);
    } else { /* For old FileSinkHeader before 2023.06.15 */
        uint64_t magic{0};
        fInputFile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic == FileSinkHeader::Magic) {
            LOG(debug) << "Old FS header (Order: FS header size + Magic)";
            fInputFile.seekg(buf - 2*sizeof(uint64_t), std::ios_base::cur);
        }
    }
}


//_____________________________________________________________________________
bool STFBFilePlayer::ConditionalRun()
{
    namespace STF = SubTimeFrame;
    namespace FST = FileSinkTrailer;

    if (fInputFile.eof()) {
        LOG(warn) << "Reached end of input file. stop RUNNING";
        return false;
    }

    FairMQParts outParts;

    outParts.AddPart(NewMessage(sizeof(STF::Header)));
    auto &msgSTFHeader = outParts[0];
    fInputFile.read(reinterpret_cast<char*>(msgSTFHeader.GetData()), msgSTFHeader.GetSize());
    if (static_cast<size_t>(fInputFile.gcount()) < msgSTFHeader.GetSize()) {
        LOG(warn) << "No data read. request = " << msgSTFHeader.GetSize()
                  << " bytes. gcount = " << fInputFile.gcount();
        return false;
    }
    auto stfHeader = reinterpret_cast<STF::Header*>(msgSTFHeader.GetData());

    // This code may not be beautiful. This can be rewritten by Takahashi-san or Igarashi-san. Nobu 2023.06.15
    if (stfHeader->magic != STF::Magic) {
        if (stfHeader->magic == FST::Magic) {
            auto fsTrailer = reinterpret_cast<FST::Trailer*>(stfHeader);
	    LOG(info) << "maigic : " << std::hex << fsTrailer->magic
	              << " size : " << std::dec << fsTrailer->size;
	    return false;
	}else{
	    LOG(error) << "Unkown magic = " << stfHeader->magic;
	    return false;
	}
    }
    
    std::cout << "#D Header size :" << msgSTFHeader.GetSize() << std::endl;
    std::cout << "#D maigic : " << std::hex << stfHeader->magic
              << " length : " << std::dec << stfHeader->length << std::endl;

    uint32_t *ddd = reinterpret_cast<uint32_t *>(stfHeader);
    for (int ii = 0 ; ii < 32 ; ii++) {
        if ((ii % 8) == 0) std::cout << std::endl << std::hex << std::setw(4) << ii << " : ";
        std::cout << " " << std::setw(8) << std::hex << ddd[ii];
    }
    std::cout << std::dec << std::endl;


    std::vector<char> buf(stfHeader->length - sizeof(STF::Header));
    fInputFile.read(buf.data(), buf.size());

    if (static_cast<size_t>(fInputFile.gcount()) < msgSTFHeader.GetSize()) {
        LOG(warn) << "No data read. request = " << msgSTFHeader.GetSize()
                  << " bytes. gcount = " << fInputFile.gcount();
        return false;
    }
    LOG(debug4) << " buf size = " << buf.size();
    auto bufBegin = buf.data();

#if 0
    std::for_each(reinterpret_cast<uint64_t*>(bufBegin),
                  reinterpret_cast<uint64_t*>(bufBegin)+buf.size()/sizeof(uint64_t),
                  HexDump(4));
#endif


#if 1
    LOG(debug4) << fmt::format(
                    "STF header: magic = {:016x}, tf-id = {:d}, rsv = {:08x}, FEM-type = {:08x}, FEM-id = {:08x}, bytes = {:d}, n-msg = {:d}, sec = {:d}, usec = {:d}",
                    stfHeader->magic, stfHeader->timeFrameId, stfHeader->reserved, stfHeader->FEMType,
                    stfHeader->FEMId, stfHeader->length, stfHeader->numMessages, stfHeader->time_sec,
                    stfHeader->time_usec);
#endif

    //auto header = reinterpret_cast<char*>(msgSTFHeader.GetData());
    auto headerNBytes = msgSTFHeader.GetSize();
    auto wordBegin = reinterpret_cast<uint64_t*>(bufBegin);
    auto bodyNBytes = stfHeader->length - headerNBytes;
    auto nWords = bodyNBytes/sizeof(uint64_t);
    LOG(debug4) << " nWords = " << nWords;

#if 0
    std::for_each(wordBegin, wordBegin+nWords, HexDump(4));
#endif

    auto wBegin = wordBegin;
    auto wEnd   = wordBegin + nWords;
    AmQStrTdc::Data::Bits *prev;
    for (auto ptr = wBegin ; ptr != wEnd ; ++ptr) {
        auto d = reinterpret_cast<AmQStrTdc::Data::Bits*>(ptr);

#if 0
        uint16_t type = d->head;
        LOG(debug4) << fmt::format(" data type = {:x}", type);
#endif

        if (fSplitMethod == 0) {
            switch (d->head) {
            //-----------------------------
            case AmQStrTdc::Data::SpillEnd:
            //-----------------------------
            case AmQStrTdc::Data::Heartbeat: {
                outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
                auto & msg = outParts[outParts.Size()-1];
                LOG(debug4) << " found Heartbeat data. " << msg.GetSize() << " bytes";
                std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());

#if 1
                std::cout << "mode 0: Msg: " << std::dec << outParts.Size();
                std::for_each(
                    reinterpret_cast<uint64_t*>(msg.GetData()),
                    reinterpret_cast<uint64_t*>(msg.GetData())+msg.GetSize()/sizeof(uint64_t),
                    HexDump(4));
#endif

                wBegin = ptr+1;
                break;
            }
            //-----------------------------
            default:
                break;
            }
        } else {
            switch (d->head) {
            //-----------------------------
            case AmQStrTdc::Data::SpillEnd:
            //-----------------------------
            case AmQStrTdc::Data::Heartbeat: {

		if ((ptr - wBegin) > 0) {

                //fair::mq::Message & msg;
                fair::mq::Message * pmsg;
                if (prev->head == AmQStrTdc::Data::Heartbeat) {
                    outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
                    //auto & msg = outParts[outParts.Size() - 1];
                    pmsg = &outParts[outParts.Size() - 1];
                    auto & msg = *pmsg;
                    LOG(debug4) << " found Heartbeat data. " << msg.GetSize() << " bytes";
                    std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
                    wBegin = ptr + 1;
                } else {
                    outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin)));
                    //auto & msg = outParts[outParts.Size() - 1];
                    pmsg = &outParts[outParts.Size() - 1];
                    auto & msg = *pmsg;
                    LOG(debug4) << " found Heartbeat data. " << msg.GetSize() << " bytes";
                    std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
                    wBegin = ptr;
                }

#if 1
                auto & msg = *pmsg;
                std::cout << "mode 1: Msg: " << std::dec << outParts.Size()
                          << " size: " << outParts[outParts.Size() - 1].GetSize();
                std::for_each(
                    reinterpret_cast<uint64_t*>(msg.GetData()),
                    reinterpret_cast<uint64_t*>(msg.GetData())+msg.GetSize()/sizeof(uint64_t),
                    HexDump(4));
#endif
		}

                break;
            }
            //-----------------------------
            default:
                break;

            }
        }
        prev = d;
    }

    LOG(debug4) << " n-iteration = " << fNumIteration
                << ": out parts.size() = " << outParts.Size();

    auto poller = NewPoller(fOutputChannelName);
    while (!NewStatePending()) {
        poller->Poll(fPollTimeoutMS);
        auto direction = fDirection % fNumDestination;
        ++fDirection;
        if (poller->CheckOutput(fOutputChannelName, direction)) {
            if (Send(outParts, fOutputChannelName, direction) > 0) {
                // successfully sent
                break;
            } else {
                LOG(warn) << "Failed to enqueue time frame : STF = " << stfHeader->timeFrameId;
            }
        }
    }

    ++fNumIteration;
    if (fMaxIterations>0 && fMaxIterations <= fNumIteration) {
        LOG(info) << "number of iterations of ConditionalRun() reached maximum.";
        return false;
    }

    if (fWait != 0) std::this_thread::sleep_for(std::chrono::milliseconds(fWait));

    return true;
}


//______________________________________________________________________________
void STFBFilePlayer::PostRun()
{
    if (fInputFile.is_open()) {
        LOG(info) << " close input file";
        fInputFile.close();
        fInputFile.clear();
    }

    LOG(debug) << __func__ << " done";
}


namespace bpo = boost::program_options;
//______________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    using opt = STFBFilePlayer::OptionKey;
    options.add_options()
    (opt::InputFileName.data(), bpo::value<std::string>(),
     "path to input data file")
    (opt::FEMId.data(),             bpo::value<std::string>(),
     "FEM ID")
    (opt::InputChannelName.data(),  bpo::value<std::string>()->default_value("in"),
     "Name of the input channel")
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"),
     "Name of the output channel")
    (opt::DQMChannelName.data(),    bpo::value<std::string>()->default_value("dqm"),
     "Name of the data quality monitoring")

    (opt::MaxIterations.data(),     bpo::value<std::string>()->default_value("0"),
     "maximum number of iterations")
    (opt::PollTimeout.data(),       bpo::value<std::string>()->default_value("0"),
     "Timeout of send-socket polling (in msec)")

    (opt::SplitMethod.data(),       bpo::value<std::string>()->default_value("1"),
     "STF split method")
    (opt::IterationWait.data(),     bpo::value<std::string>()->default_value("0"),
     "Iteration wait time (ms)")
    (opt::TimeFrameIdType.data(),   bpo::value<std::string>()->default_value("0"),
     "Time frame ID type:"
     " 0 = first HB delimiter,"
     " 1 = last HB delimiter,"
     " 2 = sequence number of time frames")
    ;
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<STFBFilePlayer>();
}


#if 0
// memo

auto hoge = [&]() -> decltype(auto) {
    if (prev->head == AmQStrTdc::Data::Heartbeat) {
        outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
        auto & msg = outParts[outParts.Size() - 1];
        LOG(debug4) << " found Heartbeat data. " << msg.GetSize() << " bytes";
        std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
        wBegin = ptr + 1;
        return msg;
    } else {
        outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin)));
        auto & msg = outParts[outParts.Size() - 1];
        LOG(debug4) << " found Heartbeat data. " << msg.GetSize() << " bytes";
        std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
        wBegin = ptr;
        return msg;
    }
}

auto& msg = hoge();  
std::cout << "mode 1: Msg: " << std::dec << outParts.Size();
#endif
