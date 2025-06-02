#!/usr/bin/env python3

import os
import sys
import numpy as np
import matplotlib.pyplot as plt
import argparse
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
    'figure.figsize': (12, 8)
})

def load_data_safe(file_path, verbose=False):
    """Safely load data from file with comprehensive error handling."""
    try:
        if not file_path.exists():
            if verbose:
                print(f"  File not found: {file_path.name}")
            return None
        
        if file_path.stat().st_size == 0:
            if verbose:
                print(f"  Empty file: {file_path.name}")
            return None
            
        data = np.loadtxt(file_path)
        if data.size == 0:
            if verbose:
                print(f"  No data in file: {file_path.name}")
            return None
            
        # Ensure 2D array for consistency
        if data.ndim == 1:
            data = data.reshape(-1, 1)
            
        if verbose:
            print(f"  ✓ Loaded {file_path.name}: {data.shape[0]} points")
        return data
        
    except Exception as e:
        if verbose:
            print(f"  Error loading {file_path.name}: {e}")
        return None

def get_experiment_info(exp_name):
    """Extract experiment information from name."""
    algorithm = "prague" if exp_name.startswith("P-") else "cubic"
    is_wifi = exp_name.startswith(('P-W', 'C-W'))
    
    # Determine scenario
    if 'B1' in exp_name:
        scenario = '25→100 Mbps'
    elif 'B2' in exp_name:
        scenario = '100→25 Mbps'
    elif 'W1' in exp_name:
        scenario = 'MCS 2→7'
    elif 'W2' in exp_name:
        scenario = 'MCS 7→2'
    elif 'W3' in exp_name:
        scenario = 'MCS 4→9'
    elif 'W4' in exp_name:
        scenario = 'MCS 9→4'
    elif 'W5' in exp_name:
        scenario = 'MCS 4→7'
    elif 'W6' in exp_name:
        scenario = 'MCS 7→4'
    else:
        scenario = 'Unknown'
    
    return algorithm, is_wifi, scenario

def analyze_individual_experiment(exp_dir, verbose=True):
    """Create detailed 4-panel analysis for a single experiment."""
    exp_name = exp_dir.name
    algorithm, is_wifi, scenario = get_experiment_info(exp_name)
    
    if verbose:
        print(f"\n📊 Analyzing {exp_name}")
        print(f"   Algorithm: {algorithm.capitalize()}")
        print(f"   Type: {'WiFi' if is_wifi else 'Wired'}")
        print(f"   Scenario: {scenario}")

    # Load core data files
    print("  Loading data files:")
    throughput_data = load_data_safe(exp_dir / f"{algorithm}-throughput.{exp_name}.dat", verbose)
    cwnd_data = load_data_safe(exp_dir / f"{algorithm}-cwnd.{exp_name}.dat", verbose)
    
    # Load queue data based on experiment type and algorithm
    if is_wifi:
        # WiFi uses DualPI2 with different queues for Prague and Cubic
        if algorithm == "prague":
            queue_delay_data = load_data_safe(exp_dir / f"wifi-dualpi2-l-sojourn.{exp_name}.dat", verbose)
        else:  # cubic
            queue_delay_data = load_data_safe(exp_dir / f"wifi-dualpi2-c-sojourn.{exp_name}.dat", verbose)
        queue_bytes_data = load_data_safe(exp_dir / f"wifi-dualpi2-bytes.{exp_name}.dat", verbose)
    else:
        # Wired uses different queues based on algorithm
        if algorithm == "prague":
            queue_delay_data = load_data_safe(exp_dir / f"wired-dualpi2-l-sojourn.{exp_name}.dat", verbose)
            queue_bytes_data = load_data_safe(exp_dir / f"wired-dualpi2-bytes.{exp_name}.dat", verbose)
        else:  # cubic
            queue_delay_data = load_data_safe(exp_dir / f"wired-fqcodel-sojourn.{exp_name}.dat", verbose)
            queue_bytes_data = load_data_safe(exp_dir / f"wired-fqcodel-bytes.{exp_name}.dat", verbose)

    if throughput_data is None:
        print(f"  ❌ No throughput data found for {exp_name}")
        return None

    # Analysis parameters
    warmup_time = 5.0
    teardown_time = 5.0
    total_time = 40.0
    change_time = 20.0

    # Create 4-panel analysis plot
    print("  Creating individual analysis plot...")
    fig, ((ax1, ax2), (ax3, ax4)) = plt.subplots(2, 2, figsize=(16, 12))
    fig.suptitle(f'RQ2 Individual Analysis: {exp_name}\n{algorithm.capitalize()} - {"WiFi" if is_wifi else "Wired"} - {scenario}', 
                 fontsize=16, y=0.95)

    # Panel 1: Interface Throughput
    if throughput_data is not None:
        ax1.plot(throughput_data[:, 0], throughput_data[:, 1], 'b-', label='Interface Throughput', linewidth=2)
        ax1.set_ylim(bottom=0)
    
    ax1.axvspan(0, warmup_time, alpha=0.15, color='gray', label='Excluded periods')
    ax1.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax1.axvline(x=change_time, color='red', linestyle='--', linewidth=2, 
                label=f'{"MCS" if is_wifi else "Rate"} Change @ 20s')
    ax1.set_title('Interface Throughput', fontsize=12, fontweight='bold')
    ax1.set_xlabel('Time (s)')
    ax1.set_ylabel('Throughput (Mbps)')
    ax1.grid(True, alpha=0.3)
    ax1.legend()

    # Panel 2: Queue Delay
    if queue_delay_data is not None:
        queue_type = "DualPI2" if (algorithm == "prague" or is_wifi) else "FqCoDel"
        ax2.plot(queue_delay_data[:, 0], queue_delay_data[:, 1], 'g-', 
                label=f'{queue_type} Queue Delay', linewidth=2)
        ax2.set_ylim(bottom=0)
    
    ax2.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax2.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax2.axvline(x=change_time, color='red', linestyle='--', linewidth=2)
    ax2.set_title('Queue Delay', fontsize=12, fontweight='bold')
    ax2.set_xlabel('Time (s)')
    ax2.set_ylabel('Delay (ms)')
    ax2.grid(True, alpha=0.3)
    ax2.legend()

    # Panel 3: Queue Occupancy
    if queue_bytes_data is not None:
        ax3.plot(queue_bytes_data[:, 0], queue_bytes_data[:, 1] / 1000, 'orange', 
                 label='Queue Size', linewidth=2)
        ax3.set_ylim(bottom=0)
    
    ax3.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax3.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax3.axvline(x=change_time, color='red', linestyle='--', linewidth=2)
    ax3.set_title('Queue Occupancy', fontsize=12, fontweight='bold')
    ax3.set_xlabel('Time (s)')
    ax3.set_ylabel('Queue Size (KB)')
    ax3.grid(True, alpha=0.3)
    ax3.legend()

    # Panel 4: Congestion Window
    if cwnd_data is not None:
        ax4.plot(cwnd_data[:, 0], cwnd_data[:, 1] / 1000, 'purple', 
                label='Congestion Window', linewidth=2)
        ax4.set_ylim(bottom=0)
    
    ax4.axvspan(0, warmup_time, alpha=0.15, color='gray')
    ax4.axvspan(total_time - teardown_time, total_time, alpha=0.15, color='gray')
    ax4.axvline(x=change_time, color='red', linestyle='--', linewidth=2)
    ax4.set_title('Congestion Window', fontsize=12, fontweight='bold')
    ax4.set_xlabel('Time (s)')
    ax4.set_ylabel('cwnd (KB)')
    ax4.grid(True, alpha=0.3)
    ax4.legend()

    plt.tight_layout(rect=[0, 0, 1, 0.92])
    output_file = exp_dir / f'rq2_individual_{exp_name}.png'
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    plt.close()
    print(f"  ✓ Saved: {output_file.name}")

    # Calculate and save statistics
    stats = calculate_statistics(exp_name, algorithm, is_wifi, scenario,
                               throughput_data, queue_delay_data, 
                               warmup_time, teardown_time, change_time)
    
    if stats:
        save_statistics(exp_dir, stats)
    
    return stats

def calculate_statistics(exp_name, algorithm, is_wifi, scenario, 
                        throughput_data, queue_delay_data,
                        warmup_time, teardown_time, change_time):
    """Calculate before/after statistics for the experiment."""
    
    def extract_stats(data, time_col=0, val_col=1):
        if data is None or len(data) == 0:
            return None, None
        
        # Trim warmup/teardown periods
        trimmed = data[(data[:, time_col] >= warmup_time) & 
                      (data[:, time_col] <= (40.0 - teardown_time))]
        
        if len(trimmed) == 0:
            return None, None
            
        # Split before/after change
        before = trimmed[trimmed[:, time_col] < change_time]
        after = trimmed[trimmed[:, time_col] >= change_time]
        
        if len(before) == 0 or len(after) == 0:
            return None, None
            
        before_stats = {
            'mean': np.mean(before[:, val_col]),
            'std': np.std(before[:, val_col]),
            'min': np.min(before[:, val_col]),
            'max': np.max(before[:, val_col])
        }
        
        after_stats = {
            'mean': np.mean(after[:, val_col]),
            'std': np.std(after[:, val_col]),
            'min': np.min(after[:, val_col]),
            'max': np.max(after[:, val_col])
        }
        
        return before_stats, after_stats
    
    stats = {
        'experiment': exp_name,
        'algorithm': algorithm,
        'type': 'WiFi' if is_wifi else 'Wired',
        'scenario': scenario
    }
    
    # Throughput statistics
    tput_before, tput_after = extract_stats(throughput_data)
    if tput_before and tput_after:
        stats['throughput'] = {'before': tput_before, 'after': tput_after}
    
    # Queue delay statistics
    qdelay_before, qdelay_after = extract_stats(queue_delay_data)
    if qdelay_before and qdelay_after:
        stats['queue_delay'] = {'before': qdelay_before, 'after': qdelay_after}
    
    return stats if ('throughput' in stats or 'queue_delay' in stats) else None

def save_statistics(exp_dir, stats):
    """Save statistics summary to file."""
    output_file = exp_dir / f"rq2_stats_{stats['experiment']}.txt"
    
    with open(output_file, 'w') as f:
        f.write(f"RQ2 Statistics Summary: {stats['experiment']}\n")
        f.write("=" * 50 + "\n\n")
        f.write(f"Algorithm: {stats['algorithm'].capitalize()}\n")
        f.write(f"Type: {stats['type']}\n")
        f.write(f"Scenario: {stats['scenario']}\n\n")
        
        if 'throughput' in stats:
            f.write("THROUGHPUT (Mbps)\n")
            f.write("-" * 20 + "\n")
            before = stats['throughput']['before']
            after = stats['throughput']['after']
            f.write(f"Before change: {before['mean']:.2f} ± {before['std']:.2f}\n")
            f.write(f"After change:  {after['mean']:.2f} ± {after['std']:.2f}\n")
            
            change = after['mean'] - before['mean']
            pct_change = (change / before['mean']) * 100
            f.write(f"Change: {change:+.2f} Mbps ({pct_change:+.1f}%)\n\n")
        
        if 'queue_delay' in stats:
            f.write("QUEUE DELAY (ms)\n")
            f.write("-" * 16 + "\n")
            before = stats['queue_delay']['before']
            after = stats['queue_delay']['after']
            f.write(f"Before change: {before['mean']:.3f} ± {before['std']:.3f}\n")
            f.write(f"After change:  {after['mean']:.3f} ± {after['std']:.3f}\n")
            
            change = after['mean'] - before['mean']
            if before['mean'] > 0:
                pct_change = (change / before['mean']) * 100
                f.write(f"Change: {change:+.3f} ms ({pct_change:+.1f}%)\n")
            else:
                f.write(f"Change: {change:+.3f} ms\n")

def create_comparison_plots(base_dir):
    """Create comparison plots between Prague and Cubic."""
    
    # Define experiment pairs
    comparisons = [
        # Wired experiments
        ('Wired: 25→100 Mbps', 'P-B1', 'C-B1', False),
        ('Wired: 100→25 Mbps', 'P-B2', 'C-B2', False),
        # WiFi experiments
        ('WiFi: MCS 2→7', 'P-W1', 'C-W1', True),
        ('WiFi: MCS 7→2', 'P-W2', 'C-W2', True),
        ('WiFi: MCS 4→9', 'P-W3', 'C-W3', True),
        ('WiFi: MCS 9→4', 'P-W4', 'C-W4', True),
        ('WiFi: MCS 4→7', 'P-W5', 'C-W5', True),
        ('WiFi: MCS 7→4', 'P-W6', 'C-W6', True),
    ]

    print("\n🔄 Creating comparison plots...")
    
    for title, prague_id, cubic_id, is_wifi in comparisons:
        prague_dir = base_dir / prague_id
        cubic_dir = base_dir / cubic_id
        
        if not (prague_dir.exists() and cubic_dir.exists()):
            print(f"  ⚠️  Skipping {title}: Missing directories")
            continue
            
        print(f"  📈 {title}")
        
        # Load data
        prague_tput = load_data_safe(prague_dir / f"prague-throughput.{prague_id}.dat")
        cubic_tput = load_data_safe(cubic_dir / f"cubic-throughput.{cubic_id}.dat")
        
        if is_wifi:
            prague_qdelay = load_data_safe(prague_dir / f"wifi-dualpi2-l-sojourn.{prague_id}.dat")
            cubic_qdelay = load_data_safe(cubic_dir / f"wifi-dualpi2-c-sojourn.{cubic_id}.dat")
        else:
            prague_qdelay = load_data_safe(prague_dir / f"wired-dualpi2-l-sojourn.{prague_id}.dat")
            cubic_qdelay = load_data_safe(cubic_dir / f"wired-fqcodel-sojourn.{cubic_id}.dat")
        
        # Create comparison plot
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))
        fig.suptitle(f'RQ2 Comparison: {title}', fontsize=16, y=0.95)
        
        # Throughput comparison
        if prague_tput is not None:
            ax1.plot(prague_tput[:, 0], prague_tput[:, 1], 'b-', 
                    label='Prague', linewidth=2.5, alpha=0.8)
        if cubic_tput is not None:
            ax1.plot(cubic_tput[:, 0], cubic_tput[:, 1], 'r--', 
                    label='Cubic', linewidth=2.5, alpha=0.8)
        
        ax1.axvspan(0, 5, alpha=0.15, color='gray', label='Excluded periods')
        ax1.axvspan(35, 40, alpha=0.15, color='gray')
        ax1.axvline(x=20, color='orange', linestyle=':', linewidth=3, 
                   alpha=0.9, label=f'{"MCS" if is_wifi else "Rate"} Change')
        ax1.set_title('Interface Throughput Comparison', fontweight='bold')
        ax1.set_ylabel('Throughput (Mbps)')
        ax1.grid(True, alpha=0.3)
        ax1.legend()
        ax1.set_ylim(bottom=0)
        
        # Queue delay comparison
        if prague_qdelay is not None:
            queue_label = 'Prague (DualPI2)' if is_wifi else 'Prague (DualPI2)'
            ax2.plot(prague_qdelay[:, 0], prague_qdelay[:, 1], 'b-', 
                    label=queue_label, linewidth=2.5, alpha=0.8)
        if cubic_qdelay is not None:
            queue_label = 'Cubic (DualPI2)' if is_wifi else 'Cubic (FqCoDel)'
            ax2.plot(cubic_qdelay[:, 0], cubic_qdelay[:, 1], 'r--', 
                    label=queue_label, linewidth=2.5, alpha=0.8)
        
        ax2.axvspan(0, 5, alpha=0.15, color='gray')
        ax2.axvspan(35, 40, alpha=0.15, color='gray')
        ax2.axvline(x=20, color='orange', linestyle=':', linewidth=3, alpha=0.9)
        ax2.set_title('Queue Delay Comparison', fontweight='bold')
        ax2.set_xlabel('Time (s)')
        ax2.set_ylabel('Queue Delay (ms)')
        ax2.grid(True, alpha=0.3)
        ax2.legend()
        ax2.set_ylim(bottom=0)
        
        plt.tight_layout(rect=[0, 0, 1, 0.92])
        
        # Create safe filename
        safe_name = title.replace(':', '').replace('→', '_to_').replace(' ', '_')
        output_file = base_dir / f'rq2_comparison_{safe_name}.png'
        plt.savefig(output_file, dpi=300, bbox_inches='tight')
        plt.close()
        print(f"     ✓ Saved: {output_file.name}")

def create_summary_csv(base_dir, all_stats):
    """Create CSV summary of all experiments."""
    if not all_stats:
        print("  ⚠️  No statistics to summarize")
        return
        
    output_file = base_dir / 'rq2_experiment_summary.csv'
    
    with open(output_file, 'w') as f:
        f.write("Experiment,Algorithm,Type,Scenario,")
        f.write("Throughput_Before,Throughput_After,Throughput_Change_Pct,")
        f.write("QueueDelay_Before,QueueDelay_After,QueueDelay_Change_Pct\n")
        
        for stats in all_stats:
            exp = stats['experiment']
            alg = stats['algorithm']
            exp_type = stats['type']
            scenario = stats['scenario']
            
            # Throughput data
            if 'throughput' in stats:
                tput_before = stats['throughput']['before']['mean']
                tput_after = stats['throughput']['after']['mean']
                tput_change_pct = ((tput_after - tput_before) / tput_before) * 100
            else:
                tput_before = tput_after = tput_change_pct = 'N/A'
            
            # Queue delay data
            if 'queue_delay' in stats:
                qdelay_before = stats['queue_delay']['before']['mean']
                qdelay_after = stats['queue_delay']['after']['mean']
                qdelay_change_pct = ((qdelay_after - qdelay_before) / qdelay_before) * 100 if qdelay_before > 0 else 0
            else:
                qdelay_before = qdelay_after = qdelay_change_pct = 'N/A'
            
            f.write(f"{exp},{alg},{exp_type},{scenario},")
            f.write(f"{tput_before},{tput_after},{tput_change_pct},")
            f.write(f"{qdelay_before},{qdelay_after},{qdelay_change_pct}\n")
    
    print(f"  ✓ CSV summary saved: {output_file.name}")

def main():
    parser = argparse.ArgumentParser(description='Enhanced RQ2 Analysis with Robust Plot Generation')
    parser.add_argument(
        '--exp-dir',
        type=str,
        help='Specific experiment directory to analyze (e.g., results/rq2/P-B1)'
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
        
        print("=== RQ2 Single Experiment Analysis ===")
        analyze_individual_experiment(exp_dir, args.verbose)
        print("✅ Analysis complete!")
        
    else:
        # Full analysis of all experiments
        base_dir = Path(__file__).parent.parent / "rq2"
        if not base_dir.exists():
            print(f"❌ Error: Directory {base_dir} does not exist")
            sys.exit(1)

        print("🚀 === RQ2 ENHANCED ANALYSIS === 🚀")
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
            stats = analyze_individual_experiment(exp_dir, args.verbose)
            if stats:
                all_stats.append(stats)

        print(f"\n✅ Individual analysis complete: {len(all_stats)} experiments processed")
        
        # Comparison plots
        create_comparison_plots(base_dir)
        
        # Summary
        print("\n📄 === CREATING SUMMARY ===")
        create_summary_csv(base_dir, all_stats)
        
        print(f"\n🎉 === ANALYSIS COMPLETE === 🎉")
        print(f"📁 Results saved in: {base_dir}")
        print(f"📊 Individual plots: {len(all_stats)} files")
        print(f"🔄 Comparison plots: Up to 8 scenarios")
        print(f"📄 Summary: rq2_experiment_summary.csv")
        print("\nReady for publication! 🚀")

if __name__ == '__main__':
    main() 