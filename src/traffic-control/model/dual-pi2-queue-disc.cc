/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 NITK Surathkal
 * Copyright (c) 2019 Tom Henderson (update to IETF draft -10)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Shravya K.S. <shravya.ks0@gmail.com>
 *
 */

#include "dual-pi2-queue-disc.h"

#include "ns3/abort.h"
#include "ns3/assert.h"
#include "ns3/double.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/enum.h"
#include "ns3/fatal-error.h"
#include "ns3/log.h"
#include "ns3/net-device-queue-interface.h"
#include "ns3/object-factory.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <cmath>
#include <cstddef>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("DualPi2QueueDisc");

NS_OBJECT_ENSURE_REGISTERED(DualPi2QueueDisc);

// Used as an index into the GetInternalQueue() method
constexpr std::size_t CLASSIC = 0;
constexpr std::size_t L4S = 1;
constexpr std::size_t NONE = 2;

TypeId
DualPi2QueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::DualPi2QueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<DualPi2QueueDisc>()
            .AddAttribute("Mtu",
                          "Device MTU (bytes); if zero, will be automatically configured",
                          UintegerValue(0),
                          MakeUintegerAccessor(&DualPi2QueueDisc::m_mtu),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("A",
                          "Value of alpha (Hz)",
                          DoubleValue(0.15),
                          MakeDoubleAccessor(&DualPi2QueueDisc::m_alpha),
                          MakeDoubleChecker<double>())
            .AddAttribute("B",
                          "Value of beta (Hz)",
                          DoubleValue(3),
                          MakeDoubleAccessor(&DualPi2QueueDisc::m_beta),
                          MakeDoubleChecker<double>())
            .AddAttribute("Tupdate",
                          "Time period to calculate drop probability",
                          TimeValue(Seconds(0.015)),
                          MakeTimeAccessor(&DualPi2QueueDisc::m_tUpdate),
                          MakeTimeChecker())
            .AddAttribute("QueueLimit",
                          "Queue limit in bytes",
                          UintegerValue(1562500), // 250 ms at 50 Mbps
                          MakeUintegerAccessor(&DualPi2QueueDisc::m_queueLimit),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Target",
                          "PI AQM Classic queue delay target",
                          TimeValue(MilliSeconds(15)),
                          MakeTimeAccessor(&DualPi2QueueDisc::m_target),
                          MakeTimeChecker())
            .AddAttribute("L4SMarkThresold",
                          "L4S marking threshold in Time",
                          TimeValue(MicroSeconds(475)),
                          MakeTimeAccessor(&DualPi2QueueDisc::m_minTh),
                          MakeTimeChecker())
            .AddAttribute("K",
                          "Coupling factor",
                          DoubleValue(2),
                          MakeDoubleAccessor(&DualPi2QueueDisc::m_k),
                          MakeDoubleChecker<double>())
            .AddAttribute("StartTime", // Only if user wants to change queue start time
                          "Simulation time to start scheduling the update timer",
                          TimeValue(Seconds(0.0)),
                          MakeTimeAccessor(&DualPi2QueueDisc::m_startTime),
                          MakeTimeChecker())
            .AddAttribute("SchedulingWeight",
                          "Scheduling weight to apply to DRR dequeue (L4S closer to 1)",
                          DoubleValue(0.9),
                          MakeDoubleAccessor(&DualPi2QueueDisc::m_schedulingWeight),
                          MakeDoubleChecker<double>(0, 1))
            .AddAttribute("DrrQuantum",
                          "Quantum used in weighted DRR policy (bytes)",
                          UintegerValue(1500),
                          MakeUintegerAccessor(&DualPi2QueueDisc::m_drrQuantum),
                          MakeUintegerChecker<uint32_t>())
            .AddTraceSource("ProbCL",
                            "Coupled probability (p_CL)",
                            MakeTraceSourceAccessor(&DualPi2QueueDisc::m_pCL),
                            "ns3::TracedValueCallback::Double")
            .AddTraceSource("ProbL",
                            "L4S mark probability (p_L)",
                            MakeTraceSourceAccessor(&DualPi2QueueDisc::m_pL),
                            "ns3::TracedValueCallback::Double")
            .AddTraceSource("ProbC",
                            "Classic drop/mark probability (p_C)",
                            MakeTraceSourceAccessor(&DualPi2QueueDisc::m_pC),
                            "ns3::TracedValueCallback::Double")
            .AddTraceSource("ClassicSojournTime",
                            "Sojourn time of the last packet dequeued from the Classic queue",
                            MakeTraceSourceAccessor(&DualPi2QueueDisc::m_traceClassicSojourn),
                            "ns3::Time::TracedCallback")
            .AddTraceSource("L4sSojournTime",
                            "Sojourn time of the last packet dequeued from the L4S queue",
                            MakeTraceSourceAccessor(&DualPi2QueueDisc::m_traceL4sSojourn),
                            "ns3::Time::TracedCallback");
    return tid;
}

