import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from pathlib import Path
import argparse
import sys
import glob

def analyze_experiment(exp_dir):
    exp_name = exp_dir.name
    algorithm = "prague" if exp_name.startswith("P-") else "cubic"
    
    print(f"\nAnalyzing {exp_name} (Algorithm: {algorithm})...")

    # Data loading with error handling
    def load_data(file_pattern):
        try:
            return np.loadtxt(exp_dir / file_pattern.format(algorithm=algorithm, exp_name=exp_name))
        except FileNotFoundError:
            print(f"Warning: File {file_pattern.format(algorithm=algorithm, exp_name=exp_name)} not found. Skipping corresponding plot/analysis.")
            return None
        except Exception as e:
            print(f"Warning: Could not load {file_pattern.format(algorithm=algorithm, exp_name=exp_name)}. Error: {e}. Skipping.")
            return None

    throughput_data = load_data("{algorithm}-throughput.{exp_name}.dat")
    
    if algorithm == "prague":
        queue_delay_data = load_data("wired-dualpi2-l-sojourn.{exp_name}.dat")
        queue_bytes_data = load_data("wired-dualpi2-bytes.{exp_name}.dat")
    else: # cubic
        queue_delay_data = load_data("wired-fqcodel-sojourn.{exp_name}.dat")
        queue_bytes_data = load_data("wired-fqcodel-bytes.{exp_name}.dat")

    sink_rx_data = load_data("{algorithm}-sink-rx.{exp_name}.dat")
    cwnd_data = load_data("{algorithm}-cwnd.{exp_name}.dat")
    rtt_data = load_data("{algorithm}-rtt.{exp_name}.dat")
    pacing_rate_data = load_data("{algorithm}-pacing-rate.{exp_name}.dat")

    # Ensure essential data is present for basic stats
    if throughput_data is None or queue_delay_data is None or sink_rx_data is None:
        print(f"Error: Essential data files missing for {exp_name}. Cannot generate full analysis.")
        pass

    # Calculate goodput (Mbps)
    goodput = None
    goodput_time = None
    if sink_rx_data is not None and len(sink_rx_data) > 1:
        dt = np.diff(sink_rx_data[:, 0])  # Time differences
        dt_safe = np.where(dt == 0, 1e-9, dt)
        goodput_diff = np.diff(sink_rx_data[:, 1])
        goodput = goodput_diff * 8 / dt_safe / 1e6  # Convert to Mbps
        goodput_time = sink_rx_data[:-1, 0] + dt_safe/2
    else:
        print("Warning: Sink RX data insufficient for goodput calculation.")

    # Trim warm-up and teardown periods (first and last 5 seconds)
    warmup_time = 5.0
    teardown_time = 5.0
    
    if throughput_data is not None and len(throughput_data) > 0:
        total_time = throughput_data[-1, 0]
    elif goodput_time is not None and len(goodput_time) > 0:
        total_time = goodput_time[-1]
    else:
        total_time = 60.0
        print(f"Warning: Could not determine total_time accurately for {exp_name}. Using default {total_time}s.")

    def trim_data(data, time_col_idx=0, val_col_idx=1):
        if data is None or len(data) == 0:
            return None
        return data[(data[:, time_col_idx] >= warmup_time) & 
                    (data[:, time_col_idx] <= (total_time - teardown_time))]

    throughput_trimmed = trim_data(throughput_data)
    queue_delay_trimmed = trim_data(queue_delay_data)
    queue_bytes_trimmed = trim_data(queue_bytes_data)
    
    goodput_trimmed = None
    if goodput is not None and goodput_time is not None:
        goodput_for_trimming = np.column_stack((goodput_time, goodput))
        goodput_trimmed_stacked = trim_data(goodput_for_trimming)
        if goodput_trimmed_stacked is not None:
            goodput_trimmed = goodput_trimmed_stacked[:, 1]

    # Create detailed analysis plots
    fig, axs = plt.subplots(3, 2, figsize=(18, 15))
    fig.suptitle(f"Experiment Analysis - {exp_name}", fontsize=16)

    # Plot 1: Throughput (Interface Rate & Goodput)
    ax = axs[0, 0]
    if throughput_data is not None:
        ax.plot(throughput_data[:, 0], throughput_data[:, 1], label='Interface Rate')
    if goodput is not None and goodput_time is not None:
        ax.plot(goodput_time, goodput, label='Goodput', alpha=0.7)
    ax.axvspan(0, warmup_time, alpha=0.2, color='gray')
    ax.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    ax.set_title('Throughput & Goodput')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Rate (Mbps)')
    ax.legend()
    ax.grid(True)

    # Plot 2: Queueing Delay
    ax = axs[0, 1]
    if queue_delay_data is not None:
        ax.plot(queue_delay_data[:, 0], queue_delay_data[:, 1], label=f'{"L4S Sojourn" if algorithm == "prague" else "FqCoDel Sojourn"}')
        ax.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    ax.set_title('Queueing Delay')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Delay (ms)')
    ax.legend()
    ax.grid(True)

    # Plot 3: Congestion Window (cwnd)
    ax = axs[1, 0]
    if cwnd_data is not None:
        ax.plot(cwnd_data[:, 0], cwnd_data[:, 1], label='cwnd (bytes)')
        ax.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    ax.set_title('Congestion Window')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('cwnd (Bytes)')
    ax.legend()
    ax.grid(True)

    # Plot 4: RTT Samples
    ax = axs[1, 1]
    if rtt_data is not None:
        ax.plot(rtt_data[:, 0], rtt_data[:, 1], label='RTT samples (ms)', marker='.', linestyle='None', alpha=0.5)
        ax.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    ax.set_title('RTT Samples')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('RTT (ms)')
    ax.legend()
    ax.grid(True)

    # Plot 5: Pacing Rate
    ax = axs[2, 0]
    if pacing_rate_data is not None:
        ax.plot(pacing_rate_data[:, 0], pacing_rate_data[:, 1] / 1e6, label='Pacing Rate (Mbps)')
        ax.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    ax.set_title('Pacing Rate')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Rate (Mbps)')
    ax.legend()
    ax.grid(True)
    
    # Plot 6: Queue Bytes
    ax = axs[2, 1]
    if queue_bytes_data is not None:
        ax.plot(queue_bytes_data[:, 0], queue_bytes_data[:, 1], label=f'{"L4S" if algorithm == "prague" else "FqCoDel"} Queue (Bytes)')
        ax.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    ax.set_title('Queue Size')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Bytes')
    ax.legend()
    ax.grid(True)

    # Calculate statistics
    stats = {}
    if throughput_trimmed is not None and len(throughput_trimmed) > 0:
        stats['interface_rate'] = {
            'mean': np.mean(throughput_trimmed[:, 1]), 'std': np.std(throughput_trimmed[:, 1]),
            'min': np.min(throughput_trimmed[:, 1]), 'max': np.max(throughput_trimmed[:, 1]),
            'p95': np.percentile(throughput_trimmed[:, 1], 95), 'p99': np.percentile(throughput_trimmed[:, 1], 99)
        }
    if goodput_trimmed is not None and len(goodput_trimmed) > 0:
        stats['goodput'] = {
            'mean': np.mean(goodput_trimmed), 'std': np.std(goodput_trimmed),
            'min': np.min(goodput_trimmed), 'max': np.max(goodput_trimmed),
            'p95': np.percentile(goodput_trimmed, 95), 'p99': np.percentile(goodput_trimmed, 99)
        }
    if queue_delay_trimmed is not None and len(queue_delay_trimmed) > 0:
        stats['queue_delay'] = {
            'mean': np.mean(queue_delay_trimmed[:, 1]), 'std': np.std(queue_delay_trimmed[:, 1]),
            'min': np.min(queue_delay_trimmed[:, 1]), 'max': np.max(queue_delay_trimmed[:, 1]),
            'p95': np.percentile(queue_delay_trimmed[:, 1], 95), 'p99': np.percentile(queue_delay_trimmed[:, 1], 99)
        }
    
    if stats:
        print("\nCalculated statistics (excluding warm-up/teardown):")
        if 'interface_rate' in stats: print(f"Interface Rate - Mean: {stats['interface_rate']['mean']:.2f} Mbps")
        if 'goodput' in stats: print(f"Goodput - Mean: {stats['goodput']['mean']:.2f} Mbps")
        if 'queue_delay' in stats: print(f"Queue Delay - Mean: {stats['queue_delay']['mean']:.3f} ms")
        
        with open(exp_dir / 'analysis_summary.txt', 'w') as f:
            f.write(f"Experiment Analysis Summary - {exp_name}\n")
            f.write("=========================\n\n")
            if 'interface_rate' in stats:
                f.write("Interface Rate Statistics (Mbps):\n")
                for key, val in stats['interface_rate'].items(): f.write(f"  {key.capitalize()}: {val:.2f}\n")
                f.write("\n")
            if 'goodput' in stats:
                f.write("Goodput Statistics (Mbps):\n")
                for key, val in stats['goodput'].items(): f.write(f"  {key.capitalize()}: {val:.2f}\n")
                f.write("\n")
            if 'queue_delay' in stats:
                f.write("Queueing Delay Statistics (ms):\n")
                for key, val in stats['queue_delay'].items(): f.write(f"  {key.capitalize()}: {val:.3f}\n")
            print(f"Saved statistics to {exp_dir / 'analysis_summary.txt'}")
    else:
        print("Warning: No statistics calculated due to missing trimmed data.")

    # Save detailed analysis plots
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    plt.savefig(exp_dir / 'analysis_plots.png')
    plt.close(fig)

    # Create and save focused comparison plots
    create_focused_plots(exp_dir, algorithm, throughput_data, goodput_time, goodput, 
                        queue_delay_data, queue_bytes_data, warmup_time, total_time, teardown_time)
    
    return True

