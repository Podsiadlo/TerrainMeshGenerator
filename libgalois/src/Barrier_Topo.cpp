#include "galois/substrate/PerThreadStorage.h"
#include "galois/substrate/Barrier.h"
#include "galois/substrate/CompilerSpecific.h"

#include <atomic>

namespace {

class TopoBarrier : public galois::substrate::Barrier {
  struct treenode {
    //vpid is galois::runtime::LL::getTID()

    //package binary tree
    treenode* parentpointer; //null of vpid == 0
    treenode* childpointers[2];

    //waiting values:
    unsigned havechild;
    std::atomic<unsigned> childnotready;

    //signal values
    std::atomic<unsigned> parentsense;

  };

  galois::substrate::PerPackageStorage<treenode> nodes;
  galois::substrate::PerThreadStorage<unsigned> sense;

  void _reinit(unsigned P) {
    auto& tp = galois::substrate::getThreadPool();
    unsigned pkgs = tp.getCumulativeMaxPackage(P-1) + 1;
    for (unsigned i = 0; i < pkgs; ++i) {
      treenode& n = *nodes.getRemoteByPkg(i);
      n.childnotready = 0;
      n.havechild = 0;
      for (int j = 0; j < 4; ++j) {
	if ((4*i+j+1) < pkgs) {
	  ++n.childnotready;
          ++n.havechild;
	}
      }
      for (unsigned j = 0; j < P; ++j) {
	if (tp.getPackage(j) == i && !tp.isLeader(j)) {
          ++n.childnotready;
          ++n.havechild;
        }
      }
      n.parentpointer = (i == 0) ? 0 : nodes.getRemoteByPkg((i-1)/4);
      n.childpointers[0] = ((2*i + 1) >= pkgs) ? 0 : nodes.getRemoteByPkg(2*i+1);
      n.childpointers[1] = ((2*i + 2) >= pkgs) ? 0 : nodes.getRemoteByPkg(2*i+2);
      n.parentsense = 0;
    }
    for (unsigned i = 0; i < P; ++i)
      *sense.getRemote(i) = 1;
  }

public:

  TopoBarrier(unsigned v) {
    _reinit(v);
  }

  //not safe if any thread is in wait
  virtual void reinit(unsigned val) {
    _reinit(val);
  }

  virtual void wait() {
    unsigned id = galois::substrate::ThreadPool::getTID();
    treenode& n = *nodes.getLocal();
    unsigned& s = *sense.getLocal();
    bool leader = galois::substrate::ThreadPool::isLeader();
    //completion tree
    if (leader) {
      while (n.childnotready) { galois::substrate::asmPause(); }
      n.childnotready = n.havechild;
      if (n.parentpointer) {
	--n.parentpointer->childnotready;
      }
    } else {
      --n.childnotready;
    }
    
    //wait for signal
    if (id != 0) {
      while (n.parentsense != s) {
	galois::substrate::asmPause();
      }
    }
    
    //signal children in wakeup tree
    if (leader) {
      if (n.childpointers[0])
	n.childpointers[0]->parentsense = s;
      if (n.childpointers[1])
	n.childpointers[1]->parentsense = s;
      if (id == 0)
	n.parentsense = s;
    }
    ++s;
  }

  virtual const char* name() const { return "TopoBarrier"; }

};

}

std::unique_ptr<galois::substrate::Barrier> galois::substrate::createTopoBarrier(unsigned activeThreads) {
  return std::unique_ptr<Barrier>(new TopoBarrier(activeThreads));
}