DualPi2QueueDisc::DualPi2QueueDisc()
    : QueueDisc()
{
    NS_LOG_FUNCTION(this);
    m_uv = CreateObject<UniformRandomVariable>();
    m_rtrsEvent = Simulator::Schedule(m_startTime, &DualPi2QueueDisc::DualPi2Update, this);
}

DualPi2QueueDisc::~DualPi2QueueDisc()
{
    NS_LOG_FUNCTION(this);
}

void
DualPi2QueueDisc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_rtrsEvent.Cancel();
    QueueDisc::DoDispose();
}

void
DualPi2QueueDisc::SetQueueLimit(uint32_t lim)
{
    NS_LOG_FUNCTION(this << lim);
    m_queueLimit = lim;
}

uint32_t
DualPi2QueueDisc::GetQueueSize() const
{
    NS_LOG_FUNCTION(this);
    return (GetInternalQueue(CLASSIC)->GetNBytes() + GetInternalQueue(L4S)->GetNBytes());
}

int64_t
DualPi2QueueDisc::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_uv->SetStream(stream);
    return 1;
}

void
DualPi2QueueDisc::PendingDequeueCallback(uint32_t oldBytes, uint32_t newBytes)
{
    NS_LOG_FUNCTION(this << newBytes);
    if (!GetNetDeviceQueueInterface() || !GetNetDeviceQueueInterface()->GetTxQueue(0)->IsStopped())
    {
        NS_LOG_DEBUG("Queue is not stopped so no need to process the value");
        return;
    }
    else
    {
        NS_LOG_DEBUG("Queue is stopped; process the reported value " << newBytes);
        // newBytes represents the Wi-Fi framed value of any packet
        // For every QueueDiscItem packet in this queue, add 38 bytes
        // for its queue size below.
        NS_LOG_DEBUG("QueueDisc holds " << GetNBytes() << " bytes in " << GetNPackets()
                                        << " packets");
        uint32_t queueDiscPending = GetNBytes() + 38 * GetNPackets();
        NS_LOG_DEBUG("The amount to be queued at WifiMacQueue is " << queueDiscPending);
        if (newBytes > queueDiscPending)
        {
            NS_LOG_DEBUG("WifiMacQueue can handle the pending " << newBytes);
            return;
        }
    }
    // The current queue size exceeds the pending dequeue.  Determine which
    // packets will be dequeued, and which packets should be marked.
    uint32_t lBytes [[maybe_unused]] = GetInternalQueue(L4S)->GetNBytes();
    uint32_t lPackets [[maybe_unused]] = GetInternalQueue(L4S)->GetNPackets();
    uint32_t cBytes [[maybe_unused]] = GetInternalQueue(CLASSIC)->GetNBytes();
    uint32_t cPackets [[maybe_unused]] = GetInternalQueue(CLASSIC)->GetNPackets();

    NS_LOG_DEBUG("State before PendingDequeue logic: pendingBytes "
                 << newBytes << " l4sBytes " << lBytes << " l4sPackets " << lPackets
                 << " classicBytes " << cBytes << " cPackets " << cPackets);

    // Dequeue enough packets to use up to queueDiscPending bytes, and add to staging queues
    // Keep track of how many of these are L4S packets and are marked
    // Dequeue using the scheduler logic which will apply Laqm and coupled marking, and drops

    uint32_t pendingBytes = newBytes;
    uint32_t markedCount = 0; // How many of those L4S packets are marked
    while (pendingBytes)
    {
        auto scheduled = Scheduler(pendingBytes);
        if (scheduled == L4S)
        {
            bool marked = false;
            auto qdItem = DequeueFromL4sQueue(marked);
            if (qdItem)
            {
                NS_ASSERT_MSG(qdItem->GetSize() <= pendingBytes, "Error, insufficient bytes");
                AddToL4sStagingQueue(qdItem);
                pendingBytes -= qdItem->GetSize();
                if (marked)
                {
                    markedCount++;
                }
            }
        }
        else if (scheduled == CLASSIC)
        {
            bool dropped [[maybe_unused]] = false;
            auto qdItem = DequeueFromClassicQueue(dropped);
            AddToClassicStagingQueue(qdItem);
            pendingBytes -= qdItem->GetSize();
        }
        else
        {
            break;
        }
    }
    // There are 'M' markedCount packets marked in the staging queue.  There are 'N' packets
    // remaining.  If N <= M, then do nothing; at least N are already marked.  Otherwise,
    // traverse the L4S staging queue and mark up to (N-M) more packets.
    if (GetInternalQueue(L4S)->GetNPackets() > markedCount)
    {
        uint32_t pendingMarks = GetInternalQueue(L4S)->GetNPackets() - markedCount;
        NS_LOG_DEBUG("After PendingDequeue logic:  Apply " << pendingMarks << " more marks");
        auto it = m_l4sStagingQueue.begin();
        while (it != m_l4sStagingQueue.end() && pendingMarks)
        {
            uint8_t tosByte = 0;
            if ((*it)->GetUint8Value(QueueItem::IP_DSFIELD, tosByte) && ((tosByte & 0x3) == 1))
            {
                (*it)->Mark();
                pendingMarks--;
            }
            it++;
        }
    }
    else
    {
        NS_LOG_DEBUG("After PendingDequeue logic:  No further marks needed");
    }
}

