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
                          ::HexDump{4});
        }
    }

    for (long unsigned int i = 0 ; i < nWord ; ++i) {
        auto word = reinterpret_cast<Data::Bits*>(msgBegin+i);

        uint8_t h = word->head;

        if(mdebug)
            LOG(debug) << " head = " << std::hex << static_cast<uint16_t>(h) << std::dec << std::endl;

        bool isHeadValid = false;
        for (auto validHead : {
	    Data::Data, Data::Heartbeat, Data::Trailer, Data::SpillOn, Data::SpillEnd
                }) {
            if (h == validHead) isHeadValid = true;
        }

        if (!isHeadValid) {

            LOG(error)   << " " << i << " " << offset << " invalid head = " << std::hex << static_cast<uint16_t>(h)
			 << " " << word->raw << std::dec << std::endl;
	    
            LOG(warn) << " " << i << " " << offset << " invalid head = " << std::hex << static_cast<uint16_t>(h)
			 << " " << word->raw << std::dec << std::endl;

            if (i - offset > 0) {
                // std::cout << " fill valid part "  << std::setw(10) << offset << " -> " << std::setw(10) << i << std::endl;
                auto first = msgBegin + offset;
                auto last  = msgBegin + i;
                std::for_each(first, last, ::HexDump{4});
                fInputPayloads.insert(fInputPayloads.end(), std::make_move_iterator(first), std::make_move_iterator(last));
            }
            offset = i+1;
            continue;
        }

	// in case of one delimiter...
	if(hbf_flag == 1){
	  
	  if ( (h != Data::Heartbeat) &&  (h != Data::SpillEnd) ) {
	    LOG(warn) << "Second word is not delimi. : " << std::hex << word->raw;

	    hbf_flag = 0;	  
	    fLastHeader = 0;

	    ///
	    auto prewb = reinterpret_cast<Data::Bits*>(msgBegin + i - 1);
	    auto preh = prewb->head;
	    
	    if( (preh == Data::Heartbeat) &&  (fHBFCounter==0) ){
	      auto rdelim = prewb->hbframe % fMaxHBF;
	      //	      LOG(debug) << "rdelim:   "<< rdelim;
	      //	      LOG(debug) << "hbfframe + 1 : "<< word->hbframe + 1;
	      //	      LOG(debug) << "fMaxHBF: "<< fMaxHBF;	      	      

	      if(rdelim != 0){
		offset   = i;
		hbf_flag = 0;
		LOG(error) << "one delim. (HBF % fMaxHBF) != 0:  HBF = " << word->hbframe
			   << " fMaxHBF = " << fMaxHBF;
	        continue;
	      }
	    }
	    ///
	    
	    auto first = msgBegin + offset;
	    auto last  = msgBegin + i - 1;
	    offset     = i;

	    int32_t delimiterFrameId = ((reinterpret_cast<Data::Bits*>(last)->hbspilln & 0xFF)<<16)
	      | (reinterpret_cast<Data::Bits*>(last)->hbframe & 0xFFFF);

	    if (fTimeFrameIdType == TimeFrameIdType::FirstHeartbeatDelimiter) {
	      // first heartbeat delimiter or first spill-off delimiter
	      if (fSTFId<0) {
		fSTFId = delimiterFrameId;
	      }
	    } else { // last heartbeat delimiter or last spill-off delimiter, or sequence number
	      fSTFId = delimiterFrameId;
	    }
	  
	    FillData(first, last, fMsgType);

            if ( preh == Data::Heartbeat ) {
                ++fHBFCounter;

                if (fSplitMethod==0) {

                    if ((fHBFCounter % fMaxHBF == 0) && (fHBFCounter>0)) {
                        FinalizeSTF();
		    }
                }
            }

	  }
	}
	/////
	
        if ((h == Data::Heartbeat) || h == Data::SpillEnd) {

	    if (fLastHeader == 0) {
                fLastHeader = h;
		hbf_flag++;

		if(h == Data::SpillEnd )
		  fdelimiterFrameId = ((word->hbspilln & 0xFF)<<16) | (word->hbframe & 0xFFFF);
		
                continue;
            } else if (fLastHeader == h) {
                fLastHeader = 0;
		hbf_flag++;
            } else {
                // unexpected @TODO
            }
	    
	    int32_t delimiterFrameId = ((word->hbspilln & 0xFF)<<16) | (word->hbframe & 0xFFFF);

	    ///
	    if( (h == Data::Heartbeat) &&  (fHBFCounter==0) ){
	      auto rdelim = word->hbframe % fMaxHBF;
	      //	      LOG(debug) << "rdelim:   "<< rdelim;
	      //	      LOG(debug) << "hbfframe + 1 : "<< word->hbframe + 1;
	      //	      LOG(debug) << "fMaxHBF: "<< fMaxHBF;	      	      

	      if(rdelim != 0){
		offset   = i+1;
		hbf_flag = 0;
		LOG(error) << "(HBF % fMaxHBF) != 0:  HBF = " << word->hbframe
			   << " fMaxHBF = " << fMaxHBF;
	        continue;
	      }
	    }
	    ///
	    
	    if(h == Data::SpillEnd){
	      delimiterFrameId = fdelimiterFrameId;
	      fdelimiterFrameId = 0;
	      
	      LOG(debug) << " spill-end delimiter comes " << std::hex << word->hbframe << ", raw = " << word->raw;
	      LOG(debug) << " delimiterFrameId:    " << std::hex << delimiterFrameId;
	    }
	    
            if (fTimeFrameIdType == TimeFrameIdType::FirstHeartbeatDelimiter) {
	      // first heartbeat delimiter or first spill-off delimiter
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

            FillData(first, last, fMsgType);
	    hbf_flag = 0;
	    
            if ( h == Data::SpillEnd ) {
                FinalizeSTF();
                continue;
            }

            if ( h == Data::Heartbeat ) {
                ++fHBFCounter;

                if (fSplitMethod==0) {

                    if ((fHBFCounter % fMaxHBF == 0) && (fHBFCounter>0)) {
                        FinalizeSTF();
		    }
                }
            }
        }
    }

    if (offset < nWord) { // && !isSpillEnd)) {

      if( (fMsgType == MsgType::separatedDelimiter) && (hbf_flag == 1) ){
	if( (nWord - offset) != 1 )
	  fInputPayloads.insert(fInputPayloads.end(),
				std::make_move_iterator(msgBegin + offset),
				std::make_move_iterator(msgBegin + nWord - 1));

	fRemain = reinterpret_cast<Data::Bits*>(msgBegin + nWord - 1)->raw;
	if(mdebug)	
	  LOG(debug) << "fRemain : " << std::hex << fRemain << std::dec << std::endl;
	
      }else {

	if(mdebug){
	  if(hbf_flag == 1){
	    fRemain = reinterpret_cast<Data::Bits*>(msgBegin + nWord - 1)->raw;	  
	    LOG(debug) << "indata fRemain : " << std::hex << fRemain << std::dec << std::endl;
	  }
	}
	
        fInputPayloads.insert(fInputPayloads.end(),
                              std::make_move_iterator(msgBegin + offset),
                              std::make_move_iterator(msgBegin + nWord));	
      }
    } 

}

