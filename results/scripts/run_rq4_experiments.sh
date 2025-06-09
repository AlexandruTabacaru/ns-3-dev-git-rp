#!/bin/bash

# RQ4: Prague vs BBRv3 Fairness in L4S - Experiment Runner
# Research Question: How do competing scalable controllers (TCP Prague and ECN-enabled BBRv3) 
# interact within the L4S framework in terms of fairness when sharing a wired bottleneck link?

# Create results directory if it doesn't exist
mkdir -p results/rq4

# Function to run a single RQ4 fairness experiment
run_rq4_experiment() {
    local test_id=$1
    local num_prague=$2
    local num_bbr=$3
    
    echo "Running RQ4 fairness experiment $test_id (Prague: $num_prague, BBRv3: $num_bbr)..."
    
    # Run the simulation with RQ4 defaults
    ./ns3 run "l4s-wired-rq4 --numPrague=$num_prague --numBbr=$num_bbr --numBytes=0 --duration=30s --wanLinkDelay=10ms --bottleneckRate=100Mbps --testName=$test_id --showProgress=true --rngRun=1"
    
    # Check if simulation completed successfully
    if [ $? -ne 0 ]; then
        echo "Error: Simulation failed for $test_id"
        exit 1
    fi
    
    # Move results to appropriate directory
    mkdir -p "results/rq4/$test_id"
    mv *.pcap "results/rq4/$test_id/" 2>/dev/null || true
    mv *.dat "results/rq4/$test_id/" 2>/dev/null || true
    
    # Quick verification of key files
    key_files=("prague-throughput.$test_id.dat" "bbr-throughput.$test_id.dat" "prague-per-flow-throughput.$test_id.dat" "bbr-per-flow-throughput.$test_id.dat")
    missing_files=0
    
    for file in "${key_files[@]}"; do
        if [ ! -f "results/rq4/$test_id/$file" ]; then
            echo "  ⚠️  Warning: Missing key file $file"
            missing_files=$((missing_files + 1))
        fi
    done
    
    if [ $missing_files -eq 0 ]; then
        echo "  ✅ All key output files generated successfully"
    else
        echo "  ⚠️  $missing_files key files missing - check simulation logs"
    fi
    
    echo "  📁 Results saved in: results/rq4/$test_id/"
}

# Function to validate simulation setup
validate_setup() {
    echo "🔧 Validating RQ4 experiment setup..."
    
    # Check if ns-3 build exists
    if [ ! -f "./ns3" ]; then
        echo "❌ Error: ns-3 build script not found. Please build ns-3 first."
        exit 1
    fi
    
    # Check if l4s-wired-rq4 simulation exists
    if [ ! -f "scratch/l4s-wired-rq4.cc" ]; then
        echo "❌ Error: RQ4 simulation script not found at scratch/l4s-wired-rq4.cc"
        exit 1
    fi
    
    # Test build
    echo "  🔨 Testing ns-3 build..."
    ./ns3 build > /dev/null 2>&1
    if [ $? -ne 0 ]; then
        echo "❌ Error: ns-3 build failed. Please fix build errors first."
        exit 1
    fi
    
    echo "  ✅ Setup validation complete"
}

# Function to create experiment summary
create_experiment_summary() {
    echo "📊 Creating experiment summary..."
    
    summary_file="results/rq4/experiment_summary.txt"
    
    cat > "$summary_file" << EOF
RQ4: Prague vs BBRv3 Fairness in L4S - Experiment Summary
==========================================================

Research Question:
How do competing scalable controllers (TCP Prague and ECN-enabled BBRv3) 
interact within the L4S framework in terms of fairness when sharing a 
wired bottleneck link?

Test Configuration:
- Bottleneck: 100 Mbps, 20ms RTT (10ms one-way delay)
- Duration: 60 seconds
- Traffic: Long-lived bulk transfers (MaxBytes = 0)
- Queue: Single DualPI2 (both algorithms use L4S queue)
- Random seed: 1

Experiments Conducted:
======================

Test ID  | Prague Flows | BBRv3 Flows | Description
---------|--------------|-------------|---------------------------
P1-B1    | 1            | 1           | Balanced competition
P1-B2    | 1            | 2           | Prague minority
P2-B1    | 2            | 1           | BBRv3 minority  
P1-B4    | 1            | 4           | Heavy BBRv3 load
P4-B1    | 4            | 1           | Heavy Prague load
P2-B2    | 2            | 2           | Balanced multiple flows
P4-B4    | 4            | 4           | Heavy mixed load

Analysis Focus:
- Jain's Fairness Index between algorithms
- Per-flow throughput distribution
- Queueing delay comparison (enhanced analytics)
- Algorithm-specific behavior patterns

Expected Outcomes:
- Both algorithms should achieve good fairness (JFI > 0.8)
- Similar queueing delay characteristics (both use L4S)
- Stable coexistence without starvation

Generated: $(date)
EOF

    echo "  📄 Summary saved: $summary_file"
}

echo "🚀 === RQ4: Prague vs BBRv3 L4S Fairness Experiments ==="
echo "Testing fairness between two scalable congestion controllers in L4S"
echo

# Validate setup before starting
validate_setup

echo "📋 Experiment Plan:"
echo "  - 7 fairness test cases (Prague vs BBRv3 combinations)"
echo "  - Focus on L4S-to-L4S fairness (both algorithms)"
echo "  - Enhanced queueing delay analytics"
echo

read -p "🤔 Proceed with RQ4 experiments? (y/N): " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]; then
    echo "❌ Experiments cancelled"
    exit 0
fi

# Record start time
start_time=$(date)
echo "⏰ Experiments started at: $start_time"
echo

# =======================
# RQ4 FAIRNESS EXPERIMENTS  
# =======================
echo "=== RQ4 Fairness & Coexistence Experiments ==="

# Test 1: P1-B1 - Balanced competition (1 Prague vs 1 BBRv3)
run_rq4_experiment "P1-B1" 1 1

# Test 2: P1-B2 - Prague minority (1 Prague vs 2 BBRv3)  
run_rq4_experiment "P1-B2" 1 2

# Test 3: P2-B1 - BBRv3 minority (2 Prague vs 1 BBRv3)
run_rq4_experiment "P2-B1" 2 1

# Test 4: P1-B4 - Heavy BBRv3 load (1 Prague vs 4 BBRv3)
run_rq4_experiment "P1-B4" 1 4

# Test 5: P4-B1 - Heavy Prague load (4 Prague vs 1 BBRv3)
run_rq4_experiment "P4-B1" 4 1

# Test 6: P2-B2 - Balanced multiple flows (2 Prague vs 2 BBRv3)
run_rq4_experiment "P2-B2" 2 2

# Test 7: P4-B4 - Heavy mixed load (4 Prague vs 4 BBRv3)
run_rq4_experiment "P4-B4" 4 4

echo "✅ All RQ4 fairness experiments completed!"
echo

# =======================
# ANALYSIS PREPARATION
# =======================
echo "=== Analysis Preparation ==="

# Create experiment summary
create_experiment_summary

# Copy analysis script if it exists
if [ -f "results/scripts/analyze_rq4.py" ]; then
    echo "📊 Analysis script available: results/scripts/analyze_rq4.py"
    echo "   Run with: cd results/scripts && python3 analyze_rq4.py --base-dir ../rq4"
else
    echo "⚠️  Analysis script not found. Create analyze_rq4.py for detailed analysis."
fi

# Calculate total runtime
end_time=$(date)
echo
echo "⏰ Experiments completed at: $end_time"
echo "🎯 Started: $start_time"

# Quick file count summary
echo
echo "📁 === Experiment Results Summary ==="
for test_id in "P1-B1" "P1-B2" "P2-B1" "P1-B4" "P4-B1" "P2-B2" "P4-B4"; do
    if [ -d "results/rq4/$test_id" ]; then
        file_count=$(find "results/rq4/$test_id" -name "*.dat" | wc -l)
        echo "  $test_id: $file_count data files"
    else
        echo "  $test_id: ❌ Directory missing"
    fi
done

echo
echo "🎉 === RQ4 Experiments Complete ==="
echo "📂 Results location: results/rq4/"
echo "🔬 Ready for analysis:"
echo "   1. Individual experiment data in results/rq4/[TEST_ID]/"
echo "   2. Run analysis script: cd results/scripts && python3 analyze_rq4.py"
echo "   3. Key metrics: Jain's fairness, per-flow throughput, queueing delays"
echo
echo "🎯 Focus Areas for Analysis:"
echo "   - Prague-BBRv3 fairness comparison"
echo "   - Algorithm-specific queueing behavior"  
echo "   - Scalability under different flow ratios"
echo "   - L4S framework effectiveness for mixed scalable traffic" 