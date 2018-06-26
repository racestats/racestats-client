#ifndef CIRC_BUF_h
#define CIRC_BUF_h
#include <inttypes.h>

template <typename T, uint16_t Size>
class CircBuf {
public:
  enum {
    Capacity = Size,
  };

  CircBuf() : wp_(buf_), rp_(buf_), used_(0) {}
  ~CircBuf() {}
  
  void push(T value) {
    *wp_++ = value;
    used_++;
    if (wp_ == buf_+Size) wp_ = buf_;
  }
  
  void read(T* &buf1, int& sz1, T* &buf2, int& sz2)
  {
    if (rp_ <= wp_ && used_ != Size) {
      buf1 = rp_;
	    sz1 = wp_ - rp_;
	    buf2 = nullptr;
	    sz2 = 0;
    } else {
      buf1 = rp_;
	    sz1 = buf_ + Size - rp_;
	    buf2 = buf_;
	    sz2 = wp_ - buf_;
    }
  }
  void resize_back(int sz)
  {
  	if (sz >= 0 && sz < Size)
  	{
  		used_ = sz;
  		rp_ = wp_ - sz;
  		if (rp_ < buf_) rp_ += Size;
  	}
  }
  
  T *back(int ofs)
  {
    //back(0) - last written, wp_ - 1
    if (ofs < 0 || ofs >= used()) return nullptr;
    if (wp_ - ofs - 1 < buf_) return wp_ - ofs - 1 + Size;
    return wp_ - ofs - 1;
  }
  
  T *front(int ofs)
  {
    if (ofs < 0 || ofs >= used()) return nullptr;
    if (rp_ + ofs >= buf_ + Size) return rp_ + ofs - Size;
    return rp_ + ofs;
  }
  
  T pop() {
    T result = *rp_++;
    used_--;
    if (rp_ == buf_+Size) rp_ = buf_;
    return result;
  }
  
  bool full() const { 
    return used_ == Size; 
  }
  
  int used() const {
     return used_;
  }

  void *temp_buf() {
    return buf_;
  }

  int temp_size() const {
    return sizeof(T) * Size;
  }

private:
  T buf_[Size];
  T *wp_;
  T *rp_;
  uint16_t used_;
};

#endif
