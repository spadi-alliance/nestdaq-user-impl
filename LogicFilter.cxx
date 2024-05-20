/********************************************************************************
 *    Copyright (C) 2014 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *										*
 *	      This software is distributed under the terms of the		*
 *	      GNU Lesser General Public Licence (LGPL) version 3,		*
 *		  copied verbatim in the file "LICENSE"				*
 ********************************************************************************/

#include <fairmq/Device.h>
#include <fairmq/runDevice.h>

#include <iostream>
#include <iomanip>
#include <string>
#include <chrono>

#include <unordered_map>
#include <unordered_set>

#include <cstring>

#include "utility/MessageUtil.h"

//#include "HulStrTdcData.h"
#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "HeartbeatFrameHeader.h"
#include "FilterHeader.h"

#include "SignalParser.cxx"
#include "KTimer.cxx"
#include "Trigger.cxx"


//std::atomic<int> gQdepth = 0;

namespace bpo = boost::program_options;

struct LogicFilter : fair::mq::Device
{
	struct OptionKey {
		static constexpr std::string_view InputChannelName   {"in-chan-name"};
		static constexpr std::string_view OutputChannelName  {"out-chan-name"};
		static constexpr std::string_view DQMChannelName     {"dqm-chan-name"};
		static constexpr std::string_view DataSuppress       {"data-suppress"};
		static constexpr std::string_view RemoveHB           {"remove-hb"};
		static constexpr std::string_view PollTimeout        {"poll-timeout"};
		static constexpr std::string_view SplitMethod        {"split"};

		static constexpr std::string_view TriggerSignals     {"trigger-signals"};
		static constexpr std::string_view TriggerFormula     {"trigger-formula"};
		static constexpr std::string_view TriggerWidth       {"trigger-width"};
	};

	struct DataBlock {
		uint32_t femId;
		uint32_t Type;
		uint32_t timeFrameId;
		int HBFrame;
		bool is_HB;
		int msg_index;
		int nTrig;
	};

	LogicFilter()
	{
		// register a handler for data arriving on "data" channel
		//OnData("in", &LogicFilter::HandleData);

		fTrig = new Trigger();
		fKt1 = new KTimer(1000);
		fKt2 = new KTimer(1000);
		fKt3 = new KTimer(1000);
		fKt4 = new KTimer(1000);
	}

	void InitTask() override;
#if 0
	void InitTask() override
	{
		using opt = OptionKey;

		// Get the fMaxIterations value from the command line options (via fConfig)
		fMaxIterations = fConfig->GetProperty<uint64_t>("max-iterations");
		fInputChannelName  = fConfig->GetValue<std::string>(opt::InputChannelName.data());
		fOutputChannelName = fConfig->GetValue<std::string>(opt::OutputChannelName.data());
		LOG(info) << "InitTask: Input Channel : " << fInputChannelName
			<< " Output Channel : " << fOutputChannelName;

		//OnData(fInputChannelName, &LogicFilter::HandleData);
	}
#endif

#if 0
	bool HandleData(fair::mq::MessagePtr& msg, int)
	{
		LOG(info) << "Received: \""
			<< std::string(static_cast<char*>(msg->GetData()), msg->GetSize())
			<< "\"";

		if (fMaxIterations > 0 && ++fNumIterations >= fMaxIterations) {
			LOG(info) << "Configured maximum number of iterations reached."
				<< " Leaving RUNNING state.";
			return false;
		}

		// return true if you want the handler to be called again
		// (otherwise return false go to the Ready state)
		return true;
	}
#endif

	
	bool ConditionalRun() override;
	void PostRun() override;

	bool CheckData(fair::mq::MessagePtr&);

private:
	int IsHartBeat(uint64_t, uint32_t);
	int RemoveData(fair::mq::Parts &, int);
	void CheckMultiPart(FairMQParts &);
	int MakeBlockMap(FairMQParts &,
		std::vector< std::vector<struct DataBlock> > &);
	int MakeBlockMap2(FairMQParts &,
		std::vector< std::vector<struct DataBlock> > &);
	int BuildHBF(
		std::vector<struct HBFIndex> &,
		std::vector< std::vector<struct DataBlock> > &,
		FairMQParts &,
		int);
	int AddFilterMessage(
		FairMQParts &,
		std::vector< std::vector<uint32_t> > &,
		uint32_t,
		uint32_t);
	
	int MarkFlagSending(
		std::vector< std::vector<struct DataBlock> > &block_map,
		int i,
		int nhits,
		std::vector<bool> &flag_sending);

	std::string fInputChannelName;
	std::string fOutputChannelName;
	std::string fDQMChannelName;
	std::string fName;

	int fNumDestination {0};
	uint32_t fDirection {0};
	int fPollTimeoutMS  {0};
	int fSplitMethod    {2};

	uint32_t fId {0};
	Trigger *fTrig;
	bool fIsDataSuppress = true;
	bool fIsRemoveHB = false;

	int fHBflag = 0;

	KTimer *fKt1;
	KTimer *fKt2;
	KTimer *fKt3;
	KTimer *fKt4;
};


void LogicFilter::InitTask()
{
	using opt = OptionKey;

	fInputChannelName  = fConfig->GetValue<std::string>(opt::InputChannelName.data());
	fOutputChannelName = fConfig->GetValue<std::string>(opt::OutputChannelName.data());
	fDQMChannelName    = fConfig->GetValue<std::string>(opt::DQMChannelName.data());

	LOG(info) << "InitTask: Input Channel : " << fInputChannelName
		<< " Output Channel : " << fOutputChannelName;

	fNumDestination = GetNumSubChannels(fOutputChannelName);
	fPollTimeoutMS  = std::stoi(fConfig->GetProperty<std::string>(opt::PollTimeout.data()));

	fName = fConfig->GetProperty<std::string>("id");
	std::istringstream ss(fName.substr(fName.rfind("-") + 1));
	ss >> fId;

	fSplitMethod    = std::stoi(
		fConfig->GetProperty<std::string>(opt::SplitMethod.data()));
	LOG(info) << "InitTask: SplitMethod : " << fSplitMethod;

	std::string sIsDataSuppress = fConfig->GetValue<std::string>(opt::DataSuppress.data());
	if (sIsDataSuppress == "true") {
		fIsDataSuppress = true;
	} else {
		fIsDataSuppress = false;
	}
	LOG(info) << "InitTask: DataSuppress : " << fIsDataSuppress;

	std::string sIsRemoveHB = fConfig->GetValue<std::string>(opt::RemoveHB.data());
	if (sIsRemoveHB == "true") {
		fIsRemoveHB = true;
	} else {
		fIsRemoveHB = false;
	}
	LOG(info) << "InitTask: RemoveHB : " << fIsRemoveHB;


	fTrig->SetTimeRegion(1024 * 128);
	fTrig->ClearEntry();

	std::string str_signals = fConfig->GetProperty<std::string>(opt::TriggerSignals.data());
	std::string formula = fConfig->GetProperty<std::string>(opt::TriggerFormula.data());
	int window_width = std::stoi(fConfig->GetProperty<std::string>(opt::TriggerWidth.data()));

	std::vector< std::vector<uint32_t> > signals = SignalParser::Parsing(str_signals);
	int i = 0;
	for (auto &v : signals) {
		if (v.size() >= 3) {
			fTrig->Entry(v[0], v[1], v[2]);
			union ipval {
				uint32_t u32;
				char c[4];
			};
			ipval mid; mid.u32 = v[0];
			//LOG(info) << "Module: " << std::hex << v[0] << ", Channel: " << v[1] << ", Offset: " << v[2];
			LOG(info) << std::setw(4) << i << ": "
				<< "M_id: " << static_cast<unsigned int>(mid.c[3] & 0xff)
				<< "."      << static_cast<unsigned int>(mid.c[2] & 0xff)
				<< "."      << static_cast<unsigned int>(mid.c[1] & 0xff)
				<< "."      << static_cast<unsigned int>(mid.c[0] & 0xff)
				<< ", Ch.: " << std::setw(3) << v[1] << ", Offset: " << std::setw(6) << v[2];
			i++;
		}
	}
	
	LOG(info) << "Formula: " << formula;
	fTrig->MakeTable(formula);

	LOG(info) << "Trigger windows width: " << window_width;
	fTrig->SetMarkLen(window_width);

}

bool LogicFilter::CheckData(fair::mq::MessagePtr &msg)
{
	unsigned int msize = msg->GetSize();
	unsigned char *pdata = reinterpret_cast<unsigned char *>(msg->GetData());
	uint64_t msg_magic = *(reinterpret_cast<uint64_t *>(pdata));

	static int fe_type = 0;

	std::cout << "#Msg Top(8B): " << std::hex << msg_magic
		<< " Size: " << std::dec << msize << std::endl;

	if (msg_magic == TimeFrame::MAGIC) {
		TimeFrame::Header *ptf = reinterpret_cast<TimeFrame::Header *>(pdata);
		std::cout << "#TF Header "
			<< std::hex << std::setw(16) << std::setfill('0') <<  ptf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  ptf->timeFrameId
			<< " Nsource: " << std::setw(8) << std::setfill('0') <<  ptf->numSource
			<< " len: " << std::dec <<  ptf->length
			<< std::endl;
		#if 0
		for (unsigned int i = 0 ; i < ptf->length ; i++) {
			if ((i % 16) == 0) {
				std::cout << std::endl
					<< "#" << std::setw(8) << std::setfill('0')
					<< i << " : ";
			}
			std::cout << " "
				<< std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<unsigned int>(pdata[i]);
		}
		std::cout << std::endl;
		#endif
		
	} else if (msg_magic == SubTimeFrame::MAGIC) {
		SubTimeFrame::Header *pstf
				= reinterpret_cast<SubTimeFrame::Header *>(pdata);
		std::cout << "#STF Header "
			<< std::hex << std::setw(8) << std::setfill('0') <<  pstf->magic
			<< " id: " << std::setw(8) << std::setfill('0') <<  pstf->timeFrameId
			<< " Type: " << std::setw(8) << std::setfill('0') <<  pstf->femType
			<< " FE: " << std::setw(8) << std::setfill('0') <<  pstf->femId
			<< std::endl << "# "
			<< " len: " << std::dec <<  pstf->length
			<< " nMsg: " << std::dec <<  pstf->numMessages
			<< std::endl << "# "
			<< " Ts: " << std::dec << pstf->timeSec
			<< " Tus: " << std::dec << pstf->timeUSec
			<< std::endl;

		fe_type = pstf->femType;

		//// toriaezu debug no tameni ireru. atodekesukoto
		//fe_type = 1;
		//pstf->femType = 1;
		//pstf->femId = 1234;
		////

	} else {
	       #if 1
		for (unsigned int j = 0 ; j < msize ; j += 8) {
			std::cout << "# " << std::setw(8) << j << " : "
				<< std::hex << std::setw(2) << std::setfill('0')
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 7]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 6]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 5]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 4]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 3]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 2]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 1]) << " "
				<< std::setw(2) << static_cast<unsigned int>(pdata[j + 0]) << " : ";

			if ((pdata[j + 7] & 0xfc) == (TDC64H::T_TDC << 2)) {
				std::cout << "TDC ";
				uint64_t *dword = reinterpret_cast<uint64_t *>(&(pdata[j]));
				if (fe_type == SubTimeFrame::TDC64H) {
					struct TDC64H::tdc64 tdc;
					TDC64H::Unpack(*dword, &tdc);
					std::cout << "H :" 
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
				} else
				if (fe_type == SubTimeFrame::TDC64L) {
					struct TDC64L::tdc64 tdc;
					TDC64L::Unpack(*dword, &tdc);
					std::cout << "L :" 
						<< " CH: " << std::dec << std::setw(3) << tdc.ch
						<< " TDC: " << std::setw(7) << tdc.tdc << std::endl;
				} else {
					std::cout << "UNKNOWN Type :0x" << std::hex << fe_type << std::endl;
				}

			} else if ((pdata[j + 7] & 0xfc) == (TDC64H::T_HB << 2)) {
				std::cout << "Hart beat" << std::endl;
			} else if ((pdata[j + 7] & 0xfc) == (TDC64H::T_SPL_START << 2)) {
				std::cout << "SPILL Start" << std::endl;
			} else if ((pdata[j + 7] & 0xfc) == (TDC64H::T_SPL_END << 2)) {
				std::cout << "SPILL End" << std::endl;
			} else {
				std::cout << std::endl;
			}
		}
		#else
		std::cout << "#Unknown Header " << std::hex << msg_magic << std::endl;
		#endif
	}


	return true;
}

int LogicFilter::IsHartBeat(uint64_t val, uint32_t type)
{
	int hbflag = -1;
	int hbframe = -1;
	if (type == SubTimeFrame::TDC64H) {
		struct TDC64H::tdc64 tdc;
		if (TDC64H::Unpack(val, &tdc) == TDC64H::T_HB) {
			hbframe = tdc.hartbeat;
			hbflag = tdc.flag;
		}
	} else
	if (type == SubTimeFrame::TDC64L) {
		struct TDC64L::tdc64 tdc;
		if (TDC64L::Unpack(val, &tdc) == TDC64L::T_HB) {
			hbframe = tdc.hartbeat;
			hbflag = tdc.flag;
		}
	} else
	if (type == SubTimeFrame::TDC64L_V1) {
		struct TDC64L::tdc64 tdc;
		if (TDC64L::v1::Unpack(val, &tdc) == TDC64L::v1::T_HB) {
			hbframe = tdc.hartbeat;
			hbflag = tdc.flag;
		}
	} else
	if (type == SubTimeFrame::TDC64H_V3) {
		struct TDC64H_V3::tdc64 tdc;
		if (TDC64H_V3::Unpack(val, &tdc) == TDC64H_V3::T_HB1) {
			hbframe = tdc.hartbeat;
			hbflag = tdc.flag;
		}
	} else
	if (type == SubTimeFrame::TDC64L_V3) {
		struct TDC64L_V3::tdc64 tdc;
		if (TDC64L_V3::Unpack(val, &tdc) == TDC64L_V3::T_HB1) {
			hbframe = tdc.hartbeat;
			hbflag = tdc.flag;
		}
	} else {
		std::cout << "Unknown device : " << std::hex << type << std::endl;
	}


	if (hbflag > 0) fHBflag |= hbflag;
	if (fKt4->Check()) {
		if (fHBflag > 0) {
			if ((fHBflag & 0x200) == 0x200) LOG(warn) << "HB Data lost";
			if ((fHBflag & 0x100) == 0x100) LOG(warn) << "HB Data confiliction";
			if ((fHBflag & 0x080) == 0x080) LOG(warn) << "HB LFN mismatch";
			if ((fHBflag & 0x040) == 0x040) LOG(warn) << "HB GFN mismatch";
		}
		fHBflag = 0;
	}

	return hbframe;
}

int LogicFilter::RemoveData(fair::mq::Parts &parts, int index)
{
	const int HB_SIZE = sizeof(uint64_t) * 2;
	const uint64_t HB_HEAD = 0x7000'0000'0000'0000;

	int rval = 0;
	
	fair::mq::Message & msg = parts[index];
	//fair::mq::MessagePtr & msg = parts.At(index);
	//auto & msg = parts[index];
	char *pdata = reinterpret_cast<char *>(msg.GetData());
	uint64_t *pdata64 = reinterpret_cast<uint64_t *>(msg.GetData());
	unsigned int msize = msg.GetSize();

	if (msize > HB_SIZE) {

		for (unsigned int i = 0 ; i < (msize / sizeof(uint64_t)) ; i++) {
			std::cout << std::hex << pdata64[i] << std::endl;
		}

		#if 0
		auto hbp = std::make_unique<char[]>(HB_SIZE);
		strncpy(hbp.get(), pdata + (msize - HB_SIZE), HB_SIZE);
		auto hbmsg = MessageUtil::NewMessage(*this, std::move(hbp));
		#endif

		#if 1
		auto hb = std::make_unique< std::vector<char> > (HB_SIZE);
		strncpy(hb->data(), pdata + (msize - HB_SIZE), HB_SIZE);
		auto hbmsg = MessageUtil::NewMessage(*this, std::move(hb));
		#endif

		#if 0
		auto hb = new char[HB_SIZE];
		fair::mq::MessagePtr hbmsg(
			NewMessage(
			reinterpret_cast<char *>(hb),
			sizeof(hb),
			[](void* ppdata, void*) {
				char *p = reinterpret_cast<char *>(ppdata);
				delete p;
			},
			nullptr)
		);
		auto phbchar = reinterpret_cast<char *>(hbmsg->GetData());
		strncpy(phbchar, pdata + (msize - HB_SIZE), HB_SIZE);
		#endif

		uint64_t *v = reinterpret_cast<uint64_t *>(hbmsg->GetData());
		for (unsigned int i = 0 ; i < (HB_SIZE / sizeof(uint64_t)) ; i++) {
			if ((v[i] & 0xfc00'0000'0000'0000) != HB_HEAD) {
				rval = -1;
				std::cout << "#E not HB Tail " << v[i];
			}
		}

		auto it = parts.fParts.begin();
		std::advance(it, index);
		parts.fParts.insert(it, std::move(hbmsg));
		parts.fParts.erase(it + 1);

		#if 1
		for (int i = 0 ; i < parts.Size() ; i++) {
			auto phead = reinterpret_cast<uint64_t *>(parts[i].GetData());
			std::cout << "#D " << i
				<< " " << std::hex << phead[0] << " " << std::hex << phead[1]
				<< " " << std::dec << parts[i].GetSize() << std::endl;
		}
		#endif
	
	}

	return rval;
}

void LogicFilter::CheckMultiPart(FairMQParts &inParts)
{
	std::cout << "#Nmsg: " << std::dec << inParts.Size() << std::endl;
	std::cout << "# " << std::dec;
	for(int i = 0 ; i < inParts.Size() ; i++) {
		uint64_t *top = reinterpret_cast<uint64_t *>(inParts[i].GetData());
		struct TimeFrame::Header *tbh
			= reinterpret_cast<TimeFrame::Header *>(inParts[i].GetData());
		struct SubTimeFrame::Header *stbh
			= reinterpret_cast<SubTimeFrame::Header *>(inParts[i].GetData());
		struct HeartbeatFrame::Header *hbh
			= reinterpret_cast<HeartbeatFrame::Header *>(inParts[i].GetData());

		if (tbh->magic == TimeFrame::MAGIC) {
			std::cout << "TF Id: "
				<< tbh->timeFrameId  << " Nsrc: " << tbh->numSource;
		} else
		if (stbh->magic == SubTimeFrame::MAGIC) {
			std::cout << std::endl << "* STF: TFid: 0x" << std::hex
				<< std::setw(8) << stbh->timeFrameId << " :"; 
		} else
		if (hbh->magic == HeartbeatFrame::MAGIC) {
			std::cout << " HB:" << std::dec
				<< std::setw(8) << inParts[i].GetSize() << " :"; 
		} else {
			std::cout << "UK " << std::setw(8) << std::setfill('0')
				<< IsHartBeat(top[0], SubTimeFrame::TDC64H);
		}
	}
	std::cout << std::dec << std::endl;

	return;
}

int LogicFilter::MakeBlockMap(
	FairMQParts &inParts,
	std::vector< std::vector<struct DataBlock> > &block_map)
{
	uint64_t femid = 0;
	uint64_t time_frame_id = 0;
	uint64_t devtype = 0;
	int ifem = 0;
	//std::vector<bool> flag_sending;
	std::vector<struct DataBlock> blocks;

	//struct TimeFrame::Header *i_tfHeader = nullptr;

	for(int i = 0 ; i < inParts.Size() ; i++) {
		//flag_sending.push_back(true);
		#if 0
		CheckData(inParts.At(i));
		#endif

		auto tfHeader = reinterpret_cast<struct TimeFrame::Header *>(inParts[i].GetData());
		auto stfHeader = reinterpret_cast<struct SubTimeFrame::Header *>(inParts[i].GetData());
		struct DataBlock dblock;

		if (tfHeader->magic == TimeFrame::MAGIC) {
			ifem = -1;
			//stf.clear();
			//stf.resize(0);
			//i_tfHeader = tfHeader;
			dblock.is_HB = false;
			dblock.msg_index = 0;
			dblock.timeFrameId = tfHeader->timeFrameId;
		} else
		if (stfHeader->magic == SubTimeFrame::MAGIC) {
			femid = stfHeader->femId;
			devtype = stfHeader->femType;
			time_frame_id = stfHeader->timeFrameId;
			if (blocks.size() > 0) block_map.push_back(blocks);
			//stf.push_back(*stfHeader);
			dblock.is_HB = false;
			dblock.msg_index = -1;
			dblock.timeFrameId = time_frame_id;
			blocks.clear();
			blocks.resize(0);
			ifem++;

			#if 0
			std::cout << "#D STF Nmsg: " << stfHeader->numMessages
				<< " Tid: " << stfHeader->timeFrameId << std::endl;
			#endif

		} else {
			// make block map;

			uint64_t *data = reinterpret_cast<uint64_t *>(inParts[i].GetData());
			int hbframe = IsHartBeat(data[0], devtype);

			#if 0
			std::cout << "#DDD msg" << std::dec << std::setw(3) << i << ":"
				<< " HBFrame:" << std::setw(6) << hbframe
				<< " blocks.size(): " << blocks.size() << std::endl;
			#endif

			//if (hbframe < 0) {
			if ((data[0] == HeartbeatFrame::MAGIC)
				|| (hbframe < 0)) {
				dblock.femId = femid;
				dblock.Type = devtype;
				dblock.is_HB = false;
				dblock.msg_index = i;
				dblock.nTrig = 0;
				dblock.HBFrame = 0;
				dblock.timeFrameId = time_frame_id;
			} else {
				if (fSplitMethod == 1) {
					//data ga nakattatokimo push_back dummy
					if (   (blocks.size() == 0)
						|| ((blocks.size() > 0)
						&& (blocks.back().is_HB == true)) ) {

						#if 0
						std::cout << "#W no data frame Msg:"
							<< std::dec << i
							<< " " << inParts.Size()
							<< " " << blocks.size()
							<< " is_HB:" << hbframe
							<< " " << std::hex << data[0];
						if (blocks.size() > 0) {
							std::cout << " p_is_HB:"
								<< blocks.back().is_HB;
						}
						std::cout << std::endl;
						//assert(0);
						#endif

						dblock.femId = femid;
						dblock.Type = SubTimeFrame::NULDEV;
						dblock.is_HB = false;
						dblock.msg_index = -2;
						dblock.nTrig = 0;
						dblock.HBFrame = 0;
						dblock.timeFrameId = time_frame_id;
						blocks.push_back(dblock);
					}
				}

				dblock.HBFrame = hbframe;
				dblock.femId = femid;
				dblock.Type = devtype;
				dblock.is_HB = true;
				dblock.msg_index = i;
				dblock.nTrig = 0;
				dblock.timeFrameId = time_frame_id;
			}
			blocks.push_back(dblock);
		}
	} /// end of the for loop
	block_map.push_back(blocks);

	#if 0 //BlockMap  check
	if (fKt2->Check()) {
	std::cout << "#D block_map.size: " << std::dec << block_map.size() << std::endl;
	for (auto& blk : block_map) {
		std::cout << "#D block " << std::setw(2) << blk.size() << " /";
		for (auto& b : blk) {
			std::cout << " " << std::setw(3) << b.msg_index << " HB:" << b.is_HB;
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
	}
	#endif

	size_t bsize_min = block_map[0].size();
	for (auto& blk : block_map) {
		if (blk.size() < bsize_min) {
			LOG(warn) << "Unmatched number of stf in TF "
				<< bsize_min << " " << blk.size();
			bsize_min = blk.size();
		}
	}

	#if 0
	std::cout << "blocks: " << blocks.size() << std::endl;
	#endif

	return 0;
}

int LogicFilter::MakeBlockMap2(
	FairMQParts &inParts,
	std::vector< std::vector<struct DataBlock> > &block_map)
{
	uint64_t femid = 0;
	uint64_t time_frame_id = 0;
	uint64_t devtype = 0;
	int ifem = 0;
	//std::vector<bool> flag_sending;
	std::vector<struct DataBlock> blocks;

	//struct TimeFrame::Header *i_tfHeader = nullptr;

	for(int i = 0 ; i < inParts.Size() ; i++) {
		//flag_sending.push_back(true);
		#if 0
		CheckData(inParts.At(i));
		#endif

		auto tfHeader = reinterpret_cast<struct TimeFrame::Header *>(inParts[i].GetData());
		auto stfHeader = reinterpret_cast<struct SubTimeFrame::Header *>(inParts[i].GetData());
		struct DataBlock dblock;

		if (tfHeader->magic == TimeFrame::MAGIC) {
			ifem = -1;
			//stf.clear();
			//stf.resize(0);
			//i_tfHeader = tfHeader;
			dblock.is_HB = false;
			dblock.msg_index = 0;
			dblock.timeFrameId = tfHeader->timeFrameId;
		} else
		if (stfHeader->magic == SubTimeFrame::MAGIC) {
			femid = stfHeader->femId;
			devtype = stfHeader->femType;
			time_frame_id = stfHeader->timeFrameId;
			if (blocks.size() > 0) block_map.push_back(blocks);
			//stf.push_back(*stfHeader);
			dblock.is_HB = false;
			dblock.msg_index = -1;
			dblock.timeFrameId = time_frame_id;
			blocks.clear();
			blocks.resize(0);
			ifem++;

			#if 0
			std::cout << "#D STF Nmsg: " << stfHeader->numMessages
				<< " Tid: " << stfHeader->timeFrameId << std::endl;
			#endif

		} else {
			// make block map;

			uint64_t *data = reinterpret_cast<uint64_t *>(inParts[i].GetData());
			int hbframe = IsHartBeat(data[0], devtype);

			#if 0
			std::cout << "#DDD msg" << std::dec << std::setw(3) << i << ":"
				<< " HBFrame:" << std::setw(6) << hbframe
				<< " blocks.size(): " << blocks.size() << std::endl;
			#endif

			if ((data[0] == HeartbeatFrame::MAGIC)
				|| (hbframe < 0)) {
				dblock.femId = femid;
				dblock.Type = devtype;
				dblock.is_HB = false;
				dblock.msg_index = i;
				dblock.nTrig = 0;
				dblock.HBFrame = 0;
				dblock.timeFrameId = time_frame_id;
			} else {
				if (fSplitMethod == 1) {
					//data ga nakattatokimo push_back dummy
					if (   (blocks.size() == 0)
						|| ((blocks.size() > 0)
						&& (blocks.back().is_HB == true)) ) {

						#if 0
						std::cout << "#W no data frame Msg:"
							<< std::dec << i
							<< " " << inParts.Size()
							<< " " << blocks.size()
							<< " is_HB:" << hbframe
							<< " " << std::hex << data[0];
						if (blocks.size() > 0) {
							std::cout << " p_is_HB:"
								<< blocks.back().is_HB;
						}
						std::cout << std::endl;
						//assert(0);
						#endif

						dblock.femId = femid;
						dblock.Type = SubTimeFrame::NULDEV;
						dblock.is_HB = false;
						dblock.msg_index = -2;
						dblock.nTrig = 0;
						dblock.HBFrame = 0;
						dblock.timeFrameId = time_frame_id;
						blocks.push_back(dblock);
					}
				}

				dblock.HBFrame = hbframe;
				dblock.femId = femid;
				dblock.Type = devtype;
				dblock.is_HB = true;
				dblock.msg_index = i;
				dblock.nTrig = 0;
				dblock.timeFrameId = time_frame_id;
			}
			blocks.push_back(dblock);
		}
	} /// end of the for loop
	block_map.push_back(blocks);

	#if 0 //BlockMap  check
	if (fKt2->Check()) {
	std::cout << "#D block_map.size: " << std::dec << block_map.size() << std::endl;
	for (auto& blk : block_map) {
		std::cout << "#D block " << std::setw(2) << blk.size() << " /";
		for (auto& b : blk) {
			std::cout << " " << std::setw(3) << b.msg_index << " HB:" << b.is_HB;
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
	}
	#endif

	size_t bsize_min = block_map[0].size();
	for (auto& blk : block_map) {
		if (blk.size() < bsize_min) {
			LOG(warn) << "Unmatched number of stf in TF "
				<< bsize_min << " " << blk.size();
			bsize_min = blk.size();
		}
	}

	#if 0
	std::cout << "blocks: " << blocks.size() << std::endl;
	#endif

	return 0;
}

int LogicFilter::BuildHBF(
	std::vector<struct HBFIndex> &hbf_list,
	std::vector< std::vector<struct DataBlock> > &block_map,
	FairMQParts &inParts,
	int imsg
)
{

	for (size_t iifem = 0 ; iifem < block_map.size() ; iifem++) {
		struct DataBlock *dbl = &block_map[iifem][imsg];
		uint64_t vfemid = dbl->femId;
		if (!(dbl->is_HB)) {
			int mindex = dbl->msg_index;

			#if 0
			std::cout << "#D1 Mark FEM ID: " << std::hex << vfemid
				<< " Type: " << dbl->Type << " HBFrame: " << std::hex
				<< dbl->HBFrame << std::endl;
			std::cout << "#D1 mindex: " << std::dec << mindex
				<< " vfemid: " << vfemid << " type: " << dbl->Type
				<< std::endl;
			std::cout << "#D1 inParts.size: " << inParts[mindex].GetSize()
				<< std::endl;
			#endif

			if (mindex > 0) {
				struct HBFIndex hbf_seg;
				hbf_seg.msg_index = imsg;
				hbf_seg.timeFrameId = dbl->timeFrameId;
				hbf_seg.femType = dbl->Type;
				hbf_seg.femId = vfemid;
				hbf_seg.data = reinterpret_cast<unsigned char *>(
					inParts[mindex].GetData());
				hbf_seg.size = inParts[mindex].GetSize();

				hbf_list.emplace_back(hbf_seg);
			}
		}
	}

	#if 0
	std::cout << "# HB: " << std::dec << i;
	for (size_t iifem = 0 ; iifem < block_map.size() ; iifem++) {
		struct DataBlock *dbl = &block_map[iifem][i];
		uint64_t vfemid = dbl->femId;
		uint64_t vhbframe = dbl->HBFrame;
		//std::cout << "# HB: " << std::dec << i
		//<< " FEM: " << std::hex << vfemid
		std::cout << " " << std::dec << (vfemid  & 0xff) << ":" << vhbframe;
	}
	std::cout << std::endl;
	#endif

	return 0;
}


int LogicFilter::AddFilterMessage(
	FairMQParts &outParts,
	std::vector< std::vector<uint32_t> > &fltdata,
	uint32_t elapse,
	uint32_t tf_id)
{
	int flt_data_len = 0;

	auto tp = std::chrono::system_clock::now() ;
	auto d = tp.time_since_epoch();
	uint64_t sec = std::chrono::duration_cast
		<std::chrono::seconds>(d).count();
	uint64_t usec = std::chrono::duration_cast
		<std::chrono::microseconds>(d).count();

	uint32_t flt_datasize = 0;
	uint32_t totalhits = 0;
	for (auto &v : fltdata) {
		flt_datasize += v.size() * sizeof(uint32_t)
			+ sizeof(struct Filter::TrgTimeHeader);
		totalhits += v.size();
	}
	uint64_t flt_len = flt_datasize + sizeof(struct Filter::Header);


	#if 0  // memo to make FairMQ Message
	gQdepth++;
	FairMQMessagePtr fltheadermsg(
		NewMessage(
		reinterpret_cast<char *>(&fltHeader),
		sizeof(fltHeader),
		[](void* pdata, void*) {
			char *p = reinterpret_cast<char *>(pdata);
			delete p;
			//gQdepth--;
		},
		nullptr));
	outParts.AddPart(std::move(fltheadermsg));
	#endif

	//add FLT Header
	auto fltHeader = std::make_unique<struct Filter::Header>();
	fltHeader->magic = Filter::MAGIC;
	fltHeader->length = flt_len;
	fltHeader->timeFrameId = tf_id;
	fltHeader->numTrigs = totalhits;
	fltHeader->workerId = fId;
        fltHeader->numMessages = fltdata.size();
	fltHeader->elapseTime = elapse;
	fltHeader->processTime.tv_sec = sec;
	fltHeader->processTime.tv_usec = usec;
	outParts.AddPart(MessageUtil::NewMessage(*this, std::move(fltHeader)));

	flt_data_len += sizeof(struct Filter::Header);

	//add FLT data
	for (auto &v : fltdata) {
		//auto trgtdc = std::make_unique<std::vector<uint32_t>>(std::move(v));
		//outParts.AddPart(MessageUtil::NewMessage(*this, std::move(trgtdc)));

		//std::cout << "#D v.size : " << v.size() << std::endl;
		//	<< " msg.size" << (*outParts.end()).get()->GetSize() << std::endl;

		int trg_time_data_len = sizeof(Filter::TrgTimeHeader)
			+ v.size() * sizeof(uint32_t);
		//std::cout << "#D TrgTimeHeader.size: " << sizeof(struct Filter::TrgTimeHeader)
		//	<< " Time.size: " << v.size() * sizeof(uint32_t) << std::endl;
		struct Filter::TrgTimeHeader trg_time_header;
		trg_time_header.length = trg_time_data_len;
		//std::cout << "#D TrgTimeHeader.uint32_t.size: " << sizeof(trg_time_header.u32data) << std::endl;

		std::vector<uint32_t> vv;
		for (auto &h : trg_time_header.u32data) vv.emplace_back(h);
		for (auto &tdc : v) {
			unsigned char trg_type = 0;
			vv.emplace_back(
				(trg_type << 24) | (0x00ffffff & tdc));
		}
		//std::cout << "#D fltmsg.size: " << vv.size()
		//	<< " TrgTime.len: " << trg_time_data_len << std::endl;
	
		outParts.AddPart(MessageUtil::NewMessage
			(*this, std::make_unique<std::vector<uint32_t>>(std::move(vv))));

		flt_data_len += outParts[outParts.Size() - 1].GetSize();
	}

	// std::cout << "#D fltH.len: " << std::dec << flt_len
	//	<< " flt_data_len: " << flt_data_len << std::endl;

	return flt_data_len;
}

int LogicFilter::MarkFlagSending(
	std::vector< std::vector<struct DataBlock> > &block_map,
	int i,
	int nhits,
	std::vector<bool> &flag_sending)
{
	for (size_t iifem = 0 ; iifem < block_map.size() ; iifem++) {
		block_map[iifem][i].nTrig = nhits;
		if (nhits == 0) {
			int mindex = block_map[iifem][i].msg_index;
			bool is_HB = block_map[iifem][i].is_HB;
			uint32_t dtype = block_map[iifem][i].Type;

			//std::cout << "#D mindex: " << mindex
			//	<< " h: " << is_HB
			//	<< " t: " << block_map[iifem][i].Type
			//	<< " i: " << std::hex << block_map[iifem][i].femId
			//	<< std::dec << std::endl;;

			if ((mindex > 0) && (! is_HB)) {
				if (dtype != SubTimeFrame::NULDEV) {
					flag_sending[mindex] = false;
				}

				if ((fSplitMethod == 0) || (fSplitMethod == 1)) {
					//// kokoha mondai gaaru ////
					if (fIsRemoveHB) {
						flag_sending[mindex + 1] = false;
					}
				}
			}

		} else {
			//int mindex = block_map[iifem][i].msg_index;
			//bool is_HB = block_map[iifem][i].is_HB;
			//std::cout << "#D H mindex: " << mindex
			//	<< " h: " << is_HB
			//	<< " t: " << block_map[iifem][i].Type
			//	<< " i: " << std::hex << block_map[iifem][i].femId
			//	<< " Nhits: " << std::dec << nhits
			//	<< std::endl;;
		}
	}

	return 0;
}


bool LogicFilter::ConditionalRun()
{
	//Receive
	FairMQParts inParts;

	FairMQMessagePtr msg_header(fTransportFactory->CreateMessage());
	struct Filter::Header fltheader;
	// struct TimeFrame::Header *i_tfHeader = nullptr;
	int i_tf_msg_index = 0;
	uint32_t tf_tf_id = 0;
	std::chrono::system_clock::time_point sw_start, sw_end;

	if (Receive(inParts, fInputChannelName, 0, 1000) > 0) {
		assert(inParts.Size() >= 2);
		sw_start = std::chrono::system_clock::now();

		#if 1
		if (fKt1->Check()) CheckMultiPart(inParts);
		#endif

		std::vector<struct DataBlock> blocks;
		std::vector< std::vector<struct DataBlock> > block_map;
		std::vector<bool> flag_sending;
		for(int i = 0 ; i < inParts.Size() ; i++) flag_sending.push_back(true);

		for(int i = 0 ; i < inParts.Size() ; i++) {
			auto tfHeader = reinterpret_cast<struct TimeFrame::Header *>(inParts[i].GetData());
			if (tfHeader->magic == TimeFrame::MAGIC) {
				// i_tfHeader = tfHeader;
				i_tf_msg_index = i;
				tf_tf_id = tfHeader->timeFrameId;
			}
		}
		MakeBlockMap(inParts, block_map);

		#if 0 //BlockMap  check
		if (fKt2->Check()) {
			std::cout << "#D block_map.size: " << std::dec << block_map.size() << std::endl;
			for (auto& blk : block_map) {
				std::cout << "#D block " << std::setw(2) << blk.size() << " /";
				for (auto& b : blk) {
					std::cout << " " << std::setw(3) << b.msg_index << " HB:" << b.is_HB;
				}
				std::cout << std::endl;
			}
			std::cout << std::endl;
		}
		#endif

		size_t bsize_min = block_map[0].size();
		for (auto& blk : block_map) {
			if (blk.size() < bsize_min) {
				LOG(warn) << "Unmatched number of stf in TF "
					<< bsize_min << " " << blk.size();
				bsize_min = blk.size();
			}
		}

		// Trigger processing in unit of HBF
		std::vector< std::vector<uint32_t> > fltdata;
		int totalhits = 0;
		for (size_t i = 0 ; i < bsize_min ; i++) {

			//HBF building
			std::vector<struct HBFIndex> hbf_list;
			BuildHBF(hbf_list, block_map, inParts, i);

			//Trigger process
			std::vector<uint32_t> *hits = fTrig->Exec(hbf_list);
			fltdata.emplace_back(*hits);
			int nhits = hits->size();

			#if 0
			if (nhits > 0) {
				std::cout << "#D1 Hits : " << hits->size() << " ";
				for (unsigned int ii = 0 ; ii < hits->size() ; ii++) {
					std::cout << " " << std::dec << (*hits)[ii];
					if (ii > 10) {
						std::cout << "...";
						break;
					}
				}
				std::cout << std::endl;
			}
			#endif

			// mark sending flag for STF
			MarkFlagSending(block_map, i, nhits, flag_sending);

			totalhits += nhits;
		}

		#if 0
		if (fKt2->Check()) {
			//std::cout << "#block_map size: " << block_map.size() << std::endl;
			for (unsigned int i = 0 ; i < block_map.size() ; i++) {
				std::cout << "#HBFrame: " << i << "/";
				for (unsigned int j = 0 ; j < block_map[i].size(); j++) {
					std::cout << "  " << j << ":"
						<< std::setw(5) << block_map[i][j].HBFrame;
				}
				std::cout << std::endl;
			}
			//std::cout << std::endl;

			if (totalhits > 0) {
				std::cout << "#D TotalHits: " << totalhits;
				std::cout << " Flag: ";
				for (const auto& v : flag_sending) std::cout << " " << v; 
				std::cout << std::endl;
			}
		}
		#endif

		#if 1
		if (totalhits > 0) {
			std::cout << "#D TotalHits: " << std::setw(4) << totalhits << " :";
			for (int i = 0 ; i < totalhits / 50 ; i++) std::cout << ".";
			// std::cout << " Flag: ";
			// for (const auto& v : flag_sending) std::cout << " " << v; 
			std::cout << std::endl;
		}
		#endif

		#if 0
		auto tfHeader = reinterpret_cast<TimeFrame::Header*>(inParts.At(0)->GetData());
		auto stfHeader = reinterpret_cast<SubTimeFrame::Header*>(inParts.At(0)->GetData());
		auto stfId     = stfHeader->timeFrameId;
		#endif

		sw_end = std::chrono::system_clock::now();
		uint32_t elapse = std::chrono::duration_cast<std::chrono::microseconds>(
			sw_end - sw_start).count();
		static uint64_t int_hits = 0;
		static uint64_t int_processed_hbf = 0;
		double trig_ratio;
		int_hits += totalhits;
		int_processed_hbf += bsize_min / 2;
		if (fKt3->Check()) {
			if (int_processed_hbf > 0) {
				trig_ratio = static_cast<double>(int_hits)
					/ static_cast<double>(int_processed_hbf);
			}

			std::cout << "#Elapse: " << std::dec << elapse << " us"
				<< " Hits: " << totalhits
				<< " T.Ratio: " << trig_ratio
				<< " Hits(inte): " << int_hits
				<< " HBF(inte): " << int_processed_hbf
				<< std::endl;

			//int_hits = 0;
			//int_processed_hbf = 0;
		}

		#if 0
		//Modify SubTimeFrameHeader
		if (fIsDataSuppress) {
			for (int ii = 0 ; ii < inParts.Size() ; ii++) {
				auto stfh = reinterpret_cast<struct SubTimeFrame::Header *>
					(inParts[ii].GetData());
				if (stfh->magic == SubTimeFrame::MAGIC) {
					uint32_t len_stf = 0;
					uint32_t nmsg_stf = 0;
					int kk = ii + 1;
					for (int jj = ii + 1 ; jj < inParts.Size() ; jj++) {
						auto sstf =
							reinterpret_cast<struct SubTimeFrame::Header *>
							(inParts[jj].GetData());
						if (sstf->magic == SubTimeFrame::MAGIC) {
							kk = jj;
							break;
						} else
						if (flag_sending[jj]) {
							len_stf += inParts[jj].GetSize();
							nmsg_stf++;
						}
					}
					// if (len_stf == 0) { // STF header kara nankunaru? OK?
					// 	flag_sending[ii] = false;
					// }
					stfh->length
						= len_stf + sizeof(struct SubTimeFrame::Header);
					stfh->numMessages = nmsg_stf;
					ii = kk - 1;
				}
			}
		}
		#endif

		#if 0
		// Modify TimeFrameHeader Length
		uint32_t tf_len = sizeof(TimeFrame::Header);
		if (fIsDataSuppress) {
			// SubTimeFrameHeader
			for (int ii = 0 ; ii < inParts.Size() ; ii++) {
				if (flag_sending[ii] || (! fIsDataSuppress)) {
					tf_len += (inParts.AtRef(ii)).GetSize();
				}
			}
		} else {
			tf_len = i_tfHeader->length;
		}
		#endif

		FairMQParts outParts;
  
		//Copy TimeFrameHeader
		if (i_tf_msg_index != 0) {
			LOG(error) << "Wrong TimeFrame Header locataion : " << i_tf_msg_index;
		}
		FairMQMessagePtr msgCopyTF(fTransportFactory->CreateMessage());
		msgCopyTF->Copy(inParts.AtRef(0));
		outParts.AddPart(std::move(msgCopyTF));

		//FilterHeader
		//tf_len += AddFilterMessage(outParts, fltdata, elapse, tf_tf_id);
		AddFilterMessage(outParts, fltdata, elapse, tf_tf_id);

		//Copy SubTimeFrame
		unsigned int msg_size = inParts.Size();
		for (unsigned int ii = 1 ; ii < msg_size ; ii++) {
			if (flag_sending[ii] || (! fIsDataSuppress)) {
				FairMQMessagePtr msgCopy(fTransportFactory->CreateMessage());
				msgCopy->Copy(inParts.AtRef(ii));
				outParts.AddPart(std::move(msgCopy));
			} else {
				// add blank SubTime Frame
				struct HeartbeatFrame::Header *hbf
					= reinterpret_cast<struct HeartbeatFrame::Header *>(inParts[ii].GetData());
				//uint64_t *hbf_u64
				//	= reinterpret_cast<uint64_t *>(inParts[ii].GetData());
				if (hbf->magic == HeartbeatFrame::MAGIC) {
				//if (hbf_u64[0] == HeartbeatFrame::MAGIC) {
					#pragma pack(4)
					struct BlankHBF{
						struct HeartbeatFrame::Header Header;
						uint64_t Delimiter[2];
					};
					#pragma pack()
					auto blank = std::make_unique<struct BlankHBF>();
					uint64_t *d = reinterpret_cast<uint64_t *>(
						reinterpret_cast<char *>(inParts[ii].GetData())
						+ inParts[ii].GetSize() - (sizeof(uint64_t) * 2));
					std::memcpy(&(blank->Header), inParts[ii].GetData(),
						sizeof(struct HeartbeatFrame::Header));
					blank->Header.length = sizeof(HeartbeatFrame::Header)
						+ (sizeof(uint64_t) * 2);
					std::memcpy(&(blank->Delimiter), d, sizeof(uint64_t) * 2);
					outParts.AddPart(MessageUtil::NewMessage(*this, std::move(blank)));
					//flag_sending[ii] = true;
					//tf_len += sizeof(outParts[outParts.Size() - 1]);

					#if 0
					std::cout << "#D null HBF size: " << std::dec
						<< outParts[outParts.Size() - 1].GetSize()
						<< " " << outParts.Size() << std::endl;
					uint64_t *b = reinterpret_cast<uint64_t *>(outParts[outParts.Size() - 1].GetData());
					std::cout << "#D : " << std::hex << std::flush;
					for (unsigned int i = 0 ; i < outParts[outParts.Size() -1].GetSize() / sizeof(uint64_t) ; i++) {
						std::cout << " " << b[i] << std::flush;
					}
					std::cout << std::dec << std::endl;
					#endif

				}
			}
		}

		//Modify SubTimeFrameHeader
		if (fIsDataSuppress) {
			for (int ii = 0 ; ii < outParts.Size() ; ii++) {
				auto stfh = reinterpret_cast<struct SubTimeFrame::Header *>
					(outParts[ii].GetData());
				if (stfh->magic == SubTimeFrame::MAGIC) {
					uint32_t len_stf = 0;
					uint32_t nmsg_stf = 0;
					int kk = ii + 1;
					for (int jj = ii + 1 ; jj < outParts.Size() ; jj++) {
						auto sstf =
							reinterpret_cast<struct SubTimeFrame::Header *>
							(outParts[jj].GetData());
						if (sstf->magic == SubTimeFrame::MAGIC) {
							kk = jj;
							break;
						} else
						if (flag_sending[jj]) {
							len_stf += outParts[jj].GetSize();
							nmsg_stf++;
						}
					}
					// if (len_stf == 0) { // STF header kara nankunaru? OK?
					// 	flag_sending[ii] = false;
					// }
					stfh->length
						= len_stf + sizeof(struct SubTimeFrame::Header);
					stfh->numMessages = nmsg_stf + 1; // Nmsg = Nstf + NFLTHeader(1)
					ii = kk - 1;
				}
			}
		}

		// rewrite TimeFrameHeader length
		auto tfHeader = reinterpret_cast<struct TimeFrame::Header *>(
			//outParts[i_tf_msg_index].GetData());
			outParts[0].GetData());
		// Calcurate TimeFrame.length
		uint32_t tf_len = 0;
		for (auto &msg : outParts) tf_len += msg->GetSize();
		tfHeader->length = tf_len;

		#if 0
		std::cout << "#D Blocks" << std::endl;
		for (unsigned int ii = 0 ; ii < block_map.size() ; ii++) {
			auto lblocks = block_map[ii];
			for (unsigned int jj = 0 ; jj < lblocks.size() ; jj++) {
				std::cout << " " << lblocks[jj].is_HB << ":" << lblocks[jj].HBFrame;
			}
			std::cout << std::endl;
		}
		std::cout << "#DD outParts.Size: " << outParts.Size() << std::endl;
		std::cout << "#DD flag_sending: ";
		int flagcount = 0;
		for (unsigned int ii = 0 ; ii < msg_size ; ii++) {
			auto stfHeader = reinterpret_cast<struct SubTimeFrame::Header *>
				(inParts[ii].GetData());
			if (stfHeader->magic == SubTimeFrame::MAGIC) std::cout << std::endl;
			std::cout << flag_sending[ii];
			if (flag_sending[ii]) flagcount++;
		}
		std::cout << " : " << flagcount << std::endl;
		#endif



		#if 1
		FairMQParts dqmParts;
		bool dqmSocketExists = fChannels.count(fDQMChannelName);
		if (dqmSocketExists) {
			for (auto & m : outParts) {
				FairMQMessagePtr msgCopy(fTransportFactory->CreateMessage());
				msgCopy->Copy(*m);
				dqmParts.AddPart(std::move(msgCopy));
			}


			if (Send(dqmParts, fDQMChannelName) < 0) {
				if (NewStatePending()) {
					LOG(info) << "Device is not RUNNING";
				} else {
					LOG(error) << "Failed to enqueue dqm-channel";
				}
			} else {
				//std::cout << "+" << std::flush;
			}
		} else {
			#if 0
			std::cout << "NoDQM socket" << std::endl;
			#endif
		}
		#endif


		//Send
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

	}

	return true;
}

void LogicFilter::PostRun()
{
	LOG(info) << "Post Run";
	return;
}


#if 0
bool LogicFilter::HandleData(fair::mq::MessagePtr& msg, int val)
{
	(void)val;
	#if 0
	LOG(info) << "Received: \""
		<< std::string(static_cast<char*>(msg->GetData()), msg->GetSize())
		<< "\"";
	#endif

	#if 0
	LOG(info) << "Received: " << msg->GetSize() << " : " << val;
	#endif

	if (fMaxIterations > 0 && ++fNumIterations >= fMaxIterations) {
		LOG(info) << "Configured maximum number of iterations reached."
			<< " Leaving RUNNING state.";
		return false;
	}

	CheckData(msg);
	

	// return true if you want the handler to be called again
	// (otherwise return false go to the Ready state)
	return true;
}
#endif


void addCustomOptions(bpo::options_description& options)
{
	using opt = LogicFilter::OptionKey;

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
			bpo::value<std::string>()->default_value("false"),
			"Data suppression enable")
		(opt::RemoveHB.data(),
			bpo::value<std::string>()->default_value("false"),
			"Remove HB without hit")
		(opt::PollTimeout.data(), 
			bpo::value<std::string>()->default_value("1"),
			"Timeout of polling (in msec)")
		(opt::SplitMethod.data(),
			bpo::value<std::string>()->default_value("2"),
			"STF split method")

		(opt::TriggerSignals.data(),
			bpo::value<std::string>()->default_value(
			"((0xc0a802a9 0 0) (0xc0a802a9 1 0)"),
			"Triger signals (module_IP Channel_number Offset)")
		(opt::TriggerFormula.data(),
			bpo::value<std::string>()->default_value("RPN 0 1 &"),
			"Trigger formula")
		(opt::TriggerWidth.data(),
			bpo::value<std::string>()->default_value("10"),
			"Trigger window width (4 ns unit)")

    		;
}


std::unique_ptr<fair::mq::Device> getDevice(fair::mq::ProgOptions& /*config*/)
{
	return std::make_unique<LogicFilter>();
}
