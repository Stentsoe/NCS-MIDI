import re
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

# Define the input and output file paths
input_file_path = 'RTT_Log_test_26.log'
output_file_path = 'output.log'


def read_lines(path):
    # Open the input file for reading
    with open(path, 'r') as input_file:
        # Read all lines from the input file
        lines = input_file.readlines()
    return lines

def find_starting_point(lines):
    # Find the index of the first sent message after the first received message
    start_index = next((i for i, line in enumerate(lines) if line.startswith("00> R")), None)
    start_index += next((i for i, line in enumerate(lines[start_index:]) if line.startswith("00> S")), None)

    # If a matching line is found, create a new list of lines to keep
    if start_index is not None:
        lines_to_keep = lines[start_index:]
    else:
        # If no matching line is found, keep all lines
        lines_to_keep = lines

    return lines_to_keep


def create_output(lines, path):
    # Open the output file for writing and write the filtered lines
    with open(path, 'w') as output_file:
        output_file.writelines(lines)

    print(f"Filtered log saved to {path}")


def filter_line_1(lines, filter):
    # Create a new list to store the filtered lines
    filtered_lines = [line for line in lines if line.startswith(filter) ]
    return filtered_lines


def filter_line_2(lines, filter_1, filter_2):
    # Create a new list to store the filtered lines
    filtered_lines = [line for line in lines if line.startswith(filter_1) or line.startswith(filter_2)]
    return filtered_lines

def extract_times(lines):
    times = []
    
    for line in lines:
        # Use regular expressions to find numbers after "00> S" or "00> R"
        matches = re.findall(r'00> [SR]\s+(\d+)', line)

        times.append(int(matches[0]))
    
    return times

def set_t0_to_0(times, offset_time):
    adjusted_times = [x - offset_time for x in times]
    return adjusted_times


def adjust_for_lost_messages(times_send, times_receive, expected_latency):
    outliers = 0
    i = 0
    for _ in range(len(times_send)):

        if (i >= len(times_receive)):
            print("no more messages")
            return times_send[:len(times_receive)], times_receive
        
        difference = times_receive[i] - times_send[i]
        deviation = abs(difference - expected_latency)

        if (deviation > 1000):
            outliers += 1
            # print(i)
            if(outliers > 10):
                print("packet lost, index", i-10, "deviation", deviation, "send", times_send[i-10], "receive", times_receive[i-10])
                # times_send.insert(0, [i-10])
                times_receive.insert(i-10, 0)
                i = i-10
                outliers = 0

        else:
            outliers = 0
        i +=1
    print(len(times_send), len(times_receive)) 
    
    
    if (len(times_send) < len(times_receive)):  
        return times_send, times_receive[:len(times_send)]
    else:
        return times_send[:len(times_receive)], times_receive

        
def calculate_latency(times_send, times_receive):
    latencies = []
    for i in range(len(times_send)):
        # if(times_receive != 0):
        latencies.append(times_receive[i] - times_send[i])
    # print(latencies[27], times_receive[27], times_send[27])
    times_receive
    return latencies

def plot_timeline_latency(times_send, times_receive, latencies):
    fig, ax = plt.subplots()
    x = [] 
    y = []
    for i in range(len(times_send)):
        if(times_receive != 0):
            x.append(times_send[i])
            y.append(latencies[i])
    # print(x)
    # print(y)
    # print(y[27])
    ax.scatter(x, y, s=1.5)
    
    ax.set(ylim=(0, 10000), yticks=np.arange(500, 10000, 1000))
    plt.show()
    # xlim=(0, x[len(x)-1]), xticks=np.arange(1, 8),


def plot_histogram(latencies):
     # Sample latency data (you should replace this with your actual data)
    # latencies = [10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90]

    # Define the latency bins
    bins = np.arange(1000, 10030, 30)
    # print(bins)

    # Calculate the histogram of latencies
    hist, _ = np.histogram(latencies, bins)

    # Calculate the percentage of messages within each bin
    total_messages = len(latencies)
    percentage_messages = [(count / total_messages) * 100 for count in hist]

    # Create a line graph
    bin_labels = [f'{bins[i]}-{bins[i+1]}' for i in range(len(bins)-1)]
    # bin_labels = np.arange(0, 105, 5)
    x = np.arange(1000, 10000, 30)

    plt.plot(x, percentage_messages, linestyle='-')

    # Add labels and title
    plt.xlabel('Latency Bins')
    plt.ylabel('Percentage of Total Messages (%)')
    plt.title('Message Distribution by Latency Bins')

    # Customize x-axis labels
    # plt.xticks(x, bin_labels)

    plt.show()

def main():
    lines = read_lines(input_file_path)
    lines = find_starting_point(lines)
    lines_send_receive = filter_line_2(lines, "00> S", "00> R")
    lines_send = filter_line_1(lines, "00> S")
    lines_receive = filter_line_1(lines, "00> R")
    times_send = extract_times(lines_send)
    times_receive = extract_times(lines_receive)
    
    
    times_receive = set_t0_to_0(times_receive, times_send[0])
    times_send = set_t0_to_0(times_send, times_send[0])
    print(times_receive[0] - times_send[0])
    times_send, times_receive = adjust_for_lost_messages(times_send, times_receive, 6100)
    print(len(times_send), len(times_receive))
    latencies = calculate_latency(times_send, times_receive)
    plot_timeline_latency(times_send, times_receive, latencies)
    # print(latencies)
    plot_histogram(latencies)

    # print(times_receive)
    create_output(lines_receive, output_file_path)



  


if __name__ == "__main__":
    main()
