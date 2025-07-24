import matplotlib.pyplot as plt

# Define file path
file_path = '../build/app/result.txt'

# Initialize lists for data storage
clientCount = []
latAvg = []
latP50 = []
latP90 = []
latP99 = []
throughput = []

# Read the data from the specified file path
with open(file_path, 'r') as file:
    # Skip the headers and any dashed lines
    for line in file:
        if line.strip() and not line.startswith('#') and not line.startswith('-'):
            # Assume the values are tab-separated
            data = line.split()
            clientCount.append(int(data[0]))
            latAvg.append(float(data[1]))
            latP50.append(float(data[2]))
            latP90.append(float(data[3]))
            latP99.append(float(data[4]))
            throughput.append(float(data[5]))

# Create a plot
plt.figure(figsize=(12, 8))

# Plot latency vs. throughput for different percentiles using line plots
plt.plot(throughput, latAvg, marker='o', label='Average Latency (ms)', linestyle='-')
plt.plot(throughput, latP50, marker='o', label='P50 Latency (ms)', linestyle='-')
plt.plot(throughput, latP90, marker='o', label='P90 Latency (ms)', linestyle='-')
plt.plot(throughput, latP99, marker='o', label='P99 Latency (ms)', linestyle='-')

# Enhance plot with labels, title, and grid
plt.title('Latency vs. Throughput Analysis')
plt.xlabel('Throughput (ops/sec)')
plt.ylabel('Latency (ms)')
plt.legend()
plt.grid(True)

# Save the plot as an image file
plt.savefig('LatencyThroughputLineChart.png', dpi=300)

# Optionally display the plot
# plt.show()
