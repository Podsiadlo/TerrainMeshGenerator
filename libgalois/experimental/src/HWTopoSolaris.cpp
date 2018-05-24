#include "galois/runtime/ll/HWTopo.h"

#include <vector>

#include <unistd.h>
#include <stdio.h>
#include <thread.h>
#include <sys/types.h>
#include <sys/processor.h>
#include <sys/procset.h>

using namespace galois::runtime::LL;

namespace {

static bool sunBindToProcessor(int proc) {
  if (processor_bind(P_LWPID,  thr_self(), proc, 0) == -1) {
    gWarn("Could not set CPU affinity for thread ", proc, "(", strerror(errno), ")");
    return false;
  }
  return true;
}

//Flat machine with the correct number of threads and binding
struct Policy {
  std::vector<int> procmap; //Galois id -> solaris id

  unsigned numThreads, numCores, numPackages;

  Policy() {
    processorid_t i, cpuid_max;
    cpuid_max = sysconf(_SC_CPUID_MAX);
    for (i = 0; i <= cpuid_max; i++) {
      if (p_online(i, P_STATUS) != -1) {
	procmap.push_back(i);
	//printf("processor %d present\n", i);
      }
    }

    numThreads = procmap.size();
    numCores = procmap.size();
    numPackages = 1;
  }
};

static Policy& getPolicy() {
  static Policy A;
  return A;
}

} //namespace

bool galois::runtime::LL::bindThreadToProcessor(int id) {
  assert(size_t(id) < procmap.size ());
  return sunBindToProcessor(getPolicy().procmap[id]);
}

unsigned galois::runtime::LL::getProcessorForThread(int id) {
  assert(size_t(id) < procmap.size ());
  return getPolicy().procmap[id];
}

unsigned galois::runtime::LL::getMaxThreads() {
  return getPolicy().numThreads;
}

unsigned galois::runtime::LL::getMaxCores() {
  return getPolicy().numCores;
}

unsigned galois::runtime::LL::getMaxPackages() {
  return getPolicy().numPackages;
}

unsigned galois::runtime::LL::getMaxPackageForThread(int id) {
  return getPolicy().numPackages - 1;
}

unsigned galois::runtime::LL::getPackageForThread(int id) {
  return 0;
}

bool galois::runtime::LL::isPackageLeader(int id) {
  return id == 0;
}
