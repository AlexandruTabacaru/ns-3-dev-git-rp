
== Simulation Campaign "1,2,3"

Basic L4S implementation (BE queues only – single AP & single station, with & without OBSS interference)

Purpose: explore L4S implementation with EDCA updates

Campaign Definition:  All 1080 combinations of the following simulation variables:  

* AP Settings (*AP*):
** *AP0*: L4S (DualPI2) disabled
** *AP1*: L4S (DualPI2) enabled
* Traffic Test Cases (*TC*):
** *TC1*: (baseline): single STA with single classic flow
** *TC2*: single STA with single L4S flow  
** *TC3*: single station with 1 L4S and 1 classic flows
** *TC4*: single station with 2 L4S and 2 classic flows
** *TC5*: single station with 4 L4S and 4 classic flows
** All long-running flows for 30 seconds, capturing startup behavior
* Topology Settings (*TS*):
** *TS1*: All with base (wired) RTT of 2 ms
** *TS2*: All with base (wired) RTT of 10 ms
** *TS3*: All with base (wired) RTT of 50 ms
* Link Settings (*LS*):
** *LS1*: 20 MHz MCS 6 
** *LS2*: 80 MHz MCS 6
** *LS3*: 160 MHz MCS 6  
** All with 2 spatial streams, only collision-related retransmissions (no MCS errors/decoding failures), fixed MCS
* OBSS Conditions (*MS*):
** *MS0*: no OBSS
** *MS1*:  Add one additional AP and one associated STA for overlapping traffic 
*** upstream saturating UDP traffic  (600M)
** *MS2*: Similar to MS1, but with one AP and 4 associated STAs, aiming for a fairly high level of channel access contention
*** upstream saturating UDP traffic for each STA (600M / 4)
** no hidden nodes, -45 RSSI, no spatial reuse, 5GHz channel
* EDCA Settings (*ED*):
** *ED0*: CWmin/CWmax = 15,1023, AIFSN=3, TxOpLimit=2.5ms, Aggregation Limit=2.5ms [*default settings*]
** *ED1*: CWmin/CWmax = 63,63, AIFSN=1, TxOpLimit=2.5ms, Aggregation Limit=2.5ms
** *ED2*: CWmin/CWmax = 63,63, AIFSN=1, TxOpLimit=1ms, Aggregation Limit=1ms
** *ED3*: CWmin/CWmax = 63,63, AIFSN=1, TxOpLimit=0.25ms, Aggregation Limit=0.25ms