//______________________________________________________________________________
void
AmQStrTdcSTFBuilder::FillData(AmQStrTdc::Data::Word* first,
                              AmQStrTdc::Data::Word* last,
                              MsgType isType)
{
    namespace Data = AmQStrTdc::Data;
    // construct send buffer with remained data on heap
    auto buf  = std::make_unique<decltype(fInputPayloads)>(std::move(fInputPayloads));
    auto sbuf = std::make_unique<decltype(fInputDelimiter)>(std::move(fInputDelimiter));


    if(isType == MsgType::separatedDelimiter){

      //      LOG(info) << "isType: separated : " ;
      
      /* for data */
      if ( ((last - first) > 1) && (hbf_flag == 2) ) { // when data + two delimiter comes (more than 3 words)
	// insert new data to send buffer
	//        buf->insert(buf->end(), std::make_move_iterator(first), std::make_move_iterator(last));

	buf->insert(buf->end(), std::make_move_iterator(first), std::make_move_iterator(last-1));
      
      }else if ( (last != first) && (hbf_flag == 1) ) {
	LOG(warn) << "just one delimiter is coming.." << std::hex << reinterpret_cast<Data::Bits*>(last)->raw;
	buf->insert(buf->end(), std::make_move_iterator(first), std::make_move_iterator(last));      
      }

      /* for delimiter */    
      if ( (last != first) && (hbf_flag == 2) ) { // two delimiter, *(last-1) and *last

	auto first_  = reinterpret_cast<Data::Bits*>(last-1)->head;
	auto second_ = reinterpret_cast<Data::Bits*>(last)->head;


	//for debug
	if( first_ == Data::SpillEnd ) {
	  //nothing
	}else if ( first_ != Data::Heartbeat || second_ != Data::Heartbeat ){
	  
	  LOG(warn) << "wrong delimiter--> first: " << std::hex<< reinterpret_cast<Data::Bits*>(last-1)->raw
		    << "    second: " << std::hex << reinterpret_cast<Data::Bits*>(last)->raw
		    << "    third? " << std::hex << reinterpret_cast<Data::Bits*>(last+1)->raw;	
	}
	//

	//	sbuf->insert(sbuf->end(), std::make_move_iterator(last-1), std::make_move_iterator(last+1));
	sbuf->insert(sbuf->end(), *(last-1));
	sbuf->insert(sbuf->end(), *last);	
      
      }else if( (last != first) && (hbf_flag == 1) ) {

	auto first_ = reinterpret_cast<Data::Bits*>(last)->raw;
	LOG(warn) << " Fill one delimiter: "<< std::hex << first_;

	sbuf->insert(sbuf->end(), *last);      	
      
      }else if( (last == first) && (hbf_flag == 2) ) { // only one *last

	const auto& tRemain = reinterpret_cast<Data::Word*>(&fRemain);

	if(mdebug){
	  LOG(debug) << "fRemain : " << std::hex << fRemain << std::dec << std::endl;      	
	  LOG(debug) << "tRemain : " << std::hex 
		     << reinterpret_cast<Data::Bits*>(tRemain)->raw << std::dec << std::endl;
	}

	auto first_  = reinterpret_cast<Data::Bits*>(tRemain)->head;
	auto second_ = reinterpret_cast<Data::Bits*>(last)->head;

	if( first_ == Data::SpillEnd ) {
	  //nothing      
	}else if ( first_ != Data::Heartbeat || second_ != Data::Heartbeat ){      
	  LOG(warn) << "wrong delimiter--> first: " << std::hex << reinterpret_cast<Data::Bits*>(tRemain)->raw
		    << "    second: " << std::hex << reinterpret_cast<Data::Bits*>(last)->raw
		    << "    third? " << std::hex  << reinterpret_cast<Data::Bits*>(last+1)->raw;	

	  LOG(warn) << "This is in case of tRemain ";
	}

	sbuf->insert(sbuf->end(), *tRemain);
	sbuf->insert(sbuf->end(), *last);
      }


      NewData();
      if (!buf->empty()) {
	fWorkingPayloads->emplace_back(MessageUtil::NewMessage(*this, std::move(buf)));
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
      if (!sbuf->empty()) {
	fWorkingPayloads->emplace_back(MessageUtil::NewMessage(*this, std::move(sbuf)));
      }
      //    (void)isSpillEnd;    
      ////if (!isSpillEnd) {
      //    fWorkingPayloads->emplace_back(NewSimpleMessage(*last));
      ////}

    }else if(isType == MsgType::indataDelimiter){
      //      LOG(info) << "isType: indata : ";
      
      /* for data */
      if ( ((last - first) > 1) && (hbf_flag == 2) ) { // when data + two delimiter comes (more than 3 words)
	// insert new data to send buffer
	//        buf->insert(buf->end(), std::make_move_iterator(first), std::make_move_iterator(last));

	buf->insert(buf->end(), std::make_move_iterator(first), std::make_move_iterator(last));
	buf->insert(buf->end(), *last);	
      
      }else if ( (last != first) && (hbf_flag == 1) ) {
	LOG(warn) << "just one delimiter is coming.." << std::hex << reinterpret_cast<Data::Bits*>(last)->raw;
	buf->insert(buf->end(), std::make_move_iterator(first), std::make_move_iterator(last));
	buf->insert(buf->end(), *last);		

      }else if ((last == first) && (hbf_flag ==2)) {
	//	fH_flag = 1;	
	buf->insert(buf->end(), *last);
      }

      NewData();
      if (!buf->empty()) {
	fWorkingPayloads->emplace_back(MessageUtil::NewMessage(*this, std::move(buf)));
      }

      
      if (fSplitMethod!=0) {
	if ((fHBFCounter % fMaxHBF == 0) && (fHBFCounter>0)) {
	  LOG(debug) << " calling FinalizeSTF() from FillData()";
	  FinalizeSTF();
	  NewData();
	}
      }
      
    }
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
    stfHeader->femType      = fFEMType;
    stfHeader->femId        = fFEMId;
    stfHeader->length       = std::accumulate(fWorkingPayloads->begin(), fWorkingPayloads->end(), sizeof(STF::Header),
    [](auto init, auto& m) {
        return (!m) ? init : init + m->GetSize();
    });
    stfHeader->numMessages  = fWorkingPayloads->size();

    struct timeval curtime;
    gettimeofday(&curtime, NULL);
    stfHeader->timeSec = curtime.tv_sec;
    stfHeader->timeUSec = curtime.tv_usec;

    //  std::cout << "femtype:  "<< stfHeader->femType << std::endl;
    //  std::cout << "sec:  "<< stfHeader->timeSec << std::endl;
    //  std::cout << "usec: "<< stfHeader->timeUSec << std::endl;

    // replace first element with STF header
    fWorkingPayloads->at(0) = MessageUtil::NewMessage(*this, std::move(stfHeader));

    fOutputPayloads.emplace(std::move(fWorkingPayloads));

    ++fSTFSequenceNumber;
    fHBFCounter = 0;
    
}


//______________________________________________________________________________
bool AmQStrTdcSTFBuilder::HandleData(FairMQMessagePtr& msg, int index)
{
    namespace Data = AmQStrTdc::Data;
    using Bits     = Data::Bits;

    if(mdebug)
        std::cout << "HandleData() HBF " << fHBFCounter << " input message " << msg->GetSize() << std::endl;

    //Reporter::AddInputMessageSize(msg->GetSize());


    //  std::cout << "============ data in ============= "<< std::endl;
    //  auto indata_size = msg->GetSize();
    //  std::for_each(reinterpret_cast<uint64_t*>(msg->GetData()),
    //		reinterpret_cast<uint64_t*>(msg->GetData() + msg->GetSize()),
    //		::HexDump{4});

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
	  //      std::for_each(reinterpret_cast<uint64_t*>(tmsg->GetData()),
	  //                    reinterpret_cast<uint64_t*>(tmsg->GetData() + tmsg->GetSize()),
	  //                    ::HexDump{4});


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

	/// for debug

	if(fMsgType == MsgType::separatedDelimiter){
	  
	  const auto part_size = parts.Size();
	  auto& smsg = parts.At(part_size - 1);
	  auto n   = smsg->GetSize()/sizeof(Data::Word);	
	  auto b   = reinterpret_cast<Bits*>(smsg->GetData());

	  //	if( (n == 2) && b->head == Data::SpillEnd ) {
	  if( b->head == Data::SpillEnd ) {	
	    //nothing
	  
	  }else if ( b->head != Data::Heartbeat ){
	    LOG(error) << "=== Wrong last message === " ;
	    LOG(error) << "size: " << part_size ;	  	  	  
	    std::for_each(reinterpret_cast<Data::Word*>(smsg->GetData()),
			  reinterpret_cast<Data::Word*>(smsg->GetData()) + n,
			  ::HexDump{4});	
	  }

	}

	/*	
#if 1	
	{ // for debug-begin
          std::cout << " parts size = " << parts.Size() << std::endl;
          for (int i=0; i<parts.Size(); ++i){
	    const auto& msg = parts.At(i);

	    if (i==0) {
	      auto stfh = reinterpret_cast<STF::Header*>(msg->GetData());
	      LOG(debug) << "STF " << stfh->timeFrameId << " length " << stfh->length << " header " << msg->GetSize() << std::endl;
	      auto msize = msg->GetSize();
	      std::for_each(reinterpret_cast<uint64_t*>(msg->GetData()),
			    reinterpret_cast<uint64_t*>(msg->GetData() + msize),
			    ::HexDump{4});
	    } else {
	      LOG(debug) << " body " << i << " " << msg->GetSize() << " "
			 << std::showbase << std::hex <<  msg->GetSize() << std::noshowbase<< std::dec << std::endl;
	      auto n = msg->GetSize()/sizeof(Data::Word);

	      std::for_each(reinterpret_cast<Data::Word*>(msg->GetData()),
			    reinterpret_cast<Data::Word*>(msg->GetData()) + n,
			    ::HexDump{4});
	    }
	  }

	} // for debug-end
    //	fH_flag = 0;
#endif	
	*/
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
                LOG(error) << "Failed to enqueue sub time frame (DQM) : FEM = " << std::hex << h->femId << std::dec << "  STF = " << h->timeFrameId << std::endl;
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
                LOG(error) << "Failed to enqueue sub time frame (data) : FEM = " << std::hex << h->femId << std::dec << "  STF = " << h->timeFrameId << std::endl;

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

    fMsgType = static_cast<MsgType>(std::stoi(fConfig->GetProperty<std::string>(opt::MsgType.data())));
    
    fTimeFrameIdType = static_cast<TimeFrameIdType>(std::stoi(fConfig->GetProperty<std::string>(opt::TimeFrameIdType.data())));
    
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
    auto femID   = femInfo->femId;
    auto femType = femInfo->femType;

    LOG(info) << "magic: "<< std::hex << femInfo->magic << std::dec;
    LOG(info) << "femId: "<< std::hex << femID << std::dec;
    LOG(info) << "femType: "<< std::hex << femType << std::dec;
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
    fInputDelimiter.clear();
    fWorkingPayloads.reset();
    SendBuffer ().swap(fOutputPayloads);

    fLastHeader = 0;
    hbf_flag = 0;
    
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
void AmQStrTdcSTFBuilder::PreRun()
{
  fRemain = 0;
  fHBFCounter = 0;
  hbf_flag = 0;
  fdelimiterFrameId = 0;
  fSTFSequenceNumber = 0;
  fLastHeader = 0;
  fSTFId = -1;
}

//______________________________________________________________________________
void addCustomOptions(bpo::options_description& options)
{
    using opt = AmQStrTdcSTFBuilder::OptionKey;
    options.add_options()
    (opt::FEMId.data(),             bpo::value<std::string>(),                        "FEM ID")
    (opt::InputChannelName.data(),  bpo::value<std::string>()->default_value("in"),   "Name of the input channel")
    (opt::OutputChannelName.data(), bpo::value<std::string>()->default_value("out"),  "Name of the output channel")
    (opt::DQMChannelName.data(),    bpo::value<std::string>()->default_value("dqm"),  "Name of the data quality monitoring")
    (opt::MaxHBF.data(),            bpo::value<std::string>()->default_value("1"),    "maximum number of heartbeat frame in one sub time frame")
    (opt::MsgType.data(),           bpo::value<std::string>()->default_value("0"),    "type of the data message")      
    (opt::SplitMethod.data(),       bpo::value<std::string>()->default_value("0"),    "STF split method")
    (opt::TimeFrameIdType.data(),   bpo::value<std::string>()->default_value("0"),    "Time frame ID type: 0 = first HB delimiter, 1 = last HB delimiter, 2 = sequence number of time frames")
    ;
}

//______________________________________________________________________________
std::unique_ptr<fair::mq::Device> getDevice(FairMQProgOptions&)
{
    return std::make_unique<AmQStrTdcSTFBuilder>();
}