bool
DualPi2QueueDisc::IsL4s(Ptr<QueueDiscItem> item)
{
    uint8_t tosByte = 0;
    if (item->GetUint8Value(QueueItem::IP_DSFIELD, tosByte))
    {
        // ECT(1) or CE
        if ((tosByte & 0x3) == 1 || (tosByte & 0x3) == 3)
        {
            NS_LOG_DEBUG("L4S detected: " << static_cast<uint16_t>(tosByte & 0x3));
            return true;
        }
    }
    NS_LOG_DEBUG("Classic detected: " << static_cast<uint16_t>(tosByte & 0x3));
    return false;
}

bool
DualPi2QueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);
    auto queueNumber = CLASSIC;

    auto nQueued = GetQueueSize();
    // in pseudocode, it compares to MTU, not packet size
    if (nQueued + item->GetSize() > m_queueLimit)
    {
        // Drops due to queue limit
        DropBeforeEnqueue(item, FORCED_DROP);
        return false;
    }
    else
    {
        if (IsL4s(item))
        {
            queueNumber = L4S;
        }
    }

    bool retval = GetInternalQueue(queueNumber)->Enqueue(item);
    NS_LOG_LOGIC("Packets enqueued in queue-" << queueNumber << ": "
                                              << GetInternalQueue(queueNumber)->GetNPackets());
    return retval;
}

void
DualPi2QueueDisc::InitializeParams()
{
    if (m_mtu == 0)
    {
        Ptr<NetDeviceQueueInterface> ndqi = GetNetDeviceQueueInterface();
        Ptr<NetDevice> dev;
        // if the NetDeviceQueueInterface object is aggregated to a
        // NetDevice, get the MTU of such NetDevice
        if (ndqi && (dev = ndqi->GetObject<NetDevice>()))
        {
            m_mtu = dev->GetMtu();
        }
    }
    NS_ABORT_MSG_IF(m_mtu < 68, "Error: MTU does not meet RFC 791 minimum");
    m_thLen = 2 * m_mtu;
    m_prevQ = Time(Seconds(0));
    m_pCL = 0;
    m_pC = 0;
    m_pL = 0;
}

