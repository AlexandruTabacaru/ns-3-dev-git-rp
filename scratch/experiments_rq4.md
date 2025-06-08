# RQ4: Prague vs BBR Fairness in L4S

**Research question:**  
How do competing scalable controllers (TCP Prague and ECN-enabled BBRv3) interact within the L4S framework in terms of fairness when sharing a wired bottleneck link?

---

## 1. Wired (ns-3 “l4s-wired”)

Tests use a 100 Mb/s bottleneck link with fixed 10 ms one-way delay (≈ 20 ms RTT), no jitter. All flows are long-lived bulk transfers (`MaxBytes = 0`), run for 60 s.

### 1.1 Fairness & Coexistence

| #  | Prague Flows | BBRv3 Flows | Test ID |
|:--:|:------------:|:-----------:|:--------:|
| 1  | 1            | 1           | P1-B1    |
| 2  | 1            | 2           | P1-B2    |
| 3  | 2            | 1           | P2-B1    |
| 4  | 1            | 4           | P1-B4    |
| 5  | 4            | 1           | P4-B1    |
| 6  | 2            | 2           | P2-B2    |
| 7  | 4            | 4           | P4-B4    |

> **Notes (wired):**  
> – QueueDisc: DualPI2 only (both Prague and BBRv3 share a single DualPI2)  
> – Measure per-flow steady-state throughput and queue sojourn time  
> – Each flow is a `BulkSendApplication` with `MaxBytes = 0`, start at t=1 s  

---

## Common Settings (all runs)

- **TCP**  
  - `SegmentSize = 1448 B`  
  - `SndBufSize = RcvBufSize = 750 000 B`  
  - `EnablePacing = true`  
  - `PaceInitialWindow = true`  

- **Applications**  
  - **BulkSendApplication**:  
    - `MaxBytes = 0`  
    - `EnableSeqTsSizeHeader = true`  
    - `SendSize = 1448`  
  - **PacketSink**:  
    - `EnableSeqTsSizeHeader = true`  

- **Simulation**  
  - `Duration = 60 s`  
  - `rngRun = 1`  
