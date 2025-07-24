#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <numeric>   // For std::accumulate
#include <algorithm> // For std::sort

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/spdlog.h"

#include "common/config.hpp"
#include "toolings/config_gen.hpp"
#include "toolings/test_ctrl.hpp"

constexpr std::string_view node_path = "./node";
constexpr std::string_view ctrl_addr = "0.0.0.0:55000";

ABSL_FLAG(uint64_t, num, 3, "number of nodes to spawn (>= 3)");
ABSL_FLAG(std::string, bin, "./node", "the binary of node app");
ABSL_FLAG(int, verbosity, 1,
          "Verbosity level: 0 (silent), 1 (raft message (file sink only))");
ABSL_FLAG(int, fail_type, 0, "Failure Type: 0 (disonnection), 1 (partition)");

static pid_t pgid = 0;

void signal_handler(int signal)
{
  if (signal == SIGINT)
  {
    std::cout << "Caught SIGINT (Ctrl+C), cleaning up..." << std::endl;
    if (::kill(-pgid, SIGKILL) == 0)
    {
      // well... dead...
    }
    else
    {
      std::perror("Failed to kill process");
      std::exit(1);
    }
    exit(0);
  }
}

/********************* CODES TO FILL IN *********************/
template <typename T>
double compute_avg(const std::vector<T> &values)
{
  if (values.empty())
    return 0.0;
  double sum = std::accumulate(values.begin(), values.end(), 0.0);
  return sum / values.size();
}

template <bool SORTED = false, typename T>
double compute_percentile(std::vector<T> &values, double percentile)
{
  if (values.empty())
    return 0.0;

  if constexpr (!SORTED)
  {
    std::sort(values.begin(), values.end());
  }
  if (percentile < 0.0)
    percentile = 0.0;
  if (percentile > 100.0)
    percentile = 100.0;
  size_t idx = static_cast<size_t>(std::ceil(percentile / 100.0 * values.size())) - 1;
  if (idx >= values.size())
    idx = values.size() - 1;
  return static_cast<double>(values[idx]);
}

template <bool SORTED = false, typename T>
double compute_p50(std::vector<T> &values)
{
  return compute_percentile<SORTED>(values, 50.0);
}

template <bool SORTED = false, typename T>
double compute_p99(std::vector<T> &values)
{
  return compute_percentile<SORTED>(values, 99.0);
}

void measure_once(toolings::RaftTestCtrl &ctrl)
{
  constexpr size_t N = 1000;
  std::vector<double> latencies;
  latencies.reserve(N);
  std::string data = "hello, world";

  for (size_t i = 0; i < N; ++i)
  {
    auto start = std::chrono::steady_clock::now();

    // Send synchronous proposal
    auto responses = ctrl.propose_to_all_sync(data);

    auto end = std::chrono::steady_clock::now();
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();

    // Check if proposal was accepted by the leader
    bool success = false;
    for (const auto &response : responses)
    {
      if (response.is_leader())
      {
        success = true;
        break;
      }
    }

    if (success)
    {
      latencies.push_back(elapsed_ms);
    }
    else
    {
      // Handle failure if needed
      std::cerr << "Proposal not accepted at iteration " << i << std::endl;
    }

    // if (i % 100 == 0)
    //     std::cout << i << " " << elapsed_ms << " ms" << std::endl;
  }

  // Compute latency statistics
  double latAvg = compute_avg(latencies);
  double latP50 = compute_p50(latencies);
  double latP99 = compute_p99(latencies);

  // Output the results
  std::cout << "######################################" << std::endl;
  std::cout << "#      latAvg       latP50      latP99" << std::endl;
  std::cout << "#                    (ms)         (ms)" << std::endl;
  std::cout << "--------------------------------------" << std::endl;
  std::cout << std::fixed << std::setprecision(3);
  std::cout << std::setw(13) << latAvg
            << std::setw(13) << latP50
            << std::setw(13) << latP99 << std::endl;
}

/********************* CODES TO FILL IN *********************/

/**
 * @brief Runs the Raft servers and registers an applier handler.
 *
 * This function registers an applier handler that does nothing when applied.
 * It then runs the Raft test controller and sleeps for 3 seconds.
 *
 * @param ctrl Reference to the RaftTestCtrl object that controls the Raft test.
 */
void run_raft_servers(toolings::RaftTestCtrl &ctrl)
{
  ctrl.register_applier_handler({[](testerpb::ApplyResult _) -> void { /* Do nothing */ }});
  ctrl.run();
  std::this_thread::sleep_for(std::chrono::seconds(3));
}

/**
 * @brief Cleans up the Raft servers by terminating them.
 *
 * This function calls the `kill` method on the provided RaftTestCtrl
 * object to terminate all the Raft servers managed by the controller.
 *
 * @param ctrl Reference to a RaftTestCtrl object that manages the Raft servers.
 */
void cleanup_raft_servers(toolings::RaftTestCtrl &ctrl)
{
  ctrl.kill();
}

int main(int argc, char **argv)
{
  absl::ParseCommandLine(argc, argv);

  auto num = absl::GetFlag(FLAGS_num);
  auto binary_path = absl::GetFlag(FLAGS_bin);
  auto fail_type = absl::GetFlag(FLAGS_fail_type);
  auto verbosity = absl::GetFlag(FLAGS_verbosity);

  pgid = getpid();
  // Register the signal handler for Ctrl+C (SIGINT)
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, nullptr);

  std::vector<rafty::Config> configs;
  std::unordered_map<uint64_t, uint64_t> node_tester_ports;
  uint64_t tester_port = 55001;

  auto insts = toolings::ConfigGen::gen_local_instances(num, 50050);
  for (const auto &inst : insts)
  {
    std::map<uint64_t, std::string> peer_addrs;
    for (const auto &peer : insts)
    {
      if (peer.id == inst.id)
        continue;
      peer_addrs[peer.id] = peer.external_addr;
    }
    rafty::Config config = {
        .id = inst.id, .addr = inst.listening_addr, .peer_addrs = peer_addrs};
    configs.push_back(config);
    node_tester_ports[inst.id] = tester_port;
    tester_port++;
  }

  // logger setup
  auto logger_name = "multinode";
  auto logger = spdlog::get(logger_name);
  if (!logger)
  {
    // Create the logger if it doesn't exist
    logger = spdlog::basic_logger_mt(
        logger_name, std::format("logs/{}.log", logger_name), true);
  }
  spdlog::flush_every(std::chrono::seconds(3));

  toolings::RaftTestCtrl ctrl(
      configs, node_tester_ports,
      std::string(node_path), std::string(ctrl_addr), fail_type,
      verbosity, logger);

  // don't forget to invoke this function to start up the raft servers
  run_raft_servers(ctrl);
  // do the measurement
  measure_once(ctrl);
  // don't forget to invoke this function to clean up the raft servers
  cleanup_raft_servers(ctrl);
  return 0;
}