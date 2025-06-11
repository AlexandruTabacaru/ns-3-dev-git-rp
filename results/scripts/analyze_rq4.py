#!/usr/bin/env python3
"""
RQ4 Analysis: Prague vs BBRv3 L4S Fairness Analysis

Analyzes results from RQ4 experiments focusing on fairness between
two scalable congestion controllers (Prague and BBRv3) in L4S:
- Jain's fairness index between algorithms
- Per-flow throughput distribution
- Queueing delay comparison (enhanced analytics)
- Algorithm-specific behavior patterns
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
    """Load data file with comprehensive error handling and proper numeric conversion"""
    try:
        if not filepath.exists():
            if verbose:
                print(f"  File not found: {filepath.name}")
            return None
        
        if filepath.stat().st_size == 0:
            if verbose:
                print(f"  Empty file: {filepath.name}")
            return None
            
        # Read CSV with proper numeric conversion
        data = pd.read_csv(filepath, sep=' ', header=None, dtype=float, na_values=['', 'nan', 'inf', '-inf'])
        
        # Drop any rows with NaN values
        data = data.dropna()
        
        if data.empty:
            if verbose:
                print(f"  No valid data in file: {filepath.name}")
            return None
            
        if verbose:
            print(f"  ✓ Loaded {filepath.name}: {len(data)} points")
        return data
        
    except Exception as e:
        if verbose:
            print(f"  Error loading {filepath.name}: {e}")
        return None

def get_experiment_info(exp_name):
    """Extract experiment information from RQ4 name (P#-B# format)"""
    if not exp_name.startswith('P') or '-B' not in exp_name:
        return 'unknown', 'unknown', exp_name
    
    parts = exp_name.split('-B')
    if len(parts) != 2:
        return 'unknown', 'unknown', exp_name
    
    try:
        num_prague = int(parts[0][1:])  # P1 -> 1
        num_bbr = int(parts[1])         # B2 -> 2
        description = f"{num_prague} Prague vs {num_bbr} BBRv3"
        return num_prague, num_bbr, description
    except ValueError:
        return 'unknown', 'unknown', exp_name

def check_experiment_files(exp_dir):
    """Check which files exist for an RQ4 experiment"""
    exp_name = exp_dir.name
    
    expected_files = {
        'prague_throughput': f"prague-throughput.{exp_name}.dat",
        'prague_per_flow': f"prague-per-flow-throughput.{exp_name}.dat",
        'bbr_throughput': f"bbr-throughput.{exp_name}.dat", 
        'bbr_per_flow': f"bbr-per-flow-throughput.{exp_name}.dat",
        'prague_cwnd': f"prague-cwnd.{exp_name}.dat",
        'bbr_cwnd': f"bbr-cwnd.{exp_name}.dat",
        'l4s_sojourn': f"wired-dualpi2-l-sojourn.{exp_name}.dat",
        'dualpi2_bytes': f"wired-dualpi2-bytes.{exp_name}.dat",
        'queueing_analytics': f"queueing-analytics.{exp_name}.dat"
    }
    
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

def analyze_rq4_experiment(exp_dir, verbose=True):
    """Analyze a single RQ4 fairness experiment"""
    exp_name = exp_dir.name
    num_prague, num_bbr, description = get_experiment_info(exp_name)
    
    if verbose:
        print(f"\n📊 Analyzing RQ4: {exp_name}")
        print(f"   Description: {description}")

    # Check files
    existing_files, missing_files = check_experiment_files(exp_dir)
    
    if missing_files and verbose:
        print(f"  ⚠️  Missing {len(missing_files)} files:")
        for missing in missing_files[:3]:
            print(f"     - {missing}")
        if len(missing_files) > 3:
            print(f"     ... and {len(missing_files)-3} more")
    
    print("  Loading data files:")
    
    # Load data
    prague_tput = load_data_safely(existing_files.get('prague_throughput'), verbose)
    bbr_tput = load_data_safely(existing_files.get('bbr_throughput'), verbose) 
    prague_per_flow = load_data_safely(existing_files.get('prague_per_flow'), verbose)
    bbr_per_flow = load_data_safely(existing_files.get('bbr_per_flow'), verbose)
    l4s_sojourn = load_data_safely(existing_files.get('l4s_sojourn'), verbose)

    # Analysis parameters
    warmup_time = 8.0
    teardown_time = 0.0

    # Calculate fairness statistics
    print("  Computing RQ4 fairness statistics...")
    stats = calculate_rq4_fairness_statistics(exp_name, description, num_prague, num_bbr,
                                             prague_per_flow, bbr_per_flow,
                                             prague_tput, bbr_tput, 
                                             l4s_sojourn,
                                             warmup_time, teardown_time)
    
    if stats:
        save_rq4_statistics(exp_dir, stats)
        print(f"  📊 Key Results:")
        if 'jains_fairness_index' in stats:
            jfi = stats['jains_fairness_index']
            fairness_level = "Excellent" if jfi > 0.9 else "Good" if jfi > 0.8 else "Moderate" if jfi > 0.6 else "Poor"
            print(f"     - Jain's Fairness Index: {jfi:.3f} ({fairness_level})")
        if 'total_flows' in stats:
            print(f"     - Total flows: {stats['total_flows']} (Prague: {stats.get('prague_flows', 0)}, BBRv3: {stats.get('bbr_flows', 0)})")
        if 'algorithm_fairness' in stats:
            algo_jfi = stats['algorithm_fairness']
            print(f"     - Algorithm-level fairness: {algo_jfi:.3f}")
    
    return stats

def calculate_rq4_fairness_statistics(exp_name, description, num_prague, num_bbr,
                                     prague_per_flow, bbr_per_flow,
                                     prague_tput, bbr_tput, 
                                     l4s_sojourn,
                                     warmup_time, teardown_time):
    """Calculate comprehensive RQ4 fairness statistics"""
    
    stats = {
        'experiment': exp_name,
        'type': 'rq4_fairness',
        'description': description,
        'prague_flows': num_prague,
        'bbr_flows': num_bbr,
        'total_flows': num_prague + num_bbr if isinstance(num_prague, int) and isinstance(num_bbr, int) else 0
    }
    
    # Analysis time window (experiments run for 30 seconds)
    analysis_start = warmup_time
    analysis_end = 30.0 - teardown_time
    
    # Calculate fairness from per-flow data
    if prague_per_flow is not None or bbr_per_flow is not None:
        all_final_throughputs = []
        prague_throughputs = []
        bbr_throughputs = []
        flow_labels = []
        
        # Get final throughputs for Prague flows
        if prague_per_flow is not None:
            final_data = prague_per_flow[
                (prague_per_flow.iloc[:, 0] >= analysis_end - 10) & 
                (prague_per_flow.iloc[:, 0] <= analysis_end)
            ]
            for port in final_data.iloc[:, 1].unique():
                port_data = final_data[final_data.iloc[:, 1] == port]
                if len(port_data) > 0:
                    avg_tput = port_data.iloc[:, 2].mean()
                    all_final_throughputs.append(avg_tput)
                    prague_throughputs.append(avg_tput)
                    flow_labels.append(f'Prague-{int(port)-100}')
        
        # Get final throughputs for BBRv3 flows
        if bbr_per_flow is not None:
            final_data = bbr_per_flow[
                (bbr_per_flow.iloc[:, 0] >= analysis_end - 10) & 
                (bbr_per_flow.iloc[:, 0] <= analysis_end)
            ]
            for port in final_data.iloc[:, 1].unique():
                port_data = final_data[final_data.iloc[:, 1] == port]
                if len(port_data) > 0:
                    avg_tput = port_data.iloc[:, 2].mean()
                    all_final_throughputs.append(avg_tput)
                    bbr_throughputs.append(avg_tput)
                    flow_labels.append(f'BBRv3-{int(port)-200}')
        
        if all_final_throughputs:
            # Overall fairness (all flows)
            stats['jains_fairness_index'] = calculate_jains_fairness_index(all_final_throughputs)
            stats['individual_throughputs'] = all_final_throughputs
            stats['flow_labels'] = flow_labels
            
            # Algorithm-specific throughputs
            stats['prague_individual_throughputs'] = prague_throughputs
            stats['bbr_individual_throughputs'] = bbr_throughputs
            
            # Algorithm-level fairness (Prague vs BBRv3 as groups)
            if prague_throughputs and bbr_throughputs:
                prague_avg = np.mean(prague_throughputs)
                bbr_avg = np.mean(bbr_throughputs)
                stats['algorithm_fairness'] = calculate_jains_fairness_index([prague_avg, bbr_avg])
                stats['prague_avg_throughput'] = prague_avg
                stats['bbr_avg_throughput'] = bbr_avg
                stats['throughput_ratio'] = prague_avg / bbr_avg if bbr_avg > 0 else float('inf')
            
            # Intra-algorithm fairness
            if len(prague_throughputs) > 1:
                stats['prague_intra_fairness'] = calculate_jains_fairness_index(prague_throughputs)
            if len(bbr_throughputs) > 1:
                stats['bbr_intra_fairness'] = calculate_jains_fairness_index(bbr_throughputs)
            
            # Flow starvation analysis
            avg_tput = np.mean(all_final_throughputs)
            starvation_threshold = avg_tput * 0.1
            starved_flows = sum(1 for x in all_final_throughputs if x < starvation_threshold)
            stats['starved_flows'] = starved_flows
            stats['starvation_rate'] = starved_flows / len(all_final_throughputs)
    
    # Queueing delay analysis
    def extract_queue_stats(data, queue_name):
        if data is None or len(data) == 0:
            return None
        analysis_data = data[
            (data.iloc[:, 0] >= analysis_start) & 
            (data.iloc[:, 0] <= analysis_end)
        ]
        if len(analysis_data) == 0:
            return None
        
        delays = analysis_data.iloc[:, 1]
        return {
            'mean_delay': delays.mean(),
            'p95_delay': np.percentile(delays, 95),
            'p99_delay': np.percentile(delays, 99),
            'delay_std': delays.std(),
            'delay_cv': delays.std() / delays.mean() if delays.mean() > 0 else 0,
            'max_delay': delays.max(),
            'samples': len(delays),
            'queue_name': queue_name
        }
    
    # L4S queueing analysis for RQ4
    if l4s_sojourn is not None:
        stats['l4s_queue'] = extract_queue_stats(l4s_sojourn, 'L4S')
    
    # Overall throughput statistics
    def extract_throughput_stats(data, label):
        if data is None or len(data) == 0:
            return None
        analysis_data = data[
            (data.iloc[:, 0] >= analysis_start) & 
            (data.iloc[:, 0] <= analysis_end)
        ]
        if len(analysis_data) == 0:
            return None
        
        tput = analysis_data.iloc[:, 1]
        return {
            'mean': tput.mean(),
            'std': tput.std(),
            'cv': tput.std() / tput.mean() if tput.mean() > 0 else 0,
            'min': tput.min(),
            'max': tput.max(),
            'p95': np.percentile(tput, 95),
            'algorithm': label
        }
    
    if prague_tput is not None:
        stats['prague_throughput'] = extract_throughput_stats(prague_tput, 'Prague')
    if bbr_tput is not None:
        stats['bbr_throughput'] = extract_throughput_stats(bbr_tput, 'BBRv3')
    
    return stats 

def save_rq4_statistics(exp_dir, stats):
    """Save RQ4 fairness statistics to file"""
    output_file = exp_dir / f"rq4_fairness_stats_{stats['experiment']}.txt"
    
    with open(output_file, 'w') as f:
        f.write(f"RQ4 Fairness Statistics: {stats['experiment']}\n")
        f.write("=" * 50 + "\n\n")
        f.write(f"Description: {stats['description']}\n")
        f.write(f"Prague flows: {stats.get('prague_flows', 0)}\n")
        f.write(f"BBRv3 flows: {stats.get('bbr_flows', 0)}\n")
        f.write(f"Total flows: {stats.get('total_flows', 0)}\n\n")
        
        if 'jains_fairness_index' in stats:
            jfi = stats['jains_fairness_index']
            f.write(f"OVERALL FAIRNESS METRICS:\n")
            f.write(f"  Jain's Fairness Index (all flows): {jfi:.4f}\n")
            f.write(f"  Starved flows: {stats.get('starved_flows', 0)}\n")
            f.write(f"  Starvation rate: {stats.get('starvation_rate', 0):.1%}\n")
            
            if jfi > 0.9:
                f.write("  → Excellent overall fairness\n")
            elif jfi > 0.8:
                f.write("  → Good overall fairness\n")
            elif jfi > 0.6:
                f.write("  → Moderate overall fairness\n")
            else:
                f.write("  → Poor overall fairness\n")
            f.write("\n")
        
        # Algorithm-level fairness
        if 'algorithm_fairness' in stats:
            algo_jfi = stats['algorithm_fairness']
            f.write(f"ALGORITHM-LEVEL FAIRNESS:\n")
            f.write(f"  Algorithm fairness (Prague vs BBRv3): {algo_jfi:.4f}\n")
            f.write(f"  Prague avg throughput: {stats.get('prague_avg_throughput', 0):.2f} Mbps\n")
            f.write(f"  BBRv3 avg throughput: {stats.get('bbr_avg_throughput', 0):.2f} Mbps\n")
            f.write(f"  Throughput ratio (Prague/BBRv3): {stats.get('throughput_ratio', 0):.3f}\n")
            f.write("\n")
        
        # Intra-algorithm fairness
        if 'prague_intra_fairness' in stats:
            f.write(f"INTRA-ALGORITHM FAIRNESS:\n")
            f.write(f"  Prague intra-fairness: {stats['prague_intra_fairness']:.4f}\n")
        if 'bbr_intra_fairness' in stats:
            f.write(f"  BBRv3 intra-fairness: {stats['bbr_intra_fairness']:.4f}\n")
        if 'prague_intra_fairness' in stats or 'bbr_intra_fairness' in stats:
            f.write("\n")
        
        # Individual flow performance
        if 'individual_throughputs' in stats and 'flow_labels' in stats:
            f.write("INDIVIDUAL FLOW PERFORMANCE:\n")
            for label, tput in zip(stats['flow_labels'], stats['individual_throughputs']):
                f.write(f"  {label}: {tput:.2f} Mbps\n")
            f.write("\n")
        
        # Queueing analysis
        if 'l4s_queue' in stats:
            q = stats['l4s_queue']
            f.write(f"L4S QUEUE PERFORMANCE:\n")
            f.write(f"  Mean delay: {q['mean_delay']:.2f} ms\n")
            f.write(f"  95th percentile: {q['p95_delay']:.2f} ms\n")
            f.write(f"  99th percentile: {q['p99_delay']:.2f} ms\n")
            f.write(f"  Delay CV: {q['delay_cv']:.3f}\n")
            f.write(f"  Samples: {q['samples']}\n\n")
        


def create_rq4_experiment_plot(exp_dir, stats, verbose=True):
    """Create comprehensive plot for a single RQ4 experiment"""
    
    if not stats:
        print(f"  ⚠️  No statistics available for plotting")
        return
    
    exp_name = stats['experiment']
    if verbose:
        print(f"  📊 Creating plot for {exp_name}...")
    
    # Load data for plotting
    existing_files, _ = check_experiment_files(exp_dir)
    
    prague_tput = load_data_safely(existing_files.get('prague_throughput'))
    bbr_tput = load_data_safely(existing_files.get('bbr_throughput'))
    prague_per_flow = load_data_safely(existing_files.get('prague_per_flow'))
    bbr_per_flow = load_data_safely(existing_files.get('bbr_per_flow'))
    l4s_sojourn = load_data_safely(existing_files.get('l4s_sojourn'))
    
    # Create comprehensive plot
    fig = plt.figure(figsize=(16, 12))
    gs = fig.add_gridspec(3, 2, height_ratios=[2, 2, 1], hspace=0.3, wspace=0.3)
    
    # Main title with key metrics
    jfi = stats.get('jains_fairness_index', 0)
    algo_jfi = stats.get('algorithm_fairness', 0)
    fig.suptitle(f'RQ4 Fairness Analysis: {exp_name} (JFI: {jfi:.3f}, Algo: {algo_jfi:.3f})', 
                 fontsize=16, fontweight='bold')
    
    # Plot 1: Throughput over time
    ax1 = fig.add_subplot(gs[0, :])
    plot_rq4_throughput_comparison(ax1, prague_tput, bbr_tput, exp_name)
    
    # Plot 2: Individual flow performance
    ax2 = fig.add_subplot(gs[1, 0])
    plot_rq4_individual_flows(ax2, stats)
    
    
    # Plot 4: Fairness metrics summary
    ax4 = fig.add_subplot(gs[2, :])
    plot_rq4_fairness_summary(ax4, stats)
    
    # Save plot
    output_file = exp_dir / f'rq4_analysis_{exp_name}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    
    if verbose:
        print(f"     ✓ Saved: {output_file.name}")

def plot_rq4_throughput_comparison(ax, prague_tput, bbr_tput, exp_name):
    """Plot throughput comparison for RQ4"""
    ax.set_title('Algorithm Throughput Comparison', fontweight='bold', fontsize=12)
    
    # Prague throughput
    if prague_tput is not None:
        ax.plot(prague_tput.iloc[:, 0], prague_tput.iloc[:, 1], 
               color='#1f77b4', linestyle='-', linewidth=3, alpha=0.9,
               label='Prague')
    
    # BBRv3 throughput
    if bbr_tput is not None:
        ax.plot(bbr_tput.iloc[:, 0], bbr_tput.iloc[:, 1],
               color='#ff7f0e', linestyle='-', linewidth=3, alpha=0.9,
               label='BBRv3')
    
    # Add analysis window
    ax.axvspan(0, 5, alpha=0.15, color='gray', label='Excluded periods')
    ax.axvspan(25, 30, alpha=0.15, color='gray')
    
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Throughput (Mbps)')
    ax.grid(True, alpha=0.3)
    ax.legend()
    ax.set_ylim(bottom=0)

def plot_rq4_individual_flows(ax, stats):
    """Plot individual flow performance for RQ4"""
    ax.set_title('Individual Flow Throughput', fontweight='bold', fontsize=12)
    
    if 'individual_throughputs' in stats and 'flow_labels' in stats:
        throughputs = stats['individual_throughputs']
        labels = stats['flow_labels']
        
        # Color code by algorithm
        colors = ['#1f77b4' if 'Prague' in label else '#ff7f0e' for label in labels]
        
        bars = ax.bar(range(len(throughputs)), throughputs, color=colors, alpha=0.8)
        
        # Add value labels
        for bar, tput in zip(bars, throughputs):
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height + 1,
                   f'{tput:.1f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
        
        ax.set_xticks(range(len(labels)))
        ax.set_xticklabels(labels, rotation=45, ha='right')
        ax.set_ylabel('Throughput (Mbps)')
        ax.grid(True, alpha=0.3)
        
        # Add legend
        from matplotlib.patches import Patch
        legend_elements = [Patch(facecolor='#1f77b4', alpha=0.8, label='Prague'),
                          Patch(facecolor='#ff7f0e', alpha=0.8, label='BBRv3')]
        ax.legend(handles=legend_elements, loc='upper right')
    else:
        ax.text(0.5, 0.5, 'No flow data available', ha='center', va='center', 
                transform=ax.transAxes, fontsize=12)

def plot_l4s_queue_delay(ax, l4s_sojourn):
    """Plot L4S/DualPI2 queue delay distribution"""
    ax.set_title('DualPI2 Queue Delays', fontweight='bold', fontsize=12)
    
    if l4s_sojourn is not None:
        analysis_data = l4s_sojourn[
            (l4s_sojourn.iloc[:, 0] >= 5) & 
            (l4s_sojourn.iloc[:, 0] <= 25)  # 30s experiment
        ]
        if len(analysis_data) > 0:
            delays = analysis_data.iloc[:, 1].values
            
            # Create histogram
            ax.hist(delays, bins=50, alpha=0.7, color='#3498db', edgecolor='black')
            
            # Add statistics text
            mean_delay = np.mean(delays)
            p95_delay = np.percentile(delays, 95)
            p99_delay = np.percentile(delays, 99)
            median_delay = np.median(delays)
            
            stats_text = f"Mean: {mean_delay:.2f} ms\nMedian: {median_delay:.2f} ms\n95th: {p95_delay:.2f} ms\n99th: {p99_delay:.2f} ms"
            ax.text(0.98, 0.98, stats_text, transform=ax.transAxes, 
                   fontsize=10, va='top', ha='right',
                   bbox=dict(boxstyle="round,pad=0.3", facecolor="white", alpha=0.8))
        else:
            ax.text(0.5, 0.5, 'No queue delay data available', ha='center', va='center', 
                   transform=ax.transAxes, fontsize=12)
    else:
        ax.text(0.5, 0.5, 'No queue delay data available', ha='center', va='center', 
               transform=ax.transAxes, fontsize=12)
    
    ax.set_xlabel('Queue Delay (ms)')
    ax.set_ylabel('Frequency')
    ax.set_xscale('log')
    ax.grid(True, alpha=0.3)

def plot_rq4_fairness_summary(ax, stats):
    """Plot fairness metrics summary for RQ4"""
    ax.set_title('Fairness Metrics Summary', fontweight='bold', fontsize=12)
    
    metrics = []
    values = []
    colors = []
    
    # Overall fairness
    if 'jains_fairness_index' in stats:
        metrics.append('Overall\nFairness')
        jfi = stats['jains_fairness_index']
        values.append(jfi)
        colors.append('#2ecc71' if jfi > 0.9 else '#f39c12' if jfi > 0.8 else '#e74c3c')
    
    # Algorithm fairness
    if 'algorithm_fairness' in stats:
        metrics.append('Algorithm\nFairness')
        algo_jfi = stats['algorithm_fairness']
        values.append(algo_jfi)
        colors.append('#2ecc71' if algo_jfi > 0.9 else '#f39c12' if algo_jfi > 0.8 else '#e74c3c')
    
    # Intra-algorithm fairness
    if 'prague_intra_fairness' in stats:
        metrics.append('Prague\nIntra-Fairness')
        values.append(stats['prague_intra_fairness'])
        colors.append('#1f77b4')
    
    if 'bbr_intra_fairness' in stats:
        metrics.append('BBRv3\nIntra-Fairness')
        values.append(stats['bbr_intra_fairness'])
        colors.append('#ff7f0e')
    
    if metrics:
        bars = ax.bar(metrics, values, color=colors, alpha=0.8)
        
        # Add value labels
        for bar, value in zip(bars, values):
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height + 0.01,
                   f'{value:.3f}', ha='center', va='bottom', fontweight='bold')
        
        # Reference lines
        ax.axhline(y=0.9, color='green', linestyle='--', alpha=0.7, label='Excellent (>0.9)')
        ax.axhline(y=0.8, color='orange', linestyle='--', alpha=0.7, label='Good (>0.8)')
        
        ax.set_ylabel('Fairness Index')
        ax.set_ylim(0, 1.05)
        ax.grid(True, alpha=0.3)
        ax.legend(loc='upper right', fontsize=9) 


