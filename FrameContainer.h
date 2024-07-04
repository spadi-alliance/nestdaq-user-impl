/**
 * @file FrameContainer.h
 * @brief Frame Structure Container
 * @date Created : 2024-05-04 12:27:57 JST
 *       Last Modified : 2024-06-30 18:56:56 JST
 *
 * @author Shinsuke OTA <ota@rcnp.osaka-u.ac.jp>
 *
 */
#ifndef NESTDAQ_FRAMECONTAINEW_H
#define NESTDAQ_FRAMECONTAINEW_H

#include "SubTimeFrameHeader.h"
#include "TimeFrameHeader.h"
#include "FilterHeader.h"
#include "HeartbeatFrameHeader.h"
#include <iostream>

namespace nestdaq {
   template <typename frametype> class Header {
   public:
      void SetHeader(void* data) {
         this->fHeader = static_cast<frametype*>(data);
      }
      frametype* GetHeader() { return this->fHeader; }
      template<typename unit>
      void CopyHeaderTo(std::vector<unit>* output) {
         for (uint32_t i = 0, n = sizeof(frametype)/sizeof(unit); i < n; ++i) {
            output->push_back(*((unit*)fHeader+i));
         }
      }
   protected:
      frametype *fHeader;
   };

   template <class frametype, class datatype>
   class DataFrame : public Header<frametype>  {
   public:
      void Set(void* data) {
         this->SetHeader(data);
         fData = reinterpret_cast<datatype*>((char*)data + sizeof(frametype));
         auto len = this->GetHeader()->length;
         auto hlen = this->GetHeader()->hLength;
         fNumData = (len > hlen) ? (len - hlen )/sizeof(datatype) : 0;
      }

      datatype UncheckedAt(uint64_t idx) { return *(fData+idx); }
      auto GetNumData() const { return fNumData; }
      auto GetData() { return fData; }
      template <typename unit>
      void CopyDataTo(std::vector<unit>* output) {
         auto nj = sizeof(datatype)/sizeof(unit);
         for (uint32_t i =0, n = fNumData; i < n; ++i) {
            for (uint32_t j = 0 ; j < nj; ++j) {
               output->push_back(*((unit*)(fData+i) + j));
            }
         }
      }

      template <typename unit>
      void CopyDataTo(std::vector<unit>* output, uint64_t idx) {
         auto nj = sizeof(datatype)/sizeof(unit);
         for (uint32_t j = 0; j < nj; ++j) {
            output->push_back(*((unit*)(fData+idx)+j));
         }
      }

      template <typename unit>
      void CopyAllTo(std::vector<unit>* output) {
         std::cout << std::hex << this->GetHeader()->magic << " n data = " << this->GetNumData() <<  std::endl;
         this->template CopyHeaderTo<unit>(output);
         this->template CopyDataTo<unit>(output);
      }

      uint64_t GetRealLength() {
         return sizeof(frametype) + fNumData * sizeof(datatype);
      }

      void Clear() {
         ;
      }
                                               
   protected:
      datatype* fData;
      uint64_t fNumData;
   };

   
   template <class frametype, class child>
   class ContainerFrame : public Header<frametype>, public std::vector<child*>  {
   public:
      uint64_t GetRealLength() {
         uint64_t length = sizeof(frametype);
         for (int i = 0, n = this->size(); i<n; ++i) {
            length += this->at(i)->GetRealLength();
         }
         return length;
      }
      template <typename unit> void CopyAllTo(std::vector<unit>* output) {
         this->template CopyHeaderTo<unit>(output);
         std::cout << std::hex << this->GetHeader()->magic << std::endl;
         for (int i = 0,n = this->size(); i < n; ++i) {
            this->at(i)->template CopyAllTo<unit>(output);
         }
      }

      void Clear() {
         std::for_each(this->begin(),this->end(),
                       [] (child* c) {
                          c->Clear();
                          delete c;
                       });
         this->std::vector<child*>::clear();
      }

   protected:
      void clear();
   };


   using THBF = DataFrame<struct HeartbeatFrame::Header,uint64_t>;
   using TSTF = ContainerFrame<struct SubTimeFrame::Header,THBF>;
   
   using TTT = DataFrame<struct Filter::TrgTimeHeader,struct Filter::TrgTime>;
   using TLF = ContainerFrame<struct Filter::Header,TTT>;
   using TTF = ContainerFrame<struct TimeFrame::Header,TSTF>;

}
#endif // NESTDAQ_FRAMECONTAINEW_H
