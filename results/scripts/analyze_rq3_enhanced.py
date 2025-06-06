#!/usr/bin/env python3
"""
Enhanced RQ3 Analysis: L4S Fairness and Loss-Sensitivity Analysis

Analyzes results from RQ3 experiments with enhanced visualizations and statistics:
- Wired: Fairness between Prague and Cubic flows (Jain's fairness index over time)
- WiFi: Loss-sensitivity comparison (throughput degradation under packet loss)
"""

import argparse
import os
import sys
import glob
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

# Set publication-quality plot parameters
plt.rcParams.update({
    'font.size': 11,
    'axes.titlesize': 13,
    'axes.labelsize': 11,
    'xtick.labelsize': 9,
    'ytick.labelsize': 9,
    'legend.fontsize': 10,
    'figure.titlesize': 15,
    'lines.linewidth': 2.0,
    'grid.alpha': 0.4,
    'figure.figsize': (14, 10)
})

def load_data_safely(filepath, verbose=False):
    """Load data file with comprehensive error handling"""
    try:
        if not filepath.exists():
            if verbose:
                print(f"  File not found: {filepath.name}")
            return None
        
        if filepath.stat().st_size == 0:
            if verbose:
                print(f"  Empty file: {filepath.name}")
            return None
            
        data = pd.read_csv(filepath, sep=' ', header=None)
        if data.empty:
            if verbose:
                print(f"  No data in file: {filepath.name}")
            return None
            
        if verbose:
            print(f"  ✓ Loaded {filepath.name}: {len(data)} points")
        return data
        
    except Exception as e:
        if verbose:
            print(f"  Error loading {filepath.name}: {e}")
        return None

def get_experiment_info(exp_name):
    """Extract experiment information from name"""
    if exp_name.startswith('P-FC'):
        return 'prague', 'wired', 'fairness', f"Prague vs {exp_name[4:]} Cubic flow(s)"
    elif exp_name.startswith('P-FP'):
        return 'prague', 'wired', 'fairness', f"{exp_name[4:]} Prague flows only"
    elif exp_name.startswith('P-FMIX'):
        return 'prague', 'wired', 'fairness', f"Mixed Prague+Cubic ({exp_name})"
    elif exp_name.startswith('P-WLS'):
        loss_rate = '0.1%' if 'WLS1' in exp_name else '1%'
        return 'prague', 'wifi', 'loss_sensitivity', f"Prague with {loss_rate} loss"
    elif exp_name.startswith('C-WLS'):
        loss_rate = '0.1%' if 'WLS1' in exp_name else '1%'
        return 'cubic', 'wifi', 'loss_sensitivity', f"Cubic with {loss_rate} loss"
    else:
        return 'unknown', 'unknown', 'unknown', exp_name

def check_experiment_files(exp_dir):
    """Check which files exist for an experiment"""
    exp_name = exp_dir.name
    algorithm, exp_type, test_type, description = get_experiment_info(exp_name)
    
    expected_files = {}
    
    if test_type == 'fairness':
        # Wired fairness files
        expected_files.update({
            'prague_throughput': f"prague-throughput.{exp_name}.dat",
            'prague_per_flow': f"prague-per-flow-throughput.{exp_name}.dat",
            'cubic_throughput': f"cubic-throughput.{exp_name}.dat", 
            'cubic_per_flow': f"cubic-per-flow-throughput.{exp_name}.dat",
            'prague_cwnd': f"prague-cwnd.{exp_name}.dat",
            'cubic_cwnd': f"cubic-cwnd.{exp_name}.dat",
            'dualpi2_l_sojourn': f"wired-dualpi2-l-sojourn.{exp_name}.dat",
            'dualpi2_c_sojourn': f"wired-dualpi2-c-sojourn.{exp_name}.dat",
            'dualpi2_bytes': f"wired-dualpi2-bytes.{exp_name}.dat"
        })
    else:
        # WiFi loss-sensitivity files
        if algorithm == 'prague':
            expected_files.update({
                'throughput': f"prague-throughput.{exp_name}.dat",
                'cwnd': f"prague-cwnd.{exp_name}.dat",
                'queue_delay': f"wifi-dualpi2-l-sojourn.{exp_name}.dat",
                'queue_bytes': f"wifi-dualpi2-bytes.{exp_name}.dat"
            })
        else:  # cubic
            expected_files.update({
                'throughput': f"cubic-throughput.{exp_name}.dat", 
                'cwnd': f"cubic-cwnd.{exp_name}.dat",
                'queue_delay': f"wifi-dualpi2-c-sojourn.{exp_name}.dat",
                'queue_bytes': f"wifi-dualpi2-bytes.{exp_name}.dat"
            })
    
    # Check file existence
    existing_files = {}
    missing_files = []
    
    for file_type, filename in expected_files.items():
        file_path = exp_dir / filename
        if file_path.exists() and file_path.stat().st_size > 0:
            existing_files[file_type] = file_path
        else:
            missing_files.append(filename)
    
    return existing_files, missing_files

