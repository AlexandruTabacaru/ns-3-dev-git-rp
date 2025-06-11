#!/usr/bin/env python3
"""
RQ3 Analysis for Publication: L4S Fairness and Loss-Sensitivity

Creates publication-ready plots and statistics to answer:
1. Fairness: How fair is L4S (Prague) when sharing bottleneck with classic TCP (Cubic)?
2. Loss-sensitivity: How does performance degrade under packet loss?

Key metrics:
- Jain's Fairness Index
- Prague vs Cubic throughput ratios
- Queue delays (properly scaled)
- Mean throughput by algorithm
- Loss degradation percentages
"""

import argparse
import os
import sys
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path

# Publication-quality plot settings
plt.rcParams.update({
    'font.size': 10,
    'axes.titlesize': 11,
    'axes.labelsize': 10,
    'xtick.labelsize': 9,
    'ytick.labelsize': 9,
    'legend.fontsize': 9,
    'figure.titlesize': 12,
    'lines.linewidth': 1.5,
    'grid.alpha': 0.3,
    'figure.figsize': (12, 8)
})

def load_data_safely(filepath):
    """Load data file with error handling"""
    try:
        if not filepath.exists() or filepath.stat().st_size == 0:
            return None
        data = pd.read_csv(filepath, sep=' ', header=None)
        return data if not data.empty else None
    except:
        return None

def get_experiment_info(exp_name):
    """Extract experiment information from name"""
    if exp_name.startswith('P-FC'):
        num_cubic = exp_name[4:] if len(exp_name) > 4 else "1"
        return 'fairness', f"1 Prague vs {num_cubic} Cubic"
    elif exp_name.startswith('P-FP'):
        num_prague = exp_name[4:] if len(exp_name) > 4 else "2"
        return 'fairness', f"{num_prague} Prague flows"
    elif exp_name.startswith('P-FMIX'):
        return 'fairness', f"Mixed Prague+Cubic"
    elif exp_name.startswith('P-WLS'):
        if 'WLS1' in exp_name:
            return 'loss_sensitivity', "Prague 1% loss"
        elif 'WLS2' in exp_name:
            return 'loss_sensitivity', "Prague 5% loss"
        elif 'WLS3' in exp_name:
            return 'loss_sensitivity', "Prague 10% loss"
    elif exp_name.startswith('C-WLS'):
        if 'WLS1' in exp_name:
            return 'loss_sensitivity', "Cubic 1% loss"
        elif 'WLS2' in exp_name:
            return 'loss_sensitivity', "Cubic 5% loss"
        elif 'WLS3' in exp_name:
            return 'loss_sensitivity', "Cubic 10% loss"
    return 'unknown', exp_name

def calculate_jains_fairness_index(throughputs):
    """Calculate Jain's fairness index"""
    if len(throughputs) == 0:
        return 1.0
    
    throughputs = np.array(throughputs)
    sum_throughput = np.sum(throughputs)
    sum_squared_throughput = np.sum(throughputs**2)
    n = len(throughputs)
    
    if sum_squared_throughput == 0:
        return 1.0
    
    return (sum_throughput ** 2) / (n * sum_squared_throughput)

