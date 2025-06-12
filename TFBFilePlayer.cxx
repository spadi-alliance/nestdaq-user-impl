#include <functional>

//#include <fmt/core.h>
//#include <fairmq/FairMQLogger.h>
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
    (opt::SplitMethod.data(),       bpo::value<std::string>()->default_value("2"),
     "STF split method")
    (opt::EnableRecbe.data(),       bpo::value<std::string>()->default_value("false"),
     "Enable RECBE handling")
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
    if (fInputFile.eof()) return false;
    if (! static_cast<bool>(fInputFile)) {
        LOG(error) << "Fail to read " << fInputFileName;
        return false;
    }

    if (magic == Filter::MAGIC) {
        fInputFile.read(tempbuf, sizeof(struct Filter::Header) - sizeof(uint64_t));
    } else
    if (magic == FSH::MAGIC) {
        FSH::Header fsh;
        fsh.magic = magic;
        char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0 ; i < 8 ; i++) hmagic[i] = *(reinterpret_cast<char *>(&magic) + i);
        fInputFile.read(reinterpret_cast<char *>(&fsh) + sizeof(uint64_t),
                        sizeof(FSH::Header) - sizeof(uint64_t));
        if (fInputFile.eof()) return false;
        LOG(info) << "FileSink Header magic : " << std::hex << fsh.magic
                  << "(" << hmagic << ")" << " size : " << std::dec << fsh.size << std::endl;
        return true;
    } else
    if (magic == FST::MAGIC) {
        FST::Trailer fst;
        fst.magic = magic;
        char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0 ; i < 8 ; i++) hmagic[i] = *(reinterpret_cast<char *>(&magic) + i);
        fInputFile.read(reinterpret_cast<char *>(&fst) + sizeof(uint64_t),
                        sizeof(FST::Trailer) - sizeof(uint64_t));
        if (fInputFile.eof()) return false;
        LOG(info) << "FileSink Trailer magic : " << std::hex << fst.magic
                  << "(" << hmagic << ")" << " size : " << std::dec << fst.size << std::endl;
        return true;
    } else
    if (magic != TF::MAGIC) {
        char hmagic[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0 ; i < 8 ; i++) {
            char c = *(reinterpret_cast<char *>(&magic) + i);
            if  ((c >= 0x20) && (c < 0x7f)) {
                hmagic[i] = c;
            } else {
                hmagic[i] = '.';
            }
        }
        LOG(error) << "Unknown magic = " << std::hex << magic << "(" << hmagic << ")";
        return true;
    }


    fair::mq::Parts outParts;

    // TF header
    outParts.AddPart(NewMessage(sizeof(TF::Header)));
    auto &msgTFHeader = outParts[0];

    if (magic == TF::MAGIC) {
        *(reinterpret_cast<uint64_t *>(msgTFHeader.GetData())) = magic;
        fInputFile.read(reinterpret_cast<char*>(msgTFHeader.GetData()) + sizeof(uint64_t),
                        msgTFHeader.GetSize() - sizeof(uint64_t));
        if (fInputFile.eof()) return false;
        //LOG(debug4) << "TF Header : " << std::hex << magic << std::dec;
    } else {
        fInputFile.read(reinterpret_cast<char*>(msgTFHeader.GetData()),
                        msgTFHeader.GetSize());
        if (fInputFile.eof()) return false;
    }

    if (static_cast<size_t>(fInputFile.gcount()) < (msgTFHeader.GetSize() - sizeof(uint64_t))) {
        LOG(warn) << "No data read. request = " << msgTFHeader.GetSize()
                  << " bytes. gcount = " << fInputFile.gcount();
        return false;
    }
    auto tfHeader = reinterpret_cast<TF::Header*>(msgTFHeader.GetData());

#if 1
    //LOG(debug4) << fmt::format("TF header: magic = {:016x}, tf-id = {:d}, n-src = {:d}, bytes = {:d}",
    //    tfHeader->magic, tfHeader->timeFrameId, tfHeader->numSource, tfHeader->length);
    LOG(debug4) << "TF header: magic = 0x" << std::hex << tfHeader->magic
        << ", tf-id = " << std::dec << tfHeader->timeFrameId
        << ", n-src = " << tfHeader->numSource
        << ", bytes = " << tfHeader->length;
#endif

    std::vector<char> buf(tfHeader->length - sizeof(TF::Header));
    fInputFile.read(buf.data(), buf.size());
    if (fInputFile.eof()) return false;
    if (static_cast<size_t>(fInputFile.gcount()) < msgTFHeader.GetSize()) {
        LOG(warn) << "No data read. request = " << msgTFHeader.GetSize()
                  << " bytes. gcount = " << fInputFile.gcount();
        return false;
    }
    LOG(debug4) << " buf size = " << buf.size();
    auto bufBegin = buf.data();

    LOG(debug4) << "Pack TFH : out parts.size() = " << outParts.Size();
#if 0
    std::for_each(reinterpret_cast<uint64_t*>(bufBegin),
        reinterpret_cast<uint64_t*>(bufBegin)+buf.size()/sizeof(uint64_t),
        HexDump());
#endif

    for (auto i=0u; i<tfHeader->numSource; ++i) {
        outParts.AddPart(NewMessage(sizeof(STF::Header)));
        //LOG(debug4) << " i-sub = " << i << " size = " << outParts.Size();
        auto &msgSTFHeader = outParts[outParts.Size()-1];
        //LOG(debug4) << " STF header size =  " << msgSTFHeader.GetSize();
        auto header = reinterpret_cast<char*>(msgSTFHeader.GetData());
        auto headerNBytes = msgSTFHeader.GetSize();
        std::memcpy(header, bufBegin, headerNBytes);
        auto stfHeader = reinterpret_cast<STF::Header*>(header);

        LOG(debug4)
            << "STF header: magic = 0x" << std::hex <<  stfHeader->magic
            << ", TF-id = " << std::dec << stfHeader->timeFrameId
#if 1
            << "  type = 0x" << std::hex << stfHeader->femType
            << ", id = 0x" << std::setw(2) << stfHeader->femId
#endif
            << "  bytes = " << std::dec << stfHeader->length
            << ", n-msg = " << stfHeader->numMessages
#if 0
            << ", sec = " << stfHeader->timeSec
            << ", usec = " << stfHeader->timeUSec
#endif
            ;

        auto wordBegin = reinterpret_cast<uint64_t*>(bufBegin + headerNBytes);
        auto bodyNBytes = stfHeader->length - headerNBytes;
        auto nWords = bodyNBytes/sizeof(uint64_t);

#if 0
        LOG(debug4) << " nWords = " << nWords << " : " << std::hex << *wordBegin;
        std::for_each(wordBegin, wordBegin+16, HexDump());
        //std::for_each(wordBegin, wordBegin+nWords, HexDump());
#endif

        auto wBegin = wordBegin;
        auto wEnd   = wordBegin + nWords;
        for (auto ptr = wBegin; ptr!=wEnd; ++ptr) {
            auto d = reinterpret_cast<AmQStrTdc::Data::Bits*>(ptr);
#if 0
            uint16_t type = d->head;
            LOG(debug4) << " data type = 0x" << std::hex << type;
#endif
            //---------------------------------
            //  case Recbe
            //---------------------------------
            //LOG(debug4) << "HEAD: " << std::hex << *ptr;
            if (fEnableRecbe) {
                if ((*ptr & 0x0000'0000'0000'0022) == 0x22) {

                    LOG(debug4) << "RECBE: " << std::hex << *ptr << " " << d->head;

                    outParts.AddPart(NewMessage(bodyNBytes));
                    auto & msg = outParts[outParts.Size() - 1];
                    std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());

                    break;
                }
            }

            switch (d->head) {
            //---------------------------------
            //  case AmQStrTdc::Data::SpillEnd:
            //---------------------------------
            case AmQStrTdc::Data::Heartbeat: {
                fair::mq::Message [[maybe_unused]] *pmsg;
                if (fSplitMethod == 0) {
                    outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
                    auto & msg = outParts[outParts.Size() - 1];
                    std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());
                    pmsg = &msg;
                    wBegin = ptr + 1;
                } else if (fSplitMethod == 1) {
                    if ((ptr - wBegin) == 0) {
                        continue;
                    } else if ( ((ptr - wBegin) > 1)
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
                } else if (fSplitMethod == 2) {
                    break;

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
            case AmQStrTdc::Data::Heartbeat2nd: {
                fair::mq::Message [[maybe_unused]] *pmsg;
                if (fSplitMethod == 2) {
                    outParts.AddPart(NewMessage(sizeof(uint64_t) * (ptr - wBegin + 1)));
                    auto & msg = outParts[outParts.Size() - 1];
                    std::memcpy(msg.GetData(), reinterpret_cast<char*>(wBegin), msg.GetSize());

#if 0
                    char     *cheader     = reinterpret_cast<char *>(wBegin);
                    uint64_t *checkHeader = reinterpret_cast<uint64_t *>(wBegin);
                    uint64_t *checkHB1    = reinterpret_cast<uint64_t *>(ptr - 1);
                    uint64_t *checkHB2    = reinterpret_cast<uint64_t *>(ptr);
                    std::cout << "#D " << cheader << " " << std::hex << *checkHeader << " "
                              << " size: " << msg.GetSize() << " : "
                              << *checkHB1 << " " << *checkHB2 << std::dec << std::endl;
#endif

                    wBegin = ptr + 1;
                } else {
                }
                break;
            }

            //-----------------------------
            default:
                break;
            }
        }
        bufBegin += stfHeader->length;

        LOG(debug4) << "Pack STF : out parts.size() = " << outParts.Size();

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

    fEnableRecbe       = (fConfig->GetProperty<std::string>(opt::EnableRecbe.data()) == "true");

    LOG(info) << "InitTask: Iteration   : " << fMaxIterations;
    LOG(info) << "InitTask: Wait        : " << fWait;
    LOG(info) << "InitTask: SplitMethod : " << fSplitMethod;
    LOG(info) << "InitTask: File name   : " << fInputFileName;
    LOG(info) << "InitTask: Recbe mode  : " << fEnableRecbe;
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
    LOG(info) << "Successfully opened file: " << fInputFileName;

    // check FileSinkHeader
    uint64_t buf{0};
    fInputFile.read(reinterpret_cast<char*>(&buf), sizeof(buf));
    if (fInputFile.gcount() != sizeof(buf)) {
        LOG(warn) << "Failed to read the first 8 bytes";
        return;
    }
    LOG(info) << "check FS header";
    if (buf == TimeFrame::MAGIC) {
        fInputFile.seekg(0, std::ios_base::beg);
        fInputFile.clear();
        LOG(debug) << "No FS header";
    } else if (buf == FileSinkHeader::v0::MAGIC) { /* For new FileSinkHeader after 2023.06.15 */
        uint64_t hsize{0};
        fInputFile.read(reinterpret_cast<char*>(&hsize), sizeof(hsize));
        LOG(debug) << "New FS header (Order: MAGIC + FS header size)";
        fInputFile.seekg(hsize - 2*sizeof(uint64_t), std::ios_base::cur);
    } else if (buf == FileSinkHeader::v1::MAGIC) { /* For new FileSinkHeader after 2023.06.15 */
        uint32_t hsize{0};
        fInputFile.read(reinterpret_cast<char*>(&hsize), sizeof(hsize));
        LOG(debug) << "New FS header (New: MAGIC + FS(32) header size)";
        fInputFile.seekg(hsize - sizeof(uint64_t) - sizeof(uint32_t), std::ios_base::cur);
    } else { /* For old FileSinkHeader before 2023.06.15 */
        uint64_t magic{0};
        fInputFile.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic == FileSinkHeader::MAGIC) {
            LOG(debug) << "Old FS header (Order: FS header size + MAGIC)";
            fInputFile.seekg(buf - 2*sizeof(uint64_t), std::ios_base::cur);
        }
    }
}
