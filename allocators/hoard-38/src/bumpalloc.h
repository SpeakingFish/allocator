#ifndef _BUMPALLOC_H_
#define _BUMPALLOC_H_

/**
 * @class BumpAlloc
 * @brief Obtains memory in chunks and bumps a pointer through the chunks.
 * @author Emery Berger <http://www.cs.umass.edu/~emery>
 */

namespace Hoard {

template <int ChunkSize,
	  class super>
class BumpAlloc : public super {
public:

  BumpAlloc (void)
    : _bump (NULL),
      _remaining (0)
  {}

  inline void * malloc (size_t sz) {
    // If there's not enough space left to fulfill this request, get
    // another chunk.
    if (_remaining < sz) {
      refill(sz);
    }
    if (_bump) {
      char * old = _bump;
      _bump += sz;
      _remaining -= sz;
      return old;
    } else {
      // We were unable to get memory.
      return NULL;
    }
  }

  /// Free is disabled (we only bump, never reclaim).
  inline void free (void *) {}

private:

  /// The bump pointer.
  char * _bump;

  /// How much space remains in the current chunk.
  size_t _remaining;

  // Get another chunk.
  void refill (size_t sz) {
    // Always get at least a ChunkSize worth of memory.
    if (sz < ChunkSize) {
      sz = ChunkSize;
    }
    _bump = (char *) super::malloc (sz);
    if (_bump) {
      _remaining = sz;
    } else {
      _remaining = 0;
    }
  }

};

}

#endif