def analyze_fairness_experiment(exp_dir):
    """Analyze a single fairness experiment"""
    exp_name = exp_dir.name
    exp_type, description = get_experiment_info(exp_name)
    
    print(f"📊 Analyzing: {exp_name} - {description}")

    # Load data files
    prague_tput = load_data_safely(exp_dir / f"prague-throughput.{exp_name}.dat")
    cubic_tput = load_data_safely(exp_dir / f"cubic-throughput.{exp_name}.dat")
    prague_per_flow = load_data_safely(exp_dir / f"prague-per-flow-throughput.{exp_name}.dat")
    cubic_per_flow = load_data_safely(exp_dir / f"cubic-per-flow-throughput.{exp_name}.dat")
    l_sojourn = load_data_safely(exp_dir / f"wired-dualpi2-l-sojourn.{exp_name}.dat")
    c_sojourn = load_data_safely(exp_dir / f"wired-dualpi2-c-sojourn.{exp_name}.dat")

    # Create focused 2x2 analysis plot
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(12, 8))
    fig.suptitle(f'{exp_name}: {description}', fontsize=12, y=0.96)

    # Panel 1: Individual Flow Throughput
    flow_colors = plt.cm.tab10(np.linspace(0, 1, 10))
    flow_idx = 0
    
    if prague_per_flow is not None:
        for port in sorted(prague_per_flow.iloc[:, 1].unique()):
            flow_data = prague_per_flow[prague_per_flow.iloc[:, 1] == port]
            ax1.plot(flow_data.iloc[:, 0], flow_data.iloc[:, 2], 
                    color=flow_colors[flow_idx % 10], linewidth=2.5,
                    label=f'Prague-{int(port)-100}')
            flow_idx += 1
    
    if cubic_per_flow is not None:
        for port in sorted(cubic_per_flow.iloc[:, 1].unique()):
            flow_data = cubic_per_flow[cubic_per_flow.iloc[:, 1] == port]
            ax1.plot(flow_data.iloc[:, 0], flow_data.iloc[:, 2], 
                    color=flow_colors[flow_idx % 10], linestyle='--', linewidth=2.5,
                    label=f'Cubic-{int(port)-200}')
            flow_idx += 1
    
    ax1.axvspan(0, 5, alpha=0.1, color='gray')
    ax1.axvspan(55, 60, alpha=0.1, color='gray')
    ax1.set_title('Individual Flow Throughput', fontweight='bold', fontsize=11)
    ax1.set_ylabel('Throughput (Mbps)')
    ax1.grid(True, alpha=0.3)
    ax1.legend(ncol=2, fontsize=8)
    ax1.set_ylim(bottom=0)

    # Panel 2: Aggregate Throughput
    if prague_tput is not None:
        ax2.plot(prague_tput.iloc[:, 0], prague_tput.iloc[:, 1], 'b-', 
                linewidth=3, label='Prague Total')
    if cubic_tput is not None:
        ax2.plot(cubic_tput.iloc[:, 0], cubic_tput.iloc[:, 1], 'r-', 
                linewidth=3, label='Cubic Total')
    
    ax2.axvspan(0, 5, alpha=0.1, color='gray')
    ax2.axvspan(55, 60, alpha=0.1, color='gray')
    ax2.set_title('Aggregate Throughput', fontweight='bold', fontsize=11)
    ax2.set_ylabel('Throughput (Mbps)')
    ax2.grid(True, alpha=0.3)
    ax2.legend()
    ax2.set_ylim(bottom=0)

    # Panel 3: Queue Delays (dual y-axis for proper scaling)
    ax3_twin = ax3.twinx()
    
    if l_sojourn is not None:
        line1 = ax3.plot(l_sojourn.iloc[:, 0], l_sojourn.iloc[:, 1], 'b-', 
                linewidth=2.5, label='L4S Queue')
        ax3.set_ylabel('L4S Queue Delay (ms)', color='blue')
        ax3.tick_params(axis='y', labelcolor='blue')
    
    if c_sojourn is not None:
        line2 = ax3_twin.plot(c_sojourn.iloc[:, 0], c_sojourn.iloc[:, 1], 'r-', 
                linewidth=2.5, label='Classic Queue')
        ax3_twin.set_ylabel('Classic Queue Delay (ms)', color='red')
        ax3_twin.tick_params(axis='y', labelcolor='red')
    
    ax3.axvspan(0, 5, alpha=0.1, color='gray')
    ax3.axvspan(55, 60, alpha=0.1, color='gray')
    ax3.set_title('Queue Delays (Separate Scales)', fontweight='bold', fontsize=11)
    ax3.set_xlabel('Time (s)')
    ax3.grid(True, alpha=0.3)
    ax3.set_ylim(bottom=0)
    ax3_twin.set_ylim(bottom=0)

    # Panel 4: Flow Fairness Statistics
    if prague_per_flow is not None or cubic_per_flow is not None:
        # Calculate steady-state throughputs
        flow_throughputs = []
        flow_labels = []
        
        if prague_per_flow is not None:
            for port in sorted(prague_per_flow.iloc[:, 1].unique()):
                flow_data = prague_per_flow[
                    (prague_per_flow.iloc[:, 1] == port) &
                    (prague_per_flow.iloc[:, 0] >= 5) & 
                    (prague_per_flow.iloc[:, 0] <= 55)
                ]
                if len(flow_data) > 0:
                    flow_throughputs.append(flow_data.iloc[:, 2].mean())
                    flow_labels.append(f'P{int(port)-100}')
        
        if cubic_per_flow is not None:
            for port in sorted(cubic_per_flow.iloc[:, 1].unique()):
                flow_data = cubic_per_flow[
                    (cubic_per_flow.iloc[:, 1] == port) &
                    (cubic_per_flow.iloc[:, 0] >= 5) & 
                    (cubic_per_flow.iloc[:, 0] <= 55)
                ]
                if len(flow_data) > 0:
                    flow_throughputs.append(flow_data.iloc[:, 2].mean())
                    flow_labels.append(f'C{int(port)-200}')
        
        if flow_throughputs:
            colors = ['lightblue' if 'P' in label else 'lightcoral' for label in flow_labels]
            bars = ax4.bar(range(len(flow_labels)), flow_throughputs, color=colors, alpha=0.8)
            
            # Add value labels
            for bar, value in zip(bars, flow_throughputs):
                ax4.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.5,
                        f'{value:.1f}', ha='center', va='bottom', fontsize=9)
            
            # Add JFI annotation
            jfi = calculate_jains_fairness_index(flow_throughputs)
            ax4.text(0.02, 0.98, f'JFI: {jfi:.3f}', transform=ax4.transAxes,
                    bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8),
                    verticalalignment='top', fontweight='bold')
    
    ax4.set_title('Mean Flow Throughputs', fontweight='bold', fontsize=11)
    ax4.set_xlabel('Flows')
    ax4.set_ylabel('Mean Throughput (Mbps)')
    ax4.set_xticks(range(len(flow_labels)))
    ax4.set_xticklabels(flow_labels)
    ax4.grid(True, alpha=0.3)
    ax4.set_ylim(bottom=0)

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    output_file = exp_dir / f'rq3_fairness_{exp_name}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()

    # Calculate statistics
    stats = calculate_fairness_statistics(exp_name, description, prague_per_flow, 
                                        cubic_per_flow, prague_tput, cubic_tput,
                                        l_sojourn, c_sojourn)
    return stats

