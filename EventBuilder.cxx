/**
 * @file EventBuilder.h 
 * @brief EventBuilder for NestDAQ
 * @date Created :
 *       Last Modified : 2023/07/21 10:06:28
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */

#include "EventBuilder.h"
#include "fairmq/runDevice.h"

#include "utility/MessageUtil.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "UnpackTdc.h"


constexpr int  nTimeBins = 131072;

 using nestdaq::EventBuilder;
 namespace bpo = boost::program_options;



 EventBuilder::EventBuilder()
 {

 }

// initialize options
void EventBuilder::InitTask()
{

	using opt = OptionKey;

	fInputChannelName  = fConfig->GetValue<std::string>(opt::InputChannelName.data());
	fOutputChannelName = fConfig->GetValue<std::string>(opt::OutputChannelName.data());   
   // inptut channels 
   LOG(info)
      << "InitTask : Input Channel  = " << fInputChannelName
      << "InitTask : Output Channel = " << fOutputChannelName;

   // identity
   fName = fConfig->GetProperty<std::string>("id");
	std::istringstream ss(fName.substr(fName.rfind("-") + 1));
	ss >> fId;


	fNumDestination = GetNumSubChannels(fOutputChannelName);
	fPollTimeoutMS  = std::stoi(fConfig->GetProperty<std::string>(opt::PollTimeout.data())); 
   fTrefID  = std::stol(fConfig->GetProperty<std::string>(opt::TrefID.data()),nullptr,16);   
   fTrefCH  = std::stoi(fConfig->GetProperty<std::string>(opt::TrefCH.data()));   
   
   LOG(info) << "TrefID : 0x" << std::hex << fTrefID << ", TrefCH : " << fTrefCH;

   LOG(info) << "InitTask : id = " << fName;

   // prepare time bin with 4 ns resolution
   // 
   fTref.resize(0);
   fTDCIdx.resize(nTimeBins,-1);
}




