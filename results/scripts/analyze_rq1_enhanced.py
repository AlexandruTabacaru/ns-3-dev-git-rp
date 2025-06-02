#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt
import pandas as pd
from pathlib import Path
import argparse
import sys
import glob

# Set up matplotlib for publication-quality plots
plt.style.use('seaborn-v0_8-whitegrid')
plt.rcParams['figure.figsize'] = (10, 6)
plt.rcParams['font.size'] = 12
plt.rcParams['axes.labelsize'] = 14
plt.rcParams['axes.titlesize'] = 16
plt.rcParams['legend.fontsize'] = 12
plt.rcParams['xtick.labelsize'] = 12
plt.rcParams['ytick.labelsize'] = 12

def load_data_safely(file_path):
    """Load data file with error handling"""
    try:
        if not file_path.exists():
            return None
        if file_path.stat().st_size == 0:
            return None
        return np.loadtxt(file_path)
    except Exception as e:
        print(f"Warning: Could not load {file_path}. Error: {e}")
        return None

def check_experiment_files(exp_dir):
    """Check which files exist for an experiment and report missing ones"""
    exp_name = exp_dir.name
    algorithm = "Prague" if exp_name.startswith("P-") else "Cubic"
    alg_lower = algorithm.lower()
    
    # Define expected files
    expected_files = {
        'throughput': f"{alg_lower}-throughput.{exp_name}.dat",
        'sink_rx': f"{alg_lower}-sink-rx.{exp_name}.dat",
        'cwnd': f"{alg_lower}-cwnd.{exp_name}.dat",
        'rtt': f"{alg_lower}-rtt.{exp_name}.dat",
        'pacing_rate': f"{alg_lower}-pacing-rate.{exp_name}.dat",
        'cong_state': f"{alg_lower}-cong-state.{exp_name}.dat",
    }
    
    # Algorithm-specific queue files
    if algorithm == "Prague":
        expected_files.update({
            'queue_delay': f"wired-dualpi2-l-sojourn.{exp_name}.dat",
            'queue_bytes': f"wired-dualpi2-bytes.{exp_name}.dat",
            'ecn_state': f"{alg_lower}-ecn-state.{exp_name}.dat"
        })
    else:
        expected_files.update({
            'queue_delay': f"wired-fqcodel-sojourn.{exp_name}.dat",
            'queue_bytes': f"wired-fqcodel-bytes.{exp_name}.dat"
        })
    
    # Check file existence
    missing_files = []
    existing_files = {}
    
    for file_type, filename in expected_files.items():
        file_path = exp_dir / filename
        if file_path.exists() and file_path.stat().st_size > 0:
            existing_files[file_type] = file_path
        else:
            missing_files.append(filename)
    
    return existing_files, missing_files

def analyze_rq1_experiment(exp_dir):
    """Analyze a single RQ1 experiment directory"""
    exp_name = exp_dir.name
    algorithm = "Prague" if exp_name.startswith("P-") else "Cubic"
    
    # Extract RTT level and jitter level from experiment name
    rtt_level = exp_name.split('-')[1][0]  # H, M, or L
    jitter_level = exp_name.split('-')[1][1]  # 0, 1, or 5
    
    rtt_names = {'H': '80ms RTT', 'M': '40ms RTT', 'L': '20ms RTT'}
    jitter_names = {'0': '0ms', '1': '1ms', '5': '5ms'}
    
    print(f"\nAnalyzing {exp_name}: {algorithm} with {rtt_names[rtt_level]}, {jitter_names[jitter_level]} jitter")

    # Check file existence
    existing_files, missing_files = check_experiment_files(exp_dir)
    
    if missing_files:
        print(f"⚠️  Missing files for {exp_name}:")
        for missing in missing_files:
            print(f"   - {missing}")
        print("   Run the simulation again to generate missing files.")
    
    if 'throughput' not in existing_files:
        print(f"❌ Cannot analyze {exp_name} - no throughput data found")
        return None

    print(f"✅ Found {len(existing_files)}/{len(existing_files) + len(missing_files)} expected files")

    # Load data files
    throughput_data = load_data_safely(existing_files['throughput'])
    sink_rx_data = load_data_safely(existing_files.get('sink_rx'))
    cwnd_data = load_data_safely(existing_files.get('cwnd'))
    rtt_data = load_data_safely(existing_files.get('rtt'))
    queue_delay_data = load_data_safely(existing_files.get('queue_delay'))
    queue_bytes_data = load_data_safely(existing_files.get('queue_bytes'))
    
    queue_type = "DualPI2" if algorithm == "Prague" else "FqCoDel"

    # Calculate goodput from sink data
    goodput_time = None
    goodput = None
    if sink_rx_data is not None and len(sink_rx_data) > 1:
        dt = np.diff(sink_rx_data[:, 0])
        dt_safe = np.where(dt == 0, 1e-9, dt)
        goodput_diff = np.diff(sink_rx_data[:, 1])
        goodput = goodput_diff * 8 / dt_safe / 1e6  # Convert to Mbps
        goodput_time = sink_rx_data[:-1, 0] + dt_safe/2

    # Set up time bounds (exclude first and last 5 seconds)
    warmup_time = 5.0
    teardown_time = 5.0
    total_time = throughput_data[-1, 0] if len(throughput_data) > 0 else 60.0
    analysis_start = warmup_time
    analysis_end = total_time - teardown_time

    # Create main analysis figure - Focus on 2 key metrics
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
    
    # Calculate expected RTT range for context
    base_rtt_ms = {'H': 80, 'M': 40, 'L': 20}[rtt_level]  # 2 * one-way delay
    jitter_ms = {'0': 0, '1': 1, '5': 5}[jitter_level]
    expected_rtt_min = base_rtt_ms - jitter_ms
    expected_rtt_max = base_rtt_ms + jitter_ms
    
    fig.suptitle(f'RQ1 Analysis: {exp_name} ({algorithm}, {rtt_names[rtt_level]}, {jitter_names[jitter_level]} Jitter)\n' +
                 f'Expected RTT range: {expected_rtt_min}-{expected_rtt_max}ms (without queueing)', 
                 fontsize=16, fontweight='bold')

    # Plot 1: Throughput and Goodput
    ax1.plot(throughput_data[:, 0], throughput_data[:, 1], 
             label='Interface Throughput', linewidth=2, color='blue', alpha=0.8)
    if goodput is not None and goodput_time is not None:
        ax1.plot(goodput_time, goodput, 
                 label='Application Goodput', linewidth=2, color='red', alpha=0.8)
    
    ax1.axvspan(0, warmup_time, alpha=0.2, color='gray', label='Excluded from analysis')
    ax1.axvspan(analysis_end, total_time, alpha=0.2, color='gray')
    ax1.set_title('Throughput vs Goodput Performance')
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Rate (Mbps)')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Plot 2: Queueing Delay with RTT context
    if queue_delay_data is not None:
        ax2.plot(queue_delay_data[:, 0], queue_delay_data[:, 1], 
                 label=f'{queue_type} Queueing Delay', linewidth=2, color='green')
        ax2.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax2.axvspan(analysis_end, total_time, alpha=0.2, color='gray')
        # Add horizontal line showing expected base RTT for reference
        ax2.axhline(y=base_rtt_ms, color='orange', linestyle=':', alpha=0.7, 
                   label=f'Base RTT ({base_rtt_ms}ms)')
    
    if rtt_data is not None:
        # Sample RTT data for overlay (every 100th point to avoid clutter)
        sample_indices = range(0, len(rtt_data), max(1, len(rtt_data)//100))
        ax2.scatter(rtt_data[sample_indices, 0], rtt_data[sample_indices, 1], 
                   s=8, alpha=0.4, color='purple', label='Measured RTT (sampled)')
    
    ax2.set_title('Queueing Delay and RTT Evolution')
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Delay/RTT (ms)')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(exp_dir / f'rq1_analysis_{exp_name}.png', dpi=300, bbox_inches='tight')
    plt.close()

    # Create a focused comparison figure for key metrics
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
    fig.suptitle(f'Key Metrics: {exp_name} ({algorithm}, {rtt_names[rtt_level]}, {jitter_names[jitter_level]} Jitter)', 
                 fontsize=16, fontweight='bold')

    # Throughput vs Goodput comparison
    ax1.plot(throughput_data[:, 0], throughput_data[:, 1], 
             label='Interface Throughput', linewidth=2, color='blue')
    if goodput is not None and goodput_time is not None:
        ax1.plot(goodput_time, goodput, 
                 label='Application Goodput', linewidth=2, color='red', linestyle='--')
    ax1.axvspan(0, warmup_time, alpha=0.2, color='gray', label='Excluded from analysis')
    ax1.axvspan(analysis_end, total_time, alpha=0.2, color='gray')
    ax1.set_title('Throughput vs Goodput')
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Rate (Mbps)')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Queue delay
    if queue_delay_data is not None:
        ax2.plot(queue_delay_data[:, 0], queue_delay_data[:, 1], 
                 label=f'{queue_type} Queueing Delay', linewidth=2, color='green')
        ax2.axvspan(0, warmup_time, alpha=0.2, color='gray')
        ax2.axvspan(analysis_end, total_time, alpha=0.2, color='gray')
    ax2.set_title('Queueing Delay')
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Delay (ms)')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(exp_dir / f'rq1_key_metrics_{exp_name}.png', dpi=300, bbox_inches='tight')
    plt.close()

    # Calculate statistics for the analysis period
    def calc_stats(data, time_col=0, val_col=1):
        if data is None or len(data) == 0:
            return {}
        mask = (data[:, time_col] >= analysis_start) & (data[:, time_col] <= analysis_end)
        vals = data[mask, val_col]
        if len(vals) == 0:
            return {}
        return {
            'mean': np.mean(vals),
            'std': np.std(vals),
            'min': np.min(vals),
            'max': np.max(vals),
            'p95': np.percentile(vals, 95),
            'p99': np.percentile(vals, 99)
        }

    # Calculate goodput stats
    goodput_stats = {}
    if goodput is not None and goodput_time is not None:
        mask = (goodput_time >= analysis_start) & (goodput_time <= analysis_end)
        goodput_vals = goodput[mask]
        if len(goodput_vals) > 0:
            goodput_stats = {
                'mean': np.mean(goodput_vals),
                'std': np.std(goodput_vals),
                'min': np.min(goodput_vals),
                'max': np.max(goodput_vals),
                'p95': np.percentile(goodput_vals, 95),
                'p99': np.percentile(goodput_vals, 99)
            }

    # Calculate efficiency (goodput/throughput ratio)
    efficiency_stats = {}
    if goodput is not None and goodput_time is not None:
        # Interpolate throughput to goodput timestamps for comparison
        throughput_interp = np.interp(goodput_time, throughput_data[:, 0], throughput_data[:, 1])
        efficiency = np.where(throughput_interp > 0, goodput / throughput_interp, 0)
        mask = (goodput_time >= analysis_start) & (goodput_time <= analysis_end)
        efficiency_vals = efficiency[mask]
        if len(efficiency_vals) > 0:
            efficiency_stats = {
                'mean': np.mean(efficiency_vals),
                'std': np.std(efficiency_vals),
                'min': np.min(efficiency_vals),
                'max': np.max(efficiency_vals)
            }

    stats = {
        'experiment': exp_name,
        'algorithm': algorithm,
        'rtt_level': rtt_names[rtt_level],
        'jitter_level': jitter_names[jitter_level],
        'throughput': calc_stats(throughput_data),
        'goodput': goodput_stats,
        'efficiency': efficiency_stats,
        'queue_delay': calc_stats(queue_delay_data),
        'queue_bytes': calc_stats(queue_bytes_data),
        'cwnd': calc_stats(cwnd_data),
        'rtt': calc_stats(rtt_data),
        'missing_files': missing_files
    }

    # Save statistics
    with open(exp_dir / f'rq1_stats_{exp_name}.txt', 'w') as f:
        f.write(f"RQ1 Statistics Summary - {exp_name}\n")
        f.write("=" * 50 + "\n\n")
        f.write(f"Algorithm: {algorithm}\n")
        f.write(f"Base RTT: {rtt_names[rtt_level]}\n")
        f.write(f"Jitter Level: {jitter_names[jitter_level]}\n")
        f.write(f"Analysis Period: {analysis_start:.1f}s - {analysis_end:.1f}s\n\n")
        
        if missing_files:
            f.write("MISSING FILES:\n")
            for missing in missing_files:
                f.write(f"  - {missing}\n")
            f.write("\n")
        
        for metric, values in stats.items():
            if metric in ['experiment', 'algorithm', 'rtt_level', 'jitter_level', 'missing_files']:
                continue
            if values and isinstance(values, dict):
                f.write(f"{metric.upper().replace('_', ' ')} Statistics:\n")
                for key, val in values.items():
                    if metric in ['throughput', 'goodput']:
                        f.write(f"  {key}: {val:.2f} Mbps\n")
                    elif metric == 'efficiency':
                        f.write(f"  {key}: {val:.3f} (ratio)\n")
                    elif metric == 'queue_delay':
                        f.write(f"  {key}: {val:.3f} ms\n")
                    elif metric == 'queue_bytes':
                        f.write(f"  {key}: {val:.0f} bytes\n")
                    elif metric == 'cwnd':
                        f.write(f"  {key}: {val:.0f} bytes\n")
                    elif metric == 'rtt':
                        f.write(f"  {key}: {val:.2f} ms\n")
                f.write("\n")

    print(f"✓ Analysis completed for {exp_name}")
    if goodput_stats:
        print(f"  Goodput: {goodput_stats['mean']:.2f} ± {goodput_stats['std']:.2f} Mbps")
    if efficiency_stats:
        print(f"  Efficiency: {efficiency_stats['mean']:.1%} ± {efficiency_stats['std']:.1%}")
    if stats['queue_delay']:
        print(f"  Queue Delay: {stats['queue_delay']['mean']:.3f} ± {stats['queue_delay']['std']:.3f} ms")

    return stats

def create_comparison_plots(base_dir):
    """Create comparison plots between Prague and Cubic for each RTT/jitter combination"""
    
    # Define experiment pairs
    rtt_levels = ['H', 'M', 'L']
    jitter_levels = ['0', '1', '5']
    
    rtt_names = {'H': '80ms RTT', 'M': '40ms RTT', 'L': '20ms RTT'}
    jitter_names = {'0': '0ms', '1': '1ms', '5': '5ms'}
    
    # Analysis period settings
    warmup_time = 5.0
    teardown_time = 5.0
    total_time = 60.0  # Default total time
    
    for rtt in rtt_levels:
        for jitter in jitter_levels:
            prague_exp = f"P-{rtt}{jitter}"
            cubic_exp = f"C-{rtt}{jitter}"
            
            prague_dir = base_dir / prague_exp
            cubic_dir = base_dir / cubic_exp
            
            if not (prague_dir.exists() and cubic_dir.exists()):
                print(f"Skipping comparison for {rtt}{jitter} - missing experiment directories")
                continue
                
            print(f"Creating comparison plots for {rtt_names[rtt]}, {jitter_names[jitter]} jitter")
            
            # Load data for both algorithms
            prague_throughput = load_data_safely(prague_dir / f"prague-throughput.{prague_exp}.dat")
            cubic_throughput = load_data_safely(cubic_dir / f"cubic-throughput.{cubic_exp}.dat")
            prague_delay = load_data_safely(prague_dir / f"wired-dualpi2-l-sojourn.{prague_exp}.dat")
            cubic_delay = load_data_safely(cubic_dir / f"wired-fqcodel-sojourn.{cubic_exp}.dat")
            
            # Determine actual total time from data
            if prague_throughput is not None and len(prague_throughput) > 0:
                total_time = prague_throughput[-1, 0]
            elif cubic_throughput is not None and len(cubic_throughput) > 0:
                total_time = cubic_throughput[-1, 0]
            
            # Create comparison figure with 2 subplots
            fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
            fig.suptitle(f'Prague vs Cubic Comparison\n{rtt_names[rtt]}, {jitter_names[jitter]} Jitter', 
                         fontsize=18, fontweight='bold')
            
            # Interface throughput comparison
            if prague_throughput is not None:
                ax1.plot(prague_throughput[:, 0], prague_throughput[:, 1], 
                         label='Prague', linewidth=2, color='blue')
            if cubic_throughput is not None:
                ax1.plot(cubic_throughput[:, 0], cubic_throughput[:, 1], 
                         label='Cubic', linewidth=2, color='red')
            
            # Add gray shading for excluded periods
            ax1.axvspan(0, warmup_time, alpha=0.2, color='gray', label='Excluded from analysis')
            ax1.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
            
            # Calculate y-axis limits for throughput based on analysis period
            throughput_values = []
            if prague_throughput is not None:
                mask = (prague_throughput[:, 0] >= warmup_time) & (prague_throughput[:, 0] <= total_time - teardown_time)
                throughput_values.extend(prague_throughput[mask, 1])
            if cubic_throughput is not None:
                mask = (cubic_throughput[:, 0] >= warmup_time) & (cubic_throughput[:, 0] <= total_time - teardown_time)
                throughput_values.extend(cubic_throughput[mask, 1])
            
            if throughput_values:
                y_min = min(throughput_values)
                y_max = max(throughput_values)
                y_range = y_max - y_min
                # Add 10% padding above and below
                ax1.set_ylim(max(0, y_min - 0.1 * y_range), y_max + 0.1 * y_range)
            
            ax1.set_title('Interface Throughput Comparison')
            ax1.set_xlabel('Time (s)')
            ax1.set_ylabel('Rate (Mbps)')
            ax1.legend()
            ax1.grid(True, alpha=0.3)
            
            # Queue delay comparison
            if prague_delay is not None:
                ax2.plot(prague_delay[:, 0], prague_delay[:, 1], 
                         label='Prague (DualPI2)', linewidth=2, color='blue')
            if cubic_delay is not None:
                ax2.plot(cubic_delay[:, 0], cubic_delay[:, 1], 
                         label='Cubic (FqCoDel)', linewidth=2, color='red')
            
            # Add gray shading for excluded periods
            ax2.axvspan(0, warmup_time, alpha=0.2, color='gray', label='Excluded from analysis')
            ax2.axvspan(total_time - teardown_time, total_time, alpha=0.2, color='gray')
            
            # Calculate y-axis limits for delay based on analysis period
            delay_values = []
            if prague_delay is not None:
                mask = (prague_delay[:, 0] >= warmup_time) & (prague_delay[:, 0] <= total_time - teardown_time)
                delay_values.extend(prague_delay[mask, 1])
            if cubic_delay is not None:
                mask = (cubic_delay[:, 0] >= warmup_time) & (cubic_delay[:, 0] <= total_time - teardown_time)
                delay_values.extend(cubic_delay[mask, 1])
            
            if delay_values:
                y_min = min(delay_values)
                y_max = max(delay_values)
                y_range = y_max - y_min
                # Add 10% padding above and below, but ensure we start from 0 for delay
                ax2.set_ylim(0, y_max + 0.1 * y_range)
            
            ax2.set_title('Queueing Delay Comparison')
            ax2.set_xlabel('Time (s)')
            ax2.set_ylabel('Delay (ms)')
            ax2.legend()
            ax2.grid(True, alpha=0.3)
            
            plt.tight_layout()
            plt.savefig(base_dir / f'rq1_comparison_{rtt}{jitter}_{rtt_names[rtt].replace(" ", "")}_{jitter_names[jitter]}.png', 
                       dpi=300, bbox_inches='tight')
            plt.close()

def create_summary_table(base_dir):
    """Create a comprehensive summary table of all experiments"""
    
    # Collect all statistics
    all_stats = []
    
    for exp_dir in sorted(base_dir.iterdir()):
        if exp_dir.is_dir() and not exp_dir.name.startswith('.'):
            exp_name = exp_dir.name
            algorithm = "Prague" if exp_name.startswith("P-") else "Cubic"
            rtt_level = exp_name.split('-')[1][0]
            jitter_level = exp_name.split('-')[1][1]
            
            rtt_names = {'H': '40ms', 'M': '20ms', 'L': '10ms'}
            jitter_names = {'0': '0ms', '1': '1ms', '5': '5ms'}
            
            # Check files
            existing_files, missing_files = check_experiment_files(exp_dir)
            
            all_stats.append({
                'Experiment': exp_name,
                'Algorithm': algorithm,
                'Base_RTT': rtt_names[rtt_level],
                'Jitter': jitter_names[jitter_level],
                'Files_Found': len(existing_files),
                'Files_Missing': len(missing_files),
                'Status': 'Complete' if len(missing_files) == 0 else 'Incomplete'
            })
    
    # Create summary table
    if all_stats:
        df = pd.DataFrame(all_stats)
        summary_path = base_dir / 'rq1_experiment_summary.csv'
        df.to_csv(summary_path, index=False)
        print(f"\n✓ Summary table saved to {summary_path}")
        print(f"  Analyzed {len(all_stats)} experiments")
        
        # Print status summary
        complete = len(df[df['Status'] == 'Complete'])
        incomplete = len(df[df['Status'] == 'Incomplete'])
        print(f"  Status: {complete} complete, {incomplete} incomplete")
        
        if incomplete > 0:
            print("\n⚠️  Incomplete experiments:")
            incomplete_df = df[df['Status'] == 'Incomplete']
            for _, row in incomplete_df.iterrows():
                print(f"    {row['Experiment']}: {row['Files_Missing']} missing files")

def main():
    """Main analysis function"""
    parser = argparse.ArgumentParser(description='Enhanced RQ1 Analysis Script')
    parser.add_argument('--exp-dir', type=str, 
                       help='Specific experiment directory to analyze (optional)')
    parser.add_argument('--base-dir', type=str, default='../rq1',
                       help='Base directory containing all RQ1 experiments')
    args = parser.parse_args()
    
    base_results_dir = Path(__file__).parent.parent / "rq1"
    if args.base_dir:
        base_results_dir = Path(args.base_dir)
    
    if not base_results_dir.exists():
        print(f"❌ Error: Directory {base_results_dir} does not exist")
        sys.exit(1)
    
    print("=" * 60)
    print("RQ1 Enhanced Analysis Script")
    print("=" * 60)
    
    if args.exp_dir:
        # Analyze single experiment
        exp_dir = Path(args.exp_dir)
        if not exp_dir.exists():
            print(f"❌ Error: Directory {exp_dir} does not exist")
            sys.exit(1)
        analyze_rq1_experiment(exp_dir)
    else:
        # Analyze all experiments
        experiment_count = 0
        for exp_dir in sorted(base_results_dir.iterdir()):
            if exp_dir.is_dir() and not exp_dir.name.startswith('.'):
                analyze_rq1_experiment(exp_dir)
                experiment_count += 1
        
        print(f"\n" + "=" * 60)
        print(f"Individual analysis completed for {experiment_count} experiments")
        print("Creating comparison plots...")
        
        # Create comparison plots
        create_comparison_plots(base_results_dir)
        
        # Create summary table
        create_summary_table(base_results_dir)
        
        print("\n✅ All analysis completed!")
        print(f"Results saved in: {base_results_dir}")
        print("\nGenerated files:")
        print("  - rq1_analysis_[EXP_ID].png : Individual experiment analysis (4 subplots)")
        print("  - rq1_key_metrics_[EXP_ID].png : Key metrics focus (2 subplots)")
        print("  - rq1_stats_[EXP_ID].txt : Statistical summary with missing file info")
        print("  - rq1_comparison_[RTT][JITTER]_*.png : Prague vs Cubic comparisons")
        print("  - rq1_experiment_summary.csv : Overall summary table with file status")

if __name__ == "__main__":
    main() 