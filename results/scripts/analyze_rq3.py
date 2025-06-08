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
        if 'WLS1' in exp_name:
            loss_rate = '1%'
        elif 'WLS2' in exp_name:
            loss_rate = '5%'
        elif 'WLS3' in exp_name:
            loss_rate = '10%'
        else:
            loss_rate = 'unknown%'
        return 'prague', 'wifi', 'loss_sensitivity', f"Prague with {loss_rate} loss"
    elif exp_name.startswith('C-WLS'):
        if 'WLS1' in exp_name:
            loss_rate = '1%'
        elif 'WLS2' in exp_name:
            loss_rate = '5%'
        elif 'WLS3' in exp_name:
            loss_rate = '10%'
        else:
            loss_rate = 'unknown%'
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
    """Analyze a single fairness experiment (statistics only)"""
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
    l_sojourn = load_data_safely(existing_files.get('dualpi2_l_sojourn'), verbose)
    c_sojourn = load_data_safely(existing_files.get('dualpi2_c_sojourn'), verbose)

    # Analysis parameters
    warmup_time = 5.0
    teardown_time = 5.0

    # Calculate fairness statistics
    print("  Computing fairness statistics...")
    stats = calculate_fairness_statistics(exp_name, description, prague_per_flow, cubic_per_flow,
                                        prague_tput, cubic_tput, l_sojourn, c_sojourn,
                                        warmup_time, teardown_time)
    
    if stats:
        save_fairness_statistics(exp_dir, stats)
        print(f"  📊 Key Results:")
        if 'jains_fairness_index' in stats:
            jfi = stats['jains_fairness_index']
            fairness_level = "Excellent" if jfi > 0.9 else "Good" if jfi > 0.8 else "Moderate" if jfi > 0.6 else "Poor"
            print(f"     - Jain's Fairness Index: {jfi:.3f} ({fairness_level})")
        if 'num_flows' in stats:
            print(f"     - Number of flows: {stats['num_flows']}")
        if 'starved_flows' in stats:
            print(f"     - Starved flows: {stats['starved_flows']}")
    
    return stats

def analyze_loss_sensitivity_experiment(exp_dir, verbose=True):
    """Analyze a single loss-sensitivity experiment (statistics only)"""
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
    queue_delay_data = load_data_safely(existing_files.get('queue_delay'), verbose)

    if throughput_data is None:
        print(f"  ❌ No throughput data found for {exp_name}")
        return None

    # Analysis parameters
    warmup_time = 5.0
    teardown_time = 5.0

    # Calculate loss-sensitivity statistics
    print("  Computing loss-sensitivity statistics...")
    stats = calculate_loss_sensitivity_statistics(exp_name, algorithm, description,
                                                throughput_data, queue_delay_data,
                                                warmup_time, teardown_time)
    
    if stats:
        save_loss_sensitivity_statistics(exp_dir, stats)
        print(f"  📊 Key Results:")
        if 'throughput' in stats and stats['throughput']:
            tput = stats['throughput']['mean']
            cv = stats['throughput']['cv']
            stability = "High" if cv < 0.1 else "Moderate" if cv < 0.2 else "Low"
            print(f"     - Average throughput: {tput:.1f} Mbps")
            print(f"     - Stability: {stability} (CV: {cv:.3f})")
        if 'delay_class' in stats:
            print(f"     - Queue delay class: {stats['delay_class']}")
    
    return stats

def calculate_fairness_statistics(exp_name, description, prague_per_flow, cubic_per_flow,
                                prague_tput, cubic_tput, l_sojourn, c_sojourn,
                                warmup_time, teardown_time):
    """Calculate comprehensive fairness statistics"""
    
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
                    final_throughputs.append(avg_tput)
                    flow_labels.append(f'Prague-{int(port)-100}')
        
        # Get final throughputs for Cubic flows
        if cubic_per_flow is not None:
            final_data = cubic_per_flow[
                (cubic_per_flow.iloc[:, 0] >= analysis_end - 10) & 
                (cubic_per_flow.iloc[:, 0] <= analysis_end)
            ]
            for port in final_data.iloc[:, 1].unique():
                port_data = final_data[final_data.iloc[:, 1] == port]
                if len(port_data) > 0:
                    avg_tput = port_data.iloc[:, 2].mean()
                    final_throughputs.append(avg_tput)
                    flow_labels.append(f'Cubic-{int(port)-200}')
        
        if final_throughputs:
            stats['jains_fairness_index'] = calculate_jains_fairness_index(final_throughputs)
            stats['individual_throughputs'] = final_throughputs
            stats['flow_labels'] = flow_labels
            stats['num_flows'] = len(final_throughputs)
            
            # Flow starvation analysis
            avg_tput = np.mean(final_throughputs)
            starvation_threshold = avg_tput * 0.1  # Flows getting < 10% of average
            starved_flows = sum(1 for x in final_throughputs if x < starvation_threshold)
            stats['starved_flows'] = starved_flows
            stats['starvation_rate'] = starved_flows / len(final_throughputs)
            
            # Throughput inequality metrics
            if len(final_throughputs) > 1:
                stats['throughput_std'] = np.std(final_throughputs)
                stats['throughput_cv'] = np.std(final_throughputs) / np.mean(final_throughputs)
                stats['max_min_ratio'] = max(final_throughputs) / max(min(final_throughputs), 0.1)
    
    # Queue utilization analysis
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
            'utilization_score': min(delays.mean() / 50.0, 1.0),  # Normalized to 50ms target
            'queue_name': queue_name
        }
    
    if l_sojourn is not None:
        stats['l4s_queue'] = extract_queue_stats(l_sojourn, 'L4S')
    if c_sojourn is not None:
        stats['classic_queue'] = extract_queue_stats(c_sojourn, 'Classic')
    
    # Queue efficiency comparison
    if 'l4s_queue' in stats and 'classic_queue' in stats:
        l4s_delay = stats['l4s_queue']['mean_delay']
        classic_delay = stats['classic_queue']['mean_delay']
        stats['queue_efficiency'] = {
            'l4s_advantage': (classic_delay - l4s_delay) / classic_delay if classic_delay > 0 else 0,
            'delay_ratio': l4s_delay / classic_delay if classic_delay > 0 else 0
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
    if cubic_tput is not None:
        stats['cubic_throughput'] = extract_throughput_stats(cubic_tput, 'Cubic')
    
    return stats

def calculate_loss_sensitivity_statistics(exp_name, algorithm, description,
                                        throughput_data, queue_delay_data,
                                        warmup_time, teardown_time):
    """Calculate comprehensive loss-sensitivity statistics"""
    
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
            'cv': analysis_data.iloc[:, 1].std() / analysis_data.iloc[:, 1].mean() if analysis_data.iloc[:, 1].mean() > 0 else 0,
            'min': analysis_data.iloc[:, 1].min(),
            'max': analysis_data.iloc[:, 1].max(),
            'p95': np.percentile(analysis_data.iloc[:, 1], 95),
            'p99': np.percentile(analysis_data.iloc[:, 1], 99)
        }
    
    # Throughput analysis
    stats['throughput'] = extract_stats(throughput_data)
    
    if stats['throughput']:
        # Resilience metrics
        tput_values = throughput_data[
            (throughput_data.iloc[:, 0] >= analysis_start) & 
            (throughput_data.iloc[:, 0] <= analysis_end)
        ].iloc[:, 1]
        
        # Calculate stability score (lower CV = more stable)
        stats['stability_score'] = 1.0 / (1.0 + stats['throughput']['cv'])
        
        # Calculate efficiency score (how close to max theoretical)
        theoretical_max = 100.0  # Assume 100 Mbps theoretical max
        stats['efficiency_score'] = stats['throughput']['mean'] / theoretical_max
        
        # Loss resilience score (based on variability under loss)
        if len(tput_values) > 100:
            # Calculate rolling coefficient of variation
            window_size = 50
            cv_values = []
            for i in range(window_size, len(tput_values), 10):
                window_data = tput_values.iloc[i-window_size:i]
                if window_data.mean() > 0:
                    cv_values.append(window_data.std() / window_data.mean())
            
            if cv_values:
                stats['resilience_score'] = 1.0 / (1.0 + np.mean(cv_values))
                stats['worst_variability'] = max(cv_values)
                stats['best_variability'] = min(cv_values)
    
    # Queue delay analysis
    if queue_delay_data is not None:
        stats['queue_delay'] = extract_stats(queue_delay_data)
        
        if stats['queue_delay']:
            # Delay stability under loss
            delay_values = queue_delay_data[
                (queue_delay_data.iloc[:, 0] >= analysis_start) & 
                (queue_delay_data.iloc[:, 0] <= analysis_end)
            ].iloc[:, 1]
            
            # Classify delay performance
            if stats['queue_delay']['mean'] < 5.0:
                stats['delay_class'] = 'Excellent'
            elif stats['queue_delay']['mean'] < 20.0:
                stats['delay_class'] = 'Good'
            elif stats['queue_delay']['mean'] < 50.0:
                stats['delay_class'] = 'Acceptable'
            else:
                stats['delay_class'] = 'Poor'
            
            # Delay spike analysis
            delay_spikes = delay_values[delay_values > stats['queue_delay']['p95']]
            stats['delay_spikes'] = len(delay_spikes)
            stats['spike_rate'] = len(delay_spikes) / len(delay_values) if len(delay_values) > 0 else 0
    
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
            f.write(f"FAIRNESS METRICS:\n")
            f.write(f"  Jain's Fairness Index: {jfi:.4f}\n")
            f.write(f"  Number of flows: {stats.get('num_flows', 'N/A')}\n")
            f.write(f"  Starved flows: {stats.get('starved_flows', 'N/A')}\n")
            f.write(f"  Starvation rate: {stats.get('starvation_rate', 0):.1%}\n")
            
            if 'throughput_cv' in stats:
                f.write(f"  Throughput CV: {stats['throughput_cv']:.3f}\n")
            if 'max_min_ratio' in stats:
                f.write(f"  Max/Min ratio: {stats['max_min_ratio']:.2f}\n")
            
            if jfi > 0.9:
                f.write("  → Excellent fairness\n")
            elif jfi > 0.8:
                f.write("  → Good fairness\n")
            elif jfi > 0.6:
                f.write("  → Moderate fairness\n")
            else:
                f.write("  → Poor fairness\n")
            f.write("\n")
        
        if 'individual_throughputs' in stats and 'flow_labels' in stats:
            f.write("INDIVIDUAL FLOW PERFORMANCE:\n")
            for label, tput in zip(stats['flow_labels'], stats['individual_throughputs']):
                f.write(f"  {label}: {tput:.2f} Mbps\n")
            f.write("\n")
        
        # Queue performance analysis
        if 'l4s_queue' in stats:
            q = stats['l4s_queue']
            f.write(f"L4S QUEUE PERFORMANCE:\n")
            f.write(f"  Mean delay: {q['mean_delay']:.2f} ms\n")
            f.write(f"  95th percentile: {q['p95_delay']:.2f} ms\n")
            f.write(f"  99th percentile: {q['p99_delay']:.2f} ms\n")
            f.write(f"  Utilization score: {q['utilization_score']:.3f}\n\n")
        
        if 'classic_queue' in stats:
            q = stats['classic_queue']
            f.write(f"CLASSIC QUEUE PERFORMANCE:\n")
            f.write(f"  Mean delay: {q['mean_delay']:.2f} ms\n")
            f.write(f"  95th percentile: {q['p95_delay']:.2f} ms\n")
            f.write(f"  99th percentile: {q['p99_delay']:.2f} ms\n")
            f.write(f"  Utilization score: {q['utilization_score']:.3f}\n\n")
        
        if 'queue_efficiency' in stats:
            eff = stats['queue_efficiency']
            f.write(f"QUEUE EFFICIENCY COMPARISON:\n")
            f.write(f"  L4S advantage: {eff['l4s_advantage']:.1%}\n")
            f.write(f"  Delay ratio (L4S/Classic): {eff['delay_ratio']:.3f}\n\n")
        
        # Algorithm-specific throughput stats
        for metric in ['prague_throughput', 'cubic_throughput']:
            if metric in stats and stats[metric]:
                f.write(f"{stats[metric]['algorithm'].upper()} THROUGHPUT:\n")
                s = stats[metric]
                f.write(f"  Mean: {s['mean']:.2f} Mbps\n")
                f.write(f"  Std Dev: {s['std']:.2f} Mbps\n")
                f.write(f"  Coefficient of Variation: {s['cv']:.3f}\n")
                f.write(f"  95th percentile: {s['p95']:.2f} Mbps\n")
                f.write("\n")

def save_loss_sensitivity_statistics(exp_dir, stats):
    """Save loss-sensitivity statistics to file"""
    output_file = exp_dir / f"rq3_loss_sensitivity_stats_{stats['experiment']}.txt"
    
    with open(output_file, 'w') as f:
        f.write(f"RQ3 Loss-Sensitivity Statistics: {stats['experiment']}\n")
        f.write("=" * 50 + "\n\n")
        f.write(f"Algorithm: {stats['algorithm']}\n")
        f.write(f"Description: {stats['description']}\n\n")
        
        if 'throughput' in stats and stats['throughput']:
            s = stats['throughput']
            f.write(f"THROUGHPUT PERFORMANCE:\n")
            f.write(f"  Mean: {s['mean']:.2f} Mbps\n")
            f.write(f"  Std Dev: {s['std']:.2f} Mbps\n")
            f.write(f"  Coefficient of Variation: {s['cv']:.3f}\n")
            f.write(f"  Range: {s['min']:.2f} - {s['max']:.2f} Mbps\n")
            f.write(f"  95th percentile: {s['p95']:.2f} Mbps\n")
            f.write(f"  99th percentile: {s['p99']:.2f} Mbps\n\n")
        
        # Resilience and stability metrics
        if 'stability_score' in stats:
            f.write(f"RESILIENCE METRICS:\n")
            f.write(f"  Stability score: {stats['stability_score']:.3f} (1.0 = perfect)\n")
            f.write(f"  Efficiency score: {stats.get('efficiency_score', 0):.3f}\n")
            
            if 'resilience_score' in stats:
                f.write(f"  Resilience score: {stats['resilience_score']:.3f}\n")
                f.write(f"  Worst variability: {stats.get('worst_variability', 0):.3f}\n")
                f.write(f"  Best variability: {stats.get('best_variability', 0):.3f}\n")
            f.write("\n")
        
        if 'queue_delay' in stats and stats['queue_delay']:
            s = stats['queue_delay']
            f.write(f"QUEUE DELAY PERFORMANCE:\n")
            f.write(f"  Mean: {s['mean']:.3f} ms\n")
            f.write(f"  Std Dev: {s['std']:.3f} ms\n")
            f.write(f"  Coefficient of Variation: {s['cv']:.3f}\n")
            f.write(f"  95th percentile: {s['p95']:.3f} ms\n")
            f.write(f"  99th percentile: {s['p99']:.3f} ms\n")
            f.write(f"  Classification: {stats.get('delay_class', 'N/A')}\n")
            
            if 'delay_spikes' in stats:
                f.write(f"  Delay spikes: {stats['delay_spikes']}\n")
                f.write(f"  Spike rate: {stats.get('spike_rate', 0):.1%}\n")
            f.write("\n")

def create_fairness_comparison_plots(base_dir):
    """Create clean, readable comparison plots for fairness experiments"""
    
    print("\n🔄 Creating fairness comparison plots...")
    
    # Define fairness experiment groups
    fairness_groups = [
        {
            'title': 'Prague vs Single Cubic',
            'experiments': ['P-FC1'],
            'description': 'Single Prague flow vs Single Cubic flow'
        },
        {
            'title': 'Prague vs Multiple Cubic', 
            'experiments': ['P-FC4', 'P-FC8'],
            'description': 'Single Prague flow vs Multiple Cubic flows'
        }
    ]
    
    for group in fairness_groups:
        create_fairness_group_plot(base_dir, group)
    
    # Create overall fairness summary plot
    create_overall_fairness_plot(base_dir)

def create_fairness_group_plot(base_dir, group):
    """Create a clean comparison plot for a group of fairness experiments"""
    
    print(f"  📈 {group['title']}")
    
    # Collect data for all experiments in group
    exp_data = {}
    for exp_name in group['experiments']:
        exp_dir = base_dir / exp_name
        if not exp_dir.exists():
            print(f"    ⚠️  Missing directory: {exp_name}")
            continue
            
        data = load_fairness_experiment_data(exp_dir, exp_name)
        if data:
            exp_data[exp_name] = data
    
    if not exp_data:
        print(f"    ❌ No valid data for {group['title']}")
        return
    
    # Create clean comparison plot
    fig = plt.figure(figsize=(16, 10))
    gs = fig.add_gridspec(3, 2, height_ratios=[2, 2, 1], hspace=0.3, wspace=0.3)
    
    # Main title
    fig.suptitle(f"RQ3 Fairness Analysis: {group['title']}", fontsize=16, fontweight='bold')
    
    # Plot 1: Smoothed Throughput Comparison
    ax1 = fig.add_subplot(gs[0, :])
    plot_smoothed_throughput_comparison(ax1, exp_data)
    
    # Plot 2: Queue Delay Comparison  
    ax2 = fig.add_subplot(gs[1, 0])
    plot_queue_delay_comparison(ax2, exp_data)
    
    # Plot 3: Individual Flow Performance
    ax3 = fig.add_subplot(gs[1, 1]) 
    plot_individual_flow_performance(ax3, exp_data)
    
    # Plot 4: Fairness Metrics Summary
    ax4 = fig.add_subplot(gs[2, :])
    plot_fairness_metrics_summary(ax4, exp_data)
    
    # Save plot
    safe_title = group['title'].replace(' ', '_').replace('/', '_')
    output_file = base_dir / f'rq3_fairness_comparison_{safe_title}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"     ✓ Saved: {output_file.name}")

def load_fairness_experiment_data(exp_dir, exp_name):
    """Load and process data for a fairness experiment"""
    
    # Load throughput data
    prague_tput = load_data_safely(exp_dir / f"prague-throughput.{exp_name}.dat")
    cubic_tput = load_data_safely(exp_dir / f"cubic-throughput.{exp_name}.dat")
    
    # Load per-flow data
    prague_per_flow = load_data_safely(exp_dir / f"prague-per-flow-throughput.{exp_name}.dat")
    cubic_per_flow = load_data_safely(exp_dir / f"cubic-per-flow-throughput.{exp_name}.dat")
    
    # Load queue data
    l4s_sojourn = load_data_safely(exp_dir / f"wired-dualpi2-l-sojourn.{exp_name}.dat")
    classic_sojourn = load_data_safely(exp_dir / f"wired-dualpi2-c-sojourn.{exp_name}.dat")
    
    if not (prague_tput is not None or cubic_tput is not None):
        return None
    
    # Calculate fairness statistics
    stats = calculate_fairness_statistics(exp_name, f"Fairness test {exp_name}", 
                                        prague_per_flow, cubic_per_flow,
                                        prague_tput, cubic_tput, 
                                        l4s_sojourn, classic_sojourn,
                                        5.0, 5.0)
    
    return {
        'name': exp_name,
        'prague_tput': prague_tput,
        'cubic_tput': cubic_tput,
        'prague_per_flow': prague_per_flow,
        'cubic_per_flow': cubic_per_flow,
        'l4s_sojourn': l4s_sojourn,
        'classic_sojourn': classic_sojourn,
        'stats': stats
    }

def plot_smoothed_throughput_comparison(ax, exp_data):
    """Plot smoothed throughput comparison instead of messy raw data"""
    
    ax.set_title('Throughput Comparison (10s Moving Average)', fontweight='bold', fontsize=12)
    
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
    color_idx = 0
    
    for exp_name, data in exp_data.items():
        # Extract number of flows from experiment name
        if 'FC1' in exp_name:
            flow_desc = '1v1 (Prague vs 1 Cubic)'
        elif 'FC4' in exp_name:
            flow_desc = '1v4 (Prague vs 4 Cubic)'  
        elif 'FC8' in exp_name:
            flow_desc = '1v8 (Prague vs 8 Cubic)'
        else:
            flow_desc = exp_name
        
        # Plot Prague throughput (smoothed)
        if data['prague_tput'] is not None:
            prague_smooth = smooth_data(data['prague_tput'], window=50)
            ax.plot(prague_smooth.iloc[:, 0], prague_smooth.iloc[:, 1], 
                   color=colors[color_idx], linestyle='-', linewidth=3, alpha=0.9,
                   label=f'Prague ({flow_desc})')
        
        # Plot Cubic throughput (smoothed)  
        if data['cubic_tput'] is not None:
            cubic_smooth = smooth_data(data['cubic_tput'], window=50)
            ax.plot(cubic_smooth.iloc[:, 0], cubic_smooth.iloc[:, 1],
                   color=colors[color_idx], linestyle='--', linewidth=3, alpha=0.9,
                   label=f'Cubic ({flow_desc})')
        
        color_idx = (color_idx + 1) % len(colors)
    
    # Add excluded periods
    ax.axvspan(0, 5, alpha=0.15, color='gray', label='Warmup/Teardown')
    ax.axvspan(55, 60, alpha=0.15, color='gray')
    
    ax.set_xlabel('Time (s)')
    ax.set_ylabel('Throughput (Mbps)')
    ax.grid(True, alpha=0.3)
    ax.legend(fontsize=10)
    ax.set_ylim(bottom=0)

def plot_queue_delay_comparison(ax, exp_data):
    """Plot queue delay comparison using box plots"""
    
    ax.set_title('Queue Delay Distribution', fontweight='bold', fontsize=12)
    
    # Collect delay data for analysis period (5-55s)
    l4s_delays = []
    classic_delays = []
    labels = []
    
    for exp_name, data in exp_data.items():
        # L4S queue delays
        if data['l4s_sojourn'] is not None:
            analysis_data = data['l4s_sojourn'][
                (data['l4s_sojourn'].iloc[:, 0] >= 5) & 
                (data['l4s_sojourn'].iloc[:, 0] <= 55)
            ]
            if len(analysis_data) > 0:
                l4s_delays.append(analysis_data.iloc[:, 1].values)
                labels.append(f'L4S ({exp_name})')
        
        # Classic queue delays
        if data['classic_sojourn'] is not None:
            analysis_data = data['classic_sojourn'][
                (data['classic_sojourn'].iloc[:, 0] >= 5) & 
                (data['classic_sojourn'].iloc[:, 0] <= 55)
            ]
            if len(analysis_data) > 0:
                classic_delays.append(analysis_data.iloc[:, 1].values)
                labels.append(f'Classic ({exp_name})')
    
    if l4s_delays or classic_delays:
        all_delays = l4s_delays + classic_delays
        all_labels = [f'L4S ({exp})' for exp in exp_data.keys() if exp_data[exp]['l4s_sojourn'] is not None] + \
                    [f'Classic ({exp})' for exp in exp_data.keys() if exp_data[exp]['classic_sojourn'] is not None]
        
        # Create box plot
        bp = ax.boxplot(all_delays, labels=all_labels, patch_artist=True)
        
        # Color L4S boxes blue, Classic boxes red
        for i, patch in enumerate(bp['boxes']):
            if 'L4S' in all_labels[i]:
                patch.set_facecolor('#1f77b4')
                patch.set_alpha(0.7)
            else:
                patch.set_facecolor('#d62728') 
                patch.set_alpha(0.7)
    
    ax.set_ylabel('Queue Delay (ms)')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3)
    plt.setp(ax.get_xticklabels(), rotation=45, ha='right')

def plot_individual_flow_performance(ax, exp_data):
    """Plot individual flow throughput performance"""
    
    ax.set_title('Final Flow Throughput Distribution', fontweight='bold', fontsize=12)
    
    # Collect final throughput for each flow
    all_throughputs = []
    all_labels = []
    colors = []
    
    for exp_name, data in exp_data.items():
        if data['stats'] and 'individual_throughputs' in data['stats']:
            throughputs = data['stats']['individual_throughputs']
            flow_labels = data['stats']['flow_labels']
            
            for tput, flow_label in zip(throughputs, flow_labels):
                all_throughputs.append(tput)
                all_labels.append(f"{flow_label}\n({exp_name})")
                if 'Prague' in flow_label:
                    colors.append('#1f77b4')  # Blue for Prague
                else:
                    colors.append('#d62728')  # Red for Cubic
    
    if all_throughputs:
        bars = ax.bar(range(len(all_throughputs)), all_throughputs, color=colors, alpha=0.8)
        
        # Add value labels on bars
        for bar, tput in zip(bars, all_throughputs):
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height + 1,
                   f'{tput:.1f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
        
        ax.set_xticks(range(len(all_labels)))
        ax.set_xticklabels(all_labels, fontsize=9)
        ax.set_ylabel('Throughput (Mbps)')
        ax.grid(True, alpha=0.3)
        
        # Add legend
        from matplotlib.patches import Patch
        legend_elements = [Patch(facecolor='#1f77b4', alpha=0.8, label='Prague'),
                          Patch(facecolor='#d62728', alpha=0.8, label='Cubic')]
        ax.legend(handles=legend_elements, loc='upper right')
    
    plt.setp(ax.get_xticklabels(), rotation=45, ha='right')

def plot_fairness_metrics_summary(ax, exp_data):
    """Plot fairness metrics summary as horizontal bar chart"""
    
    ax.set_title('Fairness Metrics Summary', fontweight='bold', fontsize=12)
    
    exp_names = []
    jfi_values = []
    colors = []
    
    for exp_name, data in exp_data.items():
        if data['stats'] and 'jains_fairness_index' in data['stats']:
            exp_names.append(exp_name)
            jfi = data['stats']['jains_fairness_index']
            jfi_values.append(jfi)
            
            # Color based on fairness level
            if jfi > 0.9:
                colors.append('#2ecc71')  # Green - Excellent
            elif jfi > 0.8:
                colors.append('#f39c12')  # Orange - Good
            elif jfi > 0.6:
                colors.append('#e74c3c')  # Red - Moderate
            else:
                colors.append('#8e44ad')  # Purple - Poor
    
    if jfi_values:
        bars = ax.barh(range(len(exp_names)), jfi_values, color=colors, alpha=0.8)
        
        # Add value labels on bars
        for bar, jfi in zip(bars, jfi_values):
            width = bar.get_width()
            ax.text(width + 0.01, bar.get_y() + bar.get_height()/2.,
                   f'{jfi:.3f}', ha='left', va='center', fontweight='bold')
        
        ax.set_yticks(range(len(exp_names)))
        ax.set_yticklabels(exp_names)
        ax.set_xlabel('Jain\'s Fairness Index')
        ax.set_xlim(0, 1.05)
        
        # Add reference lines
        ax.axvline(x=0.9, color='green', linestyle='--', alpha=0.7, label='Excellent (>0.9)')
        ax.axvline(x=0.8, color='orange', linestyle='--', alpha=0.7, label='Good (>0.8)')
        ax.axvline(x=0.6, color='red', linestyle='--', alpha=0.7, label='Moderate (>0.6)')
        
        ax.grid(True, alpha=0.3)
        ax.legend(loc='lower right', fontsize=9)

def create_overall_fairness_plot(base_dir):
    """Create an overall fairness comparison across all experiments"""
    
    print("  📊 Creating overall fairness summary plot...")
    
    # Find all fairness experiments
    fairness_experiments = []
    for exp_pattern in ['P-FC*', 'P-FP*', 'P-FMIX*']:
        fairness_experiments.extend(glob.glob(str(base_dir / exp_pattern)))
    
    if not fairness_experiments:
        print("    ⚠️  No fairness experiments found")
        return
    
    # Load data for all experiments
    all_data = {}
    for exp_path in sorted(fairness_experiments):
        exp_dir = Path(exp_path)
        exp_name = exp_dir.name
        if exp_dir.exists():
            data = load_fairness_experiment_data(exp_dir, exp_name)
            if data and data['stats']:
                all_data[exp_name] = data
    
    if not all_data:
        print("    ❌ No valid fairness data found")
        return
    
    # Create comprehensive fairness overview
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle('RQ3 Overall Fairness Analysis Summary', fontsize=16, fontweight='bold')
    
    # Plot 1: Jain's Fairness Index comparison
    exp_names = list(all_data.keys())
    jfi_values = [all_data[exp]['stats'].get('jains_fairness_index', 0) for exp in exp_names]
    colors = ['#2ecc71' if jfi > 0.9 else '#f39c12' if jfi > 0.8 else '#e74c3c' for jfi in jfi_values]
    
    bars1 = ax1.bar(range(len(exp_names)), jfi_values, color=colors, alpha=0.8)
    ax1.set_title('Jain\'s Fairness Index by Experiment', fontweight='bold')
    ax1.set_xticks(range(len(exp_names)))
    ax1.set_xticklabels(exp_names, rotation=45, ha='right')
    ax1.set_ylabel('Jain\'s Fairness Index')
    ax1.set_ylim(0, 1.05)
    ax1.grid(True, alpha=0.3)
    
    # Add value labels
    for bar, jfi in zip(bars1, jfi_values):
        height = bar.get_height()
        ax1.text(bar.get_x() + bar.get_width()/2., height + 0.02,
                f'{jfi:.3f}', ha='center', va='bottom', fontweight='bold', fontsize=9)
    
    # Reference lines
    ax1.axhline(y=0.9, color='green', linestyle='--', alpha=0.7)
    ax1.axhline(y=0.8, color='orange', linestyle='--', alpha=0.7)
    
    # Plot 2: Flow count vs Fairness
    flow_counts = [all_data[exp]['stats'].get('num_flows', 0) for exp in exp_names]
    ax2.scatter(flow_counts, jfi_values, c=colors, s=100, alpha=0.8)
    ax2.set_title('Flow Count vs Fairness', fontweight='bold')
    ax2.set_xlabel('Number of Flows')
    ax2.set_ylabel('Jain\'s Fairness Index')
    ax2.grid(True, alpha=0.3)
    
    # Add experiment labels
    for i, exp in enumerate(exp_names):
        ax2.annotate(exp, (flow_counts[i], jfi_values[i]), 
                    xytext=(5, 5), textcoords='offset points', fontsize=8)
    
    # Plot 3: Throughput distribution - FIX LABEL SYNC ISSUE
    all_tputs = []
    exp_labels = []
    tput_colors = []
    
    for exp_name in exp_names:
        stats = all_data[exp_name]['stats']
        # Add Prague throughput and label together
        if 'prague_throughput' in stats and stats['prague_throughput']:
            all_tputs.append(stats['prague_throughput']['mean'])
            exp_labels.append(f'Prague\n({exp_name})')
            tput_colors.append('#1f77b4')  # Blue for Prague
        # Add Cubic throughput and label together
        if 'cubic_throughput' in stats and stats['cubic_throughput']:
            all_tputs.append(stats['cubic_throughput']['mean'])
            exp_labels.append(f'Cubic\n({exp_name})')
            tput_colors.append('#d62728')  # Red for Cubic
    
    if all_tputs:
        bars3 = ax3.bar(range(len(all_tputs)), all_tputs, color=tput_colors, alpha=0.8)
        ax3.set_title('Algorithm Throughput Performance', fontweight='bold')
        ax3.set_xticks(range(len(exp_labels)))
        ax3.set_xticklabels(exp_labels, rotation=45, ha='right', fontsize=9)
        ax3.set_ylabel('Throughput (Mbps)')
        ax3.grid(True, alpha=0.3)
        
        # Add legend
        from matplotlib.patches import Patch
        legend_elements = [Patch(facecolor='#1f77b4', alpha=0.8, label='Prague'),
                          Patch(facecolor='#d62728', alpha=0.8, label='Cubic')]
        ax3.legend(handles=legend_elements)
    
    # Plot 4: Queue delay comparison - FIX LABEL SYNC ISSUE
    all_delays = []
    delay_labels = []
    delay_colors = []
    
    for exp_name in exp_names:
        stats = all_data[exp_name]['stats']
        # Add L4S delay and label together
        if 'l4s_queue' in stats and stats['l4s_queue']:
            all_delays.append(stats['l4s_queue']['mean_delay'])
            delay_labels.append(f'L4S\n({exp_name})')
            delay_colors.append('#1f77b4')  # Blue for L4S
        # Add Classic delay and label together
        if 'classic_queue' in stats and stats['classic_queue']:
            all_delays.append(stats['classic_queue']['mean_delay'])
            delay_labels.append(f'Classic\n({exp_name})')
            delay_colors.append('#d62728')  # Red for Classic
    
    if all_delays:
        bars4 = ax4.bar(range(len(all_delays)), all_delays, color=delay_colors, alpha=0.8)
        ax4.set_title('Queue Delay Performance', fontweight='bold')
        ax4.set_xticks(range(len(delay_labels)))
        ax4.set_xticklabels(delay_labels, rotation=45, ha='right', fontsize=9)
        ax4.set_ylabel('Mean Queue Delay (ms)')
        ax4.set_yscale('log')
        ax4.grid(True, alpha=0.3)
        
        # Add legend
        legend_elements = [Patch(facecolor='#1f77b4', alpha=0.8, label='L4S Queue'),
                          Patch(facecolor='#d62728', alpha=0.8, label='Classic Queue')]
        ax4.legend(handles=legend_elements)
    
    plt.tight_layout(rect=[0, 0, 1, 0.96])
    
    output_file = base_dir / 'rq3_overall_fairness_analysis.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"     ✓ Saved: {output_file.name}")

def smooth_data(data, window=50):
    """Apply moving average smoothing to data"""
    if data is None or len(data) < window:
        return data
    
    smoothed = data.copy()
    smoothed.iloc[:, 1] = data.iloc[:, 1].rolling(window=window, center=True, min_periods=1).mean()
    return smoothed

def create_loss_sensitivity_comparison_plots(base_dir):
    """Create comparison plots for loss-sensitivity experiments (throughput only)"""
    
    print("\n🔄 Creating loss-sensitivity comparison plots...")
    
    # Define comparisons with correct loss rates
    comparisons = [
        ("1% Loss Rate", "P-WLS1", "C-WLS1"),
        ("5% Loss Rate", "P-WLS2", "C-WLS2"), 
        ("10% Loss Rate", "P-WLS3", "C-WLS3")
    ]
    
    for title, prague_exp, cubic_exp in comparisons:
        prague_dir = base_dir / prague_exp
        cubic_dir = base_dir / cubic_exp
        
        if not (prague_dir.exists() and cubic_dir.exists()):
            print(f"  ⚠️  Skipping {title}: Missing directories")
            continue
            
        print(f"  📈 {title}")
        
        # Load throughput data only
        prague_tput = load_data_safely(prague_dir / f"prague-throughput.{prague_exp}.dat")
        cubic_tput = load_data_safely(cubic_dir / f"cubic-throughput.{cubic_exp}.dat")
        
        # Create single throughput comparison plot
        fig, ax1 = plt.subplots(1, 1, figsize=(14, 6))
        fig.suptitle(f'RQ3 Loss-Sensitivity Comparison: {title}', fontsize=16, y=0.95)
        
        # Consistent color scheme: Prague=Blue, Cubic=Red
        prague_color = '#1f77b4'  # Blue
        cubic_color = '#d62728'   # Red
        
        # Throughput comparison
        if prague_tput is not None:
            ax1.plot(prague_tput.iloc[:, 0], prague_tput.iloc[:, 1], 
                    color=prague_color, linestyle='-', linewidth=3, alpha=0.9,
                    label='Prague')
        if cubic_tput is not None:
            ax1.plot(cubic_tput.iloc[:, 0], cubic_tput.iloc[:, 1], 
                    color=cubic_color, linestyle='-', linewidth=3, alpha=0.9,
                    label='Cubic')
        
        ax1.axvspan(0, 5, alpha=0.15, color='gray', label='Excluded periods')
        ax1.axvspan(55, 60, alpha=0.15, color='gray')
        ax1.set_title('Throughput Under Loss', fontweight='bold', fontsize=12)
        ax1.set_xlabel('Time (s)')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.grid(True, alpha=0.3)
        ax1.legend(fontsize=11)
        ax1.set_ylim(bottom=0)
        
        plt.tight_layout(rect=[0, 0, 1, 0.92])
        
        safe_title = title.replace('%', 'pct').replace(' ', '_')
        output_file = base_dir / f'rq3_loss_sensitivity_comparison_{safe_title}.png'
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"     ✓ Saved: {output_file.name}")

def create_fairness_summary_table(base_dir, fairness_stats):
    """Create clean fairness summary table instead of messy plots"""
    
    print("\n📊 Creating fairness summary table...")
    
    if not fairness_stats:
        print("  ⚠️  No fairness data to summarize")
        return
    
    # Create summary table
    summary_data = []
    for stats in fairness_stats:
        exp = stats['experiment']
        desc = stats.get('description', '')
        
        # Extract key metrics
        jfi = stats.get('jains_fairness_index', 0)
        num_flows = stats.get('num_flows', 0)
        starved = stats.get('starved_flows', 0)
        
        # Throughput metrics
        prague_tput = stats.get('prague_throughput', {}).get('mean', 0) if 'prague_throughput' in stats else 0
        cubic_tput = stats.get('cubic_throughput', {}).get('mean', 0) if 'cubic_throughput' in stats else 0
        
        # Queue delays
        l4s_delay = stats.get('l4s_queue', {}).get('mean_delay', 0) if 'l4s_queue' in stats else 0
        classic_delay = stats.get('classic_queue', {}).get('mean_delay', 0) if 'classic_queue' in stats else 0
        
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
            'Jains_Index': f"{jfi:.3f}",
            'Fairness': fairness_class,
            'Flows': num_flows,
            'Starved': starved,
            'Prague_Mbps': f"{prague_tput:.1f}" if prague_tput > 0 else "N/A",
            'Cubic_Mbps': f"{cubic_tput:.1f}" if cubic_tput > 0 else "N/A",
            'L4S_Delay_ms': f"{l4s_delay:.1f}" if l4s_delay > 0 else "N/A",
            'Classic_Delay_ms': f"{classic_delay:.1f}" if classic_delay > 0 else "N/A"
        })
    
    # Save as clean CSV
    output_file = base_dir / 'rq3_fairness_summary.csv'
    df = pd.DataFrame(summary_data)
    df.to_csv(output_file, index=False)
    
    # Print formatted table
    print(f"\n{'='*80}")
    print(f"{'RQ3 FAIRNESS ANALYSIS SUMMARY':^80}")
    print(f"{'='*80}")
    
    for row in summary_data:
        print(f"\n🎯 {row['Experiment']:12} | {row['Description'][:40]:40}")
        print(f"   Fairness: {row['Fairness']:10} (JFI: {row['Jains_Index']})")
        print(f"   Flows: {row['Flows']:2} total, {row['Starved']:2} starved")
        print(f"   Throughput: Prague {row['Prague_Mbps']:>6} Mbps, Cubic {row['Cubic_Mbps']:>6} Mbps")
        print(f"   Queue Delay: L4S {row['L4S_Delay_ms']:>6} ms, Classic {row['Classic_Delay_ms']:>6} ms")
    
    print(f"\n✅ Fairness summary saved: {output_file.name}")
    
    # Create simple bar chart for JFI comparison
    fig, ax = plt.subplots(figsize=(12, 6))
    
    experiments = [row['Experiment'] for row in summary_data]
    jfi_values = [float(row['Jains_Index']) for row in summary_data]
    colors = ['#2ecc71' if jfi > 0.9 else '#f39c12' if jfi > 0.8 else '#e74c3c' for jfi in jfi_values]
    
    bars = ax.bar(experiments, jfi_values, color=colors, alpha=0.8)
    
    # Add value labels on bars
    for bar, jfi in zip(bars, jfi_values):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height + 0.01,
                f'{jfi:.3f}', ha='center', va='bottom', fontweight='bold')
    
    ax.axhline(y=0.9, color='green', linestyle='--', alpha=0.7, label='Excellent (>0.9)')
    ax.axhline(y=0.8, color='orange', linestyle='--', alpha=0.7, label='Good (>0.8)')
    ax.axhline(y=0.6, color='red', linestyle='--', alpha=0.7, label='Moderate (>0.6)')
    
    ax.set_title("RQ3 Fairness: Jain's Fairness Index by Experiment", fontsize=14, fontweight='bold')
    ax.set_ylabel("Jain's Fairness Index")
    ax.set_ylim(0, 1.05)
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    plt.xticks(rotation=45, ha='right')
    plt.tight_layout()
    
    plot_file = base_dir / 'rq3_fairness_jfi_comparison.png'
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✅ JFI comparison chart saved: {plot_file.name}")

def create_loss_sensitivity_summary_table(base_dir, loss_stats):
    """Create clean loss-sensitivity summary table instead of messy plots"""
    
    print("\n📊 Creating loss-sensitivity summary table...")
    
    if not loss_stats:
        print("  ⚠️  No loss-sensitivity data to summarize")
        return
    
    # Create summary table
    summary_data = []
    for stats in loss_stats:
        exp = stats['experiment']
        algo = stats.get('algorithm', 'unknown')
        desc = stats.get('description', '')
        
        # Extract loss rate
        if 'WLS1' in exp:
            loss_rate = '1%'
        elif 'WLS2' in exp:
            loss_rate = '5%'
        elif 'WLS3' in exp:
            loss_rate = '10%'
        else:
            loss_rate = 'N/A'
        
        # Throughput metrics
        tput_stats = stats.get('throughput', {})
        if tput_stats:
            mean_tput = tput_stats.get('mean', 0)
            cv = tput_stats.get('cv', 0)
            p95_tput = tput_stats.get('p95', 0)
        else:
            mean_tput = cv = p95_tput = 0
        
        # Stability classification
        if cv < 0.1:
            stability = "High"
        elif cv < 0.2:
            stability = "Moderate"
        else:
            stability = "Low"
        
        # Delay metrics
        delay_stats = stats.get('queue_delay', {})
        if delay_stats:
            mean_delay = delay_stats.get('mean', 0)
            delay_class = stats.get('delay_class', 'N/A')
        else:
            mean_delay = 0
            delay_class = 'N/A'
        
        summary_data.append({
            'Algorithm': algo.title(),
            'Loss_Rate': loss_rate,
            'Experiment': exp,
            'Mean_Throughput_Mbps': f"{mean_tput:.1f}",
            'P95_Throughput_Mbps': f"{p95_tput:.1f}",
            'Stability': stability,
            'Coefficient_Variation': f"{cv:.3f}",
            'Mean_Delay_ms': f"{mean_delay:.2f}",
            'Delay_Class': delay_class
        })
    
    # Save as clean CSV
    output_file = base_dir / 'rq3_loss_sensitivity_summary.csv'
    df = pd.DataFrame(summary_data)
    df.to_csv(output_file, index=False)
    
    # Print formatted table grouped by loss rate
    print(f"\n{'='*90}")
    print(f"{'RQ3 LOSS-SENSITIVITY ANALYSIS SUMMARY':^90}")
    print(f"{'='*90}")
    
    # Group by loss rate
    by_loss_rate = {}
    for row in summary_data:
        loss_rate = row['Loss_Rate']
        if loss_rate not in by_loss_rate:
            by_loss_rate[loss_rate] = []
        by_loss_rate[loss_rate].append(row)
    
    for loss_rate in sorted(by_loss_rate.keys()):
        print(f"\n🎯 LOSS RATE: {loss_rate}")
        print("-" * 60)
        
        for row in by_loss_rate[loss_rate]:
            algo = row['Algorithm']
            mean_tput = row['Mean_Throughput_Mbps']
            stability = row['Stability']
            cv = row['Coefficient_Variation']
            delay_class = row['Delay_Class']
            
            print(f"  {algo:8} | {mean_tput:>6} Mbps (P95: {row['P95_Throughput_Mbps']:>6}) | {stability:8} (CV: {cv})")
            print(f"           | Delay: {row['Mean_Delay_ms']:>6} ms ({delay_class})")
    
    print(f"\n✅ Loss-sensitivity summary saved: {output_file.name}")
    
    # Create throughput comparison bar chart
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Group data for plotting
    prague_data = [row for row in summary_data if row['Algorithm'] == 'Prague']
    cubic_data = [row for row in summary_data if row['Algorithm'] == 'Cubic']
    
    # Throughput comparison
    loss_rates = ['1%', '5%', '10%']
    prague_tputs = []
    cubic_tputs = []
    
    for rate in loss_rates:
        prague_row = next((r for r in prague_data if r['Loss_Rate'] == rate), None)
        cubic_row = next((r for r in cubic_data if r['Loss_Rate'] == rate), None)
        
        prague_tputs.append(float(prague_row['Mean_Throughput_Mbps']) if prague_row else 0)
        cubic_tputs.append(float(cubic_row['Mean_Throughput_Mbps']) if cubic_row else 0)
    
    x = np.arange(len(loss_rates))
    width = 0.35
    
    bars1 = ax1.bar(x - width/2, prague_tputs, width, label='Prague', color='#1f77b4', alpha=0.8)
    bars2 = ax1.bar(x + width/2, cubic_tputs, width, label='Cubic', color='#d62728', alpha=0.8)
    
    # Add value labels
    for bars in [bars1, bars2]:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                ax1.text(bar.get_x() + bar.get_width()/2., height + 1,
                        f'{height:.1f}', ha='center', va='bottom', fontweight='bold')
    
    ax1.set_title('Mean Throughput by Loss Rate', fontweight='bold')
    ax1.set_ylabel('Throughput (Mbps)')
    ax1.set_xlabel('Packet Loss Rate')
    ax1.set_xticks(x)
    ax1.set_xticklabels(loss_rates)
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # Stability comparison (CV values)
    prague_cvs = []
    cubic_cvs = []
    
    for rate in loss_rates:
        prague_row = next((r for r in prague_data if r['Loss_Rate'] == rate), None)
        cubic_row = next((r for r in cubic_data if r['Loss_Rate'] == rate), None)
        
        prague_cvs.append(float(prague_row['Coefficient_Variation']) if prague_row else 0)
        cubic_cvs.append(float(cubic_row['Coefficient_Variation']) if cubic_row else 0)
    
    bars3 = ax2.bar(x - width/2, prague_cvs, width, label='Prague', color='#1f77b4', alpha=0.8)
    bars4 = ax2.bar(x + width/2, cubic_cvs, width, label='Cubic', color='#d62728', alpha=0.8)
    
    # Add value labels
    for bars in [bars3, bars4]:
        for bar in bars:
            height = bar.get_height()
            if height > 0:
                ax2.text(bar.get_x() + bar.get_width()/2., height + 0.01,
                        f'{height:.3f}', ha='center', va='bottom', fontweight='bold')
    
    ax2.axhline(y=0.1, color='green', linestyle='--', alpha=0.7, label='High Stability')
    ax2.axhline(y=0.2, color='orange', linestyle='--', alpha=0.7, label='Moderate Stability')
    
    ax2.set_title('Stability (Lower CV = Better)', fontweight='bold')
    ax2.set_ylabel('Coefficient of Variation')
    ax2.set_xlabel('Packet Loss Rate')
    ax2.set_xticks(x)
    ax2.set_xticklabels(loss_rates)
    ax2.legend()
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    
    plot_file = base_dir / 'rq3_loss_sensitivity_comparison.png'
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"✅ Loss-sensitivity comparison chart saved: {plot_file.name}")

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
            l4s_delay = stats.get('l4s_queue', {}).get('mean_delay', 'N/A') if 'l4s_queue' in stats else 'N/A'
            classic_delay = stats.get('classic_queue', {}).get('mean_delay', 'N/A') if 'classic_queue' in stats else 'N/A'
            
            # Loss-sensitivity metrics
            algorithm = stats.get('algorithm', 'N/A')
            
            # Extract loss rate correctly
            if 'WLS1' in exp:
                loss_rate = '1%'
            elif 'WLS2' in exp:
                loss_rate = '5%'
            elif 'WLS3' in exp:
                loss_rate = '10%'
            else:
                loss_rate = 'N/A'
            
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
    # Backward compatibility arguments (kept for compatibility with run script)
    parser.add_argument('--base-dir', default='results/rq3', help='Base directory for all RQ3 experiments')
    parser.add_argument('--test-type', choices=['wired', 'wifi'], help='Type of test (auto-detected if not specified)')
    parser.add_argument('--combined', action='store_true', help='Run combined analysis (now default when using base-dir)')
    
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
        
        # Use auto-detected type unless user specified different type
        if args.test_type:
            # Validate that specified type matches detected type
            expected_type = 'fairness' if args.test_type == 'wired' else 'loss_sensitivity'
            if test_type != expected_type:
                print(f"⚠️  Warning: Specified --test-type '{args.test_type}' doesn't match detected type for {exp_name}")
        
        if test_type == 'fairness':
            analyze_fairness_experiment(exp_dir, args.verbose)
        elif test_type == 'loss_sensitivity':
            analyze_loss_sensitivity_experiment(exp_dir, args.verbose)
        else:
            print(f"❌ Unknown experiment type for {exp_name}")
            
        print("✅ Analysis complete!")
        
    else:
        # Full analysis of all experiments
        if args.combined or not args.exp_dir:
            # When --combined is specified or no specific exp-dir given, analyze all
            base_dir = Path(args.base_dir) if args.base_dir else Path('results/rq3')
        else:
            base_dir = Path("results/rq3")
            
        if not base_dir.exists():
            print(f"❌ Error: Directory {base_dir} does not exist")
            sys.exit(1)

        print("🚀 === RQ3 ENHANCED ANALYSIS === 🚀")
        print(f"Base directory: {base_dir}")
        
        # Find all experiment directories
        exp_dirs = [d for d in sorted(base_dir.iterdir()) 
                   if d.is_dir() and not d.name.startswith('.')]
        
        # Filter by test type if specified
        if args.test_type:
            filtered_dirs = []
            for exp_dir in exp_dirs:
                _, _, exp_test_type, _ = get_experiment_info(exp_dir.name)
                expected_type = 'fairness' if args.test_type == 'wired' else 'loss_sensitivity'
                if exp_test_type == expected_type:
                    filtered_dirs.append(exp_dir)
            exp_dirs = filtered_dirs
            print(f"Filtering to {args.test_type} experiments only")
        
        if not exp_dirs:
            print("❌ No experiment directories found!")
            sys.exit(1)
            
        print(f"Found {len(exp_dirs)} experiment directories")
        
        # Individual experiment analysis
        print("\n📊 === INDIVIDUAL EXPERIMENT ANALYSIS ===")
        all_stats = []
        fairness_count = 0
        loss_sens_count = 0
        
        for i, exp_dir in enumerate(exp_dirs, 1):
            exp_name = exp_dir.name
            _, _, test_type, _ = get_experiment_info(exp_name)
            
            print(f"\n[{i}/{len(exp_dirs)}] Processing {exp_name}...")
            
            if test_type == 'fairness':
                stats = analyze_fairness_experiment(exp_dir, args.verbose)
                fairness_count += 1
            elif test_type == 'loss_sensitivity':
                stats = analyze_loss_sensitivity_experiment(exp_dir, args.verbose)
                loss_sens_count += 1
            else:
                print(f"  ⚠️  Skipping unknown experiment type: {exp_name}")
                continue
                
            if stats:
                all_stats.append(stats)

        print(f"\n✅ Individual analysis complete!")
        print(f"   📊 Processed {len(all_stats)} experiments:")
        print(f"      - Fairness experiments: {fairness_count}")
        print(f"      - Loss-sensitivity experiments: {loss_sens_count}")
        
        if len(all_stats) == 0:
            print("❌ No valid experiments found to analyze!")
            sys.exit(1)
        
        # Comparison plots
        print("\n📈 === CREATING COMPARISON PLOTS ===")
        create_fairness_comparison_plots(base_dir)
        create_loss_sensitivity_comparison_plots(base_dir)
        
        # Summary
        print("\n📄 === CREATING SUMMARY ===")
        create_summary_csv(base_dir, all_stats)
        
        # Final summary statistics
        print(f"\n🎯 === ANALYSIS SUMMARY ===")
        fairness_stats = [s for s in all_stats if s['type'] == 'fairness']
        loss_stats = [s for s in all_stats if s['type'] == 'loss_sensitivity']
        
        if fairness_stats:
            jfi_values = [s.get('jains_fairness_index', 0) for s in fairness_stats if 'jains_fairness_index' in s]
            if jfi_values:
                avg_jfi = np.mean(jfi_values)
                min_jfi = min(jfi_values)
                max_jfi = max(jfi_values)
                print(f"📊 Fairness Analysis Results:")
                print(f"   - Average Jain's Fairness Index: {avg_jfi:.3f}")
                print(f"   - Range: {min_jfi:.3f} - {max_jfi:.3f}")
                excellent_count = sum(1 for jfi in jfi_values if jfi > 0.9)
                good_count = sum(1 for jfi in jfi_values if 0.8 < jfi <= 0.9)
                print(f"   - Excellent fairness (>0.9): {excellent_count}/{len(jfi_values)} experiments")
                print(f"   - Good fairness (>0.8): {good_count}/{len(jfi_values)} experiments")
        
        if loss_stats:
            prague_stats = [s for s in loss_stats if s.get('algorithm') == 'prague']
            cubic_stats = [s for s in loss_stats if s.get('algorithm') == 'cubic']
            print(f"📊 Loss-Sensitivity Analysis Results:")
            
            if prague_stats:
                prague_tputs = [s['throughput']['mean'] for s in prague_stats if 'throughput' in s and s['throughput']]
                if prague_tputs:
                    print(f"   - Prague avg throughput: {np.mean(prague_tputs):.1f} Mbps")
            
            if cubic_stats:
                cubic_tputs = [s['throughput']['mean'] for s in cubic_stats if 'throughput' in s and s['throughput']]
                if cubic_tputs:
                    print(f"   - Cubic avg throughput: {np.mean(cubic_tputs):.1f} Mbps")
        
        print(f"\n🎉 === ANALYSIS COMPLETE === 🎉")
        print(f"📁 Results saved in: {base_dir}")
        print(f"📊 Individual plots: {len(all_stats)} experiments")
        print(f"🔄 Comparison plots: Generated")
        print(f"📄 Summary: rq3_experiment_summary.csv")
        print(f"📋 Individual statistics: Available in each experiment directory")
        print("\n🚀 Ready for publication analysis!")
        print("   Use analyze_rq3_enhanced.py for publication-ready plots and advanced statistics.")

if __name__ == '__main__':
    main() 