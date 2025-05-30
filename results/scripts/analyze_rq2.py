#!/usr/bin/env python3

import os
import sys
import glob
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
import argparse

def load_data(file_pattern):
    """Load data from files matching the pattern."""
    files = glob.glob(file_pattern)
    if not files:
        print(f"Warning: No files found matching {file_pattern}")
        return None
    
    data = []
    for file in files:
        try:
            data.append(np.loadtxt(file))
        except Exception as e:
            print(f"Error loading {file}: {e}")
            continue
    
    if not data:
        return None
    
    return np.vstack(data)

def trim_data(data, time_col_idx=0, val_col_idx=1):
    """Trim data to remove warmup and teardown periods."""
    if data is None or len(data) == 0:
        return None
    
    # Sort by time
    data = data[data[:, time_col_idx].argsort()]
    
    # Remove warmup (first 5s) and teardown (last 5s)
    mask = (data[:, time_col_idx] >= 5) & (data[:, time_col_idx] <= 25)
    return data[mask]

def analyze_experiment(exp_dir):
    """Analyze a single experiment directory."""
    print(f"Analyzing {exp_dir}...")
    
    # Load throughput data
    throughput_data = load_data(os.path.join(exp_dir, "*-throughput.dat"))
    if throughput_data is not None:
        throughput_data = trim_data(throughput_data)
    
    # Load queue delay data
    queue_delay_data = load_data(os.path.join(exp_dir, "*-dualpi2-*-sojourn.dat"))
    if queue_delay_data is not None:
        queue_delay_data = trim_data(queue_delay_data)
    
    # Load queue bytes data
    queue_bytes_data = load_data(os.path.join(exp_dir, "*-dualpi2-bytes.dat"))
    if queue_bytes_data is not None:
        queue_bytes_data = trim_data(queue_bytes_data)
    
    # Calculate statistics
    stats = {}
    
    if throughput_data is not None:
        # Split data before and after rate change
        before_change = throughput_data[throughput_data[:, 0] < 10]
        after_change = throughput_data[throughput_data[:, 0] >= 10]
        
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
    
    if queue_delay_data is not None:
        # Split data before and after rate change
        before_change = queue_delay_data[queue_delay_data[:, 0] < 10]
        after_change = queue_delay_data[queue_delay_data[:, 0] >= 10]
        
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
    
    return stats, throughput_data, queue_delay_data, queue_bytes_data