def plot_rq4_overall_fairness(ax, all_stats):
    """Plot overall fairness comparison"""
    ax.set_title('Overall Jain\'s Fairness Index', fontweight='bold')
    
    exp_names = [stats['experiment'] for stats in all_stats]
    jfi_values = [stats.get('jains_fairness_index', 0) for stats in all_stats]
    colors = ['#2ecc71' if jfi > 0.9 else '#f39c12' if jfi > 0.8 else '#e74c3c' for jfi in jfi_values]
    
    bars = ax.bar(range(len(exp_names)), jfi_values, color=colors, alpha=0.8)
    
    # Add value labels
    for bar, jfi in zip(bars, jfi_values):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height + 0.01,
               f'{jfi:.3f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
    
    ax.set_xticks(range(len(exp_names)))
    ax.set_xticklabels(exp_names, rotation=45, ha='right')
    ax.set_ylabel('Jain\'s Fairness Index')
    ax.set_ylim(0, 1.05)
    ax.grid(True, alpha=0.3)
    
    # Reference lines
    ax.axhline(y=0.9, color='green', linestyle='--', alpha=0.7, label='Excellent')
    ax.axhline(y=0.8, color='orange', linestyle='--', alpha=0.7, label='Good')
    ax.legend()

def plot_rq4_algorithm_fairness(ax, all_stats):
    """Plot algorithm-level fairness"""
    ax.set_title('Algorithm-Level Fairness (Prague vs BBRv3)', fontweight='bold')
    
    exp_names = [stats['experiment'] for stats in all_stats]
    algo_jfi_values = [stats.get('algorithm_fairness', 0) for stats in all_stats]
    colors = ['#2ecc71' if jfi > 0.9 else '#f39c12' if jfi > 0.8 else '#e74c3c' for jfi in algo_jfi_values]
    
    bars = ax.bar(range(len(exp_names)), algo_jfi_values, color=colors, alpha=0.8)
    
    # Add value labels
    for bar, jfi in zip(bars, algo_jfi_values):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height + 0.01,
               f'{jfi:.3f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
    
    ax.set_xticks(range(len(exp_names)))
    ax.set_xticklabels(exp_names, rotation=45, ha='right')
    ax.set_ylabel('Algorithm Fairness Index')
    ax.set_ylim(0, 1.05)
    ax.grid(True, alpha=0.3)
    
    # Reference lines
    ax.axhline(y=0.9, color='green', linestyle='--', alpha=0.7)
    ax.axhline(y=0.8, color='orange', linestyle='--', alpha=0.7)