void
DualPi2QueueDisc::DualPi2Update()
{
    NS_LOG_FUNCTION(this);

    // Use queuing time of first-in Classic packet
    Ptr<const QueueDiscItem> item;
    Time curQ = Seconds(0);

    if ((item = GetInternalQueue(CLASSIC)->Peek()))
    {
        curQ = Simulator::Now() - item->GetTimeStamp();
    }

    m_baseProb = m_baseProb + m_alpha * (curQ - m_target).GetSeconds() +
                 m_beta * (curQ - m_prevQ).GetSeconds();
    // clamp p' to within [0,1]; page 34 of Internet-Draft
    m_baseProb = std::max<double>(m_baseProb, 0);
    m_baseProb = std::min<double>(m_baseProb, 1);
    m_pCL = m_baseProb * m_k;
    m_pCL = std::min<double>(m_pCL, 1);
    m_pC = m_baseProb * m_baseProb;
    m_prevQ = curQ;
    m_rtrsEvent = Simulator::Schedule(m_tUpdate, &DualPi2QueueDisc::DualPi2Update, this);
}

void
DualPi2QueueDisc::AddToL4sStagingQueue(Ptr<QueueDiscItem> qdItem)
{
    NS_ASSERT_MSG(qdItem, "Error, tried to add a null QueueDiscItem to staging queue");
    m_l4sStagingQueue.push_back(qdItem);
}

void
DualPi2QueueDisc::AddToClassicStagingQueue(Ptr<QueueDiscItem> qdItem)
{
    NS_ASSERT_MSG(qdItem, "Error, tried to add a null QueueDiscItem to staging queue");
    m_classicStagingQueue.push_back(qdItem);
}

Ptr<QueueDiscItem>
DualPi2QueueDisc::DequeueFromL4sStagingQueue()
{
    if (!m_l4sStagingQueue.empty())
    {
        auto qdItem = m_l4sStagingQueue.front();
        m_l4sStagingQueue.pop_front();
        return qdItem;
    }
    return nullptr;
}

Ptr<QueueDiscItem>
DualPi2QueueDisc::DequeueFromClassicStagingQueue()
{
    if (!m_l4sStagingQueue.empty())
    {
        auto qdItem = m_classicStagingQueue.front();
        m_classicStagingQueue.pop_front();
        return qdItem;
    }
    return nullptr;
}

std::size_t
DualPi2QueueDisc::Scheduler(uint32_t byteLimit)
{
    NS_LOG_FUNCTION(this);
    while (GetCurrentSize().GetValue() > 0)
    {
        if (m_drrQueues.none())
        {
            NS_LOG_LOGIC("Start new round; LL deficit: " << m_llDeficit << " classic deficit: "
                                                         << m_classicDeficit);
            m_drrQueues.set(L4S);
            m_drrQueues.set(CLASSIC);
            m_llDeficit += (m_drrQuantum * m_schedulingWeight);
            m_classicDeficit += m_drrQuantum;
            NS_LOG_LOGIC("Starting state: LL front, LL deficit "
                         << m_llDeficit << " classic deficit " << m_classicDeficit);
        }
        if (m_drrQueues.test(L4S))
        {
            if (GetInternalQueue(L4S)->Peek() &&
                GetInternalQueue(L4S)->Peek()->GetSize() <= byteLimit)
            {
                uint32_t size = GetInternalQueue(L4S)->Peek()->GetSize();
                if (size <= m_llDeficit)
                {
                    NS_LOG_LOGIC("Selecting LL queue");
                    m_llDeficit -= size;
                    NS_LOG_LOGIC("State after LL selection: LL deficit << "
                                 << m_llDeficit << " classic deficit " << m_classicDeficit);
                    return L4S;
                }
                else
                {
                    NS_LOG_LOGIC("Not enough deficit to send LL packet, LL deficit "
                                 << m_llDeficit << " classic deficit " << m_classicDeficit);
                    m_drrQueues.reset(L4S);
                    if (!GetInternalQueue(CLASSIC)->Peek())
                    {
                        NS_LOG_LOGIC("Send LL packet due to no CLASSIC packet");
                        m_llDeficit = 0;
                        return L4S;
                    }
                }
            }
            else
            {
                NS_LOG_LOGIC("LL has no packet, fall through to consider CLASSIC");
            }
        }
        else
        {
            if (!GetInternalQueue(CLASSIC)->Peek() && GetInternalQueue(L4S)->Peek() &&
                GetInternalQueue(L4S)->Peek()->GetSize() <= byteLimit)
            {
                NS_LOG_LOGIC("Send LL packet due to no CLASSIC packet");
                m_llDeficit = 0;
                return L4S;
            }
        }
        if (m_drrQueues.test(CLASSIC) ||
            (GetInternalQueue(CLASSIC)->Peek() &&
             GetInternalQueue(CLASSIC)->Peek()->GetSize() <= byteLimit))
        {
            if (GetInternalQueue(CLASSIC)->Peek())
            {
                uint32_t size = GetInternalQueue(CLASSIC)->Peek()->GetSize();
                if (size <= m_classicDeficit)
                {
                    NS_LOG_LOGIC("Selecting Classic queue");
                    m_classicDeficit -= size;
                    NS_LOG_LOGIC("State after Classic selection: LL deficit << "
                                 << m_llDeficit << " classic deficit " << m_classicDeficit);
                    return CLASSIC;
                }
                else if (size)
                {
                    NS_LOG_LOGIC("Not enough deficit to send Classic packet");
                    m_drrQueues.reset(CLASSIC);
                    if (!GetInternalQueue(L4S)->Peek())
                    {
                        // Send anyway since there is no LL packet
                        NS_LOG_LOGIC("Send CLASSIC packet due to no LL packet");
                        m_classicDeficit = 0;
                        return CLASSIC;
                    }
                    // Fall through to return to top of loop and reset
                }
            }
            else
            {
                // no classic packet either; will fall through and reset above
            }
        }
    }
    return NONE;
}

