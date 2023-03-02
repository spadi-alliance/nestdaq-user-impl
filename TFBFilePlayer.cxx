#include <functional>

//#include <fmt/core.h>
#include <fairmq/FairMQLogger.h>
#include <fairmq/Poller.h>
#include <fairmq/runDevice.h>

#include "AmQStrTdcData.h"
#include "FileSinkHeaderBlock.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "utility/HexDump.h"

#include "TFBFilePlayer.h"

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    using opt = TFBFilePlayer::OptionKey;
    options.add_options()
    (opt::InputFileName.data(), bpo::value<std::string>(), "path to input data file")
    //
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"), "Name of the data output channel")
    //
    (opt::MaxIterations.data(), bpo::value<std::string>()->default_value("0"), "maximum number of iterations")
    //
    (opt::PollTimeout.data(),   bpo::value<std::string>()->default_value("0"), "Timeout of send-socket polling (in msec)");
}

//_____________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
    return std::make_unique<TFBFilePlayer>();
}

//_____________________________________________________________________________
TFBFilePlayer::TFBFilePlayer()
    : fair::mq::Device()
{}

//_____________________________________________________________________________
bool TFBFilePlayer::ConditionalRun()
{
    namespace TF  = TimeFrame;
    namespace STF = SubTimeFrame;

    if (fInputFile.eof()) {
        LOG(warn) << "Reached end of input file. stop RUNNING";
        return false;
    }

    FairMQParts outParts;

    // TF header
    outParts.AddPart(NewMessage(sizeof(TF::Header)));
    auto &msgTFHeader = outParts[0];
    fInputFile.read(reinterpret_cast<char*>(msgTFHeader.GetData()), msgTFHeader.GetSize());
    if (fInputFile.gcount() < msgTFHeader.GetSize()) {
        LOG(warn) << "No data read. request = " << msgTFHeader.GetSize() << " bytes. gcount = " << fInputFile.gcount();
        return false;
    }
    auto tfHeader = reinterpret_cast<TF::Header*>(msgTFHeader.GetData());

//    LOG(debug4) << fmt::format("TF header: magic = {:016x}, tf-id = {:d}, n-src = {:d}, bytes = {:d}",
//                               tfHeader->magic, tfHeader->timeFrameId, tfHeader->numSource, tfHeader->length);

    std::vector<char> buf(tfHeader->length - sizeof(TF::Header));
    fInputFile.read(buf.data(), buf.size());
    if (fInputFile.gcount() < msgTFHeader.GetSize()) {
        LOG(warn) << "No data read. request = " << msgTFHeader.GetSize() << " bytes. gcount = " << fInputFile.gcount();
        return false;
    }
    LOG(debug4) << " buf size = " << buf.size();
    auto bufBegin = buf.data();
    //std::for_each(reinterpret_cast<uint64_t*>(bufBegin), reinterpret_cast<uint64_t*>(bufBegin)+buf.size()/sizeof(uint64_t), nestdaq::HexDump());

    for (auto i=0u; i<tfHeader->numSource; ++i) {
        outParts.AddPart(NewMessage(sizeof(STF::Header)));
        //LOG(debug4) << " i-sub = " << i << " size = " << outParts.Size();
        auto &msgSTFHeader = outParts[outParts.Size()-1];
        //LOG(debug4) << " STF header size =  " << msgSTFHeader.GetSize();
        auto header = reinterpret_cast<char*>(msgSTFHeader.GetData());
        auto headerNBytes = msgSTFHeader.GetSize();
        std::memcpy(header, bufBegin, headerNBytes);
        auto stfHeader = reinterpret_cast<STF::Header*>(header);

//        LOG(debug4) << fmt::format("STF header: magic = {:016x}, tf-id = {:d}, rsv = {:08x}, FEM-type = {:08x}, FEM-id = {:08x}, bytes = {:d}, n-msg = {:d}, sec = {:d}, usec = {:d}",
//                                   stfHeader->magic, stfHeader->timeFrameId, stfHeader->reserve, stfHeader->FEMType, stfHeader->FEMId, stfHeader->length, stfHeader->numMessages, stfHeader->time_sec, stfHeader->time_usec);


        auto wordBegin = reinterpret_cast<uint64_t*>(bufBegin + headerNBytes);
        auto bodyNBytes = stfHeader->length - headerNBytes;
        auto nWords = bodyNBytes/sizeof(uint64_t);
        LOG(debug4) << " nWords = " << nWords;

        //std::for_each(wordBegin, wordBegin+nWords, nestdaq::HexDump());
        auto wBegin = wordBegin;
        auto wEnd   = wordBegin + nWords;
        for (auto ptr = wBegin; ptr!=wEnd; ++ptr) {
            auto d = reinterpret_cast<AmQStrTdc::Data::Bits*>(ptr);
            uint16_t type = d->head;
//            LOG(debug4) << fmt::format(" data type = {:x}", type);
            switch (d->head) {
            //-----------------------------
            case AmQStrTdc::Data::SpillEnd:
            //-----------------------------
            case AmQStrTdc::Data::Heartbeat: {
                outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
                auto & msg = outParts[outParts.Size()-1];
                LOG(debug4) << " found Heartbeat data. " << msg.GetSize() << " bytes";
                std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
                LOG(debug4) << " dump";
                //std::for_each(reinterpret_cast<uint64_t*>(msg.GetData()), reinterpret_cast<uint64_t*>(msg.GetData())+msg.GetSize()/sizeof(uint64_t), nestdaq::HexDump());
                wBegin = ptr+1;
                break;
            }
            //-----------------------------
            default:
                break;
            }
        }
        bufBegin += stfHeader->length;
    }
    LOG(debug4) << " n-iteration = " << fNumIteration << ": out parts.size() = " << outParts.Size();

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
                LOG(warn) << "Failed to enqueue time frame : TF = " << tfHeader->timeFrameId;
            }
        }
    }

    ++fNumIteration;
    if (fMaxIterations>0 && fMaxIterations <= fNumIteration) {
        LOG(info) << "number of iterations of ConditionalRun() reached maximum.";
        return false;
    }
    return true;
}

