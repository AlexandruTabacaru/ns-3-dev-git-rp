#!/bin/bash

# Create results directory if it doesn't exist
mkdir -p results/rq2

# Function to run a single wired experiment
run_wired_experiment() {
    local test_id=$1
    local algorithm=$2
    local init_rate=$3
    local step_rate=$4
    
    echo "Running wired experiment $test_id..."
    
    # Set up parameters based on algorithm
    if [ "$algorithm" = "Prague" ]; then
        num_prague=1
        num_cubic=0
    else
        num_prague=0
        num_cubic=1
    fi
    
    # Run the simulation
    ./ns3 run "l4s-wired-rq2 --numPrague=$num_prague --numCubic=$num_cubic --initRate=${init_rate}Mbps --stepRate=${step_rate}Mbps --wanLinkDelay=20ms --duration=40 --testName=$test_id --showProgress=true --rngRun=1"
    
    # Check if simulation completed successfully
    if [ $? -ne 0 ]; then
        echo "Error: Simulation failed for $test_id"
        exit 1
    fi
    
    # Move results to appropriate directory
    mkdir -p "results/rq2/$test_id"
    mv *.pcap "results/rq2/$test_id/" 2>/dev/null || true
    mv *.dat "results/rq2/$test_id/" 2>/dev/null || true
    
    # Run analysis for this experiment
    echo "Analyzing results for $test_id..."
    cd results/scripts
    python3 analyze_rq2.py --exp-dir "../rq2/$test_id"
    cd ../..
}

# Function to run a single WiFi experiment
run_wifi_experiment() {
    local test_id=$1
    local algorithm=$2
    local mcs=$3
    local second_mcs=$4
    
    echo "Running WiFi experiment $test_id..."
    
    # Set up parameters based on algorithm
    if [ "$algorithm" = "Prague" ]; then
        num_prague=1
        num_cubic=0
    else
        num_prague=0
        num_cubic=1
    fi
    
    # Run the simulation
    ./ns3 run "l4s-wifi-rq2 --numPrague=$num_prague --numCubic=$num_cubic --mcs=$mcs --secondMcs=$second_mcs --channelWidth=20 --spatialStreams=1 --wanLinkDelay=10ms --duration=40 --testName=$test_id --showProgress=true --rngRun=1 --enableTracesAll=true"
    
    # Check if simulation completed successfully
    if [ $? -ne 0 ]; then
        echo "Error: Simulation failed for $test_id"
        exit 1
    fi
    
    # Move results to appropriate directory
    mkdir -p "results/rq2/$test_id"
    mv *.pcap "results/rq2/$test_id/" 2>/dev/null || true
    mv *.dat "results/rq2/$test_id/" 2>/dev/null || true
    
    # Run analysis for this experiment
    echo "Analyzing results for $test_id..."
    cd results/scripts
    python3 analyze_rq2.py --exp-dir "../rq2/$test_id"
    cd ../..
}

# Run wired experiments
# Prague experiments
run_wired_experiment "P-B1" "Prague" 25 100
run_wired_experiment "P-B2" "Prague" 100 25

# Cubic experiments
run_wired_experiment "C-B1" "Cubic" 25 100
run_wired_experiment "C-B2" "Cubic" 100 25

#Run WiFi experiments
#Prague experiments
run_wifi_experiment "P-W1" "Prague" 2 7
run_wifi_experiment "P-W2" "Prague" 7 2
run_wifi_experiment "P-W3" "Prague" 4 9
run_wifi_experiment "P-W4" "Prague" 9 4
run_wifi_experiment "P-W5" "Prague" 4 7
run_wifi_experiment "P-W6" "Prague" 7 4

# Cubic experiments
run_wifi_experiment "C-W1" "Cubic" 2 7
run_wifi_experiment "C-W2" "Cubic" 7 2
run_wifi_experiment "C-W3" "Cubic" 4 9
run_wifi_experiment "C-W4" "Cubic" 9 4
run_wifi_experiment "C-W5" "Cubic" 4 7
run_wifi_experiment "C-W6" "Cubic" 7 4

# Create combined analysis plots
echo "Creating combined analysis plots..."
cd results/scripts
python3 analyze_rq2.py --exp-dir "../rq2"
cd ../..

echo "All experiments completed and analyzed!" 