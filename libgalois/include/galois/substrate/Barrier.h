#ifndef GALOIS_SUBSTRATE_BARRIER_H
#define GALOIS_SUBSTRATE_BARRIER_H

#include "galois/substrate/ThreadPool.h"
#include "galois/gIO.h"

#include <memory>
#include <functional>

namespace galois {
namespace substrate {

class Barrier {
public:
  virtual ~Barrier();

  //not safe if any thread is in wait
  virtual void reinit(unsigned val) = 0;

  //Wait at this barrier
  virtual void wait() = 0;

  //wait at this barrier
  void operator()(void) { wait(); }

  //barrier type.
  virtual const char* name() const = 0;
};

/**
 * return a reference to system barrier
 */
Barrier& getBarrier(unsigned activeThreads);

/**
 * Create specific types of barriers.  For benchmarking only.  Use
 * getSystemBarrier() for all production code
 */
std::unique_ptr<Barrier> createPthreadBarrier(unsigned);
std::unique_ptr<Barrier> createMCSBarrier(unsigned);
std::unique_ptr<Barrier> createTopoBarrier(unsigned);
std::unique_ptr<Barrier> createCountingBarrier(unsigned);
std::unique_ptr<Barrier> createDisseminationBarrier(unsigned);

/**
 * Creates a new simple barrier. This barrier is not designed to be fast but
 * does gaurantee that all threads have left the barrier before returning
 * control. Useful when the number of active threads is modified to avoid a
 * race in {@link getSystemBarrier()}.  Client is reponsible for deallocating
 * returned barrier.
 */
std::unique_ptr<Barrier> createSimpleBarrier(unsigned);

namespace internal {

template <typename _UNUSED=void>
struct BarrierInstance {
  unsigned m_num_threads;
  std::unique_ptr<Barrier> m_barrier;

  BarrierInstance(void) {
    m_num_threads = getThreadPool().getMaxThreads();
    m_barrier = createTopoBarrier(m_num_threads);
  }

  Barrier& get(unsigned numT) {
    GALOIS_ASSERT(numT > 0, "substrate::getBarrier() number of threads must be > 0");

    numT = std::min(numT, getThreadPool().getMaxUsableThreads());
    numT = std::max(numT, 1u);

    if (numT != m_num_threads) {
      m_num_threads = numT;
      m_barrier->reinit(numT);
    }

    return *m_barrier;
  }

};

void setBarrierInstance(BarrierInstance<>* bi);

} // end namespace internal



} // end namespace substrate
} // end namespace galois

#endif