Ptr<QueueDiscItem>
DualPi2QueueDisc::DequeueFromL4sQueue(bool& marked)
{
    NS_LOG_FUNCTION(this);
    auto qdItem = GetInternalQueue(L4S)->Dequeue();
    double pPrimeL = Laqm(Simulator::Now() - qdItem->GetTimeStamp());
    if (pPrimeL > m_pCL)
    {
        NS_LOG_DEBUG("Laqm probability " << std::min<double>(pPrimeL, 1) << " is driving p_L");
    }
    else
    {
        NS_LOG_DEBUG("coupled probability " << std::min<double>(m_pCL, 1) << " is driving p_L");
    }
    double pL = std::max<double>(pPrimeL, m_pCL);
    pL = std::min<double>(pL, 1); // clamp p_L at 1
    m_pL = pL;                    // Trace the value of p_L
    if (Recur(pL))
    {
        marked = Mark(qdItem, UNFORCED_L4S_MARK);
        NS_ASSERT_MSG(marked == true, "Make sure we can mark in L4S queue");
        NS_LOG_DEBUG("L-queue packet is marked");
    }
    else
    {
        NS_LOG_DEBUG("L-queue packet is not marked");
    }
    return qdItem;
}

Ptr<QueueDiscItem>
DualPi2QueueDisc::DequeueFromClassicQueue(bool& dropped)
{
    NS_LOG_FUNCTION(this);
    auto qdItem = GetInternalQueue(CLASSIC)->Dequeue();
    // Heuristic in Linux code; never drop if less than 2 MTU in queue
    if (GetInternalQueue(CLASSIC)->GetNBytes() < 2 * m_mtu)
    {
        return qdItem;
    }
    while (qdItem)
    {
        if (m_pC > m_uv->GetValue())
        {
            DropAfterDequeue(qdItem, UNFORCED_CLASSIC_DROP);
            dropped = true;
            NS_LOG_DEBUG("C-queue packet is dropped");
            qdItem = GetInternalQueue(CLASSIC)->Dequeue();
            continue;
        }
        else
        {
            NS_LOG_DEBUG("C-queue packet is dequeued and returned");
            return qdItem;
        }
    }
    return nullptr;
}

