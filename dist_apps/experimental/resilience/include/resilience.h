#include <vector>
#include <algorithm>
#include <set>
#include <numeric>

/******************************************************************************/
/* Declaration of command line arguments */
/******************************************************************************/

namespace cll = llvm::cl;

enum RECOVERY_SCHEME {CP, RS, HR};

static cll::opt<bool> enableFT("enableFT", cll::desc("Enable fault tolerance: "
                                                     " Default false"),
                                           cll::init(false));
static cll::opt<RECOVERY_SCHEME> recoveryScheme("recoveryScheme",
                                            cll::desc("Fault tolerance scheme."),
                                            cll::values(
                                             clEnumValN(CP, "cp", 
                                                        "Checkpointing (default)"), 
                                             clEnumValN(RS, "rs", 
                                                        "Resilience"), 
                                             clEnumValN(HR, "hr", 
                                                        "hybrid resilience"), 
                                             clEnumValEnd),
                                            cll::init(CP));
static cll::opt<unsigned int> checkpointInterval("checkpointInterval",
                                            cll::desc("Interval of taking checkpoints"
                                                       " to disk. N means checkpoint graph"
                                                       " data every Nth iteration: "
                                                      " Default 1"),
                                            cll::init(1));
static cll::opt<unsigned int> crashIteration("crashIteration",
                                            cll::desc("iteration to crash: "
                                                      " Default -1"),
                                            cll::init(-1));
static cll::opt<unsigned int> crashNumHosts("crashNumHosts",
                                            cll::desc("number of hosts to crash: "
                                                      " Default -1"),
                                            cll::init(-1));
static cll::opt<std::string> checkpointFileName("checkpointFileName",
                                            cll::desc("File name to store the "
                                                      "checkpoint data:"
                                                      " Default checkpoint_#hostId"),
                                            cll::init("checkpoint"));


/**
 * Choose crashNumHosts hosts to crash.
 *
 * @returns set of hosts to crash
 */
std::set<uint32_t> getRandomHosts() {
  const auto& net = galois::runtime::getSystemNetworkInterface();
  uint32_t numHosts = net.Num;

  std::vector<uint32_t> allHosts(numHosts);
  std::iota(allHosts.begin(), allHosts.end(), 0);

  // Seed is used by random_shuffle.
  std::srand(1);
  std::random_shuffle(allHosts.begin(), allHosts.end());

  return std::set<uint32_t>(allHosts.begin(), allHosts.begin() + crashNumHosts);
}

/**
 * Checkpointing the node data onto disk.
 *
 * @tparam GraphTy type of graph; inferred from graph argument
 *
 * @param _num_iterations Debug argument to print out which iteration is
 * being checkpointed
 * @param _graph Graph to checkpoint
 */
template<typename GraphTy>
void saveCheckpointToDisk(unsigned _num_iterations, GraphTy& _graph){
  if (enableFT && (recoveryScheme == CP || recoveryScheme == HR)) {
    if (_num_iterations % checkpointInterval == 0) {

      galois::StatTimer TimerSaveCheckpoint(("TOTAL_TIMER_SAVE_CHECKPOINT"), "RECOVERY");
      if (galois::runtime::getSystemNetworkInterface().ID == 0) {
        galois::gPrint("Checkpoint for iteration: ", _num_iterations, "\n");    
      }

      TimerSaveCheckpoint.start();
      _graph.checkpointSaveNodeData(checkpointFileName);
      TimerSaveCheckpoint.stop();
      galois::runtime::getHostBarrier().wait();
    }
  }
}

/**
 * Crashes a node if the node is part of random hosts and applies
 * reinitialization/recovery operators.
 *
 * Version where healthy nodes do not call a recovery initializer.
 *
 * @tparam RecoveryTy Recovery operator functor
 * @tparam InitGraphCrashedTy Initialize operator for crashed nodes
 * @tparam GraphTy type of graph; inferred from graph argument
 *
 * @param _graph Graph to operate on
 */
