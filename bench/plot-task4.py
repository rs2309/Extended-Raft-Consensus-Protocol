import matplotlib.pyplot as plt
import os

# Base directory where result files are located
base = "../build/app/"

# File paths for the three configurations
files = {
    '3 Replicas': 'result3.txt',
    '5 Replicas': 'result5.txt',
    '11 Replicas': 'result11.txt'
}

# Initialize a dictionary to hold data for each configuration
data = {}

# Read data from each file
for config, file_path in files.items():
    clientCount = []
    latAvg = []
    latP50 = []
    latP90 = []
    latP99 = []
    throughput = []
    
    with open(os.path.join(base, file_path), 'r') as file:
        # Skip the headers and any dashed lines
        for line in file:
            if line.strip() and not line.startswith('#') and not line.startswith('-'):
                # Assume the values are space-separated
                values = line.split()
                clientCount.append(int(values[0]))
                latAvg.append(float(values[1]))
                latP50.append(float(values[2]))
                latP90.append(float(values[3]))
                latP99.append(float(values[4]))
                # The throughput is expected to be the 6th value
                throughput.append(float(values[5]))
    
    # Store the extracted data
    data[config] = {
        'throughput': throughput,
        'latAvg': latAvg,
        'latP50': latP50,
        'latP90': latP90,
        'latP99': latP99
    }

# List of latency metrics and their friendly names for plotting
latency_metrics = ['latAvg', 'latP50', 'latP90', 'latP99']
metric_names = {
    'latAvg': 'Average Latency',
    'latP50': 'P50 Latency',
    'latP90': 'P90 Latency',
    'latP99': 'P99 Latency'
}

# Generate and save a plot for each latency metric
for metric in latency_metrics:
    plt.figure(figsize=(12, 8))
    
    # Plot latency vs. throughput for each configuration
    for config in files.keys():
        plt.plot(data[config]['throughput'], data[config][metric], marker='o', label=config)
    
    # Enhance plot with labels, title, and grid
    plt.title(f'{metric_names[metric]} vs. Throughput for Different Replica Configurations')
    plt.xlabel('Throughput (ops/sec)')
    plt.ylabel(f'{metric_names[metric]} (ms)')
    plt.legend()
    plt.grid(True)
    
    # Save the plot as an image file
    plt.savefig(f'{metric}_vs_Throughput_Task4.png', dpi=300)
    plt.close()