def calculate_jains_fairness_index(throughputs):
    """Calculate Jain's fairness index for a list of throughputs"""
    if len(throughputs) == 0:
        return 1.0
    
    throughputs = np.array(throughputs)
    sum_throughput = np.sum(throughputs)
    sum_squared_throughput = np.sum(throughputs**2)
    n = len(throughputs)
    
    if sum_squared_throughput == 0:
        return 1.0
    
    return (sum_throughput ** 2) / (n * sum_squared_throughput)

def analyze_fairness_experiment(exp_dir, verbose=True):
    """Analyze a single fairness experiment"""
    exp_name = exp_dir.name
    algorithm, exp_type, test_type, description = get_experiment_info(exp_name)
    
    if verbose:
        print(f"\n📊 Analyzing Fairness: {exp_name}")
        print(f"   Description: {description}")

    # Check files
    existing_files, missing_files = check_experiment_files(exp_dir)
    
    if missing_files and verbose:
        print(f"  ⚠️  Missing {len(missing_files)} files:")
        for missing in missing_files[:3]:  # Show first 3
            print(f"     - {missing}")
        if len(missing_files) > 3:
            print(f"     ... and {len(missing_files)-3} more")
    
    print("  Loading data files:")
    
    # Load data
    prague_tput = load_data_safely(existing_files.get('prague_throughput'), verbose)
    cubic_tput = load_data_safely(existing_files.get('cubic_throughput'), verbose) 
    prague_per_flow = load_data_safely(existing_files.get('prague_per_flow'), verbose)
    cubic_per_flow = load_data_safely(existing_files.get('cubic_per_flow'), verbose)
    prague_cwnd = load_data_safely(existing_files.get('prague_cwnd'), verbose)
    cubic_cwnd = load_data_safely(existing_files.get('cubic_cwnd'), verbose)
    l_sojourn = load_data_safely(existing_files.get('dualpi2_l_sojourn'), verbose)
    c_sojourn = load_data_safely(existing_files.get('dualpi2_c_sojourn'), verbose)

    # Analysis parameters
    warmup_time = 5.0
    teardown_time = 5.0
    total_time = 60.0

    # Create 4-panel fairness analysis
    print("  Creating fairness analysis plot...")
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'RQ3 Fairness Analysis: {exp_name}\n{description}', fontsize=16, y=0.95)

    # Panel 1: Overall Throughput
    if prague_tput is not None:
        ax1.plot(prague_tput.iloc[:, 0], prague_tput.iloc[:, 1], 'b-', 
                label='Prague Total', linewidth=2)
    if cubic_tput is not None:
        ax1.plot(cubic_tput.iloc[:, 0], cubic_tput.iloc[:, 1], 'r-', 
                label='Cubic Total', linewidth=2)
    
    ax1.axvspan(0, warmup_time, alpha=0.15, color='gray', label='Excluded periods')
    ax1.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax1.set_title('Overall Throughput', fontweight='bold')
    ax1.set_ylabel('Throughput (Mbps)')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_ylim(bottom=0)

    # Panel 2: Per-Flow Throughput
    if prague_per_flow is not None:
        # Group by port and plot each flow
        for port in prague_per_flow.iloc[:, 1].unique():
            flow_data = prague_per_flow[prague_per_flow.iloc[:, 1] == port]
            ax2.plot(flow_data.iloc[:, 0], flow_data.iloc[:, 2], 
                    label=f'Prague-{int(port)-100}', alpha=0.7)
    
    if cubic_per_flow is not None:
        for port in cubic_per_flow.iloc[:, 1].unique():
            flow_data = cubic_per_flow[cubic_per_flow.iloc[:, 1] == port]
            ax2.plot(flow_data.iloc[:, 0], flow_data.iloc[:, 2], 
                    linestyle='--', label=f'Cubic-{int(port)-200}', alpha=0.7)
    
    ax2.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax2.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax2.set_title('Per-Flow Throughput', fontweight='bold')
    ax2.set_ylabel('Throughput (Mbps)')
    ax2.grid(True, alpha=0.3)
    ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax2.set_ylim(bottom=0)

    # Panel 3: Queue Sojourn Times
    if l_sojourn is not None:
        ax3.plot(l_sojourn.iloc[:, 0], l_sojourn.iloc[:, 1], 'b-', 
                label='L4S Queue', linewidth=2)
    if c_sojourn is not None:
        ax3.plot(c_sojourn.iloc[:, 0], c_sojourn.iloc[:, 1], 'r-', 
                label='Classic Queue', linewidth=2)
    
    ax3.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax3.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax3.set_title('Queue Sojourn Times', fontweight='bold')
    ax3.set_ylabel('Sojourn Time (ms)')
    ax3.grid(True, alpha=0.3)
    ax3.legend()
    ax3.set_ylim(bottom=0)

    # Panel 4: Congestion Windows
    if prague_cwnd is not None:
        ax4.plot(prague_cwnd.iloc[:, 0], prague_cwnd.iloc[:, 1]/1000, 'b-', 
                label='Prague cwnd', linewidth=2)
    if cubic_cwnd is not None:
        ax4.plot(cubic_cwnd.iloc[:, 0], cubic_cwnd.iloc[:, 1]/1000, 'r-', 
                label='Cubic cwnd', linewidth=2)
    
    ax4.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax4.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax4.set_title('Congestion Windows', fontweight='bold')
    ax4.set_xlabel('Time (s)')
    ax4.set_ylabel('cwnd (KB)')
    ax4.grid(True, alpha=0.3)
    ax4.legend()
    ax4.set_ylim(bottom=0)

    plt.tight_layout(rect=[0, 0, 1, 0.92])
    output_file = exp_dir / f'rq3_fairness_{exp_name}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Saved: {output_file.name}")

    # Calculate fairness statistics
    stats = calculate_fairness_statistics(exp_name, description, prague_per_flow, cubic_per_flow,
                                        prague_tput, cubic_tput, l_sojourn, c_sojourn,
                                        warmup_time, teardown_time)
    
    if stats:
        save_fairness_statistics(exp_dir, stats)
    
    return stats

