#include <iostream>
#include <limits>
#include "galois/Galois.h"
#include "galois/gstl.h"
#include "Lonestar/BoilerPlate.h"
#include "galois/runtime/CompilerHelperFunctions.h"

#include "galois/graphs/OfflineGraph.h"
#include "galois/Dist/DistGraph.h"
#include "galois/DistAccumulator.h"
#include "galois/runtime/Tracer.h"


static const char* const name = "ConnectedComp - Distributed Heterogeneous";
static const char* const desc = "Bellman-Ford ConnectedComp on Distributed Galois.";
static const char* const url = 0;

namespace cll = llvm::cl;
static cll::opt<std::string> inputFile(cll::Positional, cll::desc("<input file (Transpose graph)>"), cll::Required);
static cll::opt<unsigned int> maxIterations("maxIterations", cll::desc("Maximum iterations: Default 1024"), cll::init(1024));
static cll::opt<unsigned int> src_node("startNode", cll::desc("ID of the source node"), cll::init(0));
static cll::opt<bool> verify("verify", cll::desc("Verify ranks by printing to 'page_ranks.#hid.csv' file"), cll::init(false));


struct NodeData {
  unsigned long long comp_current;
};

typedef DistGraph<NodeData, void> Graph;
typedef typename Graph::GraphNode GNode;


struct InitializeGraph {
  Graph *graph;

  InitializeGraph(Graph* _graph) : graph(_graph){}
  void static go(Graph& _graph) {

    //XXX: needs sync pull, since is used in ConnectedComp operator.
    galois::do_all(_graph.begin(), _graph.end(), InitializeGraph {&_graph}, galois::loopname("InitGraph"));

  }

  void operator()(GNode src) const {
    NodeData& sdata = graph->getData(src);
    sdata.comp_current = graph->getGID(src);
  }
};

struct ConnectedComp {
  Graph* graph;
  static galois::DGAccumulator<int> DGAccumulator_accum;

  ConnectedComp(Graph* _graph) : graph(_graph){}
  void static go(Graph& _graph){
    unsigned iteration = 0;
    do{
      DGAccumulator_accum.reset();


      galois::do_all(_graph.begin(), _graph.end(), ConnectedComp { &_graph }, galois::loopname("ConnectedComp"));

     ++iteration;
    }while(DGAccumulator_accum.reduce());

    std::cout << " Total iteration run : " << iteration << "\n";
  }

  void operator()(GNode src) const {
    NodeData& snode = graph->getData(src);
    auto& sdist = snode.comp_current;

    unsigned long long current_min = snode.comp_current;
    unsigned long long new_dist;
    for (auto jj = graph->edge_begin(src), ej = graph->edge_end(src); jj != ej; ++jj) {
      GNode dst = graph->getEdgeDst(jj);
      auto& dnode = graph->getData(dst);
      new_dist = dnode.comp_current;

      if(current_min > new_dist){
        current_min = new_dist;
      }
    }

    if(snode.comp_current > current_min){
      snode.comp_current = current_min;
      DGAccumulator_accum += 1;
    }
  }
};
galois::DGAccumulator<int>  ConnectedComp::DGAccumulator_accum;

/********Set source Node ************/
void setSource(Graph& _graph){
  auto& net = galois::runtime::getSystemNetworkInterface();
  if(net.ID == 0){
    auto& nd = _graph.getData(src_node);
    nd.comp_current = 0;
  }
}

int main(int argc, char** argv) {
  try {
    LonestarStart(argc, argv, name, desc, url);
    auto& net = galois::runtime::getSystemNetworkInterface();
    galois::Timer T_total, T_offlineGraph_init, T_DistGraph_init, T_init, T_ConnectedComp1, T_ConnectedComp2, T_ConnectedComp3;

    T_total.start();

    T_DistGraph_init.start();
    Graph hg(inputFile, net.ID, net.Num);
    T_DistGraph_init.stop();

    std::cout << "InitializeGraph::go called\n";
    T_init.start();
    InitializeGraph::go(hg);
    T_init.stop();

    // Verify
/*
    if(verify){
      if(net.ID == 0) {
        for(auto ii = hg.begin(); ii != hg.end(); ++ii) {
          std::cout << "[" << *ii << "]  " << hg.getData(*ii).comp_current << "\n";
        }
      }
    }
*/


    std::cout << "ConnectedComp::go run1 called  on " << net.ID << "\n";
    T_ConnectedComp1.start();
      ConnectedComp::go(hg);
    T_ConnectedComp1.stop();

    std::cout << "[" << net.ID << "]" << " Total Time : " << T_total.get() << " offlineGraph : " << T_offlineGraph_init.get() << " DistGraph : " << T_DistGraph_init.get() << " Init : " << T_init.get() << " ConnectedComp1 : " << T_ConnectedComp1.get() << " (msec)\n\n";

    galois::runtime::getHostBarrier().wait();
    InitializeGraph::go(hg);

    std::cout << "ConnectedComp::go run2 called  on " << net.ID << "\n";
    T_ConnectedComp2.start();
      ConnectedComp::go(hg);
    T_ConnectedComp2.stop();

    std::cout << "[" << net.ID << "]" << " Total Time : " << T_total.get() << " offlineGraph : " << T_offlineGraph_init.get() << " DistGraph : " << T_DistGraph_init.get() << " Init : " << T_init.get() << " ConnectedComp2 : " << T_ConnectedComp2.get() << " (msec)\n\n";

    galois::runtime::getHostBarrier().wait();
    InitializeGraph::go(hg);

    std::cout << "ConnectedComp::go run3 called  on " << net.ID << "\n";
    T_ConnectedComp3.start();
      ConnectedComp::go(hg);
    T_ConnectedComp3.stop();

    std::cout << "[" << net.ID << "]" << " Total Time : " << T_total.get() << " offlineGraph : " << T_offlineGraph_init.get() << " DistGraph : " << T_DistGraph_init.get() << " Init : " << T_init.get() << " ConnectedComp3 : " << T_ConnectedComp3.get() << " (msec)\n\n";


   T_total.stop();

    auto mean_time = (T_ConnectedComp1.get() + T_ConnectedComp2.get() + T_ConnectedComp3.get())/3;

    std::cout << "[" << net.ID << "]" << " Total Time : " << T_total.get() << " offlineGraph : " << T_offlineGraph_init.get() << " DistGraph : " << T_DistGraph_init.get() << " Init : " << T_init.get() << " ConnectedComp1 : " << T_ConnectedComp1.get() << " ConnectedComp2 : " << T_ConnectedComp2.get() << " ConnectedComp3 : " << T_ConnectedComp3.get() <<" ConnectedComp mean time (3 runs ) (" << maxIterations << ") : " << mean_time << "(msec)\n\n";

    if(verify){
      for(auto ii = hg.begin(); ii != hg.end(); ++ii) {
        galois::runtime::printOutput("% %\n", hg.getGID(*ii), hg.getData(*ii).comp_current);
      }
    }
    return 0;
  } catch(const char* c) {
    std::cerr << "Error: " << c << "\n";
      return 1;
  }
}
