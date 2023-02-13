// -*- C++ -*-

// Author: Tomonori Takahashi

#include "BitDump.hh"

#include <bitset>

#include <iostream>

namespace hddaq
{

//______________________________________________________________________________
BitDump::BitDump()
  : m_count(0)
{
  std::cout << "#D n-th  : data (MSB --- LSB)" << std::hex << std::endl;
}

//______________________________________________________________________________
BitDump::~BitDump()
{
  std::cout << std::dec << std::endl;
}

//______________________________________________________________________________
void
BitDump::operator()(unsigned int data)
{
  std::bitset<32> bits(data);
//   std::cout << std::hex;
  std::cout.width(8);
  std::cout.fill('0');
  std::cout << m_count << " : ";// << std::dec;

  std::cout << bits << "   ";
  std::cout.width(8);
  std::cout.fill('0');
  std::cout << data << "\n";
  ++m_count;
  return;
}

//______________________________________________________________________________
void
BitDump::operator()(unsigned short data)
{
  std::bitset<16> bits(data);
  std::cout.width(8);
  std::cout.fill('0');
  std::cout << m_count << " : ";

  std::cout << bits << "   ";
  std::cout.width(4);
  std::cout.fill('0');
  std::cout << data << "\n";
  return;
}


}