bool EventBuilder::ConditionalRun()
{
	std::chrono::system_clock::time_point sw_start, sw_end;

   // Receive data


   FairMQParts inParts;
   FairMQParts outParts;

   
   if (Receive(inParts, fInputChannelName,0,1000) <= 0) return false;
   // inParts should have at least two messages
   assert (inParts.Size() >= 2);

   ////////////////////////////////////////////////////
   // analyze the data
   ////////////////////////////////////////////////////   

   struct stf {
      struct SubTimeFrame::Header *header;
      std::vector<uint64_t*> data;
      std::vector<uint64_t> datasize;
      std::vector<uint64_t*> hbd;
      std::vector<std::vector<uint32_t>> trefs;
      FairMQMessage* headerPtr;
      std::vector<FairMQMessage*> hbdPtr;
   };



   //
   std::vector<std::vector<uint32_t>> target = { {fTrefID, fTrefCH} };


   std::vector<struct stf> stfs;
   FairMQMessage* tfbhdrPtr;
      
   for (uint32_t iPart = 0, nParts = inParts.Size(); iPart < nParts; ++iPart) {
      auto& part = inParts[iPart];
      auto headertype = *reinterpret_cast<uint64_t *>(part.GetData());

      if (headertype == Filter::Magic) {
         continue;
      }

      // count the number of subtime frames and the heartbeat frames for  each FEM
      if (headertype == TimeFrame::Magic) {

         sw_start = std::chrono::system_clock::now();

         auto tfbheader = reinterpret_cast<struct TimeFrame::Header*>(part.GetData());
         tfbhdrPtr = &part;
#if 0         
         LOG(info) << "TFB  found " << std::hex << tfbheader->magic << std::dec << " in Part[" << iPart << "]";
         LOG(info) << "    numSources = " << tfbheader->numSource << ", length = " << tfbheader->length ;
#endif         
         stfs.resize(tfbheader->numSource);
#if 0         
            for (int itbin = 0; itbin < nTimeBins; ++itbin) {
                  fTDCData[itbin].resize(tfbheader->numSource);
            }
#endif            


         // loop for all the sources
         for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
            iPart++;
            if (iPart >= nParts) {
               LOG(warn) << "num part is not different";
               break;
            }
            auto& part = inParts[iPart];
            auto header = reinterpret_cast<struct SubTimeFrame::Header*>(part.GetData());
            if (header->magic != SubTimeFrame::Magic) {
               LOG(warn) << "header magic is required " << std::hex << header->magic; 
               break;
            }
#if 0            
            LOG(info) << "STFB found " << std::hex <<  header->magic << std::dec << " in Part[" << iPart << "]";
            LOG(info) << "    FEMType = " << header->FEMType << ", FEMID = " << std::hex <<  header->FEMId ;
            LOG(info) << "    numMessages = " << header->numMessages << ", length = " << header->length ;         
#endif
            auto& stf = stfs[iSrc];
            stf.header = header;
            stf.headerPtr = &part;

            // loop for all the messages 
            for (int iMsg = 0, nMsg = header->numMessages - 1; iMsg < nMsg; ++iMsg) {
               iPart++;
               if (iPart >= nParts) {
                  LOG(warn) << "num part is different";
                  break;
               }
               auto& part = inParts[iPart];
               auto header = reinterpret_cast<struct SubTimeFrame::Header*>(part.GetData());
               if (header->magic == SubTimeFrame::Magic) {
                  LOG(warn) << "numMessages is not correct used " << iMsg << " instead of  " << header->numMessages;
                  iPart--;
                  break;
               }

               // check if the heartbeat delimiter or spill-end delimiter exists

               auto  data = reinterpret_cast<uint64_t*>(part.GetData());
#if 0               
               LOG(info) << "  data? " << std::hex << *data << std::dec << " in Part[" << iPart << "]";
#endif               
               if (!IsEndDelimiter(*data)) {
                  stf.datasize.emplace_back(part.GetSize());
                  stf.data.emplace_back(data);
                  iMsg++;
                  iPart++;
                  data = reinterpret_cast<uint64_t*>(inParts[iPart].GetData());
#if 0                  
                  LOG(info) << "  hbd?  " << std::hex << *data << std::dec << " in Part[" << iPart << "]";
#endif                  
                  stf.hbd.emplace_back(data);
                  stf.hbdPtr.emplace_back(&inParts[iPart]);
               } else {
                  stf.data.emplace_back(nullptr);
                  stf.datasize.emplace_back(0);
                  stf.hbd.emplace_back(data);
                  stf.hbdPtr.emplace_back(&part);
               }
            }
         }
      sw_end = std::chrono::system_clock::now();
		uint32_t elapse = std::chrono::duration_cast<std::chrono::microseconds>(
			sw_end - sw_start).count();
			std::cout << "#Elapse: " << std::dec << elapse << " us" << std::endl;

   sw_start = std::chrono::system_clock::now();
         // calculate time references
         struct tdc64 tdc;
         int (* unpack[4]) (uint64_t data, struct tdc64 *tdc) = {
            nullptr, TDC64L::v1::Unpack, TDC64H::Unpack, TDC64L::v2::Unpack
         };

         int nframe = stfs[0].data.size();

         // search for TDC data for each heartbeat frames
         for (int iframe = 0; iframe < nframe; ++iframe) {
            fTref.erase(std::cbegin(fTref),std::cend(fTref));
	                fTDCData.clear();
			//            fTDCData.resize(0);
            fTDCIdx.assign(nTimeBins,-1);
#if 0            
            for (int itbin = 0; itbin < nTimeBins; ++itbin) {
               for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
                  fTDCData[itbin][iSrc].clear();
               }
            }
#endif             
      sw_end = std::chrono::system_clock::now();
		 elapse = std::chrono::duration_cast<std::chrono::microseconds>(
			sw_end - sw_start).count();
			std::cout << "#clear Elapse: " << std::dec << elapse << " us" << std::endl;
   sw_start = std::chrono::system_clock::now();

            // store TDC data and time references
            for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
               const auto& stf = stfs[iSrc];
#if 0               
               LOG(info) << "num data = " << stf.data.size() << " in " << std::hex << stf.header->FEMId << "(type = " << stf.header->FEMType << ")";
#endif               
               auto femtype = stf.header->FEMType;
               uint32_t size = stf.datasize[iframe];
               for (int idata = 0; idata < size; ++idata) {
                  unpack[femtype](stf.data[iframe][idata],&tdc);
                  if (tdc.tdc == -1) continue;  // not a TDC data
                  if (fTDCIdx[tdc.tdc4n] == -1) {
                     fTDCData.emplace_back(std::vector<std::vector<uint64_t>>(nSrc,std::vector<uint64_t>()));
                     fTDCIdx[tdc.tdc4n] = fTDCData.size() - 1;
                  }
                  fTDCData[fTDCIdx[tdc.tdc4n]][iSrc].push_back(stf.data[iframe][idata]);
                  if (stf.header->FEMId != target[0][0]) continue;
                  if (tdc.ch != target[0][1]) continue;
#if 0                  
                  LOG(info) << "target found at " << iSrc << "/" << iframe << "/" << " tdc.ch = " << tdc.ch << " tdc.tdc = " << tdc.tdc;
#endif                  
                  fTref.emplace_back(tdc.tdc4n);
               }
            }
#if 0            
            LOG(info) << fTref.size() << " targets found";
#endif            
      sw_end = std::chrono::system_clock::now();
		 elapse = std::chrono::duration_cast<std::chrono::microseconds>(
			sw_end - sw_start).count();
			std::cout << "#Calc tref Elapse: " << std::dec << elapse << " us" << std::endl;

sw_start = std::chrono::system_clock::now();            
            //////////////////////////////////////////////////
            // prepare output for this frame 
            //////////////////////////////////////////////////
            // prepare timeframe for HBD block
            FairMQMessagePtr msgTFBHDR(NewMessage(sizeof(TimeFrame::Header)));
            auto tfbhdr = reinterpret_cast<struct TimeFrame::Header*>(msgTFBHDR->GetData());
            *tfbhdr = *tfbheader;
            outParts.AddPart(std::move(msgTFBHDR));
            int totalSize = sizeof(TimeFrame::Header);
            // prepare heartbeat frames
            for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
               FairMQMessagePtr msgSTFBHDR(NewMessage(sizeof(SubTimeFrame::Header)));
               auto stfheader = reinterpret_cast<struct SubTimeFrame::Header*>(msgSTFBHDR->GetData());
               *stfheader = *stfs[iSrc].header;
               stfheader->length = sizeof(SubTimeFrame::Header) + sizeof(uint64_t) * 2; 
               totalSize += stfheader->length;
               outParts.AddPart(std::move(msgSTFBHDR));
               FairMQMessagePtr msgHBD(fTransportFactory->CreateMessage());
               msgHBD->Copy(*stfs[iSrc].hbdPtr[iframe]);
               outParts.AddPart(std::move(msgHBD));
            }
            tfbhdr->length = totalSize;

            
            for (int iref = 0, nrefs = fTref.size(); iref < nrefs; iref++) {
               int tref = fTref[iref];
               FairMQMessagePtr msgTFBHDR(NewMessage(sizeof(TimeFrame::Header)));
               auto tfbhdr = reinterpret_cast<struct TimeFrame::Header*>(msgTFBHDR->GetData());
               *tfbhdr = *tfbheader;
               outParts.AddPart(std::move(msgTFBHDR));
               totalSize = sizeof(TimeFrame::Header);
               int idx = outParts.Size() - 1;
               for (int iSrc = 0, nSrc = tfbheader->numSource; iSrc != nSrc; ++iSrc) {
                  FairMQMessagePtr msgSTFBHDR(NewMessage(sizeof(SubTimeFrame::Header)));
                  auto stfheader = reinterpret_cast<struct SubTimeFrame::Header*>(msgSTFBHDR->GetData());
                  *stfheader = *stfs[iSrc].header;
                  stfheader->length = sizeof(SubTimeFrame::Header) + sizeof(uint64_t) * 2; 
                  std::vector<uint64_t> outtdc;
                  for (int itdc = std::max(tref - 250, 0), ntdc = std::min(nTimeBins,tref + 250); itdc < ntdc; ++itdc) {

                     if (fTDCIdx[itdc] == -1 || fTDCData[fTDCIdx[itdc]][iSrc].size() == 0) continue;
#if 0                     
                     LOG(info) << fTDCData[itdc][iSrc].size() << " found in iSrc = " << iSrc ;
#endif                     
                     for (int idata = 0, ndata = fTDCData[fTDCIdx[itdc]][iSrc].size(); idata < ndata; ++idata ) {
                        outtdc.emplace_back(fTDCData[fTDCIdx[itdc]][iSrc][idata]);
                     }
                  }
                  stfheader->length = sizeof(SubTimeFrame::Header) + sizeof(uint64_t) * outtdc.size();
                  outParts.AddPart(std::move(msgSTFBHDR));
                  totalSize += stfheader->length;
                  if (outtdc.size() > 0) {
                     FairMQMessagePtr msgTDC(NewMessage(sizeof(uint64_t) * outtdc.size()));
                     for (uint32_t itdc = 0, ntdc = outtdc.size(); itdc < ntdc; ++itdc) {
                        *(((uint64_t*)msgTDC->GetData()) + itdc) = outtdc[itdc];
                     }
                     outParts.AddPart(std::move(msgTDC));
                  }
               }
               tfbhdr->length = totalSize;
            }
      sw_end = std::chrono::system_clock::now();
		 elapse = std::chrono::duration_cast<std::chrono::microseconds>(
			sw_end - sw_start).count();
			std::cout << "#Output  Elapse: " << std::dec << elapse << " us" << std::endl;

         }         

         // 

         continue;
      }