def create_focused_plots(exp_dir, algorithm, throughput_data, queue_delay_data, queue_bytes_data):
    """Create detailed plots for a single experiment."""
    if throughput_data is None and queue_delay_data is None and queue_bytes_data is None:
        return
    
    fig = plt.figure(figsize=(15, 10))
    gs = GridSpec(3, 1, height_ratios=[1, 1, 1])
    
    # Plot throughput
    if throughput_data is not None:
        ax1 = fig.add_subplot(gs[0])
        ax1.plot(throughput_data[:, 0], throughput_data[:, 1], 'b-', label='Throughput')
        ax1.axvline(x=10, color='r', linestyle='--', label='Rate Change')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.set_title(f'{algorithm} Throughput')
        ax1.grid(True)
        ax1.legend()
    
    # Plot queue delay
    if queue_delay_data is not None:
        ax2 = fig.add_subplot(gs[1])
        ax2.plot(queue_delay_data[:, 0], queue_delay_data[:, 1], 'g-', label='Queue Delay')
        ax2.axvline(x=10, color='r', linestyle='--', label='Rate Change')
        ax2.set_ylabel('Queue Delay (ms)')
        ax2.set_title(f'{algorithm} Queue Delay')
        ax2.grid(True)
        ax2.legend()
    
    # Plot queue bytes
    if queue_bytes_data is not None:
        ax3 = fig.add_subplot(gs[2])
        ax3.plot(queue_bytes_data[:, 0], queue_bytes_data[:, 1], 'm-', label='Queue Bytes')
        ax3.axvline(x=10, color='r', linestyle='--', label='Rate Change')
        ax3.set_xlabel('Time (s)')
        ax3.set_ylabel('Queue Bytes')
        ax3.set_title(f'{algorithm} Queue Bytes')
        ax3.grid(True)
        ax3.legend()
    
    plt.tight_layout()
    plt.savefig(os.path.join(exp_dir, f'{algorithm}_analysis.png'))
    plt.close()

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
            algorithm = 'Prague' if test_id.startswith('P') else 'Cubic'
            exp_dir = os.path.join(base_dir, test_id)
            
            # Load and plot throughput
            throughput_data = load_data(os.path.join(exp_dir, "*-throughput.dat"))
            if throughput_data is not None:
                throughput_data = trim_data(throughput_data)
                ax1.plot(throughput_data[:, 0], throughput_data[:, 1], 
                        label=f'{algorithm}', linestyle='-' if algorithm == 'Prague' else '--')
            
            # Load and plot queue delay
            queue_delay_data = load_data(os.path.join(exp_dir, "*-dualpi2-*-sojourn.dat"))
            if queue_delay_data is not None:
                queue_delay_data = trim_data(queue_delay_data)
                ax2.plot(queue_delay_data[:, 0], queue_delay_data[:, 1],
                        label=f'{algorithm}', linestyle='-' if algorithm == 'Prague' else '--')
        
        ax1.axvline(x=10, color='r', linestyle=':', label='Rate Change')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.set_title(f'Wired: Rate Change {rate_change} Mbps')
        ax1.grid(True)
        ax1.legend()
        
        ax2.axvline(x=10, color='r', linestyle=':', label='Rate Change')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Queue Delay (ms)')
        ax2.grid(True)
        ax2.legend()
        
        plt.tight_layout()
        plt.savefig(os.path.join(base_dir, f'wired_combined_{rate_change}.png'))
        plt.close()
    
    # Create combined plots for WiFi experiments
    for mcs_change, test_ids in wifi_experiments.items():
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
        
        for test_id in test_ids:
            algorithm = 'Prague' if test_id.startswith('P') else 'Cubic'
            exp_dir = os.path.join(base_dir, test_id)
            
            # Load and plot throughput
            throughput_data = load_data(os.path.join(exp_dir, "*-throughput.dat"))
            if throughput_data is not None:
                throughput_data = trim_data(throughput_data)
                ax1.plot(throughput_data[:, 0], throughput_data[:, 1],
                        label=f'{algorithm}', linestyle='-' if algorithm == 'Prague' else '--')
            
            # Load and plot queue delay
            queue_delay_data = load_data(os.path.join(exp_dir, "*-dualpi2-*-sojourn.dat"))
            if queue_delay_data is not None:
                queue_delay_data = trim_data(queue_delay_data)
                ax2.plot(queue_delay_data[:, 0], queue_delay_data[:, 1],
                        label=f'{algorithm}', linestyle='-' if algorithm == 'Prague' else '--')
        
        ax1.axvline(x=10, color='r', linestyle=':', label='MCS Change')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.set_title(f'WiFi: MCS Change {mcs_change}')
        ax1.grid(True)
        ax1.legend()
        
        ax2.axvline(x=10, color='r', linestyle=':', label='MCS Change')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Queue Delay (ms)')
        ax2.grid(True)
        ax2.legend()
        
        plt.tight_layout()
        plt.savefig(os.path.join(base_dir, f'wifi_combined_{mcs_change}.png'))
        plt.close()

def main():
    parser = argparse.ArgumentParser(description='Analyze RQ2 experiment results')
    parser.add_argument('--exp-dir', required=True, help='Directory containing experiment results')
    args = parser.parse_args()
    
    # Create analysis directory
    analysis_dir = os.path.join(args.exp_dir, 'analysis')
    os.makedirs(analysis_dir, exist_ok=True)
    
    # Analyze each experiment
    for test_id in os.listdir(args.exp_dir):
        exp_dir = os.path.join(args.exp_dir, test_id)
        if not os.path.isdir(exp_dir):
            continue
        
        algorithm = 'Prague' if test_id.startswith('P') else 'Cubic'
        stats, throughput_data, queue_delay_data, queue_bytes_data = analyze_experiment(exp_dir)
        
        # Create focused plots
        create_focused_plots(exp_dir, algorithm, throughput_data, queue_delay_data, queue_bytes_data)
        
        # Save statistics
        if stats:
            with open(os.path.join(exp_dir, 'statistics.txt'), 'w') as f:
                for metric, data in stats.items():
                    f.write(f"\n{metric.upper()}:\n")
                    for period, values in data.items():
                        f.write(f"\n{period}:\n")
                        for stat, value in values.items():
                            f.write(f"{stat}: {value:.2f}\n")
    
    # Create combined plots
    create_combined_plots(args.exp_dir)

if __name__ == '__main__':
    main() 