def plot_rq4_throughput_summary(ax, all_stats):
    """Plot throughput summary"""
    ax.set_title('Average Algorithm Throughput', fontweight='bold')
    
    exp_names = [stats['experiment'] for stats in all_stats]
    prague_tputs = [stats.get('prague_avg_throughput', 0) for stats in all_stats]
    bbr_tputs = [stats.get('bbr_avg_throughput', 0) for stats in all_stats]
    
    x = np.arange(len(exp_names))
    width = 0.35
    
    bars1 = ax.bar(x - width/2, prague_tputs, width, label='Prague', color='#1f77b4', alpha=0.8)
    bars2 = ax.bar(x + width/2, bbr_tputs, width, label='BBRv3', color='#ff7f0e', alpha=0.8)
    
    # Add value labels
    for bars in [bars1, bars2]:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                ax.text(bar.get_x() + bar.get_width()/2., height + 1,
                       f'{height:.1f}', ha='center', va='bottom', fontweight='bold', fontsize=8)
    
    ax.set_xticks(x)
    ax.set_xticklabels(exp_names, rotation=45, ha='right')
    ax.set_ylabel('Throughput (Mbps)')
    ax.legend()
    ax.grid(True, alpha=0.3)

def plot_rq4_queueing_summary(ax, all_stats):
    """Plot queueing delay summary"""
    ax.set_title('Mean Queueing Delays', fontweight='bold')
    
    exp_names = []
    prague_delays = []
    bbr_delays = []
    
    for stats in all_stats:
        if 'prague_queue' in stats and 'bbr_queue' in stats:
            exp_names.append(stats['experiment'])
            prague_delays.append(stats['prague_queue']['mean_delay'])
            bbr_delays.append(stats['bbr_queue']['mean_delay'])
    
    if exp_names:
        x = np.arange(len(exp_names))
        width = 0.35
        
        bars1 = ax.bar(x - width/2, prague_delays, width, label='Prague', color='#1f77b4', alpha=0.8)
        bars2 = ax.bar(x + width/2, bbr_delays, width, label='BBRv3', color='#ff7f0e', alpha=0.8)
        
        ax.set_xticks(x)
        ax.set_xticklabels(exp_names, rotation=45, ha='right')
        ax.set_ylabel('Mean Queue Delay (ms)')
        ax.set_yscale('log')
        ax.legend()
        ax.grid(True, alpha=0.3)