def analyze_loss_sensitivity_experiment(exp_dir, verbose=True):
    """Analyze a single loss-sensitivity experiment"""
    exp_name = exp_dir.name
    algorithm, exp_type, test_type, description = get_experiment_info(exp_name)
    
    if verbose:
        print(f"\n📊 Analyzing Loss-Sensitivity: {exp_name}")
        print(f"   Description: {description}")

    # Check files
    existing_files, missing_files = check_experiment_files(exp_dir)
    
    print("  Loading data files:")
    
    # Load data
    throughput_data = load_data_safely(existing_files.get('throughput'), verbose)
    cwnd_data = load_data_safely(existing_files.get('cwnd'), verbose)
    queue_delay_data = load_data_safely(existing_files.get('queue_delay'), verbose)
    queue_bytes_data = load_data_safely(existing_files.get('queue_bytes'), verbose)

    if throughput_data is None:
        print(f"  ❌ No throughput data found for {exp_name}")
        return None

    # Analysis parameters
    warmup_time = 5.0
    teardown_time = 5.0
    total_time = 60.0

    # Create 4-panel loss-sensitivity analysis
    print("  Creating loss-sensitivity analysis plot...")
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'RQ3 Loss-Sensitivity Analysis: {exp_name}\n{description}', fontsize=16, y=0.95)

    # Panel 1: Throughput
    ax1.plot(throughput_data.iloc[:, 0], throughput_data.iloc[:, 1], 'b-', 
            label=f'{algorithm.capitalize()} Throughput', linewidth=2)
    
    ax1.axvspan(0, warmup_time, alpha=0.15, color='gray', label='Excluded periods')
    ax1.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax1.set_title('Throughput Under Loss', fontweight='bold')
    ax1.set_ylabel('Throughput (Mbps)')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_ylim(bottom=0)

    # Panel 2: Queue Delay
    if queue_delay_data is not None:
        ax2.plot(queue_delay_data.iloc[:, 0], queue_delay_data.iloc[:, 1], 'g-', 
                label='Queue Delay', linewidth=2)
    
    ax2.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax2.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax2.set_title('Queue Delay', fontweight='bold')
    ax2.set_ylabel('Delay (ms)')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    ax2.set_ylim(bottom=0)

    # Panel 3: Queue Occupancy
    if queue_bytes_data is not None:
        ax3.plot(queue_bytes_data.iloc[:, 0], queue_bytes_data.iloc[:, 1]/1000, 'orange', 
                label='Queue Size', linewidth=2)
    
    ax3.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax3.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax3.set_title('Queue Occupancy', fontweight='bold')
    ax3.set_ylabel('Queue Size (KB)')
    ax3.grid(True, alpha=0.3)
    ax3.legend()
    ax3.set_ylim(bottom=0)

    # Panel 4: Congestion Window
    if cwnd_data is not None:
        ax4.plot(cwnd_data.iloc[:, 0], cwnd_data.iloc[:, 1]/1000, 'purple', 
                label='Congestion Window', linewidth=2)
    
    ax4.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax4.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax4.set_title('Congestion Window', fontweight='bold')
    ax4.set_xlabel('Time (s)')
    ax4.set_ylabel('cwnd (KB)')
    ax4.grid(True, alpha=0.3)
    ax4.legend()
    ax4.set_ylim(bottom=0)

    plt.tight_layout(rect=[0, 0, 1, 0.92])
    output_file = exp_dir / f'rq3_loss_sensitivity_{exp_name}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Saved: {output_file.name}")

    # Calculate loss-sensitivity statistics
    stats = calculate_loss_sensitivity_statistics(exp_name, algorithm, description,
                                                throughput_data, queue_delay_data,
                                                warmup_time, teardown_time)
    
    if stats:
        save_loss_sensitivity_statistics(exp_dir, stats)
    
    return stats

