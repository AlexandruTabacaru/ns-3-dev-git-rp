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
        'prague_sojourn': f"prague-sojourn.{exp_name}.dat",
        'bbr_sojourn': f"bbr-sojourn.{exp_name}.dat",
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
    prague_sojourn = load_data_safely(existing_files.get('prague_sojourn'), verbose)
    bbr_sojourn = load_data_safely(existing_files.get('bbr_sojourn'), verbose)

    # Analysis parameters
    warmup_time = 5.0
    teardown_time = 5.0

    # Calculate fairness statistics
    print("  Computing RQ4 fairness statistics...")
    stats = calculate_rq4_fairness_statistics(exp_name, description, num_prague, num_bbr,
                                             prague_per_flow, bbr_per_flow,
                                             prague_tput, bbr_tput, 
                                             l4s_sojourn, prague_sojourn, bbr_sojourn,
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
                                     l4s_sojourn, prague_sojourn, bbr_sojourn,
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
    
    # Analysis time window
    analysis_start = warmup_time
    analysis_end = 60.0 - teardown_time
    
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
    
    # Enhanced L4S queueing analysis for RQ4
    if l4s_sojourn is not None:
        stats['l4s_queue'] = extract_queue_stats(l4s_sojourn, 'L4S')
    
    # Algorithm-specific queueing delays (RQ4 enhancement)
    if prague_sojourn is not None:
        stats['prague_queue'] = extract_queue_stats(prague_sojourn, 'Prague')
    if bbr_sojourn is not None:
        stats['bbr_queue'] = extract_queue_stats(bbr_sojourn, 'BBRv3')
    
    # Compare Prague vs BBRv3 queueing behavior
    if 'prague_queue' in stats and 'bbr_queue' in stats:
        prague_delay = stats['prague_queue']['mean_delay']
        bbr_delay = stats['bbr_queue']['mean_delay']
        stats['queueing_comparison'] = {
            'prague_advantage': (bbr_delay - prague_delay) / bbr_delay if bbr_delay > 0 else 0,
            'delay_ratio': prague_delay / bbr_delay if bbr_delay > 0 else 0,
            'delay_difference': abs(prague_delay - bbr_delay),
            'similar_delays': abs(prague_delay - bbr_delay) < 5.0  # Within 5ms
        }
    
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
        
        # Algorithm-specific queueing
        if 'prague_queue' in stats:
            q = stats['prague_queue']
            f.write(f"PRAGUE QUEUEING DELAYS:\n")
            f.write(f"  Mean delay: {q['mean_delay']:.2f} ms\n")
            f.write(f"  95th percentile: {q['p95_delay']:.2f} ms\n")
            f.write(f"  Samples: {q['samples']}\n\n")
        
        if 'bbr_queue' in stats:
            q = stats['bbr_queue']
            f.write(f"BBRv3 QUEUEING DELAYS:\n")
            f.write(f"  Mean delay: {q['mean_delay']:.2f} ms\n")
            f.write(f"  95th percentile: {q['p95_delay']:.2f} ms\n")
            f.write(f"  Samples: {q['samples']}\n\n")
        
        # Queueing comparison
        if 'queueing_comparison' in stats:
            qc = stats['queueing_comparison']
            f.write(f"QUEUEING BEHAVIOR COMPARISON:\n")
            f.write(f"  Prague advantage: {qc['prague_advantage']:.1%}\n")
            f.write(f"  Delay ratio (Prague/BBRv3): {qc['delay_ratio']:.3f}\n")
            f.write(f"  Delay difference: {qc['delay_difference']:.2f} ms\n")
            f.write(f"  Similar delays: {'Yes' if qc['similar_delays'] else 'No'}\n")

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
    prague_sojourn = load_data_safely(existing_files.get('prague_sojourn'))
    bbr_sojourn = load_data_safely(existing_files.get('bbr_sojourn'))
    
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
    
    # Plot 3: Queueing delay comparison
    ax3 = fig.add_subplot(gs[1, 1])
    plot_rq4_queueing_comparison(ax3, prague_sojourn, bbr_sojourn)
    
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
    ax.axvspan(55, 60, alpha=0.15, color='gray')
    
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

def plot_rq4_queueing_comparison(ax, prague_sojourn, bbr_sojourn):
    """Plot queueing delay comparison for RQ4"""
    ax.set_title('Algorithm-Specific Queueing Delays', fontweight='bold', fontsize=12)
    
    delay_data = []
    labels = []
    
    # Prague delays
    if prague_sojourn is not None:
        analysis_data = prague_sojourn[
            (prague_sojourn.iloc[:, 0] >= 5) & 
            (prague_sojourn.iloc[:, 0] <= 55)
        ]
        if len(analysis_data) > 0:
            delay_data.append(analysis_data.iloc[:, 1].values)
            labels.append('Prague')
    
    # BBRv3 delays
    if bbr_sojourn is not None:
        analysis_data = bbr_sojourn[
            (bbr_sojourn.iloc[:, 0] >= 5) & 
            (bbr_sojourn.iloc[:, 0] <= 55)
        ]
        if len(analysis_data) > 0:
            delay_data.append(analysis_data.iloc[:, 1].values)
            labels.append('BBRv3')
    
    if delay_data:
        bp = ax.boxplot(delay_data, labels=labels, patch_artist=True)
        
        # Color boxes
        colors = ['#1f77b4', '#ff7f0e']
        for i, patch in enumerate(bp['boxes']):
            if i < len(colors):
                patch.set_facecolor(colors[i])
                patch.set_alpha(0.7)
    
    ax.set_ylabel('Queue Delay (ms)')
    ax.set_yscale('log')
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

def create_rq4_comparison_plots(base_dir, all_stats):
    """Create comprehensive comparison plots across all RQ4 experiments"""
    
    print("\n📊 Creating RQ4 comparison plots...")
    
    if not all_stats:
        print("  ⚠️  No statistics available for comparison plots")
        return
    
    # Create overall fairness comparison
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('RQ4 Overall Analysis: Prague vs BBRv3 L4S Fairness', fontsize=16, fontweight='bold')
    
    # Plot 1: Overall fairness comparison
    plot_rq4_overall_fairness(ax1, all_stats)
    
    # Plot 2: Algorithm-level fairness
    plot_rq4_algorithm_fairness(ax2, all_stats)
    
    # Plot 3: Throughput comparison
    plot_rq4_throughput_summary(ax3, all_stats)
    
    # Plot 4: Queueing delay comparison
    plot_rq4_queueing_summary(ax4, all_stats)
    
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    
    output_file = base_dir / 'rq4_overall_comparison.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Saved: {output_file.name}")
    
    # Create flow ratio analysis
    create_rq4_flow_ratio_analysis(base_dir, all_stats)

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

def create_rq4_flow_ratio_analysis(base_dir, all_stats):
    """Create flow ratio analysis plot"""
    
    print("  📈 Creating flow ratio analysis...")
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    fig.suptitle('RQ4 Flow Ratio Analysis', fontsize=16, fontweight='bold')
    
    # Extract flow ratios and fairness
    prague_flows = []
    bbr_flows = []
    overall_fairness = []
    algo_fairness = []
    
    for stats in all_stats:
        if isinstance(stats.get('prague_flows'), int) and isinstance(stats.get('bbr_flows'), int):
            prague_flows.append(stats['prague_flows'])
            bbr_flows.append(stats['bbr_flows'])
            overall_fairness.append(stats.get('jains_fairness_index', 0))
            algo_fairness.append(stats.get('algorithm_fairness', 0))
    
    # Plot 1: Flow count vs overall fairness
    total_flows = [p + b for p, b in zip(prague_flows, bbr_flows)]
    colors = ['#2ecc71' if jfi > 0.9 else '#f39c12' if jfi > 0.8 else '#e74c3c' for jfi in overall_fairness]
    
    scatter1 = ax1.scatter(total_flows, overall_fairness, c=colors, s=100, alpha=0.8, edgecolors='black')
    ax1.set_title('Total Flows vs Overall Fairness')
    ax1.set_xlabel('Total Number of Flows')
    ax1.set_ylabel('Jain\'s Fairness Index')
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(0, 1.05)
    
    # Add experiment labels
    for i, (tf, jfi, p, b) in enumerate(zip(total_flows, overall_fairness, prague_flows, bbr_flows)):
        ax1.annotate(f'P{p}-B{b}', (tf, jfi), xytext=(5, 5), 
                    textcoords='offset points', fontsize=8)
    
    # Plot 2: Prague ratio vs algorithm fairness
    prague_ratios = [p / (p + b) for p, b in zip(prague_flows, bbr_flows)]
    colors2 = ['#2ecc71' if jfi > 0.9 else '#f39c12' if jfi > 0.8 else '#e74c3c' for jfi in algo_fairness]
    
    scatter2 = ax2.scatter(prague_ratios, algo_fairness, c=colors2, s=100, alpha=0.8, edgecolors='black')
    ax2.set_title('Prague Flow Ratio vs Algorithm Fairness')
    ax2.set_xlabel('Prague Flow Ratio')
    ax2.set_ylabel('Algorithm Fairness Index')
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(0, 1.05)
    ax2.axvline(x=0.5, color='gray', linestyle='--', alpha=0.7, label='Balanced (50%)')
    ax2.legend()
    
    # Add experiment labels
    for i, (pr, jfi, p, b) in enumerate(zip(prague_ratios, algo_fairness, prague_flows, bbr_flows)):
        ax2.annotate(f'P{p}-B{b}', (pr, jfi), xytext=(5, 5), 
                    textcoords='offset points', fontsize=8)
    
    plt.tight_layout(rect=[0, 0, 1, 0.94])
    
    output_file = base_dir / 'rq4_flow_ratio_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"     ✓ Saved: {output_file.name}")

def create_rq4_summary_table(base_dir, all_stats):
    """Create comprehensive RQ4 summary table"""
    
    print("\n📊 Creating RQ4 summary table...")
    
    if not all_stats:
        print("  ⚠️  No statistics to summarize")
        return
    
    # Create summary data
    summary_data = []
    for stats in all_stats:
        exp = stats['experiment']
        desc = stats.get('description', '')
        
        # Extract key metrics
        jfi = stats.get('jains_fairness_index', 0)
        algo_jfi = stats.get('algorithm_fairness', 0)
        prague_flows = stats.get('prague_flows', 0)
        bbr_flows = stats.get('bbr_flows', 0)
        
        # Throughput metrics
        prague_tput = stats.get('prague_avg_throughput', 0)
        bbr_tput = stats.get('bbr_avg_throughput', 0)
        tput_ratio = stats.get('throughput_ratio', 0)
        
        # Queueing delays
        prague_delay = stats.get('prague_queue', {}).get('mean_delay', 0) if 'prague_queue' in stats else 0
        bbr_delay = stats.get('bbr_queue', {}).get('mean_delay', 0) if 'bbr_queue' in stats else 0
        
        # Fairness classification
        if jfi > 0.9:
            fairness_class = "Excellent"
        elif jfi > 0.8:
            fairness_class = "Good"  
        elif jfi > 0.6:
            fairness_class = "Moderate"
        else:
            fairness_class = "Poor"
        
        summary_data.append({
            'Experiment': exp,
            'Description': desc,
            'Prague_Flows': prague_flows,
            'BBRv3_Flows': bbr_flows,
            'Overall_JFI': f"{jfi:.3f}",
            'Algorithm_JFI': f"{algo_jfi:.3f}",
            'Fairness_Class': fairness_class,
            'Prague_Tput_Mbps': f"{prague_tput:.1f}" if prague_tput > 0 else "N/A",
            'BBRv3_Tput_Mbps': f"{bbr_tput:.1f}" if bbr_tput > 0 else "N/A",
            'Tput_Ratio': f"{tput_ratio:.3f}" if tput_ratio > 0 else "N/A",
            'Prague_Delay_ms': f"{prague_delay:.2f}" if prague_delay > 0 else "N/A",
            'BBRv3_Delay_ms': f"{bbr_delay:.2f}" if bbr_delay > 0 else "N/A"
        })
    
    # Save as CSV
    output_file = base_dir / 'rq4_summary.csv'
    df = pd.DataFrame(summary_data)
    df.to_csv(output_file, index=False)
    
    # Print formatted table
    print(f"\n{'='*100}")
    print(f"{'RQ4 PRAGUE vs BBRv3 L4S FAIRNESS ANALYSIS SUMMARY':^100}")
    print(f"{'='*100}")
    
    for row in summary_data:
        print(f"\n🎯 {row['Experiment']:8} | {row['Description']}")
        print(f"   Flow config: {row['Prague_Flows']} Prague + {row['BBRv3_Flows']} BBRv3")
        print(f"   Overall fairness: {row['Fairness_Class']:10} (JFI: {row['Overall_JFI']})")
        print(f"   Algorithm fairness: {row['Algorithm_JFI']} (Prague vs BBRv3)")
        print(f"   Throughput: Prague {row['Prague_Tput_Mbps']:>6} Mbps, BBRv3 {row['BBRv3_Tput_Mbps']:>6} Mbps")
        print(f"   Queue delay: Prague {row['Prague_Delay_ms']:>6} ms, BBRv3 {row['BBRv3_Delay_ms']:>6} ms")
    
    print(f"\n✅ RQ4 summary saved: {output_file.name}")

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
        print(f"Base directory: {base_dir}")
        
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
        
        # Comparison plots and summary
        print("\n📈 === CREATING COMPARISON ANALYSIS ===")
        create_rq4_comparison_plots(base_dir, all_stats)
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
        print(f"🔄 Comparison plots: Generated")
        print(f"📄 Summary: rq4_summary.csv")
        print("\n🚀 Key Findings for Publication:")
        print("   - Prague-BBRv3 fairness in L4S environment")
        print("   - Algorithm-specific queueing behavior analysis")
        print("   - Scalability across different flow ratios")
        print("   - L4S framework effectiveness for mixed scalable traffic")

if __name__ == '__main__':
    main() 