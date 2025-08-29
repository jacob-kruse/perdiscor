from json import dump
from subprocess import run
from statistics import median
from re import search, findall
from socket import gethostbyname
from argparse import ArgumentParser
import matplotlib.pyplot as plt


def main():
    # Define arguments
    desc = "Run traceroute multiple times towards a given target host"
    parser = ArgumentParser(description=desc)
    parser.add_argument("-d", dest="delay", default="0.02", help="Number of seconds to wait between two consecutive ping packets")
    parser.add_argument("-m", dest="max_pings", default="30", help="Number of max ping packets to send")
    parser.add_argument(
        "-o",
        dest="output",
        default="../output/stats/ping_stats.json",
        help="Path and name of output JSON file containing the stats",
    )
    parser.add_argument(
        "-g",
        dest="graph",
        default="../output/box_plots/ping_plot.pdf",
        help="Path and name of output PDF file containing stats graph",
    )
    parser.add_argument(
        "-t", dest="target", default="www.google.com", help="A target domain name or IP address"
    )

    # Assign arguments to variables
    args = parser.parse_args()
    delay = float(args.delay)
    max_pings = int(args.max_pings)
    output = args.output
    graph = args.graph
    target = args.target

    # Run the ping command with the passed arguments
    result = run(["ping", target, "-c", f"{max_pings}", "-i", f"{delay}"], capture_output=True, text=True)

    # Find the lines in the output string that has the data
    times = findall(r"time=([\d.]+) ms", result.stdout)
    stats = search(r"rtt min/avg/max/mdev = ([\d.]+)/([\d.]+)/([\d.]+)/([\d.]+)", result.stdout)

    # Check if there are matches in the resulting string
    if stats and times:
        # Convert the extracted times to floats and find the median
        times = [float(time) for time in times]
        med = round(median(times), 4)

        # Assign the pre-computed stats from the output to the corresponding variables
        min, avg, max, _ = map(float, stats.groups())

        # Define the JSON output
        json_output = {"avg": avg, "hop": 0, "host": f"{target} ({gethostbyname(target)})", "max": max, "med": med, "min": min}

        # Create the JSON file
        with open(output, "w") as json_file:
            dump(json_output, json_file, indent=1)

        # Plot a box plot of the data
        plt.figure(figsize=(12, 8))
        plt.boxplot(
            times,
            positions=[0],
            patch_artist=True,
            boxprops=dict(facecolor="cornflowerblue", color="midnightblue"),
            medianprops=dict(color="red"),
            whiskerprops=dict(color="midnightblue"),
            capprops=dict(color="midnightblue"),
            flierprops=dict(markerfacecolor="cornflowerblue", markeredgecolor="midnightblue"),
        )
        plt.xlabel("Hop")
        plt.ylabel("Time (ms)")
        plt.title("Ping Box Plot")
        plt.savefig(graph, dpi=300, bbox_inches="tight")
        plt.show()

    # Error checking with message
    else:
        print("Ping did not return statistics, try again")


if __name__ == "__main__":
    main()