def calculate_fairness_statistics(exp_name, description, prague_per_flow, cubic_per_flow,
                                prague_tput, cubic_tput, l_sojourn, c_sojourn,
                                warmup_time, teardown_time):
    """Calculate fairness statistics"""
    
    stats = {
        'experiment': exp_name,
        'type': 'fairness',
        'description': description
    }
    
    # Analysis time window
    analysis_start = warmup_time
    analysis_end = 60.0 - teardown_time
    
    # Calculate final fairness from per-flow data
    if prague_per_flow is not None or cubic_per_flow is not None:
        final_throughputs = []
        
        # Get final throughputs for Prague flows
        if prague_per_flow is not None:
            final_data = prague_per_flow[
                (prague_per_flow.iloc[:, 0] >= analysis_end - 5) & 
                (prague_per_flow.iloc[:, 0] <= analysis_end)
            ]
            for port in final_data.iloc[:, 1].unique():
                port_data = final_data[final_data.iloc[:, 1] == port]
                if len(port_data) > 0:
                    final_throughputs.append(port_data.iloc[:, 2].mean())
        
        # Get final throughputs for Cubic flows
        if cubic_per_flow is not None:
            final_data = cubic_per_flow[
                (cubic_per_flow.iloc[:, 0] >= analysis_end - 5) & 
                (cubic_per_flow.iloc[:, 0] <= analysis_end)
            ]
            for port in final_data.iloc[:, 1].unique():
                port_data = final_data[final_data.iloc[:, 1] == port]
                if len(port_data) > 0:
                    final_throughputs.append(port_data.iloc[:, 2].mean())
        
        if final_throughputs:
            stats['jains_fairness_index'] = calculate_jains_fairness_index(final_throughputs)
            stats['individual_throughputs'] = final_throughputs
    
    # Overall throughput statistics
    def extract_stats(data):
        if data is None or len(data) == 0:
            return None
        analysis_data = data[
            (data.iloc[:, 0] >= analysis_start) & 
            (data.iloc[:, 0] <= analysis_end)
        ]
        if len(analysis_data) == 0:
            return None
        return {
            'mean': analysis_data.iloc[:, 1].mean(),
            'std': analysis_data.iloc[:, 1].std(),
            'min': analysis_data.iloc[:, 1].min(),
            'max': analysis_data.iloc[:, 1].max()
        }
    
    if prague_tput is not None:
        stats['prague_throughput'] = extract_stats(prague_tput)
    if cubic_tput is not None:
        stats['cubic_throughput'] = extract_stats(cubic_tput)
    if l_sojourn is not None:
        stats['l4s_queue_delay'] = extract_stats(l_sojourn)
    if c_sojourn is not None:
        stats['classic_queue_delay'] = extract_stats(c_sojourn)
    
    return stats