def create_paper_quality_plots(base_dir, all_stats):
    """Create publication-quality plots for academic paper"""
    
    print("\n📊 Creating publication-quality plots...")
    
    if not all_stats:
        print("  ⚠️  No statistics available for plotting")
        return
    
    # Create main comparison figure with subplots - 2x3 layout without fairness panel
    fig = plt.figure(figsize=(18, 14))
    gs = fig.add_gridspec(3, 2, height_ratios=[2.5, 2.5, 1.2], width_ratios=[1.8, 1.2], 
                         hspace=0.4, wspace=0.3)
    
    # Main title with overall fairness summary
    overall_jfi = np.mean([s.get('jains_fairness_index', 0) for s in all_stats])
    fig.suptitle(f'RQ4: Prague vs BBRv3 Fairness in L4S Environment\nOverall Fairness (JFI): {overall_jfi:.3f}', 
                 fontsize=18, fontweight='bold', y=0.98)
    
    # Plot 1: Per-flow throughput comparison (left column, top)
    ax1 = fig.add_subplot(gs[0, 0])
    plot_per_flow_throughput_comparison(ax1, base_dir, all_stats)
    
    # Plot 2: Algorithm throughput balance (right column, top - wider now)
    ax2 = fig.add_subplot(gs[0, 1])
    plot_algorithm_balance(ax2, all_stats)
    
    # Plot 3: DualPI2 queue delay over time (left column, middle)
    ax3 = fig.add_subplot(gs[1, 0])
    plot_dualpi2_queue_analysis(ax3, base_dir, all_stats)
    
    # Right middle position - leave empty for better spacing
    ax_empty = fig.add_subplot(gs[1, 1])
    ax_empty.axis('off')
    
    # Plot 4: Summary statistics table (spans all columns, bottom)
    ax4 = fig.add_subplot(gs[2, :])
    plot_summary_table_v2(ax4, all_stats)
    
    # Save the main figure with more padding
    output_file = base_dir / 'rq4_paper_quality_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight', facecolor='white', pad_inches=0.2)
    plt.close()
    print(f"  ✓ Saved main figure: {output_file.name}")
    
    # Create individual detailed plots
    create_detailed_per_flow_plots(base_dir, all_stats)
    create_queueing_analysis_plots(base_dir, all_stats)

