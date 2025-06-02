# RQ2 – Effect of Rapid Bandwidth Fluctuations on L4S vs. Legacy TCP

**Research question**  
*What is the effect of a sudden (single-step) bandwidth change on throughput stability and queueing delay of L4S (Prague) versus legacy TCP (Cubic)?*

---

## 1  Wired scenario (ns-3 `l4s-wired`)

Each run is a single long-lived bulk transfer on a wired bottleneck link.  
The rate changes **once at *t = 20 s***. No background traffic.

| # | Alg. | Bottleneck rate change | Change @ | One-way delay | Jitter | Test ID |
|---|------|-----------------------|---------:|--------------:|-------:|:------:|
| 1 | Prague | 25 → 100 Mb/s | 20 s | 20 ms | 0 µs | **P-B1** |
| 2 | Cubic  | 25 → 100 Mb/s | 20 s | 20 ms | 0 µs | **C-B1** |
| 3 | Prague | 100 → 25 Mb/s | 20 s | 20 ms | 0 µs | **P-B2** |
| 4 | Cubic  | 100 → 25 Mb/s | 20 s | 20 ms | 0 µs | **C-B2** |

> **Notes (wired)**  
> • Fixed RTT ≈ 40 ms (20 ms each way)  
> • DualPI2 (Prague) vs. FqCoDel (Cubic)  
> • Rate change injected with a custom helper

---

## 2  Wi-Fi scenario (ns-3 `l4s-wifi` – 802.11ax 20 MHz / 1 SS)

The AP's MCS changes **once at *t = 20 s***.  
Approx. PHY rates (20 MHz, 1 SS): MCS 2 ≈ 19 Mb/s, MCS 4 ≈ 39 Mb/s, MCS 7 ≈ 78 Mb/s, MCS 9 ≈ 102 Mb/s.

| # | Alg. | MCS change | Change @ | WAN delay | Test ID |
|---|------|------------|---------:|----------:|:------:|
| 1 | Prague | 2 → 7 | 20 s | 10 ms | **P-W1** |
| 2 | Prague | 7 → 2 | 20 s | 10 ms | **P-W2** |
| 3 | Prague | 4 → 9 | 20 s | 10 ms | **P-W3** |
| 4 | Prague | 9 → 4 | 20 s | 10 ms | **P-W4** |
| 5 | Prague | 4 → 7 | 20 s | 10 ms | **P-W5** |
| 6 | Prague | 7 → 4 | 20 s | 10 ms | **P-W6** |
| 7 | Cubic  | 2 → 7 | 20 s | 10 ms | **C-W1** |
| 8 | Cubic  | 7 → 2 | 20 s | 10 ms | **C-W2** |
| 9 | Cubic  | 4 → 9 | 20 s | 10 ms | **C-W3** |
| 10| Cubic  | 9 → 4 | 20 s | 10 ms | **C-W4** |
| 11| Cubic  | 4 → 7 | 20 s | 10 ms | **C-W5** |
| 12| Cubic  | 7 → 4 | 20 s | 10 ms | **C-W6** |

> **Notes (Wi-Fi)**  
> • Fixed RTT ≈ 20 ms (10 ms each way)  
> • `MqQueueDisc` with DualPI2 child for both algorithms  
> • MCS switched with `ChangeMcs()` once at 20 s

---

## Common simulation settings

| Parameter | Value / Comment |
|-----------|-----------------|
| **Duration** | **40 s** (20 s before change + 20 s after) |
| **TCP** | `SegmentSize` 1448 B; pacing **on**; `SndBufSize` & `RcvBufSize` 750 kB (≈ 1.5 × BDP for 100 Mb/s, 40 ms) |
| **Applications** | `BulkSendApplication` (`MaxBytes` 0, `SendSize` 1448 B); `PacketSink` with timestamps enabled |
| **Random seed (`rngRun`)** | 1 |

---