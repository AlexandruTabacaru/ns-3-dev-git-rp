#!/bin/bash

# Create results directory if it doesn't exist
mkdir -p results/rq1

# Function to run a single experiment
run_experiment() {
    local test_id=$1
    local algorithm=$2
    local base_delay=$3
    local jitter=$4
    
    echo "Running experiment $test_id..."
    
    # Set up parameters based on algorithm
    if [ "$algorithm" = "Prague" ]; then
        num_prague=1
        num_cubic=0
        enable_dualpi2=true
    else
        num_prague=0
        num_cubic=1
        enable_dualpi2=false
    fi
    
    # Convert jitter to microseconds
    jitter_us=$((jitter * 1000))
    
    # Run the simulation with explicit duration and unlimited bytes
    ./ns3 run "l4s-wired-jitter --numPrague=$num_prague --numCubic=$num_cubic --wanLinkDelay=${base_delay}ms --jitterUs=$jitter_us --duration=60 --testName=$test_id --enableDualPI2=$enable_dualpi2 --showProgress=true --rngRun=1"
    
    # Check if simulation completed successfully
    if [ $? -ne 0 ]; then
        echo "Error: Simulation failed for $test_id"
        exit 1
    fi
    
    # Move results to appropriate directory
    mkdir -p "results/rq1/$test_id"
    mv *.pcap "results/rq1/$test_id/" 2>/dev/null || true
    mv *.dat "results/rq1/$test_id/" 2>/dev/null || true
    
    # Run analysis for this experiment
    echo "Analyzing results for $test_id..."
    cd results/scripts
    python3 analyze_rq1.py --exp-dir "../rq1/$test_id"
    cd ../..
}

# Run all experiments from the matrix
# Prague experiments
run_experiment "P-L0" "Prague" 10 0
run_experiment "P-L1" "Prague" 10 1
run_experiment "P-L5" "Prague" 10 5
run_experiment "P-M0" "Prague" 20 0
run_experiment "P-M1" "Prague" 20 1
run_experiment "P-M5" "Prague" 20 5
run_experiment "P-H0" "Prague" 40 0
run_experiment "P-H1" "Prague" 40 1
run_experiment "P-H5" "Prague" 40 5

# Cubic experiments
run_experiment "C-L0" "Cubic" 10 0
run_experiment "C-L1" "Cubic" 10 1
run_experiment "C-L5" "Cubic" 10 5
run_experiment "C-M0" "Cubic" 20 0
run_experiment "C-M1" "Cubic" 20 1
run_experiment "C-M5" "Cubic" 20 5
run_experiment "C-H0" "Cubic" 40 0
run_experiment "C-H1" "Cubic" 40 1
run_experiment "C-H5" "Cubic" 40 5

echo "All experiments completed!"
echo "Now run: cd results/scripts && python analyze_rq1_enhanced.py" 