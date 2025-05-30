# RQ1 Experiment Matrix

Each run is a single, long-lived bulk transfer (unlimited bytes) on a 100 Mbps bottleneck with DualPI2 (Prague) or FqCoDel (Cubic, by disabling DualPI2). No background flows.

| #  | Algorithm | Base Delay | Jitter | Test ID |
|----|-----------|------------|--------|---------|
| 1  | Prague    | 10 ms      | 0 ms   | P-L0    |
| 2  | Prague    | 10 ms      | ±1 ms  | P-L1    |
| 3  | Prague    | 10 ms      | ±5 ms  | P-L5    |
| 4  | Prague    | 20 ms      | 0 ms   | P-M0    |
| 5  | Prague    | 20 ms      | ±1 ms  | P-M1    |
| 6  | Prague    | 20 ms      | ±5 ms  | P-M5    |
| 7  | Prague    | 40 ms      | 0 ms   | P-H0    |
| 8  | Prague    | 40 ms      | ±1 ms  | P-H1    |
| 9  | Prague    | 40 ms      | ±5 ms  | P-H5    |
| 10 | Cubic     | 10 ms      | 0 ms   | C-L0    |
| 11 | Cubic     | 10 ms      | ±1 ms  | C-L1    |
| 12 | Cubic     | 10 ms      | ±5 ms  | C-L5    |
| 13 | Cubic     | 20 ms      | 0 ms   | C-M0    |
| 14 | Cubic     | 20 ms      | ±1 ms  | C-M1    |
| 15 | Cubic     | 20 ms      | ±5 ms  | C-M5    |
| 16 | Cubic     | 40 ms      | 0 ms   | C-H0    |
| 17 | Cubic     | 40 ms      | ±1 ms  | C-H1    |
| 18 | Cubic     | 40 ms      | ±5 ms  | C-H5    |

> **Legend:**  
> P = Prague, C = Cubic  
> L/M/H = Low/Medium/High base delay  
> Number suffix = jitter amplitude (ms)

---

## Common Settings

- **Bottleneck link**  
  - `DataRate`: 100 Mbps  
  - `QueueDisc`: DualPI2 (Prague) / FqCoDel (Cubic)
  - `QueueSize`: Default (1000 packets for DualPI2, 10240 packets for FqCoDel)

- **TCP configuration**  
  - `SegmentSize`: 1448 bytes  
 - `SndBufSize`: Dynamic (2 × BDP)  
    - for 10 ms one-way (20 ms RTT): BDP=250 KB → 500 KB buffer  
    - for 20 ms one-way (40 ms RTT): BDP=500 KB → 1 MB buffer  
    - for 40 ms one-way (80 ms RTT): BDP=1 MB   → 2 MB buffer 
  - `RcvBufSize`: Same as SndBufSize
  - `EnablePacing`: true  
  - `PaceInitialWindow`: true

- **Applications**  
  - **BulkSendApplication**  
    - `MaxBytes`: 0 (unlimited)  
    - `EnableSeqTsSizeHeader`: true  
    - `SendSize`: 1448 bytes  
  - **PacketSink**  
    - `EnableSeqTsSizeHeader`: true

- **RTT jitter injection**  
  - Jitter applied **only on the Server → R1 link** (the 10 ms / 40 ms leg that dominates RTT).  
  - Delay offset follows a bounded random walk with step = 10 % of the amplitude.  
  - Amplitude set via `--jitterUs` (micro-seconds); the wrapper script converts the *ms* values in the table to *µs* (`jitter_us = jitter_ms × 1000`).  
  - `UpdateChannelDelay()` reschedules every **10 ms**.

- **Simulation**  
  - `Duration`: 60 s  
  - `Random seed (rngRun)`: 1

- **Analysis**
  - Warm-up period: 5 s (excluded from statistics)
  - Teardown period: 5 s (excluded from statistics)
  - Metrics: Interface rate and goodput (application-level throughput)
  - Queue delay from DualPI2 L-queue (Prague) or FqCoDel (Cubic)