def create_focused_plots(exp_dir, algorithm, throughput_data, goodput_time, goodput, 
                        queue_delay_data, queue_bytes_data, warmup_time, total_time, teardown_time):
    """Create focused comparison plots for key metrics"""
    
    # 1. Throughput/Goodput Plot
    plt.figure(figsize=(10, 6))
    if throughput_data is not None:
        plt.plot(throughput_data[:, 0], throughput_data[:, 1], label='Interface Rate', alpha=0.7)
    if goodput is not None and goodput_time is not None:
        plt.plot(goodput_time, goodput, label='Goodput', alpha=0.7)
    plt.axvspan(0, warmup_time, alpha=0.2, color='gray')
    plt.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    plt.title(f'Throughput & Goodput - {algorithm.capitalize()}')
    plt.xlabel('Time (s)')
    plt.ylabel('Rate (Mbps)')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(exp_dir / 'throughput_comparison.png')
    plt.close()

    # 2. Queueing Delay Plot
    plt.figure(figsize=(10, 6))
    if queue_delay_data is not None:
        plt.plot(queue_delay_data[:, 0], queue_delay_data[:, 1], 
                label=f'{"L4S Sojourn" if algorithm == "prague" else "FqCoDel Sojourn"}')
    plt.axvspan(0, warmup_time, alpha=0.2, color='gray')
    plt.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    plt.title(f'Queueing Delay - {algorithm.capitalize()}')
    plt.xlabel('Time (s)')
    plt.ylabel('Delay (ms)')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(exp_dir / 'delay_comparison.png')
    plt.close()

    # 3. Queue Size Plot
    plt.figure(figsize=(10, 6))
    if queue_bytes_data is not None:
        plt.plot(queue_bytes_data[:, 0], queue_bytes_data[:, 1], 
                label=f'{"L4S" if algorithm == "prague" else "FqCoDel"} Queue')
    plt.axvspan(0, warmup_time, alpha=0.2, color='gray')
    plt.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
    plt.title(f'Queue Size - {algorithm.capitalize()}')
    plt.xlabel('Time (s)')
    plt.ylabel('Bytes')
    plt.legend()
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(exp_dir / 'queue_size_comparison.png')
    plt.close()