template<typename RecoveryTy, typename InitGraphCrashedTy, typename GraphTy>
void crashSite(GraphTy& _graph) {
  galois::StatTimer TimerRecoveryTotal(
    _graph.get_run_identifier("TIMER_RECOVERY_TOTAL").c_str(),
    "RECOVERY"
  );
  TimerRecoveryTotal.start();
  const auto& net = galois::runtime::getSystemNetworkInterface();
  std::set<uint32_t> crashHostSet = getRandomHosts();

  if (net.ID == 0) {
    galois::runtime::reportParam("RECOVERY", "NUM_HOST_CRASHED_SET", 
                                       (crashHostSet.size()));
    galois::runtime::reportParam("RECOVERY", "NUM_HOST_CRASHED",
                                       (crashNumHosts));
    galois::runtime::reportParam("RECOVERY", "NUM_ITERATION_CRASHED", 
                                       (crashIteration));
  }

  galois::StatTimer TimerRecoveryCrashed(
      _graph.get_run_identifier("TIMER_RECOVERY_CRASHED").c_str(),
      "RECOVERY"
      );
  galois::StatTimer TimerRecoveryHealthy(
      _graph.get_run_identifier("TIMER_RECOVERY_HEALTHY").c_str(),
      "RECOVERY"
      );
  galois::StatTimer TimerGraphConstructCrashed(
      _graph.get_run_identifier("TIMER_RECOVERY_GRAPH_CONSTRUCT").c_str(),
      "RECOVERY"
      );

  // Use resilience to recover
  if (recoveryScheme == RS) {
    if (net.ID == 0) {
      galois::runtime::reportParam("RECOVERY", "RECOVERY_SCHEME", "RESILIENCE");
    }
    galois::gPrint("[", net.ID, "] Using RS\n");

    // Crashed hosts need to reconstruct local graphs
    if (crashHostSet.find(net.ID) != crashHostSet.end()){
      TimerRecoveryCrashed.start();

      galois::gPrint("[", net.ID, "] CRASHED!!!\n");
 
      // Reconstruct local graph
      TimerGraphConstructCrashed.start();
      _graph.read_local_graph_from_file(localGraphFileName);
      TimerGraphConstructCrashed.stop();
      galois::runtime::getHostBarrier().wait();
      // init and recover
      InitGraphCrashedTy::go(_graph);
      RecoveryTy::go(_graph);
      TimerRecoveryCrashed.stop();
    } else {
      galois::runtime::getHostBarrier().wait();
      TimerRecoveryHealthy.start();
      RecoveryTy::go(_graph);
      TimerRecoveryHealthy.stop();
    }
  } else if (recoveryScheme == CP) {
    if (net.ID == 0) {
      galois::runtime::reportParam("RECOVERY","RECOVERY_SCHEME", "CHECKPOINT");
      galois::runtime::reportParam("RECOVERY", "CHECKPOINT_INTERVAL", 
                                       (checkpointInterval));
    }
    galois::gPrint("[", net.ID, "] Using CP\n");

    // Crashed hosts need to reconstruct local graphs
      TimerRecoveryCrashed.start();
      galois::gPrint("[", net.ID, "] CRASHED!!!\n");

      // Reconstruct local graph
      TimerGraphConstructCrashed.start();
      _graph.read_local_graph_from_file(localGraphFileName);
      TimerGraphConstructCrashed.stop();
      _graph.checkpointApplyNodeData(checkpointFileName);
      TimerRecoveryCrashed.stop();
      galois::runtime::getHostBarrier().wait();
  } else if (recoveryScheme == HR) {
    if (net.ID == 0) {
      galois::runtime::reportParam("RECOVERY","RECOVERY_SCHEME", "HYBRID");
      galois::runtime::reportParam("RECOVERY", "CHECKPOINT_INTERVAL", 
          (checkpointInterval));
    }
    galois::gPrint("[", net.ID, "] Using HR\n");

    // Crashed hosts need to reconstruct local graphs
    if (crashHostSet.find(net.ID) != crashHostSet.end()){
      // Crashed hosts need to reconstruct local graphs
      TimerRecoveryCrashed.start();
      galois::gPrint("[", net.ID, "] CRASHED!!!\n");

      // Reconstruct local graph
      TimerGraphConstructCrashed.start();
      _graph.read_local_graph_from_file(localGraphFileName);
      TimerGraphConstructCrashed.stop();
      _graph.checkpointApplyNodeData(checkpointFileName);
    }
    galois::runtime::getHostBarrier().wait();

    TimerRecoveryHealthy.start();
    RecoveryTy::go(_graph);
    TimerRecoveryHealthy.stop();
    TimerRecoveryCrashed.stop();
  }
  TimerRecoveryTotal.stop();
}

/**
 * Crashes a node if the node is part of random hosts and applies
 * reinitialization/recovery operators.
 *
 * Version where healthy nodes call a recovery initializer.
 *
 * @tparam RecoveryTy Recovery operator functor
 * @tparam InitGraphCrashedTy Initialize operator for crashed nodes
 * @tparam GraphTy type of graph; inferred from graph argument
 *
 * @param _graph Graph to operate on
 */
template<typename RecoveryTy, typename InitGraphCrashedTy, 
         typename InitGraphHealthyTy, typename GraphTy>
