#ifndef NESTDAQ_UTILITY_ECC_H_
#define NESTDAQ_UTILITY_ECC_H_

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>

// namespace nestdaq::ecc {

//____________________________________________________________________________
template <std::size_t dsize, std::size_t psize>
auto AssignBits(const std::bitset<dsize + psize + 1> &h) -> std::bitset<dsize + psize + 2>;

template <std::size_t dsize, std::size_t psize>
auto CalcParity(const std::bitset<dsize + psize + 1> &h, const std::bitset<dsize> &d) -> std::bitset<psize>;

template <std::size_t dsize, std::size_t psize>
auto Correct(const std::bitset<dsize> &d, const std::bitset<psize> &p) -> std::bitset<dsize + psize + 2>;

template <std::size_t ni, std::size_t no>
auto eccSingle(const std::bitset<ni + no> &h, const std::bitset<ni> &d) -> std::bitset<no>;

template <std::size_t ni, std::size_t no>
auto EncodeEcc(const std::bitset<ni> &d) -> std::bitset<no>;

template <std::size_t dsize, std::size_t psize>
auto ReorderBits(const std::bitset<dsize> &d, const std::bitset<psize> &p) -> std::bitset<dsize + psize>;

//____________________________________________________________________________
template <std::size_t dsize, std::size_t psize>
auto AssignBits(const std::bitset<dsize + psize + 1> &h) -> std::bitset<dsize + psize + 2>
{
   std::bitset<dsize + psize + 2> ret;
   int hh = 3;
   int hl = 3;
   int hi = 0;

   for (int i = 1; i < (psize - 2); ++i) {
      if (((2 << (i + 1)) - 1) > (ret.size() - 1)) {
         hh = ret.size() - 1;
      } else {
         hh = (2 << (i + 1)) - 1;
      }

      hl = (2 << i) - 1;
      hi = (2 << (i - 1)) - 1 + hi;

      for (int j = 0; j <= (hh - hl); ++j) {
         ret[hi + j] = h[hl + j];
      }
   }

   ret[ret.size() - 1] = h[h.size() - 1];

   for (int i = 0; i < (psize - 2); ++i) {
      ret[dsize + i] = h[2 << i];
   }

   return ret;
}

//____________________________________________________________________________
// hsize = dsize + psize + 1
// psize = hsize - dsize - 1
template <std::size_t dsize, std::size_t psize>
auto CalcParity(const std::bitset<dsize + psize + 1> &h) // , const std::bitset<dsize> &d)
   -> std::bitset<psize>
{
   std::bitset<psize> ret;

   for (int i = 0; i < (ret.size() - 2); ++i) {
      for (int j = 0; j < (h.size() - 1) / ((2 << i) - 1); ++j) {
         for (int k = 0; k < ((2 << j) - 1); ++k) {
            int hidx = (2 << (i + 1) * j) + ((2 << i) + k);
            if (hidx < (h.size() - 1)) {
               ret[i] = ret[i] xor h[hidx];
            }
         }
      }
   }
   return ret;
}

//____________________________________________________________________________
template <std::size_t dsize, std::size_t psize>
auto Correct(const std::bitset<dsize> &d, const std::bitset<psize> &p) -> std::bitset<dsize + psize + 2>
{
   std::bitset<dsize + psize + 1> h = ReorderBits(d, p);
   std::bitset<psize> p1 = CalcParity<dsize, psize>(h, d);
   bool sed = false; // single error detect
   bool ded = false; // double error detect

   auto p1sed = ((d.count() + p.count() % 2) == 1) ? true : false;

   uint32_t ipos = p1.to_ulong();
   if (p1.any()) {
      sed = true;
      if (!p1sed) {
         ded = true;
      }

      if ((ipos >= 1) && (ipos <= h.size() - 1)) {
         h.flip(ipos); // error correct by flipping a bit
      }
   } else if (p1sed) {
      sed = true;
      h.flip(ipos); // error correct by flipping a bit
   }

   std::bitset<dsize + psize + 2> ret;

   auto hr = AssignBits<dsize, psize>(h);
   for (int i = 0; i < (ret.size() - 2); ++i)
      ret[i] = hr[i];
   ret[ret.size() - 2] = sed;
   ret[ret.size() - 1] = ded;

   return ret;
}

//____________________________________________________________________________
template <std::size_t ni, std::size_t no>
std::bitset<no> eccSingle(const std::bitset<ni + no> &h, const std::bitset<ni> &d)
{
   std::bitset<no> ret;
   constexpr std::size_t hhigh = ni + no - 1;
   constexpr std::size_t rhigh = ret.size() - 1;
   std::size_t hidx = 0;

   for (auto i = 1; i <= rhigh; ++i) {
      for (auto j = 0; j <= (hhigh / (1 << i)); ++j) {
         for (auto k = 0; k <= ((1 << (i - 1)) - 1); ++k) {
            hidx = (1 << i) * j + (1 << (i - 1)) + k;

            if (hidx < hhigh) {
               ret[i] = ret[i] xor h[hidx];
            }
         }
      }
   }

   return ret;
}

//____________________________________________________________________________
template <std::size_t ni, std::size_t no>
std::bitset<no> EncodeEcc(const std::bitset<ni> &d)
{
   auto h = ReorderBits<ni, no>(d, 0);
   // std::cout << "h = " << h << std::endl;
   auto ret = eccSingle<ni, no>(h, d);

   for (auto i = 0; i < d.size(); ++i) {
      ret[0] = ret[0] xor d[i];
   }
   for (auto i = 1; i < ret.size(); ++i) {
      ret[0] = ret[0] xor ret[i];
   }
   return ret;
}

//____________________________________________________________________________
template <std::size_t dsize, std::size_t psize>
auto ReorderBits(const std::bitset<dsize> &d, const std::bitset<psize> &p) -> std::bitset<dsize + psize>
{
   std::bitset<dsize + psize> ret;
   constexpr std::size_t phigh = psize - 1;
   constexpr std::size_t rhigh = ret.size() - 1;
   // std::cout << __func__ << " ret.size() = " << ret.size() << std::endl;
   ret[0] = p[0];
   for (auto i = 1; i <= phigh; ++i) {
      std::size_t j = 1 << (i - 1);
      ret[j] = p[i];
      // std::cout <<  " ret[" << j << "] = p[" << i << "] = " << p[i] << std::endl;
   }

   int hh = 3;
   int hl = 3;
   int dh = 0;
   int dl = 0;
   for (auto i = 1; i <= (phigh - 1); ++i) {
      if ((1 << (i + 1)) > rhigh) {
         hh = rhigh;
      } else {
         hh = (1 << (i + 1)) - 1;
      }

      hl = (1 << i) + 1;
      dl = (1 << (i - 1)) - 1 + dl;
      dh = hh - hl + dl;

      for (auto j = 0; j <= (hh - hl); ++j) {
         // std::cout << "(hh, hl, dh, dl) = (" << hl << ", " << hh << ", " << dl << ", " << dh << ")" << std::endl;
         // std::cout << "ret [" << (hl+j) << "] = d[" << (dl+j) << "] = " << d[dl+j] << std::endl;
         ret[hl + j] = d[dl + j];
      }
   }

   return ret;
}
//} // namespace nestdaq::ecc

#endif // NESTDAQ_UTILITY_ECC_H_