//      LOG(info) << "Data found with a size of " << part.GetSize()<< " in Part[" << iPart << "]";
      LOG(warn) << "Unexpected";
   }

#if 0
   ////////////////////////////////////////////////////
   // (test) copy all the data
   ////////////////////////////////////////////////////   
   unsigned int msg_size = inParts.Size();
   for (unsigned int ii = 0 ; ii < msg_size ; ii++) {
      FairMQMessagePtr msgCopy(fTransportFactory->CreateMessage());
      msgCopy->Copy(inParts.AtRef(ii));
      outParts.AddPart(std::move(msgCopy));
   }   
#endif
   ////////////////////////////////////////////////////
   // Transfer the data to all of output channel
   ////////////////////////////////////////////////////
   auto poller = NewPoller(fOutputChannelName);
   while (!NewStatePending()) {
      auto direction = (fDirection++) % fNumDestination;
      poller->Poll(fPollTimeoutMS);
      if (poller->CheckOutput(fOutputChannelName, direction)) {
         if (Send(outParts, fOutputChannelName, direction) > 0) {
            // successfully sent
            break;
         } else {
            LOG(error) << "Failed to queue output-channel";
         }
      }
      if (fNumDestination==1) {
         std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
   }
   return true;
}

void EventBuilder::PostRun()
{
   
}


