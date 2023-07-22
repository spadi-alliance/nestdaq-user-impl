#include <functional>

//#include <fmt/core.h>
#include <fairmq/FairMQLogger.h>
#include <fairmq/Poller.h>
#include <fairmq/runDevice.h>

#include "AmQStrTdcData.h"
#include "FileSinkHeader.h"
#include "FileSinkTrailer.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "utility/HexDump.h"

#include "TFBFilePlayer.h"

namespace bpo = boost::program_options;

//_____________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    using opt = TFBFilePlayer::OptionKey;
    options.add_options()
    (opt::InputFileName.data(),     bpo::value<std::string>(),
        "path to input data file")
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"),
        "Name of the data output channel")
    (opt::MaxIterations.data(),     bpo::value<std::string>()->default_value("0"),
        "maximum number of iterations")
    (opt::PollTimeout.data(),       bpo::value<std::string>()->default_value("0"),
        "Timeout of send-socket polling (in msec)")
    (opt::IterationWait.data(),     bpo::value<std::string>()->default_value("0"),
        "Iteration wait time (ms)")
    (opt::SplitMethod.data(),       bpo::value<std::string>()->default_value("1"),
        "STF split method")
    ;
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
    namespace FSH = FileSinkHeader;
    namespace FST = FileSinkTrailer;

    if (fInputFile.eof()) {
        LOG(warn) << "Reached end of input file. stop RUNNING";
        return false;
    }

    uint64_t magic;
    char tempbuf[sizeof(struct Filter::Header)];
    fInputFile.read(reinterpret_cast<char*>(&magic), sizeof(uint64_t));

    if (magic == Filter::Magic) {
        fInputFile.read(tempbuf, sizeof(struct Filter::Header) - sizeof(uint64_t));
    } else 
    if (magic == FSH::Magic) {
        FSH::Header fsh;
        fsh.magic = magic;
        char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0 ; i < 8 ; i++) hmagic[i] = *(reinterpret_cast<char *>(&magic) + i);
        fInputFile.read(reinterpret_cast<char *>(&fsh) + sizeof(uint64_t),
            sizeof(FSH::Header) - sizeof(uint64_t));
        LOG(info) << "FileSink Header magic : " << std::hex << fsh.magic
            << "(" << hmagic << ")" << " size : " << std::dec << fsh.size << std::endl;
        return true;
    } else
    if (magic == FST::Magic) {
        FST::Trailer fst;
        fst.magic = magic;
        char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0 ; i < 8 ; i++) hmagic[i] = *(reinterpret_cast<char *>(&magic) + i);
        fInputFile.read(reinterpret_cast<char *>(&fst) + sizeof(uint64_t),
            sizeof(FST::Trailer) - sizeof(uint64_t));
        LOG(info) << "FileSink Trailer magic : " << std::hex << fst.magic
            << "(" << hmagic << ")" << " size : " << std::dec << fst.size << std::endl;
        return true;
    } else
    if (magic != TF::Magic) {
        char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0 ; i < 8 ; i++) hmagic[i] = *(reinterpret_cast<char *>(&magic) + i);
        LOG(error) << "Unkown magic = " << std::hex << magic << "(" << hmagic << ")";
        return true;
    }


    FairMQParts outParts;

    // TF header
    outParts.AddPart(NewMessage(sizeof(TF::Header)));
    auto &msgTFHeader = outParts[0];

    if (magic == TF::Magic) {
        *(reinterpret_cast<uint64_t *>(msgTFHeader.GetData())) = magic;
        fInputFile.read(reinterpret_cast<char*>(msgTFHeader.GetData()) + sizeof(uint64_t),
            msgTFHeader.GetSize() - sizeof(uint64_t));
    } else {
        fInputFile.read(reinterpret_cast<char*>(msgTFHeader.GetData()),
            msgTFHeader.GetSize());
    }

    if (static_cast<size_t>(fInputFile.gcount()) < (msgTFHeader.GetSize() - sizeof(uint64_t))) {
        LOG(warn) << "No data read. request = " << msgTFHeader.GetSize()
            << " bytes. gcount = " << fInputFile.gcount();
        return false;
    }
    auto tfHeader = reinterpret_cast<TF::Header*>(msgTFHeader.GetData());
    
    #if 0
    LOG(debug4) << fmt::format("TF header: magic = {:016x}, tf-id = {:d}, n-src = {:d}, bytes = {:d}",
        tfHeader->magic, tfHeader->timeFrameId, tfHeader->numSource, tfHeader->length);
    #endif

    std::vector<char> buf(tfHeader->length - sizeof(TF::Header));
    fInputFile.read(buf.data(), buf.size());
    if (static_cast<size_t>(fInputFile.gcount()) < msgTFHeader.GetSize()) {
        LOG(warn) << "No data read. request = " << msgTFHeader.GetSize()
            << " bytes. gcount = " << fInputFile.gcount();
        return false;
    }
    LOG(debug4) << " buf size = " << buf.size();
    auto bufBegin = buf.data();
    //std::for_each(reinterpret_cast<uint64_t*>(bufBegin),
    //    reinterpret_cast<uint64_t*>(bufBegin)+buf.size()/sizeof(uint64_t),
    //    HexDump());

    for (auto i=0u; i<tfHeader->numSource; ++i) {
        outParts.AddPart(NewMessage(sizeof(STF::Header)));
        //LOG(debug4) << " i-sub = " << i << " size = " << outParts.Size();
        auto &msgSTFHeader = outParts[outParts.Size()-1];
        //LOG(debug4) << " STF header size =  " << msgSTFHeader.GetSize();
        auto header = reinterpret_cast<char*>(msgSTFHeader.GetData());
        auto headerNBytes = msgSTFHeader.GetSize();
        std::memcpy(header, bufBegin, headerNBytes);
        auto stfHeader = reinterpret_cast<STF::Header*>(header);

//        LOG(debug4) << fmt::format(
//            "STF header: magic = {:016x}, tf-id = {:d}, rsv = {:08x},"
//            " FEM-type = {:08x}, FEM-id = {:08x}, bytes = {:d},"
//            " n-msg = {:d}, sec = {:d}, usec = {:d}",
//             stfHeader->magic, stfHeader->timeFrameId, stfHeader->reserve,
//             stfHeader->FEMType, stfHeader->FEMId, stfHeader->length,
//             stfHeader->numMessages, stfHeader->time_sec, stfHeader->time_usec);


        auto wordBegin = reinterpret_cast<uint64_t*>(bufBegin + headerNBytes);
        auto bodyNBytes = stfHeader->length - headerNBytes;
        auto nWords = bodyNBytes/sizeof(uint64_t);
        LOG(debug4) << " nWords = " << nWords;

        //std::for_each(wordBegin, wordBegin+nWords, HexDump());
        auto wBegin = wordBegin;
        auto wEnd   = wordBegin + nWords;
        for (auto ptr = wBegin; ptr!=wEnd; ++ptr) {
            auto d = reinterpret_cast<AmQStrTdc::Data::Bits*>(ptr);
//            uint16_t type = d->head;
//            LOG(debug4) << fmt::format(" data type = {:x}", type);
            switch (d->head) {
            //-----------------------------
            case AmQStrTdc::Data::SpillEnd:
            //-----------------------------
            case AmQStrTdc::Data::Heartbeat: {
                FairMQMessage *pmsg;
                if (fSplitMethod == 0) {
                    outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
                    auto & msg = outParts[outParts.Size() - 1];
                    std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
                    pmsg = &msg;
                    wBegin = ptr + 1;
                } else if (fSplitMethod == 1) {
                    //std::cout << "#D " << (ptr - wBegin) << " : "
                    //    << std::hex << (d-1)->head << " " << d->head << std::endl;
                    if ((ptr - wBegin) == 0) {
                        continue;
                    } else
                    if ( ((ptr - wBegin) > 1)
                        || (((ptr - wBegin) == 1)
                        && ((((d - 1)->head) != AmQStrTdc::Data::Heartbeat))) ) {
                    //if ((ptr - wBegin) > 1) {
                    //if ((((d - 1)->head) != AmQStrTdc::Data::Heartbeat) 
                    //    || ( (((d - 1)->head) == AmQStrTdc::Data::Heartbeat)
                    //        && (((d - 2)->head) == AmQStrTdc::Data::Heartbeat) )
                    //         ) {
                        outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin)));
                        auto & msg = outParts[outParts.Size() - 1];
                        std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
                        pmsg = &msg;
                        wBegin = ptr;
                    } else {
                        outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
                        auto & msg = outParts[outParts.Size() - 1];
                        std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
                        pmsg = &msg;
                        wBegin = ptr + 1;
                    }
                } else {
                    if ((d + 1)->head == AmQStrTdc::Data::Heartbeat) {
                        ptr++;
                    } else {
                        LOG(warn) << "Single Heartbeat) :"
                            << std::hex << (*ptr) << " " << (*(ptr + 1));
                    }
                    outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
                    auto & msg = outParts[outParts.Size() - 1];
                    std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
                    pmsg = &msg;
                    wBegin = ptr + 1;
                }

                #if 0
                LOG(debug4) << " found Heartbeat data. " << pmsg->GetSize() << " bytes";
                std::for_each(reinterpret_cast<uint64_t*>(pmsg->GetData()),
                    reinterpret_cast<uint64_t*>(pmsg->GetData())+pmsg->GetSize()/sizeof(uint64_t),
                    HexDump(2));
                #endif

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

    if (fWait != 0) std::this_thread::sleep_for(std::chrono::milliseconds(fWait));

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

    fNumDestination    = GetNumSubChannels(fOutputChannelName);
    fWait              = std::stoi(
                             fConfig->GetProperty<std::string>(opt::IterationWait.data()));
    fSplitMethod       = std::stoi(
                         fConfig->GetProperty<std::string>(opt::SplitMethod.data()));

    LOG(info) << "InitTask: Iteration   : " << fMaxIterations;
    LOG(info) << "InitTask: Wait        : " << fWait;
    LOG(info) << "InitTask: SplitMethod : " << fSplitMethod;
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

    // check FileSinkHeader
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
