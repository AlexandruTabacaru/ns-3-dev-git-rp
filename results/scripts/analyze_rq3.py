#!/usr/bin/env python3
"""
RQ3 Analysis: L4S Fairness and Loss-Sensitivity Analysis

Analyzes results from RQ3 experiments:
- Wired: Fairness between Prague and Cubic flows (Jain's fairness index)
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

def parse_throughput_file(filepath):
    """Parse throughput data file"""
    try:
        data = pd.read_csv(filepath, sep=' ', header=None, names=['time', 'throughput'])
        return data
    except Exception as e:
        print(f"Warning: Could not parse {filepath}: {e}")
        return None

def parse_per_flow_throughput_file(filepath):
    """Parse per-flow throughput data file"""
    try:
        data = pd.read_csv(filepath, sep=' ', header=None, names=['time', 'port', 'throughput'])
        return data
    except Exception as e:
        print(f"Warning: Could not parse {filepath}: {e}")
        return None

def calculate_jains_fairness_index(throughputs):
    """Calculate Jain's fairness index for a list of throughputs"""
    if len(throughputs) == 0:
        return 1.0
    
    sum_throughput = sum(throughputs)
    sum_squared_throughput = sum(x**2 for x in throughputs)
    n = len(throughputs)
    
    if sum_squared_throughput == 0:
        return 1.0
    
    return (sum_throughput ** 2) / (n * sum_squared_throughput)

def analyze_wired_fairness(exp_dir, test_name):
    """Analyze wired fairness experiment"""
    print(f"\n=== Analyzing Wired Fairness: {test_name} ===")
    
    # Debug: Show what files actually exist
    dat_files = glob.glob(os.path.join(exp_dir, "*.dat"))
    print(f"Available .dat files in {exp_dir}:")
    for f in sorted(dat_files):
        print(f"  {os.path.basename(f)}")
    
    results = {
        'test_name': test_name,
        'prague_flows': [],
        'cubic_flows': [],
        'prague_jfi': 0.0,
        'cubic_jfi': 0.0,
        'overall_jfi': 0.0,
        'prague_total_throughput': 0.0,
        'cubic_total_throughput': 0.0
    }
    
    # Parse Prague per-flow throughput
    prague_file = os.path.join(exp_dir, f"prague-per-flow-throughput.{test_name}.dat")
    print(f"Looking for Prague per-flow file: {prague_file}")
    if os.path.exists(prague_file):
        print("  Found Prague per-flow file")
        data = parse_per_flow_throughput_file(prague_file)
        if data is not None:
            # Get final throughputs for each Prague flow
            latest_time = data['time'].max()
            final_data = data[data['time'] >= latest_time - 1.0]  # Last 1 second
            for port in final_data['port'].unique():
                port_data = final_data[final_data['port'] == port]
                avg_throughput = port_data['throughput'].mean()
                results['prague_flows'].append(avg_throughput)
                print(f"  Prague flow port {port}: {avg_throughput:.2f} Mbps")
    else:
        print("  Prague per-flow file not found, trying overall throughput file...")
        # Fallback: try to estimate from overall throughput
        prague_overall_file = os.path.join(exp_dir, f"prague-throughput.{test_name}.dat")
        if os.path.exists(prague_overall_file):
            data = parse_throughput_file(prague_overall_file)
            if data is not None:
                avg_throughput = data['throughput'].mean()
                # Assume single flow if we can't find per-flow data
                results['prague_flows'].append(avg_throughput)
                print(f"  Estimated single Prague flow: {avg_throughput:.2f} Mbps")
    
    # Parse Cubic per-flow throughput
    cubic_file = os.path.join(exp_dir, f"cubic-per-flow-throughput.{test_name}.dat")
    print(f"Looking for Cubic per-flow file: {cubic_file}")
    if os.path.exists(cubic_file):
        print("  Found Cubic per-flow file")
        data = parse_per_flow_throughput_file(cubic_file)
        if data is not None:
            # Get final throughputs for each Cubic flow
            latest_time = data['time'].max()
            final_data = data[data['time'] >= latest_time - 1.0]  # Last 1 second
            for port in final_data['port'].unique():
                port_data = final_data[final_data['port'] == port]
                avg_throughput = port_data['throughput'].mean()
                results['cubic_flows'].append(avg_throughput)
                print(f"  Cubic flow port {port}: {avg_throughput:.2f} Mbps")
    else:
        print("  Cubic per-flow file not found, trying overall throughput file...")
        # Fallback: try to estimate from overall throughput
        cubic_overall_file = os.path.join(exp_dir, f"cubic-throughput.{test_name}.dat")
        if os.path.exists(cubic_overall_file):
            data = parse_throughput_file(cubic_overall_file)
            if data is not None:
                avg_throughput = data['throughput'].mean()
                # Assume single flow if we can't find per-flow data
                results['cubic_flows'].append(avg_throughput)
                print(f"  Estimated single Cubic flow: {avg_throughput:.2f} Mbps")
    
    # Calculate fairness indices
    if results['prague_flows']:
        results['prague_jfi'] = calculate_jains_fairness_index(results['prague_flows'])
        results['prague_total_throughput'] = sum(results['prague_flows'])
    
    if results['cubic_flows']:
        results['cubic_jfi'] = calculate_jains_fairness_index(results['cubic_flows'])
        results['cubic_total_throughput'] = sum(results['cubic_flows'])
    
    # Overall fairness
    all_flows = results['prague_flows'] + results['cubic_flows']
    if all_flows:
        results['overall_jfi'] = calculate_jains_fairness_index(all_flows)
    
    # Print results
    print(f"Prague flows: {len(results['prague_flows'])}")
    print(f"Cubic flows: {len(results['cubic_flows'])}")
    if results['prague_flows']:
        if len(results['prague_flows']) == 1:
            print(f"Prague single flow throughput: {results['prague_total_throughput']:.2f} Mbps")
        else:
            print(f"Prague JFI: {results['prague_jfi']:.4f}")
            print(f"Prague total throughput: {results['prague_total_throughput']:.2f} Mbps")
            print(f"Prague individual: {[f'{x:.2f}' for x in results['prague_flows']]}")
    if results['cubic_flows']:
        if len(results['cubic_flows']) == 1:
            print(f"Cubic single flow throughput: {results['cubic_total_throughput']:.2f} Mbps")
        else:
            print(f"Cubic JFI: {results['cubic_jfi']:.4f}")
            print(f"Cubic total throughput: {results['cubic_total_throughput']:.2f} Mbps")
            print(f"Cubic individual: {[f'{x:.2f}' for x in results['cubic_flows']]}")
    
    # Only show overall JFI if we have multiple flows total
    if len(all_flows) > 1:
        print(f"Overall JFI (across all flows): {results['overall_jfi']:.4f}")
        # Interpret the JFI value
        if results['overall_jfi'] > 0.9:
            print("  → Excellent fairness")
        elif results['overall_jfi'] > 0.8:
            print("  → Good fairness")
        elif results['overall_jfi'] > 0.6:
            print("  → Moderate fairness") 
        else:
            print("  → Poor fairness")
    
    # Create visualization
    create_fairness_plot(results, exp_dir, test_name)
    
    return results

def analyze_wifi_loss_sensitivity(exp_dir, test_name):
    """Analyze WiFi loss-sensitivity experiment"""
    print(f"\n=== Analyzing WiFi Loss-Sensitivity: {test_name} ===")
    
    results = {
        'test_name': test_name,
        'algorithm': 'Prague' if test_name.startswith('P-') else 'Cubic',
        'loss_rate': '0.1%' if 'WLS1' in test_name else '1%',
        'avg_throughput': 0.0,
        'throughput_series': []
    }
    
    # Parse overall throughput
    if results['algorithm'] == 'Prague':
        throughput_file = os.path.join(exp_dir, f"prague-throughput.{test_name}.dat")
    else:
        throughput_file = os.path.join(exp_dir, f"cubic-throughput.{test_name}.dat")
    
    if os.path.exists(throughput_file):
        data = parse_throughput_file(throughput_file)
        if data is not None:
            # Calculate average throughput over simulation
            results['avg_throughput'] = data['throughput'].mean()
            results['throughput_series'] = data['throughput'].tolist()
    
    # Print results
    print(f"Algorithm: {results['algorithm']}")
    print(f"Loss rate: {results['loss_rate']}")
    print(f"Average throughput: {results['avg_throughput']:.2f} Mbps")
    
    # Create visualization
    create_loss_sensitivity_plot(results, exp_dir, test_name)
    
    return results

def create_fairness_plot(results, exp_dir, test_name):
    """Create fairness visualization plot"""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    
    # Plot 1: Individual flow throughputs
    flow_labels = []
    flow_throughputs = []
    flow_colors = []
    
    for i, throughput in enumerate(results['prague_flows']):
        flow_labels.append(f'Prague-{i}')
        flow_throughputs.append(throughput)
        flow_colors.append('blue')
    
    for i, throughput in enumerate(results['cubic_flows']):
        flow_labels.append(f'Cubic-{i}')
        flow_throughputs.append(throughput)
        flow_colors.append('orange')
    
    if flow_throughputs:
        bars = ax1.bar(range(len(flow_throughputs)), flow_throughputs, color=flow_colors, alpha=0.7)
        ax1.set_xlabel('Flow')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.set_title(f'Individual Flow Throughputs - {test_name}')
        ax1.set_xticks(range(len(flow_labels)))
        ax1.set_xticklabels(flow_labels, rotation=45)
        ax1.grid(True, alpha=0.3)
        
        # Add value labels on bars
        for bar, value in zip(bars, flow_throughputs):
            ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.5,
                    f'{value:.1f}', ha='center', va='bottom')
    
    # Plot 2: Fairness indices
    jfi_labels = []
    jfi_values = []
    jfi_colors = []
    
    if results['prague_flows']:
        jfi_labels.append('Prague JFI')
        jfi_values.append(results['prague_jfi'])
        jfi_colors.append('blue')
    
    if results['cubic_flows']:
        jfi_labels.append('Cubic JFI')
        jfi_values.append(results['cubic_jfi'])
        jfi_colors.append('orange')
    
    if len(results['prague_flows']) + len(results['cubic_flows']) > 1:
        jfi_labels.append('Overall JFI')
        jfi_values.append(results['overall_jfi'])
        jfi_colors.append('green')
    
    if jfi_values:
        bars = ax2.bar(range(len(jfi_values)), jfi_values, color=jfi_colors, alpha=0.7)
        ax2.set_xlabel('Fairness Metric')
        ax2.set_ylabel('Jain\'s Fairness Index')
        ax2.set_title(f'Fairness Analysis - {test_name}')
        ax2.set_xticks(range(len(jfi_labels)))
        ax2.set_xticklabels(jfi_labels)
        ax2.set_ylim(0, 1.05)
        ax2.grid(True, alpha=0.3)
        
        # Add value labels on bars
        for bar, value in zip(bars, jfi_values):
            ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.02,
                    f'{value:.3f}', ha='center', va='bottom')
    
    plt.tight_layout()
    plot_file = os.path.join(exp_dir, f'fairness_analysis_{test_name}.png')
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Fairness plot saved: {plot_file}")

def create_loss_sensitivity_plot(results, exp_dir, test_name):
    """Create loss-sensitivity visualization plot"""
    if not results['throughput_series']:
        return
    
    fig, ax = plt.subplots(1, 1, figsize=(10, 6))
    
    # Create time series
    time_points = np.arange(0, len(results['throughput_series'])) * 0.1  # 100ms intervals
    
    color = 'blue' if results['algorithm'] == 'Prague' else 'orange'
    ax.plot(time_points, results['throughput_series'], color=color, alpha=0.7, linewidth=1.5)
    ax.axhline(y=results['avg_throughput'], color='red', linestyle='--', 
               label=f'Average: {results["avg_throughput"]:.2f} Mbps')
    
    ax.set_xlabel('Time (seconds)')
    ax.set_ylabel('Throughput (Mbps)')
    ax.set_title(f'{results["algorithm"]} Loss-Sensitivity ({results["loss_rate"]} loss) - {test_name}')
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    plt.tight_layout()
    plot_file = os.path.join(exp_dir, f'loss_sensitivity_{test_name}.png')
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Loss-sensitivity plot saved: {plot_file}")

def run_combined_analysis(base_dir, test_type):
    """Run combined analysis across all experiments"""
    print(f"\n=== Combined {test_type.upper()} Analysis ===")
    
    if test_type == 'wired':
        # Analyze all wired fairness experiments
        test_patterns = ['P-FC*', 'P-FP*', 'P-FMIX*']
        all_results = []
        
        for pattern in test_patterns:
            exp_dirs = glob.glob(os.path.join(base_dir, pattern))
            for exp_dir in sorted(exp_dirs):
                if os.path.isdir(exp_dir):
                    test_name = os.path.basename(exp_dir)
                    results = analyze_wired_fairness(exp_dir, test_name)
                    all_results.append(results)
        
        # Create combined fairness comparison
        create_combined_fairness_plot(all_results, base_dir)
        
    elif test_type == 'wifi':
        # Analyze all WiFi loss-sensitivity experiments
        test_patterns = ['P-WLS*', 'C-WLS*']
        all_results = []
        
        for pattern in test_patterns:
            exp_dirs = glob.glob(os.path.join(base_dir, pattern))
            for exp_dir in sorted(exp_dirs):
                if os.path.isdir(exp_dir):
                    test_name = os.path.basename(exp_dir)
                    results = analyze_wifi_loss_sensitivity(exp_dir, test_name)
                    all_results.append(results)
        
        # Create combined loss-sensitivity comparison
        create_combined_loss_sensitivity_plot(all_results, base_dir)

def create_combined_fairness_plot(all_results, base_dir):
    """Create combined fairness comparison plot"""
    if not all_results:
        return
    
    fig, ax = plt.subplots(1, 1, figsize=(12, 6))
    
    test_names = [r['test_name'] for r in all_results]
    overall_jfis = [r['overall_jfi'] for r in all_results]
    
    bars = ax.bar(range(len(test_names)), overall_jfis, alpha=0.7, color='green')
    ax.set_xlabel('Test Case')
    ax.set_ylabel('Overall Jain\'s Fairness Index')
    ax.set_title('RQ3 Wired Fairness Comparison')
    ax.set_xticks(range(len(test_names)))
    ax.set_xticklabels(test_names, rotation=45)
    ax.set_ylim(0, 1.05)
    ax.grid(True, alpha=0.3)
    
    # Add value labels
    for bar, value in zip(bars, overall_jfis):
        ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.02,
                f'{value:.3f}', ha='center', va='bottom')
    
    plt.tight_layout()
    plot_file = os.path.join(base_dir, 'combined_fairness_analysis.png')
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Combined fairness plot saved: {plot_file}")

def create_combined_loss_sensitivity_plot(all_results, base_dir):
    """Create combined loss-sensitivity comparison plot"""
    if not all_results:
        return
    
    fig, ax = plt.subplots(1, 1, figsize=(10, 6))
    
    # Group by algorithm and loss rate
    prague_01 = [r for r in all_results if r['algorithm'] == 'Prague' and r['loss_rate'] == '0.1%']
    prague_1 = [r for r in all_results if r['algorithm'] == 'Prague' and r['loss_rate'] == '1%']
    cubic_01 = [r for r in all_results if r['algorithm'] == 'Cubic' and r['loss_rate'] == '0.1%']
    cubic_1 = [r for r in all_results if r['algorithm'] == 'Cubic' and r['loss_rate'] == '1%']
    
    categories = ['Prague 0.1%', 'Prague 1%', 'Cubic 0.1%', 'Cubic 1%']
    throughputs = []
    
    for group in [prague_01, prague_1, cubic_01, cubic_1]:
        if group:
            throughputs.append(group[0]['avg_throughput'])
        else:
            throughputs.append(0)
    
    colors = ['lightblue', 'blue', 'orange', 'darkorange']
    bars = ax.bar(range(len(categories)), throughputs, color=colors, alpha=0.7)
    
    ax.set_xlabel('Algorithm and Loss Rate')
    ax.set_ylabel('Average Throughput (Mbps)')
    ax.set_title('RQ3 WiFi Loss-Sensitivity Comparison')
    ax.set_xticks(range(len(categories)))
    ax.set_xticklabels(categories)
    ax.grid(True, alpha=0.3)
    
    # Add value labels
    for bar, value in zip(bars, throughputs):
        if value > 0:
            ax.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.5,
                    f'{value:.1f}', ha='center', va='bottom')
    
    plt.tight_layout()
    plot_file = os.path.join(base_dir, 'combined_loss_sensitivity_analysis.png')
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"Combined loss-sensitivity plot saved: {plot_file}")

def main():
    parser = argparse.ArgumentParser(description='Analyze RQ3 experiment results')
    parser.add_argument('--exp-dir', required=True, help='Experiment directory')
    parser.add_argument('--test-type', choices=['wired', 'wifi'], required=True, 
                        help='Type of test (wired fairness or wifi loss-sensitivity)')
    parser.add_argument('--combined', action='store_true', 
                        help='Run combined analysis across all experiments')
    
    args = parser.parse_args()
    
    if not os.path.exists(args.exp_dir):
        print(f"Error: Directory {args.exp_dir} does not exist")
        sys.exit(1)
    
    if args.combined:
        run_combined_analysis(args.exp_dir, args.test_type)
    else:
        # Single experiment analysis
        test_name = os.path.basename(os.path.normpath(args.exp_dir))
        
        if args.test_type == 'wired':
            analyze_wired_fairness(args.exp_dir, test_name)
        else:
            analyze_wifi_loss_sensitivity(args.exp_dir, test_name)

if __name__ == '__main__':
    main() 