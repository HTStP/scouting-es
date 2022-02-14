#ifndef SLICE_H
#define SLICE_H

#include <stdint.h>
//#include "tbb/tbb_allocator.h"
#include "tbb/scalable_allocator.h"
#include "tbb/concurrent_queue.h"

//! Holds a slice of data.
class Slice {
  //! Pointer to one past last filled byte in sequence
  char* logical_end;
  //! Pointer to one past last available byte in sequence.
  char* physical_end;
  uint32_t counts;
  bool output;
  static tbb::concurrent_bounded_queue<Slice*> free_slices;

public:
  //! Allocate a Slice object that can hold up to max_size bytes
  static Slice* allocate( size_t max_size ) {
    //Slice* t = (Slice*)tbb::zero_allocator<char>().allocate( sizeof(Slice)+max_size );

    // Replacing tbb::zero_allocator with aligned allocator.
    // Alignment to 32 bytes (256 bits) is required by MicronDMA.
    Slice* t = (Slice*) scalable_aligned_malloc( sizeof(Slice)+max_size, 32);

    t->logical_end = t->begin();
    t->physical_end = t->begin()+max_size;
    t->counts = 0;
    t->output = false;
    return t;
  }
  static Slice *preAllocate(size_t, size_t);
  static void shutDown();
  static Slice *getAllocated();
  static void giveAllocated(Slice *);
  Slice* clone() const { return new Slice(*this); }

  //! Free a Slice object 
  void free() {
    //	fprintf(stderr,"slice free at 0x%llx \n",(unsigned long long) this);
    //tbb::tbb_allocator<char>().deallocate((char*)this,sizeof(Slice)+(physical_end-begin())+1);

    scalable_aligned_free( this );
  } 
  //! Pointer to beginning of sequence
  char* begin() {return (char*)(this+sizeof(Slice));}
  //! Pointer to one past last character in sequence
  char* end() {return logical_end;}
  //! Length of sequence
  size_t size() const {return logical_end-(char*)(this+sizeof(Slice));}
  //! Maximum number of characters that can be appended to sequence
  size_t avail() const {return physical_end-logical_end;}
  //! Set end() to given value.
  void set_end( char* p ) {logical_end=p;}
  void set_output(bool o) {output=o;}
  void set_counts(uint32_t c){counts=c;}
  uint32_t get_counts() const {return counts;}
};
#endif