def plot_per_flow_throughput_comparison(ax, base_dir, all_stats):
    """Plot per-flow throughput comparison across all experiments"""
    ax.set_title('Per-Flow Throughput Distribution', fontweight='bold', fontsize=14)
    
    x_pos = 0
    x_labels = []
    x_positions = []
    
    for stats in all_stats:
        exp_name = stats['experiment']
        
        if 'individual_throughputs' in stats and 'flow_labels' in stats:
            throughputs = stats['individual_throughputs']
            labels = stats['flow_labels']
            
            # Separate Prague and BBR flows
            prague_tputs = [t for t, l in zip(throughputs, labels) if 'Prague' in l]
            bbr_tputs = [t for t, l in zip(throughputs, labels) if 'BBRv3' in l]
            
            # Plot Prague flows
            if prague_tputs:
                prague_x = [x_pos + i*0.8 for i in range(len(prague_tputs))]
                bars_p = ax.bar(prague_x, prague_tputs, 0.7, label='Prague' if x_pos == 0 else "", 
                              color='#1f77b4', alpha=0.8, edgecolor='black', linewidth=0.5)
                x_pos += len(prague_tputs) * 0.8
            
            # Plot BBR flows
            if bbr_tputs:
                bbr_x = [x_pos + i*0.8 for i in range(len(bbr_tputs))]
                bars_b = ax.bar(bbr_x, bbr_tputs, 0.7, label='BBRv3' if x_pos <= 1 else "", 
                              color='#ff7f0e', alpha=0.8, edgecolor='black', linewidth=0.5)
                x_pos += len(bbr_tputs) * 0.8
            
            # Add experiment label
            center_x = x_pos - (len(throughputs) * 0.8) / 2
            x_labels.append(exp_name)
            x_positions.append(center_x)
            
            # Add fairness index as text
            jfi = stats.get('jains_fairness_index', 0)
            ax.text(center_x, max(throughputs) + 5, f'JFI: {jfi:.3f}', 
                   ha='center', va='bottom', fontweight='bold', fontsize=10,
                   bbox=dict(boxstyle="round,pad=0.3", facecolor="yellow", alpha=0.7))
            
            x_pos += 1.5  # Gap between experiments
    
    ax.set_xticks(x_positions)
    ax.set_xticklabels(x_labels, rotation=45, ha='right')
    ax.set_ylabel('Throughput (Mbps)', fontsize=12)
    ax.grid(True, alpha=0.3, axis='y')
    ax.legend(loc='upper left', fontsize=11)
    ax.set_ylim(bottom=0)

def plot_fairness_numbers_display(ax, all_stats):
    """Display overall fairness metrics clearly"""
    ax.set_title('Overall Fairness', fontweight='bold', fontsize=14)
    ax.axis('off')
    
    # Create text display of fairness numbers
    y_pos = 0.9
    for i, stats in enumerate(all_stats):
        exp_name = stats['experiment']
        jfi = stats.get('jains_fairness_index', 0)
        
        # Color code based on fairness level
        if jfi > 0.9:
            color = 'green'
            fairness_level = 'Excellent'
        elif jfi > 0.8:
            color = 'orange'  
            fairness_level = 'Good'
        elif jfi > 0.6:
            color = 'blue'
            fairness_level = 'Moderate'
        else:
            color = 'red'
            fairness_level = 'Poor'
        
        text = f"{exp_name}: {jfi:.3f}"
        ax.text(0.1, y_pos, text, transform=ax.transAxes, fontsize=14, 
               fontweight='bold', color=color)
        
        y_pos -= 0.15
    
    # Add simple explanation
    ax.text(0.1, 0.15, "Jain's Fairness Index\n(all flows)", 
           transform=ax.transAxes, fontsize=11, style='italic')
    ax.text(0.1, 0.05, ">0.9=Excellent, >0.8=Good", 
           transform=ax.transAxes, fontsize=10, style='italic')

def plot_dualpi2_queue_analysis(ax, base_dir, all_stats):
    """Plot DualPI2 queue delay over time with statistics overlay"""
    ax.set_title('DualPI2 Queue Delay Over Time', fontweight='bold', fontsize=14)
    
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b', '#e377c2']
    
    # Collect statistics for text overlay
    delay_stats = []
    
    for i, stats in enumerate(all_stats):
        exp_name = stats['experiment']
        exp_dir = Path(f"{base_dir}/{exp_name}")
        
        if exp_dir.exists():
            existing_files, _ = check_experiment_files(exp_dir)
            l4s_sojourn = load_data_safely(existing_files.get('l4s_sojourn'))
            
            if l4s_sojourn is not None:
                # Plot delay over time
                times = l4s_sojourn.iloc[:, 0].values
                delays = l4s_sojourn.iloc[:, 1].values
                
                # Use a rolling mean to smooth the data
                if len(delays) > 100:
                    window = len(delays) // 50  # Adaptive window size
                    delays_smooth = pd.Series(delays).rolling(window=window, center=True).mean()
                    ax.plot(times, delays_smooth, label=exp_name, 
                           color=colors[i % len(colors)], linewidth=2, alpha=0.8)
                else:
                    ax.plot(times, delays, label=exp_name, 
                           color=colors[i % len(colors)], linewidth=2, alpha=0.8)
                
                # Calculate statistics for analysis period (excluding warmup/teardown)
                analysis_data = l4s_sojourn[
                    (l4s_sojourn.iloc[:, 0] >= 8) & 
                    (l4s_sojourn.iloc[:, 0] <= 30)
                ]
                if len(analysis_data) > 0:
                    analysis_delays = analysis_data.iloc[:, 1].values
                    delay_stats.append({
                        'exp': exp_name,
                        'mean': np.mean(analysis_delays),
                        'median': np.median(analysis_delays),
                        'p95': np.percentile(analysis_delays, 95),
                        'p99': np.percentile(analysis_delays, 99)
                    })
    
    # Add analysis window shading
    ax.axvspan(0, 8, alpha=0.15, color='gray', label='Warmup')
    
    ax.set_xlabel('Time (s)', fontsize=12)
    ax.set_ylabel('Queue Delay (ms)', fontsize=12)
    ax.set_yscale('log')
    ax.legend(loc='upper right', fontsize=9, ncol=2)  # Smaller font, 2 columns
    ax.grid(True, alpha=0.3)
    ax.set_xlim(0, 30)
    
    # Add compact statistics text overlay
    if delay_stats:
        stats_text = "Stats (mean/median/95th/99th ms):\n"
        for stat in delay_stats:
            stats_text += f"{stat['exp']}: {stat['mean']:.1f}/{stat['median']:.1f}/{stat['p95']:.1f}/{stat['p99']:.1f}\n"
        
        ax.text(0.02, 0.55, stats_text, transform=ax.transAxes, 
               fontsize=8, va='top', ha='left',
               bbox=dict(boxstyle="round,pad=0.2", facecolor="white", alpha=0.9))

def plot_fairness_metrics_comparison(ax, all_stats):
    """Plot fairness metrics with clear numerical values"""
    ax.set_title('Fairness Metrics', fontweight='bold', fontsize=14)
    
    exp_names = [stats['experiment'] for stats in all_stats]
    jfi_values = [stats.get('jains_fairness_index', 0) for stats in all_stats]
    algo_jfi_values = [stats.get('algorithm_fairness', 0) for stats in all_stats]
    
    x = np.arange(len(exp_names))
    width = 0.35
    
    # Overall fairness bars
    bars1 = ax.bar(x - width/2, jfi_values, width, label='Overall JFI', 
                   color='#2ecc71', alpha=0.8, edgecolor='black')
    
    # Algorithm fairness bars
    bars2 = ax.bar(x + width/2, algo_jfi_values, width, label='Algorithm JFI', 
                   color='#3498db', alpha=0.8, edgecolor='black')
    
    # Add value labels on bars
    for i, (bar1, bar2, jfi, algo_jfi) in enumerate(zip(bars1, bars2, jfi_values, algo_jfi_values)):
        ax.text(bar1.get_x() + bar1.get_width()/2., bar1.get_height() + 0.01,
               f'{jfi:.3f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
        ax.text(bar2.get_x() + bar2.get_width()/2., bar2.get_height() + 0.01,
               f'{algo_jfi:.3f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
    
    # Reference lines
    ax.axhline(y=0.9, color='green', linestyle='--', alpha=0.7, linewidth=2, label='Excellent (≥0.9)')
    ax.axhline(y=0.8, color='orange', linestyle='--', alpha=0.7, linewidth=2, label='Good (≥0.8)')
    
    ax.set_xticks(x)
    ax.set_xticklabels(exp_names, rotation=45, ha='right')
    ax.set_ylabel('Fairness Index', fontsize=12)
    ax.set_ylim(0, 1.05)
    ax.grid(True, alpha=0.3, axis='y')
    ax.legend(loc='lower right', fontsize=9)

def plot_queueing_delay_comparison(ax, base_dir, all_stats):
    """Plot queueing delay comparison with box plots"""
    ax.set_title('Queueing Delay Distribution by Algorithm', fontweight='bold', fontsize=14)
    
    # Collect delay data for each experiment
    prague_delays_all = []
    bbr_delays_all = []
    exp_labels = []
    
    for stats in all_stats:
        exp_name = stats['experiment']
        exp_dir = Path(f"{base_dir}/{exp_name}")
        
        if exp_dir.exists():
            existing_files, _ = check_experiment_files(exp_dir)
            
            prague_sojourn = load_data_safely(existing_files.get('prague_sojourn'))
            bbr_sojourn = load_data_safely(existing_files.get('bbr_sojourn'))
            
            # Extract delay data for analysis period
            if prague_sojourn is not None:
                analysis_data = prague_sojourn[
                    (prague_sojourn.iloc[:, 0] >= 5) & 
                    (prague_sojourn.iloc[:, 0] <= 55)
                ]
                if len(analysis_data) > 0:
                    prague_delays_all.append(analysis_data.iloc[:, 1].values)
                else:
                    prague_delays_all.append([])
            else:
                prague_delays_all.append([])
            
            if bbr_sojourn is not None:
                analysis_data = bbr_sojourn[
                    (bbr_sojourn.iloc[:, 0] >= 5) & 
                    (bbr_sojourn.iloc[:, 0] <= 55)
                ]
                if len(analysis_data) > 0:
                    bbr_delays_all.append(analysis_data.iloc[:, 1].values)
                else:
                    bbr_delays_all.append([])
            else:
                bbr_delays_all.append([])
            
            exp_labels.append(exp_name)
    
    # Create grouped box plots
    all_delays = []
    all_labels = []
    positions = []
    colors = []
    
    pos = 1
    for i, exp in enumerate(exp_labels):
        if len(prague_delays_all[i]) > 0:
            all_delays.append(prague_delays_all[i])
            all_labels.append(f'{exp}\nPrague')
            positions.append(pos)
            colors.append('#1f77b4')
            pos += 1
        
        if len(bbr_delays_all[i]) > 0:
            all_delays.append(bbr_delays_all[i])
            all_labels.append(f'{exp}\nBBRv3')
            positions.append(pos)
            colors.append('#ff7f0e')
            pos += 1
        
        pos += 0.5  # Gap between experiments
    
    if all_delays:
        bp = ax.boxplot(all_delays, positions=positions, patch_artist=True, 
                       showfliers=False, widths=0.8)
        
        # Color the boxes
        for i, (patch, color) in enumerate(zip(bp['boxes'], colors)):
            patch.set_facecolor(color)
            patch.set_alpha(0.7)
    
    ax.set_xticks(positions)
    ax.set_xticklabels(all_labels, rotation=45, ha='right', fontsize=9)
    ax.set_ylabel('Queue Delay (ms)', fontsize=12)
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add legend
    from matplotlib.patches import Patch
    legend_elements = [Patch(facecolor='#1f77b4', alpha=0.7, label='Prague'),
                      Patch(facecolor='#ff7f0e', alpha=0.7, label='BBRv3')]
    ax.legend(handles=legend_elements, loc='upper right')

def plot_algorithm_balance(ax, all_stats):
    """Plot algorithm throughput balance"""
    ax.set_title('Algorithm Throughput Balance', fontweight='bold', fontsize=14)
    
    prague_tputs = []
    bbr_tputs = []
    exp_names = []
    
    for stats in all_stats:
        prague_avg = stats.get('prague_avg_throughput', 0)
        bbr_avg = stats.get('bbr_avg_throughput', 0)
        
        if prague_avg > 0 and bbr_avg > 0:
            prague_tputs.append(prague_avg)
            bbr_tputs.append(bbr_avg)
            exp_names.append(stats['experiment'])
    
    if prague_tputs and bbr_tputs:
        x = np.arange(len(exp_names))
        width = 0.35
        
        bars1 = ax.bar(x - width/2, prague_tputs, width, label='Prague Avg', 
                       color='#1f77b4', alpha=0.8, edgecolor='black')
        bars2 = ax.bar(x + width/2, bbr_tputs, width, label='BBRv3 Avg', 
                       color='#ff7f0e', alpha=0.8, edgecolor='black')
        
        # Add value labels
        for bar1, bar2, p_tput, b_tput in zip(bars1, bars2, prague_tputs, bbr_tputs):
            ax.text(bar1.get_x() + bar1.get_width()/2., bar1.get_height() + 1,
                   f'{p_tput:.1f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
            ax.text(bar2.get_x() + bar2.get_width()/2., bar2.get_height() + 1,
                   f'{b_tput:.1f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
        
        ax.set_xticks(x)
        ax.set_xticklabels(exp_names, rotation=45, ha='right')
        ax.set_ylabel('Throughput (Mbps)', fontsize=12)
        ax.legend(loc='upper left', fontsize=10)
        ax.grid(True, alpha=0.3, axis='y')

def plot_summary_table_v2(ax, all_stats):
    """Plot summary statistics table"""
    ax.set_title('Experimental Results Summary', fontweight='bold', fontsize=12, pad=10)
    ax.axis('off')
    
    # Create table data
    table_data = []
    headers = ['Experiment', 'Prague\nFlows', 'BBRv3\nFlows', 'Overall\nJFI', 'Algorithm\nJFI', 
              'Prague Avg\n(Mbps)', 'BBRv3 Avg\n(Mbps)', 'Fairness\nClass']
    
    for stats in all_stats:
        exp = stats['experiment']
        prague_flows = stats.get('prague_flows', 0)
        bbr_flows = stats.get('bbr_flows', 0)
        jfi = stats.get('jains_fairness_index', 0)
        algo_jfi = stats.get('algorithm_fairness', 0)
        prague_tput = stats.get('prague_avg_throughput', 0)
        bbr_tput = stats.get('bbr_avg_throughput', 0)
        
        # Fairness classification
        if jfi > 0.9:
            fairness_class = "Excellent"
        elif jfi > 0.8:
            fairness_class = "Good"
        elif jfi > 0.6:
            fairness_class = "Moderate"
        else:
            fairness_class = "Poor"
        
        row = [exp, str(prague_flows), str(bbr_flows), f'{jfi:.3f}', f'{algo_jfi:.3f}',
               f'{prague_tput:.1f}' if prague_tput > 0 else 'N/A',
               f'{bbr_tput:.1f}' if bbr_tput > 0 else 'N/A', fairness_class]
        table_data.append(row)
    
    # Create table
    table = ax.table(cellText=table_data, colLabels=headers, loc='center', cellLoc='center')
    table.auto_set_font_size(False)
    table.set_fontsize(9)
    table.scale(1, 2)
    
    # Style the table
    for i in range(len(headers)):
        table[(0, i)].set_facecolor('#4CAF50')
        table[(0, i)].set_text_props(weight='bold', color='white')
    
    # Color code fairness classes
    for i, row in enumerate(table_data, 1):
        fairness = row[-1]
        if fairness == 'Excellent':
            color = '#90EE90'  # Light green
        elif fairness == 'Good':
            color = '#FFE55C'  # Light yellow
        elif fairness == 'Moderate':
            color = '#FFB347'  # Light orange
        else:
            color = '#FFB3BA'  # Light red
        
        # Check if the cell exists before setting color
        if (i, -1) in table._cells:
            table[(i, -1)].set_facecolor(color)

def create_detailed_per_flow_plots(base_dir, all_stats):
    """Create detailed per-flow analysis plots"""
    print("  📈 Creating detailed per-flow plots...")
    
    # Create individual experiment plots
    for stats in all_stats:
        exp_name = stats['experiment']
        exp_dir = Path(f"{base_dir}/{exp_name}")
        
        if exp_dir.exists():
            create_individual_experiment_plot(exp_dir, stats)

def create_individual_experiment_plot(exp_dir, stats):
    """Create detailed plot for individual experiment"""
    exp_name = stats['experiment']
    
    # Load data
    existing_files, _ = check_experiment_files(exp_dir)
    prague_per_flow = load_data_safely(existing_files.get('prague_per_flow'))
    bbr_per_flow = load_data_safely(existing_files.get('bbr_per_flow'))
    
    if prague_per_flow is None and bbr_per_flow is None:
        return
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 10))
    fig.suptitle(f'Detailed Analysis: {exp_name} ({stats.get("description", "")})', 
                 fontsize=14, fontweight='bold')
    
    # Plot 1: Per-flow throughput over time
    ax1.set_title('Individual Flow Throughput Over Time')
    
    if prague_per_flow is not None:
        for port in prague_per_flow.iloc[:, 1].unique():
            port_data = prague_per_flow[prague_per_flow.iloc[:, 1] == port]
            ax1.plot(port_data.iloc[:, 0], port_data.iloc[:, 2], 
                    label=f'Prague-{int(port)-100}', color='#1f77b4', alpha=0.7, linewidth=2)
    
    if bbr_per_flow is not None:
        for port in bbr_per_flow.iloc[:, 1].unique():
            port_data = bbr_per_flow[bbr_per_flow.iloc[:, 1] == port]
            ax1.plot(port_data.iloc[:, 0], port_data.iloc[:, 2], 
                    label=f'BBRv3-{int(port)-200}', color='#ff7f0e', alpha=0.7, linewidth=2)
    
    ax1.axvspan(0, 5, alpha=0.15, color='gray', label='Warmup/Teardown')
    ax1.axvspan(55, 60, alpha=0.15, color='gray')
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Throughput (Mbps)')
    ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
    ax1.grid(True, alpha=0.3)
    
    # Plot 2: Final throughput comparison
    ax2.set_title('Final Throughput Distribution')
    
    if 'individual_throughputs' in stats and 'flow_labels' in stats:
        throughputs = stats['individual_throughputs']
        labels = stats['flow_labels']
        colors = ['#1f77b4' if 'Prague' in label else '#ff7f0e' for label in labels]
        
        bars = ax2.bar(range(len(throughputs)), throughputs, color=colors, alpha=0.8, 
                      edgecolor='black', linewidth=0.5)
        
        # Add value labels
        for bar, tput in zip(bars, throughputs):
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height + 1,
                    f'{tput:.1f}', ha='center', va='bottom', fontweight='bold', fontsize=10)
        
        ax2.set_xticks(range(len(labels)))
        ax2.set_xticklabels(labels, rotation=45, ha='right')
        ax2.set_ylabel('Throughput (Mbps)')
        ax2.grid(True, alpha=0.3, axis='y')
        
        # Add fairness info
        jfi = stats.get('jains_fairness_index', 0)
        ax2.text(0.02, 0.98, f'Overall JFI: {jfi:.3f}', transform=ax2.transAxes, 
                fontsize=12, fontweight='bold', va='top',
                bbox=dict(boxstyle="round,pad=0.3", facecolor="yellow", alpha=0.8))
    
    plt.tight_layout()
    output_file = exp_dir / f'{exp_name}_detailed_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight', facecolor='white')
    plt.close()

def create_queueing_analysis_plots(base_dir, all_stats):
    """Create detailed queueing analysis plots"""
    print("  📊 Creating queueing analysis plots...")
    
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('Detailed Queueing Delay Analysis', fontsize=16, fontweight='bold')
    
    # Collect all queueing data
    all_prague_delays = []
    all_bbr_delays = []
    delay_stats = []
    
    for stats in all_stats:
        exp_name = stats['experiment']
        exp_dir = Path(f"{base_dir}/{exp_name}")
        
        if exp_dir.exists():
            existing_files, _ = check_experiment_files(exp_dir)
            l4s_sojourn = load_data_safely(existing_files.get('l4s_sojourn'))
            
            if l4s_sojourn is not None:
                analysis_data = l4s_sojourn[
                    (l4s_sojourn.iloc[:, 0] >= 5) & 
                    (l4s_sojourn.iloc[:, 0] <= 25)  # 30s experiment
                ]
                if len(analysis_data) > 0:
                    delays = analysis_data.iloc[:, 1].values
                    all_prague_delays.extend(delays)  # Combined L4S data
                    all_bbr_delays.extend(delays)     # Same data
                    
                    delay_stats.append({
                        'exp': exp_name,
                        'prague_mean': np.mean(delays),
                        'bbr_mean': np.mean(delays),    # Same for both since it's L4S combined
                        'prague_p95': np.percentile(delays, 95),
                        'bbr_p95': np.percentile(delays, 95)
                    })
    
    # Plot 2: Mean delays by experiment
    if delay_stats:
        exp_names = [d['exp'] for d in delay_stats]
        mean_delays = [d['prague_mean'] for d in delay_stats]
        
        ax2.bar(exp_names, mean_delays, color='#3498db', alpha=0.8)
        ax2.set_xticklabels(exp_names, rotation=45, ha='right')
        ax2.set_ylabel('Mean Delay (ms)')
        ax2.set_title('Mean L4S Queueing Delays by Experiment')
        ax2.set_yscale('log')
        ax2.grid(True, alpha=0.3)
        
        # Add value labels
        for bar, delay in zip(ax2.patches, mean_delays):
            height = bar.get_height()
            ax2.text(bar.get_x() + bar.get_width()/2., height * 1.1,
                    f'{delay:.1f}ms', ha='center', va='bottom', fontweight='bold', fontsize=9)
    
    # Plot 3: 95th percentile delays
    if delay_stats:
        p95_delays = [d['prague_p95'] for d in delay_stats]
        
        ax3.bar(exp_names, p95_delays, color='#e74c3c', alpha=0.8)
        ax3.set_xticklabels(exp_names, rotation=45, ha='right')
        ax3.set_ylabel('95th Percentile Delay (ms)')
        ax3.set_title('95th Percentile L4S Queueing Delays')
        ax3.set_yscale('log')
        ax3.grid(True, alpha=0.3)
        
        # Add value labels
        for bar, delay in zip(ax3.patches, p95_delays):
            height = bar.get_height()
            ax3.text(bar.get_x() + bar.get_width()/2., height * 1.1,
                    f'{delay:.1f}ms', ha='center', va='bottom', fontweight='bold', fontsize=9)
    
    # Plot 4: Delay statistics summary
    if delay_stats:
        ax4.axis('off')
        ax4.set_title('L4S Queueing Summary Statistics', fontweight='bold')
        
        # Create text summary
        y_pos = 0.9
        ax4.text(0.1, y_pos, "Experiment | Mean (ms) | 95th (ms)", 
                fontweight='bold', transform=ax4.transAxes, fontsize=12)
        y_pos -= 0.15
        
        for stat in delay_stats:
            text = f"{stat['exp']:8} | {stat['prague_mean']:8.1f} | {stat['prague_p95']:8.1f}"
            ax4.text(0.1, y_pos, text, transform=ax4.transAxes, 
                    fontsize=11, fontfamily='monospace')
            y_pos -= 0.12
    
    plt.tight_layout()
    output_file = base_dir / 'rq4_queueing_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight', facecolor='white')
    plt.close()
    print(f"     ✓ Saved: {output_file.name}")

def create_rq4_summary_table(base_dir, all_stats):
    """Create CSV summary table of RQ4 results"""
    print("  📊 Creating summary table...")
    
    # Create summary data
    summary_data = []
    for stats in all_stats:
        exp = stats['experiment']
        prague_flows = stats.get('prague_flows', 0)
        bbr_flows = stats.get('bbr_flows', 0)
        jfi = stats.get('jains_fairness_index', 0)
        algo_jfi = stats.get('algorithm_fairness', 0)
        prague_tput = stats.get('prague_avg_throughput', 0)
        bbr_tput = stats.get('bbr_avg_throughput', 0)
        
        # Fairness classification
        if jfi > 0.9:
            fairness_class = "Excellent"
        elif jfi > 0.8:
            fairness_class = "Good"
        elif jfi > 0.6:
            fairness_class = "Moderate"
        else:
            fairness_class = "Poor"
        
        summary_data.append([
            exp, prague_flows, bbr_flows, f'{jfi:.3f}', f'{algo_jfi:.3f}',
            f'{prague_tput:.1f}' if prague_tput > 0 else 'N/A',
            f'{bbr_tput:.1f}' if bbr_tput > 0 else 'N/A', fairness_class
        ])
    
    # Save to CSV
    output_file = base_dir / 'rq4_summary.csv'
    with open(output_file, 'w') as f:
        f.write("Experiment,Prague_Flows,BBRv3_Flows,Overall_JFI,Algorithm_JFI,Prague_Avg_Mbps,BBRv3_Avg_Mbps,Fairness_Class\n")
        for row in summary_data:
            f.write(','.join(map(str, row)) + '\n')
    
    print(f"     ✓ Saved: {output_file.name}")

def main():
    parser = argparse.ArgumentParser(description='RQ4 Analysis: Prague vs BBRv3 L4S Fairness')
    parser.add_argument(
        '--exp-dir',
        type=str,
        help='Specific experiment directory to analyze (e.g., results/rq4/P1-B1)'
    )
    parser.add_argument(
        '--base-dir', 
        default='results/rq4', 
        help='Base directory for all RQ4 experiments'
    )
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output'
    )
    parser.add_argument(
        '--plots-only',
        action='store_true',
        help='Only create plots, skip individual analysis'
    )
    parser.add_argument(
        '--paper-plots',
        action='store_true',
        help='Create only publication-quality plots for paper'
    )
    
    args = parser.parse_args()

    if args.exp_dir:
        # Single experiment analysis
        exp_dir = Path(args.exp_dir)
        if not exp_dir.exists():
            print(f"❌ Error: Directory {exp_dir} does not exist")
            sys.exit(1)
        
        print("=== RQ4 Single Experiment Analysis ===")
        stats = analyze_rq4_experiment(exp_dir, args.verbose)
        if stats:
            create_rq4_experiment_plot(exp_dir, stats, args.verbose)
        print("✅ Analysis complete!")
        
    else:
        # Full analysis of all RQ4 experiments
        base_dir = Path(args.base_dir)
        if not base_dir.exists():
            print(f"❌ Error: Directory {base_dir} does not exist")
            sys.exit(1)

        print("🚀 === RQ4 COMPREHENSIVE ANALYSIS === 🚀")
        
        # Find all RQ4 experiment directories
        exp_dirs = [d for d in sorted(base_dir.iterdir()) 
                   if d.is_dir() and not d.name.startswith('.')]
        
        if not exp_dirs:
            print("❌ No experiment directories found!")
            sys.exit(1)
            
        print(f"Found {len(exp_dirs)} experiment directories")
        
        if not args.plots_only:
            # Individual experiment analysis
            print("\n📊 === INDIVIDUAL EXPERIMENT ANALYSIS ===")
            all_stats = []
            
            for i, exp_dir in enumerate(exp_dirs, 1):
                exp_name = exp_dir.name
                print(f"\n[{i}/{len(exp_dirs)}] Processing {exp_name}...")
                
                stats = analyze_rq4_experiment(exp_dir, args.verbose)
                if stats:
                    all_stats.append(stats)
                    create_rq4_experiment_plot(exp_dir, stats, args.verbose)

            print(f"\n✅ Individual analysis complete! Processed {len(all_stats)} experiments")
        else:
            # Load existing statistics for plots
            print("\n📊 Loading existing statistics for plotting...")
            all_stats = []
            for exp_dir in exp_dirs:
                # Try to load existing stats or do quick analysis
                stats = analyze_rq4_experiment(exp_dir, False)
                if stats:
                    all_stats.append(stats)
        
        if len(all_stats) == 0:
            print("❌ No valid experiments found to analyze!")
            sys.exit(1)
        
        # Handle paper plots only option
        if args.paper_plots:
            print("\n📊 === CREATING PAPER-QUALITY PLOTS ONLY ===")
            create_paper_quality_plots(base_dir, all_stats)
            print("✅ Paper-quality plots created!")
            return
        
        # Comparison plots and summary
        print("\n📈 === CREATING COMPARISON ANALYSIS ===")
        create_paper_quality_plots(base_dir, all_stats)
        create_rq4_summary_table(base_dir, all_stats)
        
        # Final summary
        print(f"\n🎯 === RQ4 ANALYSIS SUMMARY ===")
        jfi_values = [s.get('jains_fairness_index', 0) for s in all_stats if 'jains_fairness_index' in s]
        algo_jfi_values = [s.get('algorithm_fairness', 0) for s in all_stats if 'algorithm_fairness' in s]
        
        if jfi_values:
            avg_jfi = np.mean(jfi_values)
            min_jfi = min(jfi_values)
            max_jfi = max(jfi_values)
            excellent_count = sum(1 for jfi in jfi_values if jfi > 0.9)
            good_count = sum(1 for jfi in jfi_values if 0.8 < jfi <= 0.9)
            
            print(f"📊 Overall Fairness Results:")
            print(f"   - Average JFI: {avg_jfi:.3f}")
            print(f"   - Range: {min_jfi:.3f} - {max_jfi:.3f}")
            print(f"   - Excellent fairness (>0.9): {excellent_count}/{len(jfi_values)} experiments")
            print(f"   - Good fairness (>0.8): {good_count}/{len(jfi_values)} experiments")
        
        if algo_jfi_values:
            avg_algo_jfi = np.mean(algo_jfi_values)
            print(f"📊 Algorithm Fairness (Prague vs BBRv3):")
            print(f"   - Average algorithm JFI: {avg_algo_jfi:.3f}")
        
        print(f"\n🎉 === RQ4 ANALYSIS COMPLETE === 🎉")
        print(f"📁 Results saved in: {base_dir}")
        print(f"📊 Individual plots: {len(all_stats)} experiments")
        

if __name__ == '__main__':
    main() 