Ptr<QueueDiscItem>
DualPi2QueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);
    auto qdItem = DequeueFromL4sStagingQueue();
    if (qdItem)
    {
        // Packets in staging queue have already been marked or not
        // and internal Laqm probabilities have been updated
        NS_LOG_DEBUG("Dequeue from L4S staging queue");
        m_traceL4sSojourn(Simulator::Now() - qdItem->GetTimeStamp());
        return qdItem;
    }
    qdItem = DequeueFromClassicStagingQueue();
    if (qdItem)
    {
        m_traceClassicSojourn(Simulator::Now() - qdItem->GetTimeStamp());
        return qdItem;
    }
    while (GetQueueSize() > 0)
    {
        auto selected = Scheduler(GetQueueSize());
        if (selected == L4S)
        {
            bool marked [[maybe_unused]];
            qdItem = DequeueFromL4sQueue(marked);
            m_traceL4sSojourn(Simulator::Now() - qdItem->GetTimeStamp());
            return qdItem;
        }
        else if (selected == CLASSIC)
        {
            bool dropped [[maybe_unused]];
            qdItem = DequeueFromClassicQueue(dropped);
            // Since the CLASSIC queue can drop in DequeueFromClassicDequeue(), check
            // that an item was actually returned before tracing it
            if (qdItem)
            {
                m_traceClassicSojourn(Simulator::Now() - qdItem->GetTimeStamp());
            }
            return qdItem;
        }
        else
        {
            return nullptr;
        }
    }
    return nullptr;
}

double
DualPi2QueueDisc::Laqm(Time lqTime) const
{
    NS_LOG_FUNCTION(this << lqTime.GetSeconds());
    if (lqTime > m_minTh)
    {
        return 1;
    }
    return 0;
}

bool
DualPi2QueueDisc::Recur(double likelihood)
{
    NS_LOG_FUNCTION(this << likelihood);
    m_count += likelihood;
    if (m_count > 1)
    {
        m_count -= 1;
        return true;
    }
    return false;
}

Ptr<const QueueDiscItem>
DualPi2QueueDisc::DoPeek()
{
    NS_LOG_FUNCTION(this);
    Ptr<const QueueDiscItem> item;

    for (std::size_t i = 0; i < GetNInternalQueues(); i++)
    {
        if ((item = GetInternalQueue(i)->Peek()))
        {
            NS_LOG_LOGIC("Peeked from queue number " << i << ": " << item);
            NS_LOG_LOGIC("Number packets queue number " << i << ": "
                                                        << GetInternalQueue(i)->GetNPackets());
            NS_LOG_LOGIC("Number bytes queue number " << i << ": "
                                                      << GetInternalQueue(i)->GetNBytes());
            return item;
        }
    }

    NS_LOG_LOGIC("Queue empty");
    return item;
}

bool
DualPi2QueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);
    if (GetNQueueDiscClasses() > 0)
    {
        NS_LOG_ERROR("DualPi2QueueDisc cannot have classes");
        return false;
    }

    if (GetNPacketFilters() > 0)
    {
        NS_LOG_ERROR("DualPi2QueueDisc cannot have packet filters");
        return false;
    }

    if (GetNInternalQueues() == 0)
    {
        // Create 2 DropTail queues
        Ptr<InternalQueue> queue0 =
            CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>("MaxSize",
                                                                     QueueSizeValue(GetMaxSize()));
        Ptr<InternalQueue> queue1 =
            CreateObjectWithAttributes<DropTailQueue<QueueDiscItem>>("MaxSize",
                                                                     QueueSizeValue(GetMaxSize()));
        QueueSize queueSize(BYTES, m_queueLimit);
        queue0->SetMaxSize(queueSize);
        queue1->SetMaxSize(queueSize);
        AddInternalQueue(queue0);
        AddInternalQueue(queue1);
    }

    if (GetNInternalQueues() != 2)
    {
        NS_LOG_ERROR("DualPi2QueueDisc needs 2 internal queue");
        return false;
    }

    if (GetInternalQueue(CLASSIC)->GetMaxSize().GetValue() < m_queueLimit)
    {
        NS_LOG_ERROR(
            "The size of the internal Classic traffic queue is less than the queue disc limit");
        return false;
    }

    if (GetInternalQueue(L4S)->GetMaxSize().GetValue() < m_queueLimit)
    {
        NS_LOG_ERROR(
            "The size of the internal L4S traffic queue is less than the queue disc limit");
        return false;
    }

    return true;
}

} // namespace ns3