def calculate_loss_sensitivity_statistics(exp_name, algorithm, description,
                                        throughput_data, queue_delay_data,
                                        warmup_time, teardown_time):
    """Calculate loss-sensitivity statistics"""
    
    stats = {
        'experiment': exp_name,
        'algorithm': algorithm,
        'type': 'loss_sensitivity',
        'description': description
    }
    
    # Analysis time window
    analysis_start = warmup_time
    analysis_end = 60.0 - teardown_time
    
    def extract_stats(data):
        if data is None or len(data) == 0:
            return None
        analysis_data = data[
            (data.iloc[:, 0] >= analysis_start) & 
            (data.iloc[:, 0] <= analysis_end)
        ]
        if len(analysis_data) == 0:
            return None
        return {
            'mean': analysis_data.iloc[:, 1].mean(),
            'std': analysis_data.iloc[:, 1].std(),
            'min': analysis_data.iloc[:, 1].min(),
            'max': analysis_data.iloc[:, 1].max()
        }
    
    stats['throughput'] = extract_stats(throughput_data)
    if queue_delay_data is not None:
        stats['queue_delay'] = extract_stats(queue_delay_data)
    
    return stats

def save_fairness_statistics(exp_dir, stats):
    """Save fairness statistics to file"""
    output_file = exp_dir / f"rq3_fairness_stats_{stats['experiment']}.txt"
    
    with open(output_file, 'w') as f:
        f.write(f"RQ3 Fairness Statistics: {stats['experiment']}\n")
        f.write("=" * 50 + "\n\n")
        f.write(f"Description: {stats['description']}\n\n")
        
        if 'jains_fairness_index' in stats:
            jfi = stats['jains_fairness_index']
            f.write(f"Jain's Fairness Index: {jfi:.4f}\n")
            if jfi > 0.9:
                f.write("  → Excellent fairness\n")
            elif jfi > 0.8:
                f.write("  → Good fairness\n")
            elif jfi > 0.6:
                f.write("  → Moderate fairness\n")
            else:
                f.write("  → Poor fairness\n")
            f.write("\n")
        
        if 'individual_throughputs' in stats:
            f.write("Individual Flow Throughputs (Mbps):\n")
            for i, tput in enumerate(stats['individual_throughputs']):
                f.write(f"  Flow {i}: {tput:.2f}\n")
            f.write("\n")
        
        for metric in ['prague_throughput', 'cubic_throughput', 'l4s_queue_delay', 'classic_queue_delay']:
            if metric in stats and stats[metric]:
                f.write(f"{metric.upper().replace('_', ' ')}:\n")
                s = stats[metric]
                if 'throughput' in metric:
                    f.write(f"  Mean: {s['mean']:.2f} Mbps\n")
                    f.write(f"  Std:  {s['std']:.2f} Mbps\n")
                else:
                    f.write(f"  Mean: {s['mean']:.3f} ms\n")
                    f.write(f"  Std:  {s['std']:.3f} ms\n")
                f.write("\n")

def save_loss_sensitivity_statistics(exp_dir, stats):
    """Save loss-sensitivity statistics to file"""
    output_file = exp_dir / f"rq3_loss_sensitivity_stats_{stats['experiment']}.txt"
    
    with open(output_file, 'w') as f:
        f.write(f"RQ3 Loss-Sensitivity Statistics: {stats['experiment']}\n")
        f.write("=" * 50 + "\n\n")
        f.write(f"Algorithm: {stats['algorithm'].capitalize()}\n")
        f.write(f"Description: {stats['description']}\n\n")
        
        if 'throughput' in stats and stats['throughput']:
            s = stats['throughput']
            f.write(f"THROUGHPUT:\n")
            f.write(f"  Mean: {s['mean']:.2f} Mbps\n")
            f.write(f"  Std:  {s['std']:.2f} Mbps\n")
            f.write(f"  Range: {s['min']:.2f} - {s['max']:.2f} Mbps\n\n")
        
        if 'queue_delay' in stats and stats['queue_delay']:
            s = stats['queue_delay']
            f.write(f"QUEUE DELAY:\n")
            f.write(f"  Mean: {s['mean']:.3f} ms\n")
            f.write(f"  Std:  {s['std']:.3f} ms\n")
            f.write(f"  Range: {s['min']:.3f} - {s['max']:.3f} ms\n")

