import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from pathlib import Path
import argparse
import sys

def analyze_experiment(exp_dir):
    # Read data files
    exp_name = exp_dir.name
    algorithm = "prague" if exp_name.startswith("P-") else "cubic"
    
    try:
        # Read throughput data (interface rate)
        throughput = np.loadtxt(exp_dir / f"{algorithm}-throughput.{exp_name}.dat")
        
        # Read queue delay data
        if algorithm == "prague":
            queue_delay = np.loadtxt(exp_dir / f"wired-dualpi2-l-sojourn.{exp_name}.dat")
        else:
            queue_delay = np.loadtxt(exp_dir / f"wired-fqcodel-sojourn.{exp_name}.dat")
        
        # Read sink RX bytes for goodput calculation
        sink_rx = np.loadtxt(exp_dir / f"{algorithm}-sink-rx.{exp_name}.dat")
        
        # Calculate goodput (Mbps)
        dt = np.diff(sink_rx[:, 0])  # Time differences
        goodput = np.diff(sink_rx[:, 1]) * 8 / dt / 1e6  # Convert to Mbps
        
        # Create time points for goodput (midpoints of the intervals)
        goodput_time = sink_rx[:-1, 0] + dt/2
        
        # Trim warm-up and teardown periods (first and last 5 seconds)
        warmup_time = 5.0  # seconds
        teardown_time = 5.0  # seconds
        total_time = throughput[-1, 0]
        
        throughput_trimmed = throughput[(throughput[:, 0] >= warmup_time) & 
                                      (throughput[:, 0] <= (total_time - teardown_time))]
        queue_delay_trimmed = queue_delay[(queue_delay[:, 0] >= warmup_time) & 
                                        (queue_delay[:, 0] <= (total_time - teardown_time))]
        goodput_trimmed = goodput[(goodput_time >= warmup_time) & 
                                (goodput_time <= (total_time - teardown_time))]
        
        print(f"Read throughput data: {len(throughput)} points")
        print(f"Read queue delay data: {len(queue_delay)} points")
        print(f"Read goodput data: {len(goodput)} points")
        print(f"After trimming: {len(throughput_trimmed)} throughput points, {len(queue_delay_trimmed)} queue delay points, {len(goodput_trimmed)} goodput points")
        
        # Create plots
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(10, 8))
        
        # Throughput plot (both interface rate and goodput)
        ax1.plot(throughput[:, 0], throughput[:, 1], label='Interface Rate')
        ax1.plot(goodput_time, goodput, label='Goodput', alpha=0.7)
        ax1.axvspan(0, warmup_time, alpha=0.2, color='gray', label='Warm-up')
        ax1.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray', label='Teardown')
        ax1.set_title(f'Throughput over Time - {exp_name}')
        ax1.set_xlabel('Time (s)')
        ax1.set_ylabel('Rate (Mbps)')
        ax1.legend()
        
        # Queueing delay plot
        ax2.plot(queue_delay[:, 0], queue_delay[:, 1])
        ax2.axvspan(0, warmup_time, alpha=0.2, color='gray', label='Warm-up')
        ax2.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray', label='Teardown')
        ax2.set_title(f'Queueing Delay over Time - {exp_name}')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Delay (ms)')
        ax2.legend()
        
        # Calculate statistics on trimmed data
        stats = {
            'interface_rate': {
                'mean': np.mean(throughput_trimmed[:, 1]),
                'std': np.std(throughput_trimmed[:, 1]),
                'min': np.min(throughput_trimmed[:, 1]),
                'max': np.max(throughput_trimmed[:, 1]),
                'p95': np.percentile(throughput_trimmed[:, 1], 95),
                'p99': np.percentile(throughput_trimmed[:, 1], 99)
            },
            'goodput': {
                'mean': np.mean(goodput_trimmed),
                'std': np.std(goodput_trimmed),
                'min': np.min(goodput_trimmed),
                'max': np.max(goodput_trimmed),
                'p95': np.percentile(goodput_trimmed, 95),
                'p99': np.percentile(goodput_trimmed, 99)
            },
            'queue_delay': {
                'mean': np.mean(queue_delay_trimmed[:, 1]),
                'std': np.std(queue_delay_trimmed[:, 1]),
                'min': np.min(queue_delay_trimmed[:, 1]),
                'max': np.max(queue_delay_trimmed[:, 1]),
                'p95': np.percentile(queue_delay_trimmed[:, 1], 95),
                'p99': np.percentile(queue_delay_trimmed[:, 1], 99)
            }
        }
        
        print("\nCalculated statistics (excluding warm-up/teardown):")
        print(f"Interface Rate - Mean: {stats['interface_rate']['mean']:.2f} Mbps")
        print(f"Goodput - Mean: {stats['goodput']['mean']:.2f} Mbps")
        print(f"Queue Delay - Mean: {stats['queue_delay']['mean']:.3f} ms")
        
        # Save plots
        plt.tight_layout()
        plt.savefig(exp_dir / 'analysis_plots.png')
        print(f"\nSaved plot to {exp_dir / 'analysis_plots.png'}")
        
        # Save statistics
        with open(exp_dir / 'analysis_summary.txt', 'w') as f:
            f.write(f"Experiment Analysis Summary - {exp_name}\n")
            f.write("=========================\n\n")
            f.write("Interface Rate Statistics (Mbps):\n")
            f.write(f"  Mean: {stats['interface_rate']['mean']:.2f}\n")
            f.write(f"  Std:  {stats['interface_rate']['std']:.2f}\n")
            f.write(f"  Min:  {stats['interface_rate']['min']:.2f}\n")
            f.write(f"  Max:  {stats['interface_rate']['max']:.2f}\n")
            f.write(f"  P95:  {stats['interface_rate']['p95']:.2f}\n")
            f.write(f"  P99:  {stats['interface_rate']['p99']:.2f}\n\n")
            f.write("Goodput Statistics (Mbps):\n")
            f.write(f"  Mean: {stats['goodput']['mean']:.2f}\n")
            f.write(f"  Std:  {stats['goodput']['std']:.2f}\n")
            f.write(f"  Min:  {stats['goodput']['min']:.2f}\n")
            f.write(f"  Max:  {stats['goodput']['max']:.2f}\n")
            f.write(f"  P95:  {stats['goodput']['p95']:.2f}\n")
            f.write(f"  P99:  {stats['goodput']['p99']:.2f}\n\n")
            f.write("Queueing Delay Statistics (ms):\n")
            f.write(f"  Mean: {stats['queue_delay']['mean']:.3f}\n")
            f.write(f"  Std:  {stats['queue_delay']['std']:.3f}\n")
            f.write(f"  Min:  {stats['queue_delay']['min']:.3f}\n")
            f.write(f"  Max:  {stats['queue_delay']['max']:.3f}\n")
            f.write(f"  P95:  {stats['queue_delay']['p95']:.3f}\n")
            f.write(f"  P99:  {stats['queue_delay']['p99']:.3f}\n")
        print(f"Saved statistics to {exp_dir / 'analysis_summary.txt'}")
        
    except FileNotFoundError as e:
        print(f"Error: Could not find required data files for {exp_name}")
        print(f"Missing file: {e.filename}")
        return False
    except Exception as e:
        print(f"Error analyzing {exp_name}: {str(e)}")
        return False
    
    return True

def main():
    parser = argparse.ArgumentParser(description='Analyze RQ1 experiment results')
    parser.add_argument('--exp-dir', type=str, help='Specific experiment directory to analyze')
    args = parser.parse_args()
    
    if args.exp_dir:
        # Analyze specific experiment
        exp_dir = Path(args.exp_dir)
        if not exp_dir.exists():
            print(f"Error: Directory {exp_dir} does not exist")
            sys.exit(1)
        analyze_experiment(exp_dir)
    else:
        # Process all experiments
        print("Analyzing all experiments in results/rq1...")
        results_dir = Path("results/rq1")
        if not results_dir.exists():
            print(f"Error: Directory {results_dir} does not exist")
            sys.exit(1)
            
        for exp_dir in results_dir.glob("*"):
            if exp_dir.is_dir():
                print(f"\nAnalyzing {exp_dir.name}...")
                analyze_experiment(exp_dir)

if __name__ == "__main__":
    main()