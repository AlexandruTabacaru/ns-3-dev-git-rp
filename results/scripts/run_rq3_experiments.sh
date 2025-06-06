#!/bin/bash

# Create results directory if it doesn't exist
mkdir -p results/rq3

# Function to run a single wired fairness experiment
run_wired_experiment() {
    local test_id=$1
    local num_prague=$2
    local num_cubic=$3
    
    echo "Running wired fairness experiment $test_id (Prague: $num_prague, Cubic: $num_cubic)..."
    
    # Run the simulation with RQ3 defaults
    ./ns3 run "l4s-wired-rq3 --numPrague=$num_prague --numCubic=$num_cubic --numBytes=0 --duration=60 --wanLinkDelay=10ms --bottleneckRate=100Mbps --testName=$test_id --showProgress=true --rngRun=1"
    
    # Check if simulation completed successfully
    if [ $? -ne 0 ]; then
        echo "Error: Simulation failed for $test_id"
        exit 1
    fi
    
    # Move results to appropriate directory
    mkdir -p "results/rq3/$test_id"
    mv *.pcap "results/rq3/$test_id/" 2>/dev/null || true
    mv *.dat "results/rq3/$test_id/" 2>/dev/null || true
    
    # Run analysis for this experiment
    echo "Analyzing results for $test_id..."
    cd results/scripts
    python3 analyze_rq3.py --exp-dir "../rq3/$test_id" --test-type wired
    cd ../..
}

# Function to run a single WiFi loss-sensitivity experiment
run_wifi_experiment() {
    local test_id=$1
    local algorithm=$2
    local error_rate=$3
    
    # Calculate percentage for display
    local error_percent=$(echo "scale=2; $error_rate * 100" | bc -l)
    
    echo "Running WiFi loss-sensitivity experiment $test_id ($algorithm, ${error_percent}% loss)..."
    
    # Set up parameters based on algorithm
    if [ "$algorithm" = "Prague" ]; then
        num_prague=1
        num_cubic=0
    else
        num_prague=0
        num_cubic=1
    fi
    
    # Use the error rate directly as decimal fraction
    error_decimal="$error_rate"
    
    # Run the simulation with RQ3 WiFi defaults
    ./ns3 run "l4s-wifi-rq3 --numPrague=$num_prague --numCubic=$num_cubic --numBytes=0 --duration=60 --wanLinkDelay=10ms --mcs=2 --channelWidth=20 --spatialStreams=1 --errorRate=$error_decimal --testName=$test_id --showProgress=true --rngRun=1 --enableTraces=true"
    
    # Check if simulation completed successfully
    if [ $? -ne 0 ]; then
        echo "Error: Simulation failed for $test_id"
        exit 1
    fi
    
    # Move results to appropriate directory
    mkdir -p "results/rq3/$test_id"
    mv *.pcap "results/rq3/$test_id/" 2>/dev/null || true
    mv *.dat "results/rq3/$test_id/" 2>/dev/null || true
    
    # Run analysis for this experiment
    echo "Analyzing results for $test_id..."
    cd results/scripts
    python3 analyze_rq3.py --exp-dir "../rq3/$test_id" --test-type wifi
    cd ../..
}

echo "=== Starting RQ3 Experiments ==="
echo "Testing fairness and loss-sensitivity for L4S vs legacy TCP"
echo

# =======================
# WIRED FAIRNESS EXPERIMENTS
# =======================
echo "=== Wired Fairness Experiments ==="

# Prague vs Cubic coexistence tests
#run_wired_experiment "P-FC1" 1 1   # 1 Prague vs 1 Cubic
#run_wired_experiment "P-FC4" 1 4   # 1 Prague vs 4 Cubic  
#run_wired_experiment "P-FC8" 1 8   # 1 Prague vs 8 Cubic

# Prague-only fairness tests
#run_wired_experiment "P-FP2" 2 0   # 2 Prague flows
#run_wired_experiment "P-FP4" 4 0   # 4 Prague flows
#run_wired_experiment "P-FP8" 8 0   # 8 Prague flows

# Mixed scenarios
#run_wired_experiment "P-FMIX"  2 2  # 2 Prague + 2 Cubic
#run_wired_experiment "P-FMIX2" 3 3  # 3 Prague + 3 Cubic  
#run_wired_experiment "P-FMIX3" 4 2  # 4 Prague + 2 Cubic

echo "Wired fairness experiments completed!"
echo

# =======================
# WIFI LOSS-SENSITIVITY EXPERIMENTS  
# =======================
echo "=== WiFi Loss-Sensitivity Experiments ==="

# Prague loss-sensitivity tests
run_wifi_experiment "P-WLS1" "Prague" "0.001"  # Prague with 0.1% loss
run_wifi_experiment "P-WLS2" "Prague" "0.01"    # Prague with 1% loss

# Cubic loss-sensitivity tests
run_wifi_experiment "C-WLS1" "Cubic" "0.001"   # Cubic with 0.1% loss
run_wifi_experiment "C-WLS2" "Cubic" "0.01"     # Cubic with 1% loss

echo "WiFi loss-sensitivity experiments completed!"
echo

# =======================
# COMBINED ANALYSIS
# =======================
echo "=== Running Combined Analysis ==="
cd results/scripts

# Run combined fairness analysis for all wired experiments
echo "Analyzing wired fairness results..."
python3 analyze_rq3.py --exp-dir "../rq3" --test-type wired --combined

# Run combined loss-sensitivity analysis for all WiFi experiments  
echo "Analyzing WiFi loss-sensitivity results..."
python3 analyze_rq3.py --exp-dir "../rq3" --test-type wifi --combined

cd ../..

echo
echo "=== RQ3 Experiments Complete ==="
echo "Results saved in: results/rq3/"
echo "Analysis plots and summaries generated for:"
echo "  - Wired fairness (Jain's fairness index)"
echo "  - WiFi loss-sensitivity (throughput degradation)"
echo "  - Individual experiment details"
echo "  - Combined comparative analysis" 