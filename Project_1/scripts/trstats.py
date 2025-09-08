from json import dump
from re import findall
from time import sleep
from subprocess import run
from statistics import median
import matplotlib.pyplot as plt
from argparse import ArgumentParser


def main():
    # Define arguments
    desc = "Run traceroute multiple times towards a given target host"
    parser = ArgumentParser(description=desc)
    parser.add_argument("-n", dest="num_runs", default="10", help="Number of times traceroute will run")
    parser.add_argument("-d", dest="run_delay", default="0", help="Number of seconds to wait between two consecutive runs")
    parser.add_argument("-m", dest="max_hops", default="30", help="Number of max hops per traceroute run")
    parser.add_argument(
        "-o",
        dest="output",
        default="../output/stats/tr_stats.json",
        help="Path and name of output JSON file containing the stats",
    )
    parser.add_argument(
        "-g",
        dest="graph",
        default="../output/box_plots/tr_plot.pdf",
        help="Path and name of output PDF file containing stats graph",
    )
    parser.add_argument("-t", dest="target", default="google.com", help="A target domain name or IP address")
    parser.add_argument(
        "--test",
        dest="test_dir",
        default="",
        help="Directory containing num_runs text files, \
                        each of which contains the output of a traceroute run. If present, this will override all \
                        other options and traceroute will not be invoked. Stats will be computed over the traceroute \
                        output stored in the text files",
    )

    # Assign arguments to variables
    args = parser.parse_args()
    num_runs = int(args.num_runs)
    run_delay = float(args.run_delay)
    max_hops = int(args.max_hops)
    output = args.output
    graph = args.graph
    target = args.target
    test_dir = args.test_dir

    # Define global variables to be used later
    hops = None
    json_output = []
    hops_times = [[] for _ in range(max_hops)]
    hops_hosts = [[] for _ in range(max_hops)]

    # Run the traceroute command if no test directory is provided
    if test_dir == "":
        # Loop for num_runs to generate output files
        for i in range(1, num_runs + 1):
            # Run the traceroute command with the passed arguments
            result = run(["traceroute", target, "-m", f"{max_hops}"], capture_output=True, text=True)

            # Send the current loop results to a output_*.txt file
            with open(f"../output/tr_outputs/tr_output_{i}.txt", "w") as f:
                print(result.stdout, file=f)

            # Add a delay based on passed argument
            sleep(run_delay)

    # Define the output files to be parsed
    path = "../output/tr_outputs" if test_dir == "" else test_dir

    # Loop for num_runs to parse output files
    for i in range(1, num_runs + 1):
        # Open the output files one-by-one and extract the lines as a list
        with open(f"{path}/tr_output_{i}.txt") as f:
            lines = f.readlines()

        # Define current_hops variable for next loop
        current_hops = None

        # Loop for each line in the current output file
        for line in lines:
            # Don't process header and empty lines
            if line != lines[0] and line != "\n":
                # Split the current line into its parts and extract the hop number
                parts = line.split()
                hop = int(parts[0])
                hop_info = line[len(parts[0]) :].strip()

                # Reassign current_hops to get total number of hops in current file
                current_hops = hop

                # Extract the data from the current hop
                times = findall(r"([\d\.]+)\s+ms", hop_info)
                times = [float(time) for time in times]
                hosts = findall(r"[^\s\(\)]+\s+\([\d\.]+\)", hop_info)

                # Subtract 1 from the hop for array assignment
                hop_index = hop - 1

                # Add the times of the current hop to the global array
                hops_times[hop_index].extend(times)

                # Check if there are any new hosts for the current hop and add them if there are any
                new_hosts = [host for host in hosts if host not in hops_hosts[hop_index]]
                hops_hosts[hop_index].extend(new_hosts)

        # If hops hasn't been defined or if the current file has more hops than previously assigned
        if not hops or current_hops > hops:
            hops = current_hops

    # Modify the hops_times array so that it doesn't hold empty arrays for hops that never occurred
    for i in range(max_hops - hops):
        hops_times.pop()

    # Loop for each hop to calculate stats
    for hop_times in hops_times:
        # Only do calculations if there are latency values
        if len(hop_times) > 0:
            # Calculate the statistics for the current hop
            avg = sum(hop_times) / len(hop_times)
            maximum = max(hop_times)
            med = round(median(hop_times), 4)
            minimum = min(hop_times)

            # Define the JSON output for the current hop
            hop_json_output = {
                "avg": avg,
                "hop": (hops_times.index(hop_times) + 1),
                "hosts": hops_hosts[hops_times.index(hop_times)],
                "max": maximum,
                "med": med,
                "min": minimum,
            }

            # Append the current output to the running list
            json_output.append(hop_json_output)

    # Create the JSON file
    with open(output, "w") as json_file:
        dump(json_output, json_file, indent=1)

    # Plot
    plt.figure(figsize=(12, 8))
    plt.boxplot(
        hops_times,
        patch_artist=True,
        boxprops=dict(facecolor="cornflowerblue", color="midnightblue"),
        medianprops=dict(color="red"),
        whiskerprops=dict(color="midnightblue"),
        capprops=dict(color="midnightblue"),
        flierprops=dict(markerfacecolor="cornflowerblue", markeredgecolor="midnightblue", markersize=3),
    )
    plt.xlabel("Hop")
    plt.ylabel("Time (ms)")
    plt.title("Traceroute Box Plot")
    plt.savefig(graph, dpi=300, bbox_inches="tight")
    plt.show()


if __name__ == "__main__":
    main()