//_____________________________________________________________________________
void TFBFilePlayer::InitTask()
{
    using opt = OptionKey;
    fInputFileName     = fConfig->GetProperty<std::string>(opt::InputFileName.data());
    fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());
    fMaxIterations     = std::stoll(fConfig->GetProperty<std::string>(opt::MaxIterations.data()));
    fPollTimeoutMS     = std::stoi(fConfig->GetProperty<std::string>(opt::PollTimeout.data()));

    fNumDestination = GetNumSubChannels(fOutputChannelName);
}

// ----------------------------------------------------------------------------
void TFBFilePlayer::PostRun()
{
    if (fInputFile.is_open()) {
        LOG(info) << " close input file";
        fInputFile.close();
        fInputFile.clear();
    }
}

// ----------------------------------------------------------------------------
void TFBFilePlayer::PreRun()
{
    fNumIteration = 0;
    fDirection = 0;
    fInputFile.open(fInputFileName.data(), std::ios::binary);
    if (!fInputFile) {
        LOG(error) << " failed to open file = " << fInputFileName;
        return;
    }

    // check FileSinkHeaderBlock
    uint64_t buf{0};
    fInputFile.read(reinterpret_cast<char*>(&buf), sizeof(buf));
    if (fInputFile.gcount() != sizeof(buf)) {
        LOG(warn) << "Failed to read the first 8 bytes";
        return;
    }
    LOG(info) << "check FS header";
    if (buf == TimeFrame::Magic) {
        fInputFile.seekg(0, std::ios_base::beg);
        fInputFile.clear();
        LOG(debug) << "no FS header";
    } else {
        uint64_t magic{0};
        fInputFile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic == nestdaq::FileSinkHeaderBlock::kMagic) {
            LOG(debug) << " header";
            fInputFile.seekg(buf - 2*sizeof(uint64_t), std::ios_base::cur);
        }
    }

}