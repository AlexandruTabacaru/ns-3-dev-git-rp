# RQ1 Experiment Matrix

Each run is a single, long-lived bulk transfer (unlimited bytes) on a 100 Mbps bottleneck with DualPI2 (Prague) or FqCoDel (Cubic, by disabling DualPI2). No background flows.

| #  | Algorithm | Base Delay | Jitter | Test ID |
|----|-----------|------------|--------|---------|
| 1  | Prague    | 10 ms      | 0 ms   | P-M0    |
| 2  | Prague    | 10 ms      | ±1 ms  | P-M1    |
| 3  | Prague    | 10 ms      | ±5 ms  | P-M5    |
| 4  | Prague    | 40 ms      | 0 ms   | P-H0    |
| 5  | Prague    | 40 ms      | ±1 ms  | P-H1    |
| 6  | Prague    | 40 ms      | ±5 ms  | P-H5    |
| 7  | Cubic     | 10 ms      | 0 ms   | C-M0    |
| 8  | Cubic     | 10 ms      | ±1 ms  | C-M1    |
| 9  | Cubic     | 10 ms      | ±5 ms  | C-M5    |
| 10 | Cubic     | 40 ms      | 0 ms   | C-H0    |
| 11 | Cubic     | 40 ms      | ±1 ms  | C-H1    |
| 12 | Cubic     | 40 ms      | ±5 ms  | C-H5    |

> **Legend:**  
> P = Prague, C = Cubic  
> M/H = Medium/High base delay  
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
    - For 10ms base delay: 500,000 bytes (BDP = 250KB at 20ms RTT)
    - For 40ms base delay: 2,000,000 bytes (BDP = 1MB at 80ms RTT)
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
  - `UniformRandomVariable` over ±`jitterUs` on bottleneck link only
  - `UpdateChannelDelay(...)` scheduled every 1 ms

- **Simulation**  
  - `Duration`: 60 s  
  - `Random seed (rngRun)`: 1

- **Analysis**
  - Warm-up period: 5 s (excluded from statistics)
  - Teardown period: 5 s (excluded from statistics)
  - Metrics: Interface rate and goodput (application-level throughput)
  - Queue delay from DualPI2 L-queue (Prague) or FqCoDel (Cubic)