def create_fairness_comparison_plots(base_dir):
    """Create comparison plots for fairness experiments"""
    
    print("\n🔄 Creating fairness comparison plots...")
    
    # Define comparison groups
    comparisons = [
        ("Prague vs Cubic (1:1)", ["P-FC1"]),
        ("Prague vs Multiple Cubic", ["P-FC4", "P-FC8"]),
        ("Multiple Prague", ["P-FP2", "P-FP4", "P-FP8"]),
        ("Mixed Scenarios", ["P-FMIX", "P-FMIX2", "P-FMIX3"])
    ]
    
    for title, exp_names in comparisons:
        # Check if experiments exist
        existing_exps = []
        for exp_name in exp_names:
            exp_dir = base_dir / exp_name
            if exp_dir.exists():
                existing_exps.append((exp_name, exp_dir))
        
        if not existing_exps:
            print(f"  ⚠️  Skipping {title}: No experiments found")
            continue
            
        print(f"  📈 {title}")
        
        # Create comparison plot
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
        fig.suptitle(f'RQ3 Fairness Comparison: {title}', fontsize=16, y=0.95)
        
        colors = plt.cm.tab10(np.linspace(0, 1, len(existing_exps)))
        
        for i, (exp_name, exp_dir) in enumerate(existing_exps):
            # Load data
            prague_tput = load_data_safely(exp_dir / f"prague-throughput.{exp_name}.dat")
            cubic_tput = load_data_safely(exp_dir / f"cubic-throughput.{exp_name}.dat")
            l_sojourn = load_data_safely(exp_dir / f"wired-dualpi2-l-sojourn.{exp_name}.dat")
            c_sojourn = load_data_safely(exp_dir / f"wired-dualpi2-c-sojourn.{exp_name}.dat")
            
            color = colors[i]
            
            # Throughput comparison
            if prague_tput is not None:
                ax1.plot(prague_tput.iloc[:, 0], prague_tput.iloc[:, 1], 
                        color=color, linestyle='-', label=f'{exp_name} Prague', linewidth=2)
            if cubic_tput is not None:
                ax1.plot(cubic_tput.iloc[:, 0], cubic_tput.iloc[:, 1], 
                        color=color, linestyle='--', label=f'{exp_name} Cubic', linewidth=2)
            
            # Queue delay comparison  
            if l_sojourn is not None:
                ax2.plot(l_sojourn.iloc[:, 0], l_sojourn.iloc[:, 1], 
                        color=color, linestyle='-', label=f'{exp_name} L4S Queue', linewidth=2)
            if c_sojourn is not None:
                ax2.plot(c_sojourn.iloc[:, 0], c_sojourn.iloc[:, 1], 
                        color=color, linestyle='--', label=f'{exp_name} Classic Queue', linewidth=2)
        
        # Add excluded periods
        ax1.axvspan(0, 5, alpha=0.15, color='gray', label='Excluded periods')
        ax1.axvspan(55, 60, alpha=0.15, color='gray')
        ax2.axvspan(0, 5, alpha=0.15, color='gray')
        ax2.axvspan(55, 60, alpha=0.15, color='gray')
        
        ax1.set_title('Throughput Comparison', fontweight='bold')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.grid(True, alpha=0.3)
        ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax1.set_ylim(bottom=0)
        
        ax2.set_title('Queue Delay Comparison', fontweight='bold')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Delay (ms)')
        ax2.grid(True, alpha=0.3)
        ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        ax2.set_ylim(bottom=0)
        
        plt.tight_layout(rect=[0, 0, 1, 0.92])
        
        safe_title = title.replace(':', '').replace(' ', '_').replace('(', '').replace(')', '')
        output_file = base_dir / f'rq3_fairness_comparison_{safe_title}.png'
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"     ✓ Saved: {output_file.name}")

