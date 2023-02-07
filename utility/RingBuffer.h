#ifndef NESTDAQ_RingBuffer_h
#define NESTDAQ_RingBuffer_h

#include <atomic>
#include <condition_variable>
#include <functional>
#include <iostream>
#include <mutex>
#include <vector>

namespace nestdaq {
template <class T>
class RingBuffer {
public:
   explicit RingBuffer(int n = 0) { resize(n); }
   RingBuffer(int n, const T &x) { resize(n, x); }
   RingBuffer(const RingBuffer &) = delete;
   RingBuffer &operator=(const RingBuffer &) = delete;
   ~RingBuffer() = default;

   //___________________________________________________________________________
   T &back()
   {
      /*{
        std::lock_guard<std::mutex> lk{fMutex};
        std::cout << " WritePtr = " << fWritePtr << std::endl;
      }*/
      if (fFillCount < fCapacity) {
         return fBuffer[fWritePtr];
      }
      std::unique_lock<std::mutex> lock{fMutex};
      fNotFull.wait(lock, [this] {
         // std::cout << " writer wait" << std::endl;
         return fFillCount < fCapacity;
      });
      return fBuffer[fWritePtr];
   }

   //___________________________________________________________________________
   int capcacity() const
   {
      std::lock_guard<std::mutex> lock{fMutex};
      return fCapacity;
   }

   //___________________________________________________________________________
   void clear()
   {
      std::lock_guard<std::mutex> lock{fMutex};
      fCapacity = 0;
      fFillCount = 0;
      fReadPtr = 0;
      fWritePtr = 0;
      fBuffer.clear();
   }

   //___________________________________________________________________________
   bool empty() const { return fFillCount == 0; }

   //___________________________________________________________________________
   T &front()
   {
      /*{
        std::lock_guard<std::mutex> lk{fMutex};
        std::cout << " ReadPtr = " << fReadPtr << std::endl;
      }*/
      if (fFillCount > 0) {
         return fBuffer[fReadPtr];
      }
      std::unique_lock<std::mutex> lock{fMutex};
      fNotEmpty.wait(lock, [this] {
         // std::cout << " reader wait" << std::endl;
         return fFillCount > 0;
      });
      return fBuffer[fReadPtr];
   }

   //___________________________________________________________________________
   bool full() const { return fFillCount == fCapacity; }

   //___________________________________________________________________________
   int length() const { return fFillCount; }

   //___________________________________________________________________________
   void pop()
   {
      fReadPtr = (fReadPtr + 1) % fCapacity;
      --fFillCount;
      fNotFull.notify_all();
      // std::cout << __func__ << std::endl;
   }

   //___________________________________________________________________________
   T pop_release()
   {
      if (fFillCount > 0) {
         auto ret = std::move(fBuffer[fReadPtr]);
         pop();
         return std::move(ret);
      }
      std::unique_lock<std::mutex> lock{fMutex};
      fNotEmpty.wait(lock, [this] { return fFillCount > 0; });
      auto ret = std::move(fBuffer[fReadPtr]);
      pop();
      return std::move(ret);
   }

   //___________________________________________________________________________
   void push()
   {
      fWritePtr = (fWritePtr + 1) % fCapacity;
      ++fFillCount;
      fNotEmpty.notify_all();
      // std::cout << __func__ << std::endl;
   }

   //___________________________________________________________________________
   void push(const T &x)
   {
      if (fFillCount < fCapacity) {
         fBuffer[fWritePtr] = x;
         push();
         return;
      }
      std::unique_lock<std::mutex> lock{fMutex};
      fNotFull.wait(lock, [this] { return fFillCount < fCapacity; });
      fBuffer[fWritePtr] = x;
      push();
   }

   //___________________________________________________________________________
   void push(const T &&x)
   {
      if (fFillCount < fCapacity) {
         fBuffer[fWritePtr] = std::move(x);
         push();
         return;
      }
      std::unique_lock<std::mutex> lock{fMutex};
      fNotFull.wait(lock, [this] { return fFillCount < fCapacity; });
      fBuffer[fWritePtr] = std::move(x);
      push();
   }

   //___________________________________________________________________________
   int reserve() const { return fCapacity - fFillCount; }

   //___________________________________________________________________________
   template <class... Args>
   void resize(int n, Args... args)
   {
      std::lock_guard<std::mutex> lock{fMutex};
      fCapacity = n;
      fBuffer.reserve(n);
      for (auto i = 0; i < n; ++i) {
         fBuffer.emplace_back(args...);
      }
   }

   //___________________________________________________________________________
   int size() const { return length(); }

private:
   mutable std::mutex fMutex;
   std::condition_variable fNotFull;
   std::condition_variable fNotEmpty;
   int fCapacity{0};
   std::atomic<int> fFillCount{0};
   std::vector<T> fBuffer;
   int fReadPtr{0};
   int fWritePtr{0};
};

} // namespace nestdaq

#endif