def analyze_loss_sensitivity_experiment(exp_dir):
    """Analyze a single loss-sensitivity experiment"""
    exp_name = exp_dir.name
    exp_type, description = get_experiment_info(exp_name)
    algorithm = 'Prague' if exp_name.startswith('P-') else 'Cubic'
    
    print(f"📊 Analyzing: {exp_name} - {description}")

    # Load data
    if algorithm == 'Prague':
        throughput_data = load_data_safely(exp_dir / f"prague-throughput.{exp_name}.dat")
    else:
        throughput_data = load_data_safely(exp_dir / f"cubic-throughput.{exp_name}.dat")

    if throughput_data is None:
        print(f"  ❌ No throughput data found")
        return None

    # Create focused analysis plot
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    fig.suptitle(f'{exp_name}: {description}', fontsize=12, y=0.96)

    # Panel 1: Throughput over time
    color = 'blue' if algorithm == 'Prague' else 'red'
    ax1.plot(throughput_data.iloc[:, 0], throughput_data.iloc[:, 1], 
             color=color, alpha=0.7, linewidth=1.5, label='Raw Throughput')
    
    # Add rolling mean for trend
    if len(throughput_data) > 50:
        rolling_mean = throughput_data.iloc[:, 1].rolling(window=50, center=True).mean()
        ax1.plot(throughput_data.iloc[:, 0], rolling_mean, 
                color=color, linewidth=3, label='Rolling Mean')
    
    ax1.axvspan(0, 5, alpha=0.1, color='gray')
    ax1.axvspan(55, 60, alpha=0.1, color='gray')
    ax1.set_title(f'{algorithm} Throughput Under Loss', fontweight='bold', fontsize=11)
    ax1.set_ylabel('Throughput (Mbps)')
    ax1.grid(True, alpha=0.3)
    ax1.legend()
    ax1.set_ylim(bottom=0)

    # Panel 2: Throughput Statistics
    analysis_data = throughput_data[
        (throughput_data.iloc[:, 0] >= 5) & (throughput_data.iloc[:, 0] <= 55)
    ]
    
    if len(analysis_data) > 0:
        tput_values = analysis_data.iloc[:, 1]
        
        stats_data = {
            'Mean': tput_values.mean(),
            'P95': np.percentile(tput_values, 95),
            'P99': np.percentile(tput_values, 99),
            'Min': tput_values.min(),
            'Max': tput_values.max()
        }
        
        metrics = list(stats_data.keys())
        values = list(stats_data.values())
        
        bars = ax2.bar(metrics, values, color=color, alpha=0.8)
        
        # Add value labels
        for bar, value in zip(bars, values):
            ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.5,
                    f'{value:.1f}', ha='center', va='bottom', fontsize=9)
    
    ax2.set_title('Throughput Statistics', fontweight='bold', fontsize=11)
    ax2.set_xlabel('Metrics')
    ax2.set_ylabel('Throughput (Mbps)')
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(bottom=0)
    
    plt.tight_layout(rect=[0, 0, 1, 0.93])
    output_file = exp_dir / f'rq3_loss_sensitivity_{exp_name}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()

    # Calculate statistics
    stats = calculate_loss_sensitivity_statistics(exp_name, algorithm, description,
                                                throughput_data)
    return stats

def calculate_fairness_statistics(exp_name, description, prague_per_flow, cubic_per_flow,
                                prague_tput, cubic_tput, l_sojourn, c_sojourn):
    """Calculate fairness statistics"""
    stats = {
        'experiment': exp_name,
        'type': 'fairness',
        'description': description
    }
    
    # Calculate steady-state flow throughputs (5-55s)
    flow_throughputs = []
    prague_flows = []
    cubic_flows = []
    
    # Prague flows
    if prague_per_flow is not None:
        analysis_data = prague_per_flow[
            (prague_per_flow.iloc[:, 0] >= 5) & (prague_per_flow.iloc[:, 0] <= 55)
        ]
        for port in sorted(analysis_data.iloc[:, 1].unique()):
            port_data = analysis_data[analysis_data.iloc[:, 1] == port]
            if len(port_data) > 0:
                mean_tput = port_data.iloc[:, 2].mean()
                flow_throughputs.append(mean_tput)
                prague_flows.append(mean_tput)
    
    # Cubic flows
    if cubic_per_flow is not None:
        analysis_data = cubic_per_flow[
            (cubic_per_flow.iloc[:, 0] >= 5) & (cubic_per_flow.iloc[:, 0] <= 55)
        ]
        for port in sorted(analysis_data.iloc[:, 1].unique()):
            port_data = analysis_data[analysis_data.iloc[:, 1] == port]
            if len(port_data) > 0:
                mean_tput = port_data.iloc[:, 2].mean()
                flow_throughputs.append(mean_tput)
                cubic_flows.append(mean_tput)
    
    if flow_throughputs:
        stats['jains_fairness_index'] = calculate_jains_fairness_index(flow_throughputs)
        stats['num_flows'] = len(flow_throughputs)
        
        # Prague vs Cubic comparison
        if prague_flows and cubic_flows:
            prague_avg = np.mean(prague_flows)
            cubic_avg = np.mean(cubic_flows)
            stats['prague_vs_cubic_ratio'] = prague_avg / cubic_avg if cubic_avg > 0 else float('inf')
            stats['prague_mean_tput'] = prague_avg
            stats['cubic_mean_tput'] = cubic_avg
    
    # Queue delay statistics
    if l_sojourn is not None:
        analysis_data = l_sojourn[(l_sojourn.iloc[:, 0] >= 5) & (l_sojourn.iloc[:, 0] <= 55)]
        if len(analysis_data) > 0:
            stats['l4s_mean_delay'] = analysis_data.iloc[:, 1].mean()
            stats['l4s_p95_delay'] = np.percentile(analysis_data.iloc[:, 1], 95)
    
    if c_sojourn is not None:
        analysis_data = c_sojourn[(c_sojourn.iloc[:, 0] >= 5) & (c_sojourn.iloc[:, 0] <= 55)]
        if len(analysis_data) > 0:
            stats['classic_mean_delay'] = analysis_data.iloc[:, 1].mean()
            stats['classic_p95_delay'] = np.percentile(analysis_data.iloc[:, 1], 95)
    
    return stats