void crashSite(GraphTy& _graph) {
  galois::StatTimer TimerRecoveryTotal(
    _graph.get_run_identifier("TIMER_RECOVERY_TOTAL").c_str(),
    "RECOVERY"
  );
  TimerRecoveryTotal.start();
  const auto& net = galois::runtime::getSystemNetworkInterface();
  std::set<uint32_t> crashHostSet = getRandomHosts();

  if (net.ID == 0) {
    galois::runtime::reportParam("RECOVERY", "NUM_HOST_CRASHED_SET", 
                                       (crashHostSet.size()));
    galois::runtime::reportParam("RECOVERY", "NUM_HOST_CRASHED",
                                       (crashNumHosts));
    galois::runtime::reportParam("RECOVERY", "NUM_ITERATION_CRASHED", 
                                       (crashIteration));

  }

  galois::StatTimer TimerRecoveryCrashed(
    _graph.get_run_identifier("TIMER_RECOVERY_CRASHED").c_str(),
    "RECOVERY"
  );
  galois::StatTimer TimerRecoveryHealthy(
    _graph.get_run_identifier("TIMER_RECOVERY_HEALTHY").c_str(),
    "RECOVERY"
  );
  galois::StatTimer TimerGraphConstructCrashed(
      _graph.get_run_identifier("TIMER_RECOVERY_GRAPH_CONSTRUCT").c_str(),
      "RECOVERY"
      );
  // Use resilience to recover
  if (recoveryScheme == RS) {
    if (net.ID == 0) {
      galois::runtime::reportParam("RECOVERY", "RECOVERY_SCHEME", "RESILIENCE");
    }
    galois::gPrint("[", net.ID, "] Using RS\n");

    // Crashed hosts need to reconstruct local graphs
    if(crashHostSet.find(net.ID) != crashHostSet.end()){
      TimerRecoveryCrashed.start();
      galois::gPrint("[", net.ID, "] CRASHED!!!\n");

      // Reconstruct local graph
      TimerGraphConstructCrashed.start();
      _graph.read_local_graph_from_file(localGraphFileName);
      TimerGraphConstructCrashed.stop();
      galois::runtime::getHostBarrier().wait();
      // init and recover
      InitGraphCrashedTy::go(_graph);
      RecoveryTy::go(_graph);
      TimerRecoveryCrashed.stop();
    } else {
      galois::runtime::getHostBarrier().wait();
      TimerRecoveryHealthy.start();
      InitGraphHealthyTy::go(_graph); // healthy init operator
      RecoveryTy::go(_graph);
      TimerRecoveryHealthy.stop();
    }
  } else if (recoveryScheme == CP) {
    if (net.ID == 0) {
      galois::runtime::reportParam("RECOVERY","RECOVERY_SCHEME", "CHECKPOINT");
      galois::runtime::reportParam("RECOVERY", "CHECKPOINT_INTERVAL", 
          (checkpointInterval));
    }
    galois::gPrint("[", net.ID, "] Using CP\n");
    // Crashed hosts need to reconstruct local graphs
    TimerRecoveryCrashed.start();
    galois::gPrint("[", net.ID, "] CRASHED!!!\n");

    // Reconstruct local graph
    TimerGraphConstructCrashed.start();
    _graph.read_local_graph_from_file(localGraphFileName);
    TimerGraphConstructCrashed.stop();
    _graph.checkpointApplyNodeData(checkpointFileName);
    TimerRecoveryCrashed.stop();
    galois::runtime::getHostBarrier().wait();
  } else if (recoveryScheme == HR) {
    if (net.ID == 0) {
      galois::runtime::reportParam("RECOVERY","RECOVERY_SCHEME", "HYBRID");
      galois::runtime::reportParam("RECOVERY", "CHECKPOINT_INTERVAL", 
          (checkpointInterval));
    }
    galois::gPrint("[", net.ID, "] Using HR\n");

    // Crashed hosts need to reconstruct local graphs
    if (crashHostSet.find(net.ID) != crashHostSet.end()){
      // Crashed hosts need to reconstruct local graphs
      TimerRecoveryCrashed.start();
      galois::gPrint("[", net.ID, "] CRASHED!!!\n");

      // Reconstruct local graph
      TimerGraphConstructCrashed.start();
      _graph.read_local_graph_from_file(localGraphFileName);
      TimerGraphConstructCrashed.stop();
      _graph.checkpointApplyNodeData(checkpointFileName);
    }
    galois::runtime::getHostBarrier().wait();

    //TODO: Does healthy host initializes
    //TODO: NOT FINISHED YET
    TimerRecoveryHealthy.start();
    RecoveryTy::go(_graph);
    TimerRecoveryHealthy.stop();
    TimerRecoveryCrashed.stop();
  }
  TimerRecoveryTotal.stop();
}

/**
 * Recovery adjustment call for non-checkpoint resilience.
 *
 * @tparam RecoveryAdjustTy functor used to adjust data on nodes
 * @tparam GraphTy type of graph; inferred from graph argument
 *
 * @param _graph Graph to operate on
 */
template<typename RecoveryAdjustTy, typename GraphTy>
void crashSiteAdjust(GraphTy& _graph) {
  if (recoveryScheme == RS || recoveryScheme == HR) {
    const auto& net = galois::runtime::getSystemNetworkInterface();
    galois::StatTimer TimerRecoveryCrashedAdjust(
      _graph.get_run_identifier("TIMER_RECOVERY_CRASHED_ADJUST").c_str(), 
      "RECOVERY"
    );

    galois::gPrint("[", net.ID, "] RECOVERY_ADJUST!!!\n");

    TimerRecoveryCrashedAdjust.start();
    RecoveryAdjustTy::go(_graph);
    TimerRecoveryCrashedAdjust.stop();
  }
}