def create_loss_sensitivity_comparison_plots(base_dir):
    """Create comparison plots for loss-sensitivity experiments"""
    
    print("\n🔄 Creating loss-sensitivity comparison plots...")
    
    # Define comparisons
    comparisons = [
        ("0.1% Loss Rate", "P-WLS1", "C-WLS1"),
        ("1% Loss Rate", "P-WLS2", "C-WLS2")
    ]
    
    for title, prague_exp, cubic_exp in comparisons:
        prague_dir = base_dir / prague_exp
        cubic_dir = base_dir / cubic_exp
        
        if not (prague_dir.exists() and cubic_dir.exists()):
            print(f"  ⚠️  Skipping {title}: Missing directories")
            continue
            
        print(f"  📈 {title}")
        
        # Load data
        prague_tput = load_data_safely(prague_dir / f"prague-throughput.{prague_exp}.dat")
        cubic_tput = load_data_safely(cubic_dir / f"cubic-throughput.{cubic_exp}.dat")
        prague_qdelay = load_data_safely(prague_dir / f"wifi-dualpi2-l-sojourn.{prague_exp}.dat")
        cubic_qdelay = load_data_safely(cubic_dir / f"wifi-dualpi2-c-sojourn.{cubic_exp}.dat")
        
        # Create comparison plot
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
        fig.suptitle(f'RQ3 Loss-Sensitivity Comparison: {title}', fontsize=16, y=0.95)
        
        # Throughput comparison
        if prague_tput is not None:
            ax1.plot(prague_tput.iloc[:, 0], prague_tput.iloc[:, 1], 'b-', 
                    label='Prague', linewidth=2.5, alpha=0.8)
        if cubic_tput is not None:
            ax1.plot(cubic_tput.iloc[:, 0], cubic_tput.iloc[:, 1], 'r--', 
                    label='Cubic', linewidth=2.5, alpha=0.8)
        
        ax1.axvspan(0, 5, alpha=0.15, color='gray', label='Excluded periods')
        ax1.axvspan(55, 60, alpha=0.15, color='gray')
        ax1.set_title('Throughput Under Loss', fontweight='bold')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.grid(True, alpha=0.3)
        ax1.legend()
        ax1.set_ylim(bottom=0)
        
        # Queue delay comparison
        if prague_qdelay is not None:
            ax2.plot(prague_qdelay.iloc[:, 0], prague_qdelay.iloc[:, 1], 'b-', 
                    label='Prague (L4S Queue)', linewidth=2.5, alpha=0.8)
        if cubic_qdelay is not None:
            ax2.plot(cubic_qdelay.iloc[:, 0], cubic_qdelay.iloc[:, 1], 'r--', 
                    label='Cubic (Classic Queue)', linewidth=2.5, alpha=0.8)
        
        ax2.axvspan(0, 5, alpha=0.15, color='gray')
        ax2.axvspan(55, 60, alpha=0.15, color='gray')
        ax2.set_title('Queue Delay Under Loss', fontweight='bold')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Queue Delay (ms)')
        ax2.grid(True, alpha=0.3)
        ax2.legend()
        ax2.set_ylim(bottom=0)
        
        plt.tight_layout(rect=[0, 0, 1, 0.92])
        
        safe_title = title.replace('%', 'pct').replace(' ', '_')
        output_file = base_dir / f'rq3_loss_sensitivity_comparison_{safe_title}.png'
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"     ✓ Saved: {output_file.name}")