def create_combined_plots(base_dir):
    """Create combined plots comparing Prague and Cubic for the same conditions"""
    # Find matching experiment pairs
    prague_dirs = glob.glob(str(base_dir / "P-*"))
    cubic_dirs = glob.glob(str(base_dir / "C-*"))
    
    for p_dir in prague_dirs:
        p_name = Path(p_dir).name
        c_name = "C-" + p_name[2:]  # Convert P-* to C-*
        c_dir = base_dir / c_name
        
        if not c_dir.exists():
            continue
            
        # Load data for both experiments
        p_throughput = np.loadtxt(Path(p_dir) / "prague-throughput.dat")
        c_throughput = np.loadtxt(c_dir / "cubic-throughput.dat")
        p_delay = np.loadtxt(Path(p_dir) / "wired-dualpi2-l-sojourn.dat")
        c_delay = np.loadtxt(c_dir / "wired-fqcodel-sojourn.dat")
        
        # Create combined throughput plot
        plt.figure(figsize=(12, 6))
        plt.plot(p_throughput[:, 0], p_throughput[:, 1], label='Prague', color='blue')
        plt.plot(c_throughput[:, 0], c_throughput[:, 1], label='Cubic', color='red')
        plt.title(f'Throughput Comparison - {p_name[2:]}')
        plt.xlabel('Time (s)')
        plt.ylabel('Rate (Mbps)')
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(base_dir / f'combined_throughput_{p_name[2:]}.png')
        plt.close()
        
        # Create combined delay plot
        plt.figure(figsize=(12, 6))
        plt.plot(p_delay[:, 0], p_delay[:, 1], label='Prague', color='blue')
        plt.plot(c_delay[:, 0], c_delay[:, 1], label='Cubic', color='red')
        plt.title(f'Queueing Delay Comparison - {p_name[2:]}')
        plt.xlabel('Time (s)')
        plt.ylabel('Delay (ms)')
        plt.legend()
        plt.grid(True)
        plt.tight_layout()
        plt.savefig(base_dir / f'combined_delay_{p_name[2:]}.png')
        plt.close()

def main():
    parser = argparse.ArgumentParser(description='Analyze RQ1 experiment results')
    parser.add_argument('--exp-dir', type=str, help='Specific experiment directory to analyze')
    args = parser.parse_args()
    
    if args.exp_dir:
        exp_dir = Path(args.exp_dir)
        if not exp_dir.exists():
            print(f"Error: Directory {exp_dir} does not exist")
            sys.exit(1)
        analyze_experiment(exp_dir)
    else:
        print("Analyzing all experiments in results/rq1...")
        base_results_dir = Path(__file__).parent.parent / "rq1"
        if not base_results_dir.exists():
            print(f"Error: Directory {base_results_dir} does not exist")
            sys.exit(1)
            
        for exp_sub_dir in base_results_dir.iterdir():
            if exp_sub_dir.is_dir():
                analyze_experiment(exp_sub_dir)
        
        # Create combined comparison plots
        create_combined_plots(base_results_dir)

if __name__ == "__main__":
    main()