def calculate_loss_sensitivity_statistics(exp_name, algorithm, description, throughput_data):
    """Calculate loss-sensitivity statistics"""
    stats = {
        'experiment': exp_name,
        'algorithm': algorithm,
        'type': 'loss_sensitivity',
        'description': description
    }
    
    # Extract loss rate from experiment name
    if 'WLS1' in exp_name:
        stats['loss_rate'] = 0.01
    elif 'WLS2' in exp_name:
        stats['loss_rate'] = 0.05
    elif 'WLS3' in exp_name:
        stats['loss_rate'] = 0.10
    
    # Throughput analysis (5-55s steady state)
    analysis_data = throughput_data[
        (throughput_data.iloc[:, 0] >= 5) & (throughput_data.iloc[:, 0] <= 55)
    ]
    
    if len(analysis_data) > 0:
        tput_values = analysis_data.iloc[:, 1]
        stats['mean_throughput'] = tput_values.mean()
        stats['p95_throughput'] = np.percentile(tput_values, 95)
        stats['p99_throughput'] = np.percentile(tput_values, 99)
        stats['min_throughput'] = tput_values.min()
        stats['max_throughput'] = tput_values.max()
    
    return stats

def create_fairness_jfi_ratio_plot(base_dir, fairness_stats):
    """Create a figure with Jain's Fairness Index and Prague/Cubic Throughput Ratio"""
    if not fairness_stats:
        return
    print("📈 Creating fairness JFI/ratio plot...")
    fairness_stats = sorted(fairness_stats, key=lambda x: x['experiment'])
    exp_names = [s['experiment'] for s in fairness_stats]
    colors = []
    for name in exp_names:
        if 'FP' in name:
            colors.append('#1f77b4')
        elif 'FC' in name:
            colors.append('#ff7f0e')
        else:
            colors.append('#2ca02c')
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 8))
    fig.suptitle('RQ3 Fairness: JFI and Throughput Ratio', fontsize=20, y=0.97)
    # Plot 1: Jain's Fairness Index
    jfi_values = [s.get('jains_fairness_index', 0) for s in fairness_stats]
    bars = ax1.bar(range(len(exp_names)), jfi_values, color=colors, alpha=0.8, edgecolor='black')
    ax1.set_title("Jain's Fairness Index", fontweight='bold', fontsize=17)
    ax1.set_ylabel('Fairness Index', fontsize=15)
    ax1.set_xticks(range(len(exp_names)))
    ax1.set_xticklabels(exp_names, rotation=45, ha='right', fontsize=14)
    ax1.set_ylim(0, 1.05)
    ax1.grid(True, alpha=0.3, axis='y')
    ax1.axhline(y=0.9, color='green', linestyle='--', alpha=0.7, label='Good (≥0.9)')
    ax1.axhline(y=0.8, color='orange', linestyle='--', alpha=0.7, label='Acceptable (≥0.8)')
    ax1.legend(fontsize=14)
    for bar, value in zip(bars, jfi_values):
        ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.02,
                f'{value:.3f}', ha='center', va='bottom', fontsize=15, fontweight='bold')
    # Plot 2: Prague vs Cubic Throughput Ratio
    ratios = []
    ratio_labels = []
    ratio_colors = []
    for s, color in zip(fairness_stats, colors):
        if 'prague_vs_cubic_ratio' in s and s['prague_vs_cubic_ratio'] != float('inf'):
            ratios.append(s['prague_vs_cubic_ratio'])
            ratio_labels.append(s['experiment'])
            ratio_colors.append(color)
    if ratios:
        bars = ax2.bar(range(len(ratio_labels)), ratios, color=ratio_colors, alpha=0.8, edgecolor='black')
        ax2.set_title('Prague/Cubic Throughput Ratio', fontweight='bold', fontsize=17)
        ax2.set_ylabel('Ratio (Prague/Cubic)', fontsize=15)
        ax2.set_xticks(range(len(ratio_labels)))
        ax2.set_xticklabels(ratio_labels, rotation=45, ha='right', fontsize=14)
        ax2.axhline(y=1.0, color='black', linestyle='--', alpha=0.7, label='Equal (1.0)')
        ax2.grid(True, alpha=0.3, axis='y')
        ax2.legend(fontsize=14)
        for bar, value in zip(bars, ratios):
            ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.02,
                    f'{value:.2f}', ha='center', va='bottom', fontsize=15, fontweight='bold')
    plt.tight_layout(rect=[0, 0, 1, 0.93])
    output_file = base_dir / 'rq3_fairness_jfi_ratio.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Saved: {output_file.name}")

def create_fairness_throughput_delay_plot(base_dir, fairness_stats):
    """Create a figure with Mean Throughput by Algorithm and Mean Queue Delays"""
    if not fairness_stats:
        return
    print("📈 Creating fairness throughput/delay plot...")
    fairness_stats = sorted(fairness_stats, key=lambda x: x['experiment'])
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(15, 8))
    fig.suptitle('RQ3 Fairness: Throughput and Queue Delays', fontsize=20, y=0.97)
    # Plot 1: Mean Throughput by Algorithm
    prague_means = []
    cubic_means = []
    mixed_labels = []
    for s in fairness_stats:
        if 'prague_mean_tput' in s and 'cubic_mean_tput' in s:
            prague_means.append(s['prague_mean_tput'])
            cubic_means.append(s['cubic_mean_tput'])
            mixed_labels.append(s['experiment'])
    if prague_means and cubic_means:
        x = np.arange(len(mixed_labels))
        width = 0.35
        bars1 = ax1.bar(x - width/2, prague_means, width, label='Prague', 
                       color='#1f77b4', alpha=0.8, edgecolor='black')
        bars2 = ax1.bar(x + width/2, cubic_means, width, label='Cubic', 
                       color='#d62728', alpha=0.8, edgecolor='black')
        ax1.set_title('Mean Throughput by Algorithm', fontweight='bold', fontsize=17)
        ax1.set_ylabel('Mean Throughput (Mbps)', fontsize=15)
        ax1.set_xticks(x)
        ax1.set_xticklabels(mixed_labels, rotation=45, ha='right', fontsize=14)
        ax1.legend(fontsize=14)
        ax1.grid(True, alpha=0.3, axis='y')
        ax1.set_ylim(bottom=0)
        for bars in [bars1, bars2]:
            for bar in bars:
                height = bar.get_height()
                ax1.text(bar.get_x() + bar.get_width()/2, height + 0.5,
                        f'{height:.1f}', ha='center', va='bottom', fontsize=15)
    # Plot 2: Mean Queue Delays (log scale)
    l4s_delays = []
    classic_delays = []
    delay_labels = []
    for s in fairness_stats:
        if 'l4s_mean_delay' in s and 'classic_mean_delay' in s:
            l4s_delays.append(s['l4s_mean_delay'])
            classic_delays.append(s['classic_mean_delay'])
            delay_labels.append(s['experiment'])
    if l4s_delays and classic_delays:
        x = np.arange(len(delay_labels))
        width = 0.35
        bars1 = ax2.bar(x - width/2, l4s_delays, width, label='L4S Queue', 
                       color='#1f77b4', alpha=0.8, edgecolor='black')
        bars2 = ax2.bar(x + width/2, classic_delays, width, label='Classic Queue', 
                       color='#d62728', alpha=0.8, edgecolor='black')
        ax2.set_title('Mean Queue Delays (Log Scale)', fontweight='bold', fontsize=17)
        ax2.set_ylabel('Mean Delay (ms)', fontsize=15)
        ax2.set_yscale('log')
        ax2.set_xticks(x)
        ax2.set_xticklabels(delay_labels, rotation=45, ha='right', fontsize=14)
        ax2.legend(fontsize=14)
        ax2.grid(True, alpha=0.3, which='both')
        for bars in [bars1, bars2]:
            for bar in bars:
                height = bar.get_height()
                ax2.text(bar.get_x() + bar.get_width()/2, height * 1.1,
                        f'{height:.1f}', ha='center', va='bottom', fontsize=15)
    plt.tight_layout(rect=[0, 0, 1, 0.93])
    output_file = base_dir / 'rq3_fairness_throughput_delay.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Saved: {output_file.name}")

def create_fairness_summary_plot(base_dir, fairness_stats):
    """Create publication-ready fairness summary focusing on key metrics"""
    if not fairness_stats:
        return
        
    print("📈 Creating fairness summary plot...")
    
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(14, 10))
    fig.suptitle('RQ3 Fairness Analysis Summary', fontsize=16, y=0.96)
    
    # Sort experiments logically
    fairness_stats = sorted(fairness_stats, key=lambda x: x['experiment'])
    exp_names = [s['experiment'] for s in fairness_stats]
    
    # Define consistent colors
    colors = []
    for name in exp_names:
        if 'FP' in name:  # Prague only
            colors.append('#1f77b4')  # Blue
        elif 'FC' in name:  # Prague vs Cubic
            colors.append('#ff7f0e')  # Orange
        else:  # Mixed
            colors.append('#2ca02c')  # Green
    
    # Plot 1: Jain's Fairness Index
    jfi_values = [s.get('jains_fairness_index', 0) for s in fairness_stats]
    
    bars = ax1.bar(range(len(exp_names)), jfi_values, color=colors, alpha=0.8, edgecolor='black')
    ax1.set_title('Jain\'s Fairness Index', fontweight='bold', fontsize=12)
    ax1.set_ylabel('Fairness Index')
    ax1.set_xticks(range(len(exp_names)))
    ax1.set_xticklabels(exp_names, rotation=45, ha='right', fontsize=12)
    ax1.set_ylim(0, 1.05)
    ax1.grid(True, alpha=0.3, axis='y')
    ax1.axhline(y=0.9, color='green', linestyle='--', alpha=0.7, label='Good (≥0.9)')
    ax1.axhline(y=0.8, color='orange', linestyle='--', alpha=0.7, label='Acceptable (≥0.8)')
    ax1.legend()
    
    # Add value labels
    for bar, value in zip(bars, jfi_values):
        ax1.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.02,
                f'{value:.3f}', ha='center', va='bottom', fontsize=9, fontweight='bold')
    
    # Plot 2: Prague vs Cubic Throughput Ratio
    ratios = []
    ratio_labels = []
    ratio_colors = []
    
    for s, color in zip(fairness_stats, colors):
        if 'prague_vs_cubic_ratio' in s and s['prague_vs_cubic_ratio'] != float('inf'):
            ratios.append(s['prague_vs_cubic_ratio'])
            ratio_labels.append(s['experiment'])
            ratio_colors.append(color)
    
    if ratios:
        bars = ax2.bar(range(len(ratio_labels)), ratios, color=ratio_colors, alpha=0.8, edgecolor='black')
        ax2.set_title('Prague/Cubic Throughput Ratio', fontweight='bold', fontsize=12)
        ax2.set_ylabel('Ratio (Prague/Cubic)')
        ax2.set_xticks(range(len(ratio_labels)))
        ax2.set_xticklabels(ratio_labels, rotation=45, ha='right', fontsize=12)
        ax2.axhline(y=1.0, color='black', linestyle='--', alpha=0.7, label='Equal (1.0)')
        ax2.grid(True, alpha=0.3, axis='y')
        ax2.legend()
        
        # Add value labels
        for bar, value in zip(bars, ratios):
            ax2.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.02,
                    f'{value:.2f}', ha='center', va='bottom', fontsize=9, fontweight='bold')
    
    # Plot 3: Mean Throughput by Algorithm
    prague_means = []
    cubic_means = []
    mixed_labels = []
    
    for s in fairness_stats:
        if 'prague_mean_tput' in s and 'cubic_mean_tput' in s:
            prague_means.append(s['prague_mean_tput'])
            cubic_means.append(s['cubic_mean_tput'])
            mixed_labels.append(s['experiment'])
    
    if prague_means and cubic_means:
        x = np.arange(len(mixed_labels))
        width = 0.35
        
        bars1 = ax3.bar(x - width/2, prague_means, width, label='Prague', 
                       color='#1f77b4', alpha=0.8, edgecolor='black')
        bars2 = ax3.bar(x + width/2, cubic_means, width, label='Cubic', 
                       color='#d62728', alpha=0.8, edgecolor='black')
        
        ax3.set_title('Mean Throughput by Algorithm', fontweight='bold', fontsize=12)
        ax3.set_ylabel('Mean Throughput (Mbps)')
        ax3.set_xticks(x)
        ax3.set_xticklabels(mixed_labels, rotation=45, ha='right', fontsize=12)
        ax3.legend()
        ax3.grid(True, alpha=0.3, axis='y')
        ax3.set_ylim(bottom=0)
        
        # Add value labels
        for bars in [bars1, bars2]:
            for bar in bars:
                height = bar.get_height()
                ax3.text(bar.get_x() + bar.get_width()/2, height + 0.5,
                        f'{height:.1f}', ha='center', va='bottom', fontsize=8)
    
    # Plot 4: Queue Delays (log scale for visibility)
    l4s_delays = []
    classic_delays = []
    delay_labels = []
    
    for s in fairness_stats:
        if 'l4s_mean_delay' in s and 'classic_mean_delay' in s:
            l4s_delays.append(s['l4s_mean_delay'])
            classic_delays.append(s['classic_mean_delay'])
            delay_labels.append(s['experiment'])
    
    if l4s_delays and classic_delays:
        x = np.arange(len(delay_labels))
        width = 0.35
        
        bars1 = ax4.bar(x - width/2, l4s_delays, width, label='L4S Queue', 
                       color='#1f77b4', alpha=0.8, edgecolor='black')
        bars2 = ax4.bar(x + width/2, classic_delays, width, label='Classic Queue', 
                       color='#d62728', alpha=0.8, edgecolor='black')
        
        ax4.set_title('Mean Queue Delays (Log Scale)', fontweight='bold', fontsize=12)
        ax4.set_ylabel('Mean Delay (ms)')
        ax4.set_yscale('log')
        ax4.set_xticks(x)
        ax4.set_xticklabels(delay_labels, rotation=45, ha='right', fontsize=12)
        ax4.legend()
        ax4.grid(True, alpha=0.3, which='both')
        
        # Add value labels
        for bars in [bars1, bars2]:
            for bar in bars:
                height = bar.get_height()
                ax4.text(bar.get_x() + bar.get_width()/2, height * 1.1,
                        f'{height:.1f}', ha='center', va='bottom', fontsize=8)
    
    plt.tight_layout(rect=[0, 0, 1, 0.93])
    output_file = base_dir / 'rq3_fairness_summary.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Saved: {output_file.name}")

def create_loss_sensitivity_summary_plot(base_dir, loss_stats):
    """Create publication-ready loss-sensitivity summary focusing on key metrics"""
    if not loss_stats:
        return
        
    print("📈 Creating loss-sensitivity summary plot...")
    
    # Separate Prague and Cubic results
    prague_stats = sorted([s for s in loss_stats if s['algorithm'] == 'Prague'], 
                         key=lambda x: x.get('loss_rate', 0))
    cubic_stats = sorted([s for s in loss_stats if s['algorithm'] == 'Cubic'], 
                        key=lambda x: x.get('loss_rate', 0))
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    fig.suptitle('RQ3 Loss-Sensitivity Analysis Summary', fontsize=16, y=0.98)
    
    # Plot 1: Mean Throughput vs Loss Rate
    if prague_stats:
        prague_loss_rates = [s.get('loss_rate', 0) * 100 for s in prague_stats]
        prague_means = [s.get('mean_throughput', 0) for s in prague_stats]
        ax1.plot(prague_loss_rates, prague_means, 'bo-', linewidth=3, 
                label='Prague', markersize=10, markerfacecolor='lightblue', markeredgecolor='blue')
        
        # Add value labels
        for x, y in zip(prague_loss_rates, prague_means):
            ax1.text(x, y + 0.5, f'{y:.1f}', ha='center', va='bottom', fontsize=10, fontweight='bold')
    
    if cubic_stats:
        cubic_loss_rates = [s.get('loss_rate', 0) * 100 for s in cubic_stats]
        cubic_means = [s.get('mean_throughput', 0) for s in cubic_stats]
        ax1.plot(cubic_loss_rates, cubic_means, 'rs-', linewidth=3, 
                label='Cubic', markersize=10, markerfacecolor='lightcoral', markeredgecolor='red')
        
        # Add value labels
        for x, y in zip(cubic_loss_rates, cubic_means):
            ax1.text(x, y + 0.5, f'{y:.1f}', ha='center', va='bottom', fontsize=10, fontweight='bold')
    
    ax1.set_title('Mean Throughput vs Loss Rate', fontweight='bold', fontsize=12)
    ax1.set_xlabel('Packet Loss Rate (%)')
    ax1.set_ylabel('Mean Throughput (Mbps)')
    ax1.legend(fontsize=11)
    ax1.grid(True, alpha=0.3)
    ax1.set_ylim(bottom=0)
    
    # Plot 2: P95 Throughput vs Loss Rate
    if prague_stats:
        prague_p95s = [s.get('p95_throughput', 0) for s in prague_stats]
        ax2.plot(prague_loss_rates, prague_p95s, 'bo-', linewidth=3, 
                label='Prague', markersize=10, markerfacecolor='lightblue', markeredgecolor='blue')
        
        # Add value labels
        for x, y in zip(prague_loss_rates, prague_p95s):
            ax2.text(x, y + 0.5, f'{y:.1f}', ha='center', va='bottom', fontsize=10, fontweight='bold')
    
    if cubic_stats:
        cubic_p95s = [s.get('p95_throughput', 0) for s in cubic_stats]
        ax2.plot(cubic_loss_rates, cubic_p95s, 'rs-', linewidth=3, 
                label='Cubic', markersize=10, markerfacecolor='lightcoral', markeredgecolor='red')
        
        # Add value labels
        for x, y in zip(cubic_loss_rates, cubic_p95s):
            ax2.text(x, y + 0.5, f'{y:.1f}', ha='center', va='bottom', fontsize=10, fontweight='bold')
    
    ax2.set_title('P95 Throughput vs Loss Rate', fontweight='bold', fontsize=12)
    ax2.set_xlabel('Packet Loss Rate (%)')
    ax2.set_ylabel('P95 Throughput (Mbps)')
    ax2.legend(fontsize=11)
    ax2.grid(True, alpha=0.3)
    ax2.set_ylim(bottom=0)
    
    # # Plot 3: Performance Degradation (%)
    # if prague_stats and len(prague_stats) > 1:
    #     baseline_prague = max(s.get('mean_throughput', 0) for s in prague_stats)
    #     prague_degradation = [(baseline_prague - s.get('mean_throughput', 0)) / baseline_prague * 100 
    #                          for s in prague_stats]
    #     ax3.plot(prague_loss_rates, prague_degradation, 'bo-', linewidth=3, 
    #             label='Prague', markersize=10, markerfacecolor='lightblue', markeredgecolor='blue')
        
    #     # Add value labels
    #     for x, y in zip(prague_loss_rates, prague_degradation):
    #         ax3.text(x, y + 0.3, f'{y:.1f}%', ha='center', va='bottom', fontsize=10, fontweight='bold')
    
    # if cubic_stats and len(cubic_stats) > 1:
    #     baseline_cubic = max(s.get('mean_throughput', 0) for s in cubic_stats)
    #     cubic_degradation = [(baseline_cubic - s.get('mean_throughput', 0)) / baseline_cubic * 100 
    #                         for s in cubic_stats]
    #     ax3.plot(cubic_loss_rates, cubic_degradation, 'rs-', linewidth=3, 
    #             label='Cubic', markersize=10, markerfacecolor='lightcoral', markeredgecolor='red')
        
    #     # Add value labels
    #     for x, y in zip(cubic_loss_rates, cubic_degradation):
    #         ax3.text(x, y + 0.3, f'{y:.1f}%', ha='center', va='bottom', fontsize=10, fontweight='bold')
    
    # ax3.set_title('Performance Degradation', fontweight='bold', fontsize=12)
    # ax3.set_xlabel('Packet Loss Rate (%)')
    # ax3.set_ylabel('Throughput Degradation (%)')
    # ax3.legend(fontsize=11)
    # ax3.grid(True, alpha=0.3)
    # ax3.set_ylim(bottom=0)
    
    plt.tight_layout(rect=[0, 0, 1, 0.92])
    output_file = base_dir / 'rq3_loss_sensitivity_summary.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Saved: {output_file.name}")

def save_summary_statistics(base_dir, fairness_stats, loss_stats):
    """Save comprehensive statistics summary"""
    output_file = base_dir / 'rq3_summary_statistics.txt'
    
    with open(output_file, 'w') as f:
        f.write("RQ3 SUMMARY STATISTICS\n")
        f.write("=" * 50 + "\n\n")
        
        f.write("FAIRNESS ANALYSIS RESULTS:\n")
        f.write("-" * 30 + "\n")
        
        if fairness_stats:
            for stats in fairness_stats:
                f.write(f"\n{stats['experiment']}: {stats['description']}\n")
                
                if 'jains_fairness_index' in stats:
                    jfi = stats['jains_fairness_index']
                    f.write(f"  Jain's Fairness Index: {jfi:.4f}")
                    if jfi > 0.9:
                        f.write(" (Excellent)")
                    elif jfi > 0.8:
                        f.write(" (Good)")
                    elif jfi > 0.6:
                        f.write(" (Moderate)")
                    else:
                        f.write(" (Poor)")
                    f.write("\n")
                
                if 'prague_vs_cubic_ratio' in stats:
                    ratio = stats['prague_vs_cubic_ratio']
                    f.write(f"  Prague/Cubic Throughput Ratio: {ratio:.2f}\n")
                
                if 'prague_mean_tput' in stats and 'cubic_mean_tput' in stats:
                    f.write(f"  Prague Mean: {stats['prague_mean_tput']:.2f} Mbps\n")
                    f.write(f"  Cubic Mean: {stats['cubic_mean_tput']:.2f} Mbps\n")
                
                if 'l4s_mean_delay' in stats:
                    f.write(f"  L4S Mean Delay: {stats['l4s_mean_delay']:.2f} ms\n")
                
                if 'classic_mean_delay' in stats:
                    f.write(f"  Classic Mean Delay: {stats['classic_mean_delay']:.2f} ms\n")
        
        f.write("\n\nLOSS-SENSITIVITY ANALYSIS RESULTS:\n")
        f.write("-" * 35 + "\n")
        
        if loss_stats:
            for stats in loss_stats:
                f.write(f"\n{stats['experiment']}: {stats['description']}\n")
                if 'loss_rate' in stats:
                    f.write(f"  Loss Rate: {stats['loss_rate']:.1%}\n")
                if 'mean_throughput' in stats:
                    f.write(f"  Mean Throughput: {stats['mean_throughput']:.2f} Mbps\n")
                if 'p95_throughput' in stats:
                    f.write(f"  P95 Throughput: {stats['p95_throughput']:.2f} Mbps\n")
                if 'p99_throughput' in stats:
                    f.write(f"  P99 Throughput: {stats['p99_throughput']:.2f} Mbps\n")

    print(f"  ✓ Summary statistics saved: {output_file.name}")

def main():
    parser = argparse.ArgumentParser(description='RQ3 Publication Analysis')
    parser.add_argument('--base-dir', default='results/rq3', help='Base directory')
    parser.add_argument('--exp-dir', help='Single experiment directory')
    
    args = parser.parse_args()

    if args.exp_dir:
        # Single experiment
        exp_dir = Path(args.exp_dir)
        if not exp_dir.exists():
            print(f"❌ Error: Directory {exp_dir} does not exist")
            sys.exit(1)
        
        exp_name = exp_dir.name
        exp_type, _ = get_experiment_info(exp_name)
        
        if exp_type == 'fairness':
            analyze_fairness_experiment(exp_dir)
        elif exp_type == 'loss_sensitivity':
            analyze_loss_sensitivity_experiment(exp_dir)
        else:
            print(f"❌ Unknown experiment type: {exp_name}")
    else:
        # Full analysis
        base_dir = Path(args.base_dir)
        if not base_dir.exists():
            print(f"❌ Error: Directory {base_dir} does not exist")
            sys.exit(1)

        print("🚀 === RQ3 PUBLICATION ANALYSIS === 🚀")
        print(f"Base directory: {base_dir}")
        
        # Find experiments
        exp_dirs = [d for d in sorted(base_dir.iterdir()) 
                   if d.is_dir() and not d.name.startswith('.')]
        
        if not exp_dirs:
            print("❌ No experiment directories found!")
            sys.exit(1)
            
        print(f"Found {len(exp_dirs)} experiment directories")
        
        # Analyze experiments
        fairness_stats = []
        loss_stats = []
        
        for exp_dir in exp_dirs:
            exp_name = exp_dir.name
            exp_type, _ = get_experiment_info(exp_name)
            
            if exp_type == 'fairness':
                stats = analyze_fairness_experiment(exp_dir)
                if stats:
                    fairness_stats.append(stats)
            elif exp_type == 'loss_sensitivity':
                stats = analyze_loss_sensitivity_experiment(exp_dir)
                if stats:
                    loss_stats.append(stats)

        print(f"\n✅ Individual analysis complete")
        print(f"  - Fairness experiments: {len(fairness_stats)}")
        print(f"  - Loss-sensitivity experiments: {len(loss_stats)}")
        
        # Create summary plots
        if fairness_stats:
            create_fairness_jfi_ratio_plot(base_dir, fairness_stats)
            create_fairness_throughput_delay_plot(base_dir, fairness_stats)
        if loss_stats:
            create_loss_sensitivity_summary_plot(base_dir, loss_stats)
        
        # Save statistics
        save_summary_statistics(base_dir, fairness_stats, loss_stats)
        
        print(f"\n🎉 === PUBLICATION ANALYSIS COMPLETE === 🎉")
        print(f"📁 Results in: {base_dir}")
        print(f"📊 Individual plots: {len(fairness_stats + loss_stats)}")
        print(f"📈 Summary plots: 2")
        print(f"📋 Statistics: rq3_summary_statistics.txt")

if __name__ == '__main__':
    main() 