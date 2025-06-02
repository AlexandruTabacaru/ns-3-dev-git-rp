#!/usr/bin/env python3

import os
import sys
import glob
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
import argparse
from pathlib import Path

def analyze_experiment(exp_dir):
    """Analyze a single experiment directory."""
    exp_name = exp_dir.name
    algorithm = "prague" if exp_name.startswith("P-") else "cubic"
    
    print(f"\nAnalyzing {exp_name} (Algorithm: {algorithm})...")

    # Data loading with error handling
    def load_data(file_pattern):
        try:
            file_path = exp_dir / file_pattern
            if not file_path.exists():
                print(f"Warning: File {file_path} not found. Skipping corresponding plot/analysis.")
                return None
            if file_path.stat().st_size == 0:
                print(f"Warning: File {file_path} is empty. Skipping corresponding plot/analysis.")
                return None
            return np.loadtxt(file_path)
        except Exception as e:
            print(f"Warning: Could not load {file_path}. Error: {e}. Skipping.")
            return None

    # Load all data files
    throughput_data = load_data(f"{algorithm}-throughput.{exp_name}.dat")
    
    # Determine if this is a wired or WiFi experiment
    is_wifi = exp_name.startswith(('P-W', 'C-W'))
    
    # Load queue data based on experiment type and algorithm
    if is_wifi:
        # WiFi uses DualPI2 with different queues for Prague and Cubic
        if algorithm == "prague":
            queue_delay_data = load_data(f"wifi-dualpi2-l-sojourn.{exp_name}.dat")
        else:  # cubic
            queue_delay_data = load_data(f"wifi-dualpi2-c-sojourn.{exp_name}.dat")
        queue_bytes_data = load_data(f"wifi-dualpi2-bytes.{exp_name}.dat")
    else:
        # Wired uses different queues based on algorithm
        if algorithm == "prague":
            queue_delay_data = load_data(f"wired-dualpi2-l-sojourn.{exp_name}.dat")
            queue_bytes_data = load_data(f"wired-dualpi2-bytes.{exp_name}.dat")
        else: # cubic
            queue_delay_data = load_data(f"wired-fqcodel-sojourn.{exp_name}.dat") 
            queue_bytes_data = load_data(f"wired-fqcodel-bytes.{exp_name}.dat")

    sink_rx_data = load_data(f"{algorithm}-sink-rx.{exp_name}.dat")
    cwnd_data = load_data(f"{algorithm}-cwnd.{exp_name}.dat")
    rtt_data = load_data(f"{algorithm}-rtt.{exp_name}.dat")
    pacing_rate_data = load_data(f"{algorithm}-pacing-rate.{exp_name}.dat")

    # Ensure essential data is present for basic stats
    if throughput_data is None:
        print(f"Error: Throughput data missing for {exp_name}. Cannot generate full analysis.")
        return None

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
        total_time = 40.0  # Default for RQ2 experiments
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
    ax.axvline(x=20, color='r', linestyle='--', label='Rate Change')
    ax.set_title('Throughput & Goodput')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Rate (Mbps)')
    ax.legend()
    ax.grid(True)

    # Plot 2: Queueing Delay
    ax = axs[0, 1]
    if queue_delay_data is not None:
        ax.plot(queue_delay_data[:, 0], queue_delay_data[:, 1], 
                label=f'{"L4S Sojourn" if algorithm == "prague" else "Classic Sojourn"}')
        ax.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
        ax.axvline(x=20, color='r', linestyle='--', label='Rate Change')
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
        ax.axvline(x=20, color='r', linestyle='--', label='Rate Change')
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
        ax.axvline(x=20, color='r', linestyle='--', label='Rate Change')
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
        ax.axvline(x=20, color='r', linestyle='--', label='Rate Change')
    ax.set_title('Pacing Rate')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Rate (Mbps)')
    ax.legend()
    ax.grid(True)
    
    # Plot 6: Queue Bytes
    ax = axs[2, 1]
    if queue_bytes_data is not None:
        ax.plot(queue_bytes_data[:, 0], queue_bytes_data[:, 1], 
                label=f'{"L4S" if algorithm == "prague" else "Classic"} Queue (Bytes)')
        ax.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
        ax.axvline(x=20, color='r', linestyle='--', label='Rate Change')
    ax.set_title('Queue Size')
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Bytes')
    ax.legend()
    ax.grid(True)

    # Calculate statistics
    stats = {}
    if throughput_trimmed is not None and len(throughput_trimmed) > 0:
        # Split data before and after rate change
        before_change = throughput_trimmed[throughput_trimmed[:, 0] < 20]
        after_change = throughput_trimmed[throughput_trimmed[:, 0] >= 20]
        
        stats['throughput'] = {
            'before': {
                'mean': np.mean(before_change[:, 1]),
                'std': np.std(before_change[:, 1]),
                'min': np.min(before_change[:, 1]),
                'max': np.max(before_change[:, 1])
            },
            'after': {
                'mean': np.mean(after_change[:, 1]),
                'std': np.std(after_change[:, 1]),
                'min': np.min(after_change[:, 1]),
                'max': np.max(after_change[:, 1])
            }
        }
    
    if queue_delay_trimmed is not None and len(queue_delay_trimmed) > 0:
        # Split data before and after rate change
        before_change = queue_delay_trimmed[queue_delay_trimmed[:, 0] < 20]
        after_change = queue_delay_trimmed[queue_delay_trimmed[:, 0] >= 20]
        
        stats['queue_delay'] = {
            'before': {
                'mean': np.mean(before_change[:, 1]),
                'std': np.std(before_change[:, 1]),
                'min': np.min(before_change[:, 1]),
                'max': np.max(before_change[:, 1])
            },
            'after': {
                'mean': np.mean(after_change[:, 1]),
                'std': np.std(after_change[:, 1]),
                'min': np.min(after_change[:, 1]),
                'max': np.max(after_change[:, 1])
            }
        }
    
    if stats:
        print("\nCalculated statistics (excluding warm-up/teardown):")
        if 'throughput' in stats:
            print(f"Throughput before rate change - Mean: {stats['throughput']['before']['mean']:.2f} Mbps")
            print(f"Throughput after rate change - Mean: {stats['throughput']['after']['mean']:.2f} Mbps")
        if 'queue_delay' in stats:
            print(f"Queue Delay before rate change - Mean: {stats['queue_delay']['before']['mean']:.3f} ms")
            print(f"Queue Delay after rate change - Mean: {stats['queue_delay']['after']['mean']:.3f} ms")
        
        with open(exp_dir / 'analysis_summary.txt', 'w') as f:
            f.write(f"Experiment Analysis Summary - {exp_name}\n")
            f.write("=========================\n\n")
            if 'throughput' in stats:
                f.write("Throughput Statistics (Mbps):\n")
                f.write("Before rate change:\n")
                for key, val in stats['throughput']['before'].items():
                    f.write(f"  {key.capitalize()}: {val:.2f}\n")
                f.write("\nAfter rate change:\n")
                for key, val in stats['throughput']['after'].items():
                    f.write(f"  {key.capitalize()}: {val:.2f}\n")
                f.write("\n")
            if 'queue_delay' in stats:
                f.write("Queue Delay Statistics (ms):\n")
                f.write("Before rate change:\n")
                for key, val in stats['queue_delay']['before'].items():
                    f.write(f"  {key.capitalize()}: {val:.3f}\n")
                f.write("\nAfter rate change:\n")
                for key, val in stats['queue_delay']['after'].items():
                    f.write(f"  {key.capitalize()}: {val:.3f}\n")
    
    # Save detailed analysis plots
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    plt.savefig(exp_dir / f"{algorithm}_analysis_{exp_name}.png", dpi=300, bbox_inches='tight')
    plt.close()

    return stats

def create_combined_plots(base_dir):
    """Create combined plots comparing Prague and Cubic results."""
    # Group experiments by type (wired/wifi) and rate change
    wired_experiments = {
        '25-100': ['P-B1', 'C-B1'],
        '100-25': ['P-B2', 'C-B2']
    }
    
    wifi_experiments = {
        '2-7': ['P-W1', 'C-W1'],
        '7-2': ['P-W2', 'C-W2'],
        '4-9': ['P-W3', 'C-W3'],
        '9-4': ['P-W4', 'C-W4'],
        '4-7': ['P-W5', 'C-W5'],
        '7-4': ['P-W6', 'C-W6']
    }
    
    # Create combined plots for wired experiments
    for rate_change, test_ids in wired_experiments.items():
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
        
        for test_id in test_ids:
            algorithm = 'prague' if test_id.startswith('P') else 'cubic'
            exp_dir = Path(base_dir) / test_id
            
            # Load and plot throughput
            throughput_file = exp_dir / f"{algorithm}-throughput.{test_id}.dat"
            if throughput_file.exists():
                throughput_data = np.loadtxt(throughput_file)
                ax1.plot(throughput_data[:, 0], throughput_data[:, 1], 
                        label=f'{algorithm.capitalize()}', 
                        linestyle='-' if algorithm == 'prague' else '--')
            
            # Load and plot queue delay based on algorithm
            if algorithm == 'prague':
                sojourn_file = exp_dir / f"wired-dualpi2-l-sojourn.{test_id}.dat"
            else:
                sojourn_file = exp_dir / f"wired-fqcodel-sojourn.{test_id}.dat"
                
            if sojourn_file.exists():
                queue_delay_data = np.loadtxt(sojourn_file)
                ax2.plot(queue_delay_data[:, 0], queue_delay_data[:, 1],
                        label=f'{algorithm.capitalize()}', 
                        linestyle='-' if algorithm == 'prague' else '--')
        
        ax1.axvline(x=20, color='r', linestyle=':', label='Rate Change')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.set_title(f'Wired: Rate Change {rate_change} Mbps')
        ax1.grid(True)
        ax1.legend()
        
        ax2.axvline(x=20, color='r', linestyle=':', label='Rate Change')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Queue Delay (ms)')
        ax2.grid(True)
        ax2.legend()
        
        plt.tight_layout()
        plt.savefig(Path(base_dir) / f'wired_combined_{rate_change}.png')
        plt.close()
    
    # Create combined plots for WiFi experiments
    for mcs_change, test_ids in wifi_experiments.items():
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
        
        for test_id in test_ids:
            algorithm = 'prague' if test_id.startswith('P') else 'cubic'
            exp_dir = Path(base_dir) / test_id
            
            # Load and plot throughput
            throughput_file = exp_dir / f"{algorithm}-throughput.{test_id}.dat"
            if throughput_file.exists():
                throughput_data = np.loadtxt(throughput_file)
                ax1.plot(throughput_data[:, 0], throughput_data[:, 1],
                        label=f'{algorithm.capitalize()}', 
                        linestyle='-' if algorithm == 'prague' else '--')
            
            # Load and plot queue delay - WiFi uses DualPI2 with different queues
            if algorithm == 'prague':
                sojourn_file = exp_dir / f"wifi-dualpi2-l-sojourn.{test_id}.dat"
            else:  # cubic
                sojourn_file = exp_dir / f"wifi-dualpi2-c-sojourn.{test_id}.dat"
            if sojourn_file.exists():
                queue_delay_data = np.loadtxt(sojourn_file)
                ax2.plot(queue_delay_data[:, 0], queue_delay_data[:, 1],
                        label=f'{algorithm.capitalize()}', 
                        linestyle='-' if algorithm == 'prague' else '--')
        
        ax1.axvline(x=20, color='r', linestyle=':', label='MCS Change')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.set_title(f'WiFi: MCS Change {mcs_change}')
        ax1.grid(True)
        ax1.legend()
        
        ax2.axvline(x=20, color='r', linestyle=':', label='MCS Change')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Queue Delay (ms)')
        ax2.grid(True)
        ax2.legend()
        
        plt.tight_layout()
        plt.savefig(Path(base_dir) / f'wifi_combined_{mcs_change}.png')
        plt.close()

def main():
    parser = argparse.ArgumentParser(description='Analyze RQ2 experiment results')
    parser.add_argument(
        '--exp-dir',
        type=str,
        help='Optional: specific experiment directory (e.g. results/rq2/P-B1). '
             'If omitted, analyzes all in results/rq2/'
    )
    args = parser.parse_args()

    # If they gave us one folder, just do that.
    if args.exp_dir:
        base_dir = Path(args.exp_dir)
        if not base_dir.exists():
            print(f"Error: Directory {base_dir} does not exist")
            sys.exit(1)
        analyze_experiment(base_dir)
    else:
        # Otherwise default to results/rq2
        base_dir = Path(__file__).parent.parent / "rq2"
        if not base_dir.exists():
            print(f"Error: Directory {base_dir} does not exist")
            sys.exit(1)

        # 1) run each experiment sub-dir
        for exp_dir in sorted(base_dir.iterdir()):
            if exp_dir.is_dir() and not exp_dir.name.startswith('.'):
                analyze_experiment(exp_dir)

        # 2) now build the combined wired & wifi plots
        create_combined_plots(base_dir)

if __name__ == '__main__':
    main() 