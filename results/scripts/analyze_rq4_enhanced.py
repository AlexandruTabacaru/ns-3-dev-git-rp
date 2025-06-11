#!/usr/bin/env python3
"""
RQ4 Enhanced Analysis: Prague vs BBRv3 L4S Fairness Analysis

Analyzes results from RQ4 experiments focusing on fairness between
two scalable congestion controllers (Prague and BBRv3) in L4S:
- Jain's fairness index between algorithms
- Per-flow throughput distribution
- Queueing delay analysis (enhanced analytics)
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
    
    if verbose:
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
    if verbose:
        print("  Computing RQ4 fairness statistics...")
    stats = calculate_rq4_fairness_statistics(exp_name, description, num_prague, num_bbr,
                                             prague_per_flow, bbr_per_flow,
                                             prague_tput, bbr_tput, 
                                             l4s_sojourn,
                                             warmup_time, teardown_time)
    
    if stats and verbose:
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
    
    return stats 

def create_paper_quality_plots(base_dir, all_stats):
    """Create simplified publication-quality plots focusing on key metrics"""
    
    print("\n📊 Creating enhanced paper-quality plots...")
    
    if not all_stats:
        print("  ⚠️  No statistics available for plotting")
        return
    
    # Create figure with only per-flow throughput comparison
    fig = plt.figure(figsize=(16, 7))
    fig.suptitle('RQ4: Prague vs BBRv3 Fairness Analysis in L4S Environment', 
                 fontsize=18, fontweight='bold', y=0.99)
    ax1 = fig.add_subplot(1, 1, 1)
    plot_per_flow_throughput_comparison(ax1, base_dir, all_stats)
    output_file = base_dir / 'rq4_paper_quality_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight', facecolor='white', pad_inches=0.5)
    plt.close()
    print(f"  ✓ Saved main figure: {output_file.name}")

def plot_per_flow_throughput_comparison(ax, base_dir, all_stats):
    """Plot per-flow throughput comparison across all experiments with enhanced details"""
    ax.set_title('Per-Flow Throughput Distribution with Fairness Metrics', fontweight='bold', fontsize=15, pad=20)
    
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
                
                # Add value labels on Prague bars
                # for bar, tput in zip(bars_p, prague_tputs):
                #     height = bar.get_height()
                #     ax.text(bar.get_x() + bar.get_width()/2., height + 3,
                #            f'{tput:.1f}', ha='center', va='bottom', fontweight='bold', fontsize=15)
                
                x_pos += len(prague_tputs) * 0.8
            
            # Plot BBR flows
            if bbr_tputs:
                bbr_x = [x_pos + i*0.8 for i in range(len(bbr_tputs))]
                bars_b = ax.bar(bbr_x, bbr_tputs, 0.7, label='BBRv3' if x_pos <= 1 else "", 
                              color='#ff7f0e', alpha=0.8, edgecolor='black', linewidth=0.5)
                
                # Add value labels on BBR bars
                # for bar, tput in zip(bars_b, bbr_tputs):
                #     height = bar.get_height()
                #     ax.text(bar.get_x() + bar.get_width()/2., height + 3,
                #            f'{tput:.1f}', ha='center', va='bottom', fontweight='bold', fontsize=15)
                
                x_pos += len(bbr_tputs) * 0.8
            
            # Add experiment label
            center_x = x_pos - (len(throughputs) * 0.8) / 2
            x_labels.append(exp_name)
            x_positions.append(center_x)
            
            # Add JFI metric as text (simplified)
            jfi = stats.get('jains_fairness_index', 0)
            
            # Create simplified metrics text box - only JFI
            metrics_text = f'JFI: {jfi:.3f}'
            ax.text(center_x, max(throughputs) + 5, metrics_text, 
                   ha='center', va='bottom', fontweight='bold', fontsize=15,
                   bbox=dict(boxstyle="round,pad=0.3", facecolor="lightblue", alpha=0.8))
            
            x_pos += 2.0  # Gap between experiments
    
    ax.set_xticks(x_positions)
    ax.set_xticklabels(x_labels, rotation=0, ha='center', fontsize=14, fontweight='bold')
    ax.set_ylabel('Throughput (Mbps)', fontsize=17)
    ax.tick_params(axis='y', labelsize=16)
    ax.grid(True, alpha=0.3, axis='y')
    ax.legend(loc='upper right', fontsize=14)
    ax.set_ylim(bottom=0)

def create_queueing_analysis_plots(base_dir, all_stats):
    """Create queueing analysis with both bar chart and detailed statistics table"""
    print("  📊 Creating queueing analysis with statistics table...")
    
    fig, ax1 = plt.subplots(1, 1, figsize=(14, 7))
    fig.suptitle('L4S Queue Delay Analysis', fontsize=16, fontweight='bold')
    
    # Collect queueing data
    delay_stats = []
    
    for stats in all_stats:
        exp_name = stats['experiment']
        exp_dir = Path(f"{base_dir}/{exp_name}")
        
        if exp_dir.exists():
            existing_files, _ = check_experiment_files(exp_dir)
            l4s_sojourn = load_data_safely(existing_files.get('l4s_sojourn'))
            
            if l4s_sojourn is not None:
                analysis_data = l4s_sojourn[
                    (l4s_sojourn.iloc[:, 0] >= 8) & 
                    (l4s_sojourn.iloc[:, 0] <= 30)
                ]
                if len(analysis_data) > 0:
                    delays = analysis_data.iloc[:, 1].values
                    delay_stats.append({
                        'exp': exp_name,
                        'mean': np.mean(delays),
                        'p95': np.percentile(delays, 95),
                        'p99': np.percentile(delays, 99),
                        'max': np.max(delays),
                        'samples': len(delays)
                    })
    
    # Plot 1: Mean delays bar chart
    if delay_stats:
        exp_names = [d['exp'] for d in delay_stats]
        mean_delays = [d['mean'] for d in delay_stats]
        
        bars = ax1.bar(exp_names, mean_delays, color='#3498db', alpha=0.8, edgecolor='black', linewidth=1)
        ax1.set_xticklabels(exp_names, rotation=45, ha='right', fontsize=11)
        ax1.set_ylabel('Mean Queue Delay (ms)', fontsize=17)
        ax1.tick_params(axis='y', labelsize=16)
        ax1.set_title('Mean L4S Queueing Delays by Experiment', fontweight='bold', fontsize=14)
        ax1.set_yscale('log')
        ax1.grid(True, alpha=0.3, axis='y')
        
        # Add value labels on bars
        for bar, delay, stat in zip(bars, mean_delays, delay_stats):
            height = bar.get_height()
            ax1.text(bar.get_x() + bar.get_width()/2., height * 1.1,
                   f'{delay:.2f}ms', ha='center', va='bottom', fontweight='bold', fontsize=14)
    
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
        
        # Queue delay stats
        queue_stats = stats.get('l4s_queue')
        mean_delay = queue_stats['mean_delay'] if queue_stats else 0
        
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
            f'{bbr_tput:.1f}' if bbr_tput > 0 else 'N/A', 
            f'{mean_delay:.2f}' if mean_delay > 0 else 'N/A',
            fairness_class
        ])
    
    # Save to CSV
    output_file = base_dir / 'rq4_summary.csv'
    with open(output_file, 'w') as f:
        f.write("Experiment,Prague_Flows,BBRv3_Flows,Overall_JFI,Algorithm_JFI,Prague_Avg_Mbps,BBRv3_Avg_Mbps,Mean_Queue_Delay_ms,Fairness_Class\n")
        for row in summary_data:
            f.write(','.join(map(str, row)) + '\n')
    
    print(f"     ✓ Saved: {output_file.name}")

def main():
    parser = argparse.ArgumentParser(description='RQ4 Enhanced Analysis: Prague vs BBRv3 L4S Fairness')
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
        
        print("=== RQ4 Enhanced Single Experiment Analysis ===")
        stats = analyze_rq4_experiment(exp_dir, args.verbose)
        print("✅ Analysis complete!")
        
    else:
        # Full analysis of all RQ4 experiments
        base_dir = Path(args.base_dir)
        if not base_dir.exists():
            print(f"❌ Error: Directory {base_dir} does not exist")
            sys.exit(1)

        print("🚀 === RQ4 ENHANCED COMPREHENSIVE ANALYSIS === 🚀")
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

            print(f"\n✅ Individual analysis complete! Processed {len(all_stats)} experiments")
        else:
            # Load existing statistics for plots
            print("\n📊 Loading existing statistics for plotting...")
            all_stats = []
            for exp_dir in exp_dirs:
                stats = analyze_rq4_experiment(exp_dir, False)
                if stats:
                    all_stats.append(stats)
        
        if len(all_stats) == 0:
            print("❌ No valid experiments found to analyze!")
            sys.exit(1)
        
        # Create enhanced plots and summary
        print("\n📈 === CREATING ENHANCED ANALYSIS ===")
        create_paper_quality_plots(base_dir, all_stats)
        create_queueing_analysis_plots(base_dir, all_stats)
        create_rq4_summary_table(base_dir, all_stats)
        
        # Final summary
        print(f"\n🎯 === RQ4 ENHANCED ANALYSIS SUMMARY ===")
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
        
        # Queue delay summary per experiment
        print(f"📊 L4S Queue Delay Analysis (Per Experiment):")
        print(f"{'-'*72}")
        
        total_samples = 0
        experiments_with_data = 0
        
        for stats in all_stats:
            exp_name = stats['experiment']
            queue_stats = stats.get('l4s_queue')
            
            if queue_stats:
                mean_delay = queue_stats['mean_delay']
                p95_delay = queue_stats['p95_delay']
                p99_delay = queue_stats['p99_delay']
                max_delay = queue_stats['max_delay']
                samples = queue_stats['samples']
                total_samples += samples
                experiments_with_data += 1
                
                print(f"{exp_name:<12} {mean_delay:<10.2f} {p95_delay:<10.2f} {p99_delay:<10.2f} {max_delay:<10.2f} {samples:<10,}")
            else:
                print(f"{exp_name:<12} {'N/A':<10} {'N/A':<10} {'N/A':<10} {'N/A':<10} {'N/A':<10}")
        
        print(f"{'-'*72}")
        print(f"{'SUMMARY:':<12} {'':<10} {'':<10} {'':<10} {'':<10} {total_samples:<10,}")
        print(f"   - Experiments with queue data: {experiments_with_data}/{len(all_stats)}")
        
        print(f"\n🎉 === RQ4 ENHANCED ANALYSIS COMPLETE === 🎉")
        print(f"📁 Results saved in: {base_dir}")
        print(f"📊 Enhanced plots created:")
        print(f"   - Per-flow throughput distribution with fairness metrics")
        print(f"   - Experimental results summary with algorithm balance")
        print(f"   - Mean L4S queueing delays by experiment")
        print(f"📄 Enhanced summary: rq4_summary.csv")

if __name__ == '__main__':
    main() 