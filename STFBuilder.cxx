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

#include <fairmq/runDevice.h>

#include "utility/HexDump.h"
#include "utility/MessageUtil.h"

#include "SubTimeFrameHeader.h"
#include "STFBuilder.h"
#include "AmQStrTdcData.h"

namespace STF  = SubTimeFrame;

//______________________________________________________________________________
AmQStrTdcSTFBuilder::AmQStrTdcSTFBuilder()
    : fair::mq::Device()
{
    mdebug = false;
}

//______________________________________________________________________________
void AmQStrTdcSTFBuilder::BuildFrame(FairMQMessagePtr& msg, int index)
{
    namespace Data = AmQStrTdc::Data;
    using Word     = Data::Word;
    std::size_t offset = 0;

    (void)index;

    if(mdebug) {
        LOG(debug)
                << " buildframe STF = " << fSTFSequenceNumber << " HBF = " << fHBFCounter << "\n"
                << " input payload entries = " << fInputPayloads.size()
                << " offset " << offset << std::endl;
    }

    auto msgBegin  = reinterpret_cast<Word*>(msg->GetData());
    auto msgSize   = msg->GetSize();
    auto nWord     = msgSize / sizeof(Word);

    if(mdebug) {
        LOG(debug) << " msg size " << msgSize << " bytes " << nWord << " words" << std::endl;
        {
	  std::for_each(reinterpret_cast<Word*>(msgBegin),
			msgBegin+nWord,
			nestdaq::HexDump{4});
	}
    }

    for (long unsigned int i = 0 ; i < nWord ; ++i) {
        auto word = reinterpret_cast<Data::Bits*>(msgBegin+i);

        uint8_t h = word->head;

        if(mdebug)
            LOG(debug) << " head = " << std::hex << static_cast<uint16_t>(h) << std::dec << std::endl;

        bool isHeadValid = false;
        for (auto validHead : {
                    Data::Data, Data::Heartbeat, Data::SpillOn, Data::SpillEnd
                }) {
            if (h == validHead) isHeadValid = true;
        }

        if (!isHeadValid) {

            LOG(error)   << " " << i << " " << offset << " invalid head = " << std::hex << static_cast<uint16_t>(h)
			 << " " << word->raw << std::dec << std::endl;
	    
            LOG(warning) << " " << i << " " << offset << " invalid head = " << std::hex << static_cast<uint16_t>(h)
			 << " " << word->raw << std::dec << std::endl;

            if (i - offset > 0) {
                // std::cout << " fill valid part "  << std::setw(10) << offset << " -> " << std::setw(10) << i << std::endl;
                auto first = msgBegin + offset;
                auto last  = msgBegin + i;
                std::for_each(first, last, nestdaq::HexDump{4});
                fInputPayloads.insert(fInputPayloads.end(), std::make_move_iterator(first), std::make_move_iterator(last));
            }
            offset = i+1;
            continue;
        }

        if ((h == Data::Heartbeat) || h == Data::SpillEnd) {

	    if (fLastHeader == 0) {
                fLastHeader = h;
		hbf_flag++;
                continue;
            } else if (fLastHeader == h) {
                fLastHeader = 0;
		hbf_flag++;
            } else {
                // unexpected @TODO
            }

            int32_t delimiterFrameId = ((word->hbspilln & 0xFF)<<16) | (word->hbframe & 0xFFFF);
            //LOG(debug) << " heartbeat/spill-end delimiter comes " << std::hex << hbframe << ", raw = " << word->raw;
            if (fTimeFrameIdType == TimeFrameIdType::FirstHeartbeatDelimiter) { // first heartbeat delimiter or first spill-off delimiter
                if (fSTFId<0) {
                    fSTFId = delimiterFrameId;
                }
            } else { // last heartbeat delimiter or last spill-off delimiter, or sequence number
                fSTFId = delimiterFrameId;
            }

            if(mdebug) {
                LOG(debug) << " Fill " << std::setw(10) << offset << " -> " << std::setw(10) << i << " : " << std::hex << word->raw << std::dec;
            }

            auto first = msgBegin + offset;
            auto last  = msgBegin + i;
            offset     = i+1;

            FillData(first, last, (h==Data::SpillEnd));
	    hbf_flag = 0;
	    
            if ( h == Data::SpillEnd ) {
                FinalizeSTF();
                continue;
            }

            if ( h == Data::Heartbeat ) {
                ++fHBFCounter;

                if (fSplitMethod==0) {

                    if ((fHBFCounter % fMaxHBF == 0) && (fHBFCounter>0)) {
                        //	    std::cout << "FinalizeSTF " << std::endl;
                        FinalizeSTF();
                    }
                }
            }
        }
#if 0
        if (  ) {
            if (h == Data::SpillEnd) {
                FillData(msgBegin+i, msgBegin+i+2, true);
            }
            FinalizeSTF();
            i += 1;
        }
#endif
    }

    if(true)
        std::cout << " data remains: " << (nWord - offset) << " offset =  " << offset << std::endl;

    if (offset < nWord) { // && !isSpillEnd)) {
      if(hbf_flag != 1){
        fInputPayloads.insert(fInputPayloads.end(),
                              std::make_move_iterator(msgBegin + offset),
                              std::make_move_iterator(msgBegin + nWord));
      }else {	
        fInputPayloads.insert(fInputPayloads.end(),
                              std::make_move_iterator(msgBegin + offset),
                              std::make_move_iterator(msgBegin + nWord - 1));

	fRemain = reinterpret_cast<Data::Bits*>(msgBegin + nWord - 1)->raw;
	LOG(error) << "fRemain : " << std::hex << fRemain << std::dec << std::endl;
	fa = 1;
      }
    } 

}

//______________________________________________________________________________
void
AmQStrTdcSTFBuilder::FillData(AmQStrTdc::Data::Word* first,
                              AmQStrTdc::Data::Word* last,
                              bool isSpillEnd)
{
    namespace Data = AmQStrTdc::Data;
    // construct send buffer with remained data on heap
    auto buf  = std::make_unique<decltype(fInputPayloads)>(std::move(fInputPayloads));
    auto sbuf = std::make_unique<decltype(fInputPayloads)>(std::move(fInputPayloads));    

    //    if (last != first) {
    if ( (last - first) > 1 ) {
      if(true){
            LOG(debug) << " first: "<< first << "  last: " << last;
	    LOG(debug) << " last - 1 --> "<< last - 1 ;
      }
      // insert new data to send buffer
      //        buf->insert(buf->end(), std::make_move_iterator(first), std::make_move_iterator(last));
      buf->insert(buf->end(), std::make_move_iterator(first), std::make_move_iterator(last-1));
    }

    auto delim1 = reinterpret_cast<Data::Bits*>(last-1)->raw;
    auto delim2 = reinterpret_cast<Data::Bits*>(last)->raw;

    if( delim1 != delim2 ){
      LOG(error) << "del1 : " << std::hex << delim1 << std::dec << std::endl;
      LOG(error) << "del2 : " << std::hex << delim2 << std::dec << std::endl;

      LOG(debug) << " first: "<< first << "  last: " << last;
      LOG(debug) << " last - 1 --> "<< last - 1 ;
      LOG(debug) << " hbf_flag --> "<< hbf_flag ;
      
      if(true) {
        LOG(debug) << " FillData " ;
        std::for_each(first, last+1, nestdaq::HexDump{4});
      }
      
    }
    
    if ( last != first ) {      
      sbuf->insert(sbuf->end(), std::make_move_iterator(last-1), std::make_move_iterator(last+1));
    }else if( last == first ) {

      LOG(debug) << "hbf_flag: " << hbf_flag;
      LOG(error) << "fRemain : " << std::hex << fRemain << std::dec << std::endl;      

      const auto& tRemain = reinterpret_cast<Data::Word*>(&fRemain);
      LOG(error) << "tRemain : " << std::hex 
		 << reinterpret_cast<Data::Bits*>(tRemain)->raw << std::dec << std::endl;      
      
      sbuf->insert(sbuf->end(), *tRemain);
      sbuf->insert(sbuf->end(), *last);
    }
    
    NewData();
    if (!buf->empty()) {
      fWorkingPayloads->emplace_back(nestdaq::MessageUtil::NewMessage(*this, std::move(buf)));
    }

    if(mdebug) {
        LOG(debug)
                << " single word frame : " << std::hex
                << reinterpret_cast<Data::Bits*>(last)->raw
                << std::dec << std::endl;
    }

    if (fSplitMethod!=0) {
        if ((fHBFCounter % fMaxHBF == 0) && (fHBFCounter>0)) {
            LOG(debug) << " calling FinalizeSTF() from FillData()";
            FinalizeSTF();
            NewData();
        }
    }

    /* insert 64bit*2 delimiter */
    fWorkingPayloads->emplace_back(nestdaq::MessageUtil::NewMessage(*this, std::move(sbuf)));
    
    //    if ( !sbuf->empty() && (last!=first) ) {
    //      fWorkingPayloads->emplace_back(nestdaq::MessageUtil::NewMessage(*this, std::move(sbuf)));
    //    }else if( sbuf->empty() && (last ==first) ) {
    //      fWorkingPayloads->emplace_back(NewSimpleMessage(*last));      
    //    }
    
    ////if (!isSpillEnd) {
    //    fWorkingPayloads->emplace_back(NewSimpleMessage(*last));
    ////}

}

//______________________________________________________________________________
void AmQStrTdcSTFBuilder::FinalizeSTF()
{
    //LOG(debug) << " FinalizeSTF()";
    auto stfHeader          = std::make_unique<STF::Header>();
    if (fTimeFrameIdType==TimeFrameIdType::SequenceNumberOfTimeFrames) {
        stfHeader->timeFrameId = fSTFSequenceNumber;
    } else {
        stfHeader->timeFrameId = fSTFId;
    }
    fSTFId = -1;
    stfHeader->FEMType      = fFEMType;
    stfHeader->FEMId        = fFEMId;
    stfHeader->length       = std::accumulate(fWorkingPayloads->begin(), fWorkingPayloads->end(), sizeof(STF::Header),
    [](auto init, auto& m) {
        return (!m) ? init : init + m->GetSize();
    });
    stfHeader->numMessages  = fWorkingPayloads->size();

    struct timeval curtime;
    gettimeofday(&curtime, NULL);
    stfHeader->time_sec = curtime.tv_sec;
    stfHeader->time_usec = curtime.tv_usec;

    //  std::cout << "femtype:  "<< stfHeader->FEMType << std::endl;

    //  std::cout << "sec:  "<< stfHeader->time_sec << std::endl;
    //  std::cout << "usec: "<< stfHeader->time_usec << std::endl;

    // replace first element with STF header
    fWorkingPayloads->at(0) = nestdaq::MessageUtil::NewMessage(*this, std::move(stfHeader));

    fOutputPayloads.emplace(std::move(fWorkingPayloads));

    ++fSTFSequenceNumber;
    fHBFCounter = 0;
}


//______________________________________________________________________________
bool AmQStrTdcSTFBuilder::HandleData(FairMQMessagePtr& msg, int index)
{
    namespace Data = AmQStrTdc::Data;
    //    using Bits     = Data::Bits;

    if(mdebug)
        std::cout << "HandleData() HBF " << fHBFCounter << " input message " << msg->GetSize() << std::endl;

    //Reporter::AddInputMessageSize(msg->GetSize());


    //  std::cout << "============ data in ============= "<< std::endl;
    //  auto indata_size = msg->GetSize();
    //  std::for_each(reinterpret_cast<uint64_t*>(msg->GetData()),
    //		reinterpret_cast<uint64_t*>(msg->GetData() + msg->GetSize()),
    //		nestdaq::HexDump{4});

    BuildFrame(msg, index);

    while (!fOutputPayloads.empty()) {
        // create a multipart message and move ownership of messages to the multipart message
        FairMQParts parts;
        FairMQParts dqmParts;

        bool dqmSocketExists = fChannels.count(fDQMChannelName);

        if(mdebug)
            std::cout << " send data " << fOutputPayloads.size() << std::endl;

        auto& payload = fOutputPayloads.front();

        for (auto& tmsg : *payload) {

	  //	   std::for_each(reinterpret_cast<uint64_t*>(tmsg->GetData()),
	  //			  reinterpret_cast<uint64_t*>(tmsg->GetData() + tmsg->GetSize()),
	  //			  nestdaq::HexDump{4});

            if (dqmSocketExists) {
                if (tmsg->GetSize()==sizeof(STF::Header)) {
                    FairMQMessagePtr msgCopy(fTransportFactory->CreateMessage());
                    msgCopy->Copy(*tmsg);
                    dqmParts.AddPart(std::move(msgCopy));
                } else {
		  /*
                    auto b = reinterpret_cast<Bits*>(tmsg->GetData());
                    if (b->head == Data::Heartbeat     ||
                            //	      b->head == Data::ErrorRecovery ||
			b->head == Data::Data     ||
			b->head == Data::SpillEnd) {
                        FairMQMessagePtr msgCopy(fTransportFactory->CreateMessage());
                        msgCopy->Copy(*tmsg);
                        dqmParts.AddPart(std::move(msgCopy));
		    }
		  */
		    
		  FairMQMessagePtr msgCopy(fTransportFactory->CreateMessage());
		  msgCopy->Copy(*tmsg);
		  dqmParts.AddPart(std::move(msgCopy));		  
                }
            }

            parts.AddPart(std::move(tmsg));
        }

        fOutputPayloads.pop();

        auto h = reinterpret_cast<STF::Header*>(parts.At(0)->GetData());

	
        if(fa == 1) { // for debug-begin

          std::cout << " parts size = " << parts.Size() << std::endl;
          for (int i=0; i<parts.Size(); ++i){
	    const auto& msg = parts.At(i);

	    if (i==0) {
	      auto stfh = reinterpret_cast<STF::Header*>(msg->GetData());
	      LOG(debug) << "STF " << stfh->timeFrameId << " length " << stfh->length << " header " << msg->GetSize() << std::endl;
	      auto msize = msg->GetSize();
	      std::for_each(reinterpret_cast<uint64_t*>(msg->GetData()),
			    reinterpret_cast<uint64_t*>(msg->GetData() + msize),
			    nestdaq::HexDump{4});
	    } else {
	      LOG(debug) << " body " << i << " " << msg->GetSize() << " "
			 << std::showbase << std::hex <<  msg->GetSize() << std::noshowbase<< std::dec << std::endl;
	      auto n = msg->GetSize()/sizeof(Data::Word);
	      
	      std::for_each(reinterpret_cast<Data::Word*>(msg->GetData()),
			    reinterpret_cast<Data::Word*>(msg->GetData()) + n,
			    nestdaq::HexDump{4});
	    }
          }


        } // for debug-end
	fa = 0;

        // Push multipart message into send queue
        // LOG(debug) << "send multipart message ";

        //Reporter::AddOutputMessageSize(parts);

        if (dqmSocketExists) {
            if (Send(dqmParts, fDQMChannelName) < 0) {
                // timeout
                if (NewStatePending()) {
                    LOG(info) << "Device is not RUNNING";
                    return false;
                }
                LOG(error) << "Failed to enqueue sub time frame (DQM) : FEM = " << std::hex << h->FEMId << std::dec << "  STF = " << h->timeFrameId << std::endl;
            }
        }

        auto direction = (fTimeFrameIdType==TimeFrameIdType::SequenceNumberOfTimeFrames)
                         ? (h->timeFrameId % fNumDestination)
                         : ((h->timeFrameId/fMaxHBF) % fNumDestination);

        if(mdebug)
            std::cout << "direction: " << direction << std::endl;

        unsigned int err_count = 0;
        while (Send(parts, fOutputChannelName, direction, 0) < 0) {
            // timeout
            if (NewStatePending()) {
                LOG(info) << "Device is not RUNNING";
                return false;
            }
            if( err_count < 10 )
                LOG(error) << "Failed to enqueue sub time frame (data) : FEM = " << std::hex << h->FEMId << std::dec << "  STF = " << h->timeFrameId << std::endl;

            err_count++;
        }
    }

    return true;
}

//______________________________________________________________________________
void AmQStrTdcSTFBuilder::Init()
{
    //Reporter::Instance(fConfig);
}

//______________________________________________________________________________
void AmQStrTdcSTFBuilder::InitTask()
{

    using opt = OptionKey;
    /*
    {
      // convert IP address  to 32 bit. e.g. 192.168.10.16 -> FEM id = 0xC0A80A10
      //    fFEMId = 0;

      auto femid = fConfig->GetProperty<std::string>(opt::FEMId.data());
      std::istringstream iss(femid);
      std::string token;
      int i = 3;
      while (std::getline(iss, token, '.')) {
        if (i<0)  break;
        uint32_t v = (std::stoul(token) & 0xff) << (8*i);
        // std::cout << " i = " << i << " token = " << token << " v = " << v << std::endl;
        fFEMId |= v;
        --i;
      }
      LOG(debug) << "FEM ID " << std::hex << fFEMId << std::dec << std::endl;
    }
    */

    fInputChannelName  = fConfig->GetProperty<std::string>(opt::InputChannelName.data());
    fOutputChannelName = fConfig->GetProperty<std::string>(opt::OutputChannelName.data());
    fDQMChannelName    = fConfig->GetProperty<std::string>(opt::DQMChannelName.data());

    fTimeFrameIdType = static_cast<TimeFrameIdType>(std::stoi(fConfig->GetProperty<std::string>(opt::TimeFrameIdType.data())));
    fSTFId = -1;


    //////
    FairMQMessagePtr msginfo(NewMessage());
    int nrecv=0;
    while(true) {
        if (Receive(msginfo, fInputChannelName) <= 0) {
            LOG(debug) << __func__ << " Trying to get FEMIfo " << nrecv;
            nrecv++;
        } else {
            break;
        }
    }

    //  fromFEMInfo feminfo;
    auto femInfo = reinterpret_cast<FEMInfo*>(msginfo->GetData());
    auto femID   = femInfo->FEMId;
    auto femType = femInfo->FEMType;

    LOG(info) << "magic: "<< std::hex << femInfo->magic << std::dec;
    LOG(info) << "FEMId: "<< std::hex << femID << std::dec;
    LOG(info) << "FEMType: "<< std::hex << femType << std::dec;
    ////

    fFEMId = femID;
    fFEMType = femType;

    auto s_maxHBF = fConfig->GetProperty<std::string>(opt::MaxHBF.data());
    fMaxHBF = std::stoi(s_maxHBF);
    if (fMaxHBF<1) {
        LOG(warn) << "fMaxHBF: non-positive value was specified = " << fMaxHBF;
        fMaxHBF = 1;
    }
    LOG(debug) << "fMaxHBF = " <<fMaxHBF;

    auto s_splitMethod = fConfig->GetProperty<std::string>(opt::SplitMethod.data());
    fSplitMethod = std::stoi(s_splitMethod);

    fSTFSequenceNumber = 0;
    fHBFCounter = 0;

    LOG(debug) << " output channels: name = " << fOutputChannelName
               << " num = " << GetNumSubChannels(fOutputChannelName);
    fNumDestination = GetNumSubChannels(fOutputChannelName);
    LOG(debug) << " number of desntination = " << fNumDestination;
    if (fNumDestination<1) {
        LOG(warn) << " number of destination is non-positive";
    }


    LOG(debug) << " data quality monitoring channels: name = " << fDQMChannelName;
      //	       << " num = " << fChannels.count(fDQMChannelName); 
    
    //    if (fChannels.count(fDQMChannelName)) {
    //        LOG(debug) << " data quality monitoring channels: name = " << fDQMChannelName
    //                   << " num = " << fChannels.at(fDQMChannelName).size();      
    //    }

    OnData(fInputChannelName, &AmQStrTdcSTFBuilder::HandleData);

    //Reporter::Reset();
}

//______________________________________________________________________________
void AmQStrTdcSTFBuilder::NewData()
{
    if (!fWorkingPayloads) {
        fWorkingPayloads = std::make_unique<std::vector<FairMQMessagePtr>>();
        fWorkingPayloads->reserve(fMaxHBF*2+1);
        // add an empty message, which will be replaced with sub-time-frame header later.
        fWorkingPayloads->push_back(nullptr);
    }
}

//______________________________________________________________________________
void AmQStrTdcSTFBuilder::PostRun()
{
    fInputPayloads.clear();
    fWorkingPayloads.reset();
    SendBuffer ().swap(fOutputPayloads);

    int nrecv = 0;

    while(true) {
        FairMQMessagePtr msg(NewMessage());

        if (Receive(msg, fInputChannelName) <= 0) {
	  //            LOG(debug) << __func__ << " no data received " << nrecv;
            ++nrecv;
            if (nrecv>10) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } else {
	  //            LOG(debug) << __func__ << " print data";
            //      HandleData(msg, 0);
        }
    }

    LOG(debug) << __func__ << " done";

}

namespace bpo = boost::program_options;

//______________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    using opt = AmQStrTdcSTFBuilder::OptionKey;
    options.add_options()
    (opt::FEMId.data(),             bpo::value<std::string>(),                       "FEM ID")
    (opt::InputChannelName.data(),  bpo::value<std::string>()->default_value("in"),  "Name of the input channel")
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"), "Name of the output channel")
    (opt::DQMChannelName.data(),    bpo::value<std::string>()->default_value("dqm"), "Name of the data quality monitoring")
    (opt::MaxHBF.data(),            bpo::value<std::string>()->default_value("1"),   "maximum number of heartbeat frame in one sub time frame")
    (opt::SplitMethod.data(),       bpo::value<std::string>()->default_value("0"),   "STF split method")
    (opt::TimeFrameIdType.data(),   bpo::value<std::string>()->default_value("0"),   "Time frame ID type: 0 = first HB delimiter, 1 = last HB delimiter, 2 = sequence number of time frames")
    ;
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<AmQStrTdcSTFBuilder>();
}