////////////////////////////////////////////////////
// override runDevice 
////////////////////////////////////////////////////

void addCustomOptions(bpo::options_description& options)
{
	using opt = EventBuilder::OptionKey;

	options.add_options()
		//("max-iterations", bpo::value<uint64_t>()->default_value(0),
		//"Maximum number of iterations of Run/ConditionalRun/OnData (0 - infinite)")
		(opt::InputChannelName.data(),
			bpo::value<std::string>()->default_value("in"),
			"Name of the input channel")
		(opt::OutputChannelName.data(),
			bpo::value<std::string>()->default_value("out"),
			"Name of the output channel")
		(opt::DQMChannelName.data(),
			bpo::value<std::string>()->default_value("dqm"),
			"Name of the data quality monitoring channel")
		(opt::DataSuppress.data(),
			bpo::value<std::string>()->default_value("true"),
			"Data suppression enable")
//		(opt::RemoveHB.data(),
//			bpo::value<std::string>()->default_value("false"),
//			"Remove HB without hit")
		(opt::PollTimeout.data(), 
			bpo::value<std::string>()->default_value("1"),
			"Timeout of polling (in msec)")
		(opt::SplitMethod.data(),
			bpo::value<std::string>()->default_value("1"),
			"STF split method")
      (opt::TrefID.data(),
			bpo::value<std::string>()->default_value("0xc0a82a8"),
         "FEM ID (hex) for time reference")
      (opt::TrefCH.data(),
			bpo::value<std::string>()->default_value("42"),
         "FEM Channel for time reference")
    		;

}



std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
	return std::make_unique<EventBuilder>();
}