def create_summary_csv(base_dir, all_stats):
    """Create CSV summary of all experiments"""
    if not all_stats:
        print("  ⚠️  No statistics to summarize")
        return
        
    output_file = base_dir / 'rq3_experiment_summary.csv'
    
    with open(output_file, 'w') as f:
        f.write("Experiment,Type,Description,")
        f.write("Jains_Fairness_Index,Prague_Throughput,Cubic_Throughput,")
        f.write("L4S_Queue_Delay,Classic_Queue_Delay,Algorithm,Loss_Rate\n")
        
        for stats in all_stats:
            exp = stats['experiment']
            exp_type = stats['type']
            desc = stats.get('description', '')
            
            # Fairness metrics
            jfi = stats.get('jains_fairness_index', 'N/A')
            prague_tput = stats.get('prague_throughput', {}).get('mean', 'N/A') if 'prague_throughput' in stats else 'N/A'
            cubic_tput = stats.get('cubic_throughput', {}).get('mean', 'N/A') if 'cubic_throughput' in stats else 'N/A'
            l4s_delay = stats.get('l4s_queue_delay', {}).get('mean', 'N/A') if 'l4s_queue_delay' in stats else 'N/A'
            classic_delay = stats.get('classic_queue_delay', {}).get('mean', 'N/A') if 'classic_queue_delay' in stats else 'N/A'
            
            # Loss-sensitivity metrics
            algorithm = stats.get('algorithm', 'N/A')
            loss_rate = '0.1%' if 'WLS1' in exp else '1%' if 'WLS' in exp else 'N/A'
            
            f.write(f"{exp},{exp_type},{desc},")
            f.write(f"{jfi},{prague_tput},{cubic_tput},")
            f.write(f"{l4s_delay},{classic_delay},{algorithm},{loss_rate}\n")
    
    print(f"  ✓ CSV summary saved: {output_file.name}")

def main():
    parser = argparse.ArgumentParser(description='Enhanced RQ3 Analysis: Fairness and Loss-Sensitivity')
    parser.add_argument(
        '--exp-dir',
        type=str,
        help='Specific experiment directory to analyze (e.g., results/rq3/P-FC1)'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output'
    )
    args = parser.parse_args()

    if args.exp_dir:
        # Single experiment analysis
        exp_dir = Path(args.exp_dir)
        if not exp_dir.exists():
            print(f"❌ Error: Directory {exp_dir} does not exist")
            sys.exit(1)
        
        print("=== RQ3 Single Experiment Analysis ===")
        exp_name = exp_dir.name
        _, _, test_type, _ = get_experiment_info(exp_name)
        
        if test_type == 'fairness':
            analyze_fairness_experiment(exp_dir, args.verbose)
        elif test_type == 'loss_sensitivity':
            analyze_loss_sensitivity_experiment(exp_dir, args.verbose)
        else:
            print(f"❌ Unknown experiment type for {exp_name}")
            
        print("✅ Analysis complete!")
        
    else:
        # Full analysis of all experiments
        base_dir = Path(__file__).parent.parent / "rq3"
        if not base_dir.exists():
            print(f"❌ Error: Directory {base_dir} does not exist")
            sys.exit(1)

        print("🚀 === RQ3 ENHANCED ANALYSIS === 🚀")
        print(f"Base directory: {base_dir}")
        
        # Find all experiment directories
        exp_dirs = [d for d in sorted(base_dir.iterdir()) 
                   if d.is_dir() and not d.name.startswith('.')]
        
        if not exp_dirs:
            print("❌ No experiment directories found!")
            sys.exit(1)
            
        print(f"Found {len(exp_dirs)} experiment directories")
        
        # Individual experiment analysis
        print("\n📊 === INDIVIDUAL EXPERIMENT ANALYSIS ===")
        all_stats = []
        
        for exp_dir in exp_dirs:
            exp_name = exp_dir.name
            _, _, test_type, _ = get_experiment_info(exp_name)
            
            if test_type == 'fairness':
                stats = analyze_fairness_experiment(exp_dir, args.verbose)
            elif test_type == 'loss_sensitivity':
                stats = analyze_loss_sensitivity_experiment(exp_dir, args.verbose)
            else:
                print(f"  ⚠️  Skipping unknown experiment type: {exp_name}")
                continue
                
            if stats:
                all_stats.append(stats)

        print(f"\n✅ Individual analysis complete: {len(all_stats)} experiments processed")
        
        # Comparison plots
        create_fairness_comparison_plots(base_dir)
        create_loss_sensitivity_comparison_plots(base_dir)
        
        # Summary
        print("\n📄 === CREATING SUMMARY ===")
        create_summary_csv(base_dir, all_stats)
        
        print(f"\n🎉 === ANALYSIS COMPLETE === 🎉")
        print(f"📁 Results saved in: {base_dir}")
        print(f"📊 Individual plots: {len(all_stats)} files")
        print(f"🔄 Comparison plots: Fairness and Loss-Sensitivity")
        print(f"📄 Summary: rq3_experiment_summary.csv")
        print("\nReady for publication! 🚀")

if __name__ == '__main__':
    main() 