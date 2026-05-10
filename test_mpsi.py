import subprocess
import re
import csv
import statistics
from datetime import datetime

# Configuration
EXECUTABLE = "./app"
RUNNING_MODE = "it-mpsi"
RUNS_PER_CONFIG = 5 
N_VALUES = [20, 60, 100]
M_I_MAPPING = {
    2**18: 260000,
    2**19: 520000,
    2**20: 1040000
}
D_VALUES = [2**21, 2**22, 2**23]

def run_benchmarks():
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    txt_filename = f"result_{timestamp}.txt"
    csv_filename = f"result_{timestamp}.csv"

    csv_header = [
        "n", "m", "d", "i", "iterations",
        "Avg Randomness Generation Time (s)", 
        "Avg Distribution Time (s)", 
        "Avg Total Runtime (s)", 
        "Avg Total Communication (MB)"
    ]

    print(f"Starting benchmarks ({RUNS_PER_CONFIG} runs per config).")
    print(f"Logs: {txt_filename} | Summary: {csv_filename}")

    with open(txt_filename, "w") as txt_file, open(csv_filename, "w", newline="") as csv_file:
        writer = csv.writer(csv_file)
        writer.writerow(csv_header)

        for n in N_VALUES:
            for m, i in M_I_MAPPING.items():
                for d in D_VALUES:
                    # Lists to store metrics for the current configuration
                    rand_times = []
                    dist_times = []
                    total_runtimes = []
                    comm_volumes = []

                    cmd = [EXECUTABLE, "-running", RUNNING_MODE, "-n", str(n), "-m", str(m), "-d", str(d), "-i", str(i)]
                    
                    print(f"Testing Config: n={n}, m={m}, d={d} | Runs: ", end="", flush=True)

                    for run in range(RUNS_PER_CONFIG):
                        print(f"{run + 1} ", end="", flush=True)
                        
                        process_result = subprocess.run(cmd, capture_output=True, text=True)
                        output = process_result.stdout
                        
                        # Save raw output to text file
                        txt_file.write(f"--- Config: n={n},m={m},d={d} | Run: {run+1}/{RUNS_PER_CONFIG} ---\n")
                        txt_file.write(output)
                        txt_file.write("\n" + "-"*40 + "\n")

                        try:
                            # Extraction via Regex
                            r_ms = float(re.search(r"get randomness time:\s*([\d.]+)", output).group(1))
                            d_ms = float(re.search(r"distribution time:\s*([\d.]+)", output).group(1))
                            t_ms = float(re.search(r"Total Runtime\s*:\s*([\d.]+)", output).group(1))
                            c_mb = float(re.search(r"Total Communication:\s*([\d.]+)", output).group(1))

                            rand_times.append(r_ms / 1000.0)    # Convert ms to s
                            dist_times.append(d_ms / 1000.0)    # Convert ms to s
                            total_runtimes.append(t_ms / 1000.0) # Convert ms to s
                            comm_volumes.append(c_mb)
                        except (AttributeError, ValueError):
                            print(f"\n[!] Parse Error on run {run+1}")
                    
                    # Calculate averages and write to CSV
                    if len(total_runtimes) > 0:
                        writer.writerow([
                            n, m, d, i, RUNS_PER_CONFIG,
                            round(statistics.mean(rand_times), 6),
                            round(statistics.mean(dist_times), 6),
                            round(statistics.mean(total_runtimes), 6),
                            round(statistics.mean(comm_volumes), 4)
                        ])
                        csv_file.flush()
                        print(" | Done (Avg calculated)")
                    else:
                        print(" | Failed (No data collected)")

    print(f"\nAll benchmarks finished.")

if __name__ == "__main__":
    run_benchmarks()