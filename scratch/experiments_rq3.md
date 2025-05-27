
# RQ3: Mixed-Traffic Fairness & Loss-Sensitivity

**Research question:**  
How does L4S (Prague) compare to legacy TCP (Cubic) in:  
1. **Fairness** when sharing a bottleneck with classic or other L4S flows, and  
2. **Loss-sensitivity** (performance degradation under small random drop rates)?

---

## 1. Wired (ns-3 “l4s-wired”)

Tests use a 100 Mb/s bottleneck link with fixed 20 ms one-way delay (≈40 ms RTT), no jitter. All flows are long-lived bulk transfers (`MaxBytes = 0`).

### 1.1 Fairness & Coexistence

| #  | Prague Flows | Cubic Flows | Test ID |
|:--:|:------------:|:-----------:|:-------:|
| 1  | 1            | 1           | P-FC1   |
| 2  | 1            | 4           | P-FC4   |
| 3  | 1            | 8           | P-FC8   |
| 4  | 2            | 0           | P-FP2   |
| 5  | 4            | 0           | P-FP4   |
| 6  | 2            | 2           | P-FMIX  |
| 7  | 3            | 3           | P-FMIX2 |
| 8  | 4            | 2           | P-FMIX3 |
| 9  | 8            | 0           | P-FP8   |

> **Notes (wired):**  
> – QueueDisc: DualPI2 (Prague) / FqCoDel (Cubic)  
> – Measure throughput fairness and queue sojourn time  
> – Each flow is a BulkSendApplication with `MaxBytes = 0`

---

## 2. Wi-Fi (ns-3 “l4s-wifi”)

Tests use an 802.11ax link (20 MHz, MCS 2, 1 SS), one-way delay 10 ms (≈20 ms RTT), no added jitter. Focus only on **loss-sensitivity**.

### 2.1 Loss-Sensitivity

| #  | Algorithm | Drop Rate | Test ID  |
|:--:|:---------:|----------:|:--------:|
| 1  | Prague    | 0.1 %     | P-WLS1   |
| 2  | Prague    | 1 %       | P-WLS2   |
| 3  | Cubic     | 0.1 %     | C-WLS1   |
| 4  | Cubic     | 1 %       | C-WLS2   |

> **Notes (Wi-Fi):**  
> – Random losses injected via uniform error model on the Wi-Fi link  
> – QueueDisc: MqQueueDisc → DualPI2 child  

---

## Common Settings (all runs)

- **TCP**  
  - `SegmentSize` = 1448 B  
  - `SndBufSize`, `RcvBufSize` = 750 000 B  
  - `EnablePacing` = true; `PaceInitialWindow` = true  

- **Applications**  
  - **BulkSendApplication**: `MaxBytes` = 0; `EnableSeqTsSizeHeader` = true  
  - **PacketSink**: `EnableSeqTsSizeHeader` = true  

- **Simulation**  
  - `Duration` = 60 s  
  - `Random seed (rngRun)` = 1  
