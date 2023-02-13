#ifndef UNCOPYABLE_H
#define UNCOPYABLE_H

template <class T>
class Uncopyable 
{
    
protected:
  Uncopyable()
  {};
  ~Uncopyable()
  {};
    
private:
  Uncopyable(const Uncopyable&);

  T& operator=(const T&);
  
};
#endif
