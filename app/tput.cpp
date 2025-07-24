#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>
#include <numeric>   // For std::accumulate
#include <algorithm> // For std::sort>
#include <thread>
#include <mutex>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <csignal>
#include <sys/types.h>
#include <unistd.h>
#include <future>
#include <atomic>

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
ABSL_FLAG(int, verbosity, 0,
          "Verbosity level: 0 (silent), 1 (raft message (file sink only))");
ABSL_FLAG(int, fail_type, 0, "Failure Type: 0 (disconnection), 1 (partition)");

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
double compute_p90(std::vector<T> &values)
{
    return compute_percentile<SORTED>(values, 90.0);
}

template <bool SORTED = false, typename T>
double compute_p99(std::vector<T> &values)
{
    return compute_percentile<SORTED>(values, 99.0);
}

void measure_once(toolings::RaftTestCtrl &ctrl, uint64_t clientCount, std::ofstream &resultFile, std::shared_ptr<spdlog::logger> logger)
{
    constexpr size_t N = 1000; // Number of operations per client
    std::vector<double> all_latencies;
    all_latencies.reserve(N * clientCount);
    std::string data = "Hello, Raft!";
    std::mutex latencies_mutex;
    std::mutex ops_mutex;
    size_t total_successful_ops = 0;

    // Start time measurement
    auto start_time = std::chrono::steady_clock::now();

    // Create client threads
    std::vector<std::thread> clients;
    for (uint64_t i = 0; i < clientCount; ++i)
    {
        clients.emplace_back([&, i]()
                             {
            try
            {
                std::vector<double> latencies;
                latencies.reserve(N);
                size_t successful_ops = 0;

                for (size_t j = 0; j < N; ++j)
                {
                    auto start = std::chrono::steady_clock::now();

                    // Directly call propose_to_all_sync without async
                    auto responses = ctrl.propose_to_all_sync(data);
                    auto end = std::chrono::steady_clock::now();

                    bool success = false;
                    for (const auto &response : responses)
                    {
                        if (response.is_leader())
                        {
                            // Successful proposal processed by the leader
                            success = true;
                            break;
                        }
                    }

                    if (success)
                    {
                        double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
                        latencies.push_back(elapsed_ms);
                        ++successful_ops;
                    }
                    else
                    {
                        // Handle failure if needed
                        logger->error("Client {} proposal not accepted at iteration {}", i, j);
                    }
                }

                // Lock and append latencies and successful_ops
                {
                    std::lock_guard<std::mutex> lock(latencies_mutex);
                    all_latencies.insert(all_latencies.end(), latencies.begin(), latencies.end());
                }
                {
                    std::lock_guard<std::mutex> lock(ops_mutex);
                    total_successful_ops += successful_ops;
                }

                logger->debug("Client {} thread finished with {} successful operations", i, successful_ops);
            }
            catch (const std::exception &e)
            {
                logger->error("Client {} Exception occurred: {}", i, e.what());
            }
            catch (...)
            {
                logger->error("Client {} Unknown exception occurred.", i);
            } });
    }

    // Wait for all clients to finish
    for (auto &client : clients)
    {
        client.join();
    }

    // End time measurement
    auto end_time = std::chrono::steady_clock::now();
    double total_time_sec = std::chrono::duration<double>(end_time - start_time).count();

    // Compute metrics
    if (!all_latencies.empty())
    {
        double latAvg = compute_avg(all_latencies);
        double latP50 = compute_p50(all_latencies);
        double latP90 = compute_p90(all_latencies);
        double latP99 = compute_p99(all_latencies);

        // Compute throughput (operations per second)
        double throughput = total_successful_ops / total_time_sec;

        logger->info("Measurement completed. Operations: {} Total time: {:.2f}s Throughput: {:.2f} ops/sec",
                     total_successful_ops, total_time_sec, throughput);

        // Output the results to result.txt
        resultFile << std::fixed << std::setprecision(3);
        resultFile << std::setw(12) << clientCount
                   << std::setw(13) << latAvg
                   << std::setw(13) << latP50
                   << std::setw(13) << latP90
                   << std::setw(13) << latP99
                   << std::setw(16) << throughput << std::endl;
    }
    else
    {
        logger->error("No successful operations recorded.");
        resultFile << std::fixed << std::setprecision(2);
        resultFile << std::setw(12) << clientCount
                   << std::setw(12) << 0.0 // latAvg
                   << std::setw(12) << 0.0 // latP50
                   << std::setw(12) << 0.0 // latP90
                   << std::setw(12) << 0.0 // latP99
                   << std::setw(16) << 0.0 // throughput
                   << std::endl;
    }
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

    if (argc < 2)
    {
        std::cerr << "Usage: ./tput <MaxClientCount>" << std::endl;
        return 1;
    }
    uint64_t maxClientCount = std::stoull(argv[1]);

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

    // Prepare the result file
    std::ofstream resultFile("result.txt");
    if (!resultFile.is_open())
    {
        std::cerr << "Failed to open result.txt for writing." << std::endl;
        return 1;
    }

    // Write headers to result.txt
    resultFile << "##################################################################################" << std::endl;
    resultFile << "# clientCount      latAvg       latP50       latP90       latP99        throughput" << std::endl;
    resultFile << "#                    (ms)         (ms)         (ms)         (ms)         (ops/sec)" << std::endl;
    resultFile << "----------------------------------------------------------------------------------" << std::endl;

    // Logger setup
    auto logger_name = "tput";
    auto logger = spdlog::get(logger_name);
    // std::cout << "Logger level set to: " << logger->level() << std::endl;
    if (!logger)
    {
        // Create the logger if it doesn't exist
        logger = spdlog::basic_logger_mt(
            logger_name, fmt::format("logs/{}.log", logger_name), true);
    }

    // Set logger level to debug to capture all logs
    // //logger->set_level(verbosity);

    if (logger)
    {
        spdlog::flush_every(std::chrono::seconds(3));
        // Set logger level based on verbosity
        if (verbosity == 1)
        {
            logger->set_level(spdlog::level::debug);
        }
        else if (verbosity == 2)
        {
            logger->set_level(spdlog::level::info);
        }
        else if (verbosity == 3)
        {
            logger->set_level(spdlog::level::warn);
        }
        else if (verbosity == 4)
        {
            logger->set_level(spdlog::level::err);
        }
        else if (verbosity == 5)
        {
            logger->set_level(spdlog::level::critical);
        }
        else
        {
            logger->set_level(spdlog::level::off);
        }
    }

    for (uint64_t clientCount = 1; clientCount <= maxClientCount; clientCount *= 2)
    {
        logger->info("Starting test with {} clients", clientCount);

        // Setup configs and RaftTestCtrl
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

        toolings::RaftTestCtrl ctrl(
            configs, node_tester_ports,
            std::string(node_path), std::string(ctrl_addr), fail_type,
            verbosity, logger);

        // Start Raft servers
        run_raft_servers(ctrl);

        // Perform the measurement for current client count
        measure_once(ctrl, clientCount, resultFile, logger);

        // Clean up Raft servers
        cleanup_raft_servers(ctrl);

        logger->info("Test with {} clients completed", clientCount);
    }

    resultFile.close();

    logger->info("All tests completed");

    return 0;
}
