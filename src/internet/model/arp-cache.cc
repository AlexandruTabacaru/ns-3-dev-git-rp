/*
 * Copyright (c) 2006 INRIA
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "arp-cache.h"

#include "ipv4-header.h"
#include "ipv4-interface.h"

#include "ns3/assert.h"
#include "ns3/log.h"
#include "ns3/names.h"
#include "ns3/node.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("ArpCache");

NS_OBJECT_ENSURE_REGISTERED(ArpCache);

TypeId
ArpCache::GetTypeId()
{
    static TypeId tid = TypeId("ns3::ArpCache")
                            .SetParent<Object>()
                            .SetGroupName("Internet")
                            .AddAttribute("AliveTimeout",
                                          "When this timeout expires, "
                                          "the matching cache entry needs refreshing",
                                          TimeValue(Seconds(120)),
                                          MakeTimeAccessor(&ArpCache::m_aliveTimeout),
                                          MakeTimeChecker())
                            .AddAttribute("DeadTimeout",
                                          "When this timeout expires, "
                                          "a new attempt to resolve the matching entry is made",
                                          TimeValue(Seconds(100)),
                                          MakeTimeAccessor(&ArpCache::m_deadTimeout),
                                          MakeTimeChecker())
                            .AddAttribute("WaitReplyTimeout",
                                          "When this timeout expires, "
                                          "the cache entries will be scanned and "
                                          "entries in WaitReply state will resend ArpRequest "
                                          "unless MaxRetries has been exceeded, "
                                          "in which case the entry is marked dead",
                                          TimeValue(Seconds(1)),
                                          MakeTimeAccessor(&ArpCache::m_waitReplyTimeout),
                                          MakeTimeChecker())
                            .AddAttribute("MaxRetries",
                                          "Number of retransmissions of ArpRequest "
                                          "before marking dead",
                                          UintegerValue(3),
                                          MakeUintegerAccessor(&ArpCache::m_maxRetries),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("PendingQueueSize",
                                          "The size of the queue for packets pending an arp reply.",
                                          UintegerValue(3),
                                          MakeUintegerAccessor(&ArpCache::m_pendingQueueSize),
                                          MakeUintegerChecker<uint32_t>())
                            .AddTraceSource("Drop",
                                            "Packet dropped due to ArpCache entry "
                                            "in WaitReply expiring.",
                                            MakeTraceSourceAccessor(&ArpCache::m_dropTrace),
                                            "ns3::Packet::TracedCallback");
    return tid;
}

ArpCache::ArpCache()
    : m_device(nullptr),
      m_interface(nullptr)
{
    NS_LOG_FUNCTION(this);
}

ArpCache::~ArpCache()
{
    NS_LOG_FUNCTION(this);
}

void
ArpCache::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Flush();
    m_device = nullptr;
    m_interface = nullptr;
    if (!m_waitReplyTimer.IsPending())
    {
        m_waitReplyTimer.Cancel();
    }
    Object::DoDispose();
}

void
ArpCache::SetDevice(Ptr<NetDevice> device, Ptr<Ipv4Interface> interface)
{
    NS_LOG_FUNCTION(this << device << interface);
    m_device = device;
    m_interface = interface;
}

Ptr<NetDevice>
ArpCache::GetDevice() const
{
    NS_LOG_FUNCTION(this);
    return m_device;
}

Ptr<Ipv4Interface>
ArpCache::GetInterface() const
{
    NS_LOG_FUNCTION(this);
    return m_interface;
}

void
ArpCache::SetAliveTimeout(Time aliveTimeout)
{
    NS_LOG_FUNCTION(this << aliveTimeout);
    m_aliveTimeout = aliveTimeout;
}

void
ArpCache::SetDeadTimeout(Time deadTimeout)
{
    NS_LOG_FUNCTION(this << deadTimeout);
    m_deadTimeout = deadTimeout;
}

void
ArpCache::SetWaitReplyTimeout(Time waitReplyTimeout)
{
    NS_LOG_FUNCTION(this << waitReplyTimeout);
    m_waitReplyTimeout = waitReplyTimeout;
}

Time
ArpCache::GetAliveTimeout() const
{
    NS_LOG_FUNCTION(this);
    return m_aliveTimeout;
}

Time
ArpCache::GetDeadTimeout() const
{
    NS_LOG_FUNCTION(this);
    return m_deadTimeout;
}

Time
ArpCache::GetWaitReplyTimeout() const
{
    NS_LOG_FUNCTION(this);
    return m_waitReplyTimeout;
}

void
ArpCache::SetArpRequestCallback(Callback<void, Ptr<const ArpCache>, Ipv4Address> arpRequestCallback)
{
    NS_LOG_FUNCTION(this << &arpRequestCallback);
    m_arpRequestCallback = arpRequestCallback;
}

void
ArpCache::StartWaitReplyTimer()
{
    NS_LOG_FUNCTION(this);
    if (!m_waitReplyTimer.IsPending())
    {
        NS_LOG_LOGIC("Starting WaitReplyTimer at " << Simulator::Now() << " for "
                                                   << m_waitReplyTimeout);
        m_waitReplyTimer =
            Simulator::Schedule(m_waitReplyTimeout, &ArpCache::HandleWaitReplyTimeout, this);
    }
}

void
ArpCache::HandleWaitReplyTimeout()
{
    NS_LOG_FUNCTION(this);
    ArpCache::Entry* entry;
    bool restartWaitReplyTimer = false;
    for (auto i = m_arpCache.begin(); i != m_arpCache.end(); i++)
    {
        entry = (*i).second;
        if (entry != nullptr && entry->IsWaitReply())
        {
            if (entry->GetRetries() < m_maxRetries)
            {
                NS_LOG_LOGIC("node=" << m_device->GetNode()->GetId() << ", ArpWaitTimeout for "
                                     << entry->GetIpv4Address()
                                     << " expired -- retransmitting arp request since retries = "
                                     << entry->GetRetries());
                m_arpRequestCallback(this, entry->GetIpv4Address());
                restartWaitReplyTimer = true;
                entry->IncrementRetries();
            }
            else
            {
                NS_LOG_LOGIC("node=" << m_device->GetNode()->GetId() << ", wait reply for "
                                     << entry->GetIpv4Address()
                                     << " expired -- drop since max retries exceeded: "
                                     << entry->GetRetries());
                entry->MarkDead();
                entry->ClearRetries();
                Ipv4PayloadHeaderPair pending = entry->DequeuePending();
                while (pending.first)
                {
                    // add the Ipv4 header for tracing purposes
                    pending.first->AddHeader(pending.second);
                    m_dropTrace(pending.first);
                    pending = entry->DequeuePending();
                }
            }
        }
    }
    if (restartWaitReplyTimer)
    {
        NS_LOG_LOGIC("Restarting WaitReplyTimer at " << Simulator::Now().GetSeconds());
        m_waitReplyTimer =
            Simulator::Schedule(m_waitReplyTimeout, &ArpCache::HandleWaitReplyTimeout, this);
    }
}

void
ArpCache::Flush()
{
    NS_LOG_FUNCTION(this);
    for (auto i = m_arpCache.begin(); i != m_arpCache.end(); i++)
    {
        delete (*i).second;
    }
    m_arpCache.erase(m_arpCache.begin(), m_arpCache.end());
    if (m_waitReplyTimer.IsPending())
    {
        NS_LOG_LOGIC("Stopping WaitReplyTimer at " << Simulator::Now().GetSeconds()
                                                   << " due to ArpCache flush");
        m_waitReplyTimer.Cancel();
    }
}

void
ArpCache::PrintArpCache(Ptr<OutputStreamWrapper> stream)
{
    NS_LOG_FUNCTION(this << stream);
    std::ostream* os = stream->GetStream();

    for (auto i = m_arpCache.begin(); i != m_arpCache.end(); i++)
    {
        *os << i->first << " dev ";
        std::string found = Names::FindName(m_device);
        if (!Names::FindName(m_device).empty())
        {
            *os << found;
        }
        else
        {
            *os << static_cast<int>(m_device->GetIfIndex());
        }

        *os << " lladdr " << i->second->GetMacAddress();

        if (i->second->IsAlive())
        {
            *os << " REACHABLE\n";
        }
        else if (i->second->IsWaitReply())
        {
            *os << " DELAY\n";
        }
        else if (i->second->IsPermanent())
        {
            *os << " PERMANENT\n";
        }
        else if (i->second->IsAutoGenerated())
        {
            *os << " STATIC_AUTOGENERATED\n";
        }
        else
        {
            *os << " STALE\n";
        }
    }
}

void
ArpCache::RemoveAutoGeneratedEntries()
{
    NS_LOG_FUNCTION(this);
    for (auto i = m_arpCache.begin(); i != m_arpCache.end();)
    {
        if (i->second->IsAutoGenerated())
        {
            i->second->ClearPendingPacket(); // clear the pending packets for entry's ipaddress
            delete i->second;
            m_arpCache.erase(i++);
            continue;
        }
        i++;
    }
}

std::list<ArpCache::Entry*>
ArpCache::LookupInverse(Address to)
{
    NS_LOG_FUNCTION(this << to);

    std::list<ArpCache::Entry*> entryList;
    for (auto i = m_arpCache.begin(); i != m_arpCache.end(); i++)
    {
        ArpCache::Entry* entry = (*i).second;
        if (entry->GetMacAddress() == to)
        {
            entryList.push_back(entry);
        }
    }
    return entryList;
}

ArpCache::Entry*
ArpCache::Lookup(Ipv4Address to)
{
    NS_LOG_FUNCTION(this << to);
    auto it = m_arpCache.find(to);
    if (it != m_arpCache.end())
    {
        return it->second;
    }
    return nullptr;
}

ArpCache::Entry*
ArpCache::Add(Ipv4Address to)
{
    NS_LOG_FUNCTION(this << to);
    NS_ASSERT(m_arpCache.find(to) == m_arpCache.end());

    auto entry = new ArpCache::Entry(this);
    m_arpCache[to] = entry;
    entry->SetIpv4Address(to);
    return entry;
}

void
ArpCache::Remove(ArpCache::Entry* entry)
{
    NS_LOG_FUNCTION(this << entry);

    for (auto i = m_arpCache.begin(); i != m_arpCache.end(); i++)
    {
        if ((*i).second == entry)
        {
            m_arpCache.erase(i);
            entry->ClearPendingPacket(); // clear the pending packets for entry's ipaddress
            delete entry;
            return;
        }
    }
    NS_LOG_WARN("Entry not found in this ARP Cache");
}

ArpCache::Entry::Entry(ArpCache* arp)
    : m_arp(arp),
      m_state(ALIVE),
      m_retries(0)
{
    NS_LOG_FUNCTION(this << arp);
}

bool
ArpCache::Entry::IsDead()
{
    NS_LOG_FUNCTION(this);
    return (m_state == DEAD);
}

bool
ArpCache::Entry::IsAlive()
{
    NS_LOG_FUNCTION(this);
    return (m_state == ALIVE);
}

bool
ArpCache::Entry::IsWaitReply()
{
    NS_LOG_FUNCTION(this);
    return (m_state == WAIT_REPLY);
}

bool
ArpCache::Entry::IsPermanent()
{
    NS_LOG_FUNCTION(this);
    return (m_state == PERMANENT);
}

bool
ArpCache::Entry::IsAutoGenerated()
{
    NS_LOG_FUNCTION(this);
    return (m_state == STATIC_AUTOGENERATED);
}

void
ArpCache::Entry::MarkDead()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_state == ALIVE || m_state == WAIT_REPLY || m_state == DEAD);
    m_state = DEAD;
    ClearRetries();
    UpdateSeen();
}

void
ArpCache::Entry::MarkAlive(Address macAddress)
{
    NS_LOG_FUNCTION(this << macAddress);
    NS_ASSERT(m_state == WAIT_REPLY);
    m_macAddress = macAddress;
    m_state = ALIVE;
    ClearRetries();
    UpdateSeen();
}

void
ArpCache::Entry::MarkPermanent()
{
    NS_LOG_FUNCTION(this << m_macAddress);
    NS_ASSERT(!m_macAddress.IsInvalid());

    m_state = PERMANENT;
    ClearRetries();
    UpdateSeen();
}

void
ArpCache::Entry::MarkAutoGenerated()
{
    NS_LOG_FUNCTION(this << m_macAddress);
    NS_ASSERT(!m_macAddress.IsInvalid());

    m_state = STATIC_AUTOGENERATED;
    ClearRetries();
    UpdateSeen();
}

bool
ArpCache::Entry::UpdateWaitReply(Ipv4PayloadHeaderPair waiting)
{
    NS_LOG_FUNCTION(this << waiting.first);
    NS_ASSERT(m_state == WAIT_REPLY);
    /* We are already waiting for an answer so
     * we dump the previously waiting packet and
     * replace it with this one.
     */
    if (m_pending.size() >= m_arp->m_pendingQueueSize)
    {
        return false;
    }
    m_pending.push_back(waiting);
    return true;
}

void
ArpCache::Entry::MarkWaitReply(Ipv4PayloadHeaderPair waiting)
{
    NS_LOG_FUNCTION(this << waiting.first);
    NS_ASSERT(m_state == ALIVE || m_state == DEAD);
    NS_ASSERT(m_pending.empty());
    NS_ASSERT_MSG(waiting.first, "Can not add a null packet to the ARP queue");

    m_state = WAIT_REPLY;
    m_pending.push_back(waiting);
    UpdateSeen();
    m_arp->StartWaitReplyTimer();
}

Address
ArpCache::Entry::GetMacAddress() const
{
    NS_LOG_FUNCTION(this);
    return m_macAddress;
}

void
ArpCache::Entry::SetMacAddress(Address macAddress)
{
    NS_LOG_FUNCTION(this);
    m_macAddress = macAddress;
}

Ipv4Address
ArpCache::Entry::GetIpv4Address() const
{
    NS_LOG_FUNCTION(this);
    return m_ipv4Address;
}

void
ArpCache::Entry::SetIpv4Address(Ipv4Address destination)
{
    NS_LOG_FUNCTION(this << destination);
    m_ipv4Address = destination;
}

Time
ArpCache::Entry::GetTimeout() const
{
    NS_LOG_FUNCTION(this);
    switch (m_state)
    {
    case ArpCache::Entry::WAIT_REPLY:
        return m_arp->GetWaitReplyTimeout();
    case ArpCache::Entry::DEAD:
        return m_arp->GetDeadTimeout();
    case ArpCache::Entry::ALIVE:
        return m_arp->GetAliveTimeout();
    case ArpCache::Entry::PERMANENT:
    case ArpCache::Entry::STATIC_AUTOGENERATED:
        return Time::Max();
    }
    return Time(); // Silence compiler warning
}

bool
ArpCache::Entry::IsExpired() const
{
    NS_LOG_FUNCTION(this);
    Time timeout = GetTimeout();
    Time delta = Simulator::Now() - m_lastSeen;
    NS_LOG_DEBUG("delta=" << delta.GetSeconds() << "s");
    return delta > timeout;
}

ArpCache::Ipv4PayloadHeaderPair
ArpCache::Entry::DequeuePending()
{
    NS_LOG_FUNCTION(this);
    if (m_pending.empty())
    {
        Ipv4Header h;
        return Ipv4PayloadHeaderPair(nullptr, h);
    }
    else
    {
        Ipv4PayloadHeaderPair p = m_pending.front();
        m_pending.pop_front();
        return p;
    }
}

void
ArpCache::Entry::ClearPendingPacket()
{
    NS_LOG_FUNCTION(this);
    m_pending.clear();
}

void
ArpCache::Entry::UpdateSeen()
{
    NS_LOG_FUNCTION(this);
    m_lastSeen = Simulator::Now();
}

uint32_t
ArpCache::Entry::GetRetries() const
{
    NS_LOG_FUNCTION(this);
    return m_retries;
}

void
ArpCache::Entry::IncrementRetries()
{
    NS_LOG_FUNCTION(this);
    m_retries++;
    UpdateSeen();
}

void
ArpCache::Entry::ClearRetries()
{
    NS_LOG_FUNCTION(this);
    m_retries = 0;
}

void
ArpCache::Entry::Print(std::ostream& os) const
{
    os << m_ipv4Address << " lladdr " << m_macAddress << " state ";

    switch (m_state)
    {
    case ALIVE:
        os << "ALIVE";
        break;
    case WAIT_REPLY:
        os << "WAIT_REPLY";
        break;
    case DEAD:
        os << "DEAD";
        break;
    case PERMANENT:
        os << "PERMANENT";
        break;
    case STATIC_AUTOGENERATED:
        os << "STATIC_AUTOGENERATED";
    }

    os << " last seen " << m_lastSeen.As() << " timeout " << GetTimeout().As();
}

std::ostream&
operator<<(std::ostream& os, const ArpCache::Entry& entry)
{
    entry.Print(os);
    return os;
}

} // namespace ns3
