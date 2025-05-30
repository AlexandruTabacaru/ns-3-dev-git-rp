/*
 * Copyright (c) 2007 Georgia Tech Research Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Author: Raj Bhattacharjea <raj.b@gatech.edu>
 */

#include "tcp-l4-protocol.h"

#include "ipv4-end-point-demux.h"
#include "ipv4-end-point.h"
#include "ipv4-route.h"
#include "ipv4-routing-protocol.h"
#include "ipv6-end-point-demux.h"
#include "ipv6-end-point.h"
#include "ipv6-l3-protocol.h"
#include "ipv6-route.h"
#include "ipv6-routing-protocol.h"
#include "rtt-estimator.h"
#include "tcp-congestion-ops.h"
#include "tcp-cubic.h"
#include "tcp-header.h"
#include "tcp-prr-recovery.h"
#include "tcp-recovery-ops.h"
#include "tcp-socket-base.h"
#include "tcp-socket-factory-impl.h"

#include "ns3/assert.h"
#include "ns3/boolean.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/object-map.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("TcpL4Protocol");

NS_OBJECT_ENSURE_REGISTERED(TcpL4Protocol);

// TcpL4Protocol stuff----------------------------------------------------------

#undef NS_LOG_APPEND_CONTEXT
#define NS_LOG_APPEND_CONTEXT                                                                      \
    if (m_node)                                                                                    \
    {                                                                                              \
        std::clog << " [node " << m_node->GetId() << "] ";                                         \
    }

TypeId
TcpL4Protocol::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TcpL4Protocol")
            .SetParent<IpL4Protocol>()
            .SetGroupName("Internet")
            .AddConstructor<TcpL4Protocol>()
            .AddAttribute("RttEstimatorType",
                          "Type of RttEstimator objects.",
                          TypeIdValue(RttMeanDeviation::GetTypeId()),
                          MakeTypeIdAccessor(&TcpL4Protocol::m_rttTypeId),
                          MakeTypeIdChecker())
            .AddAttribute("SocketType",
                          "Socket type of TCP objects.",
                          TypeIdValue(TcpCubic::GetTypeId()),
                          MakeTypeIdAccessor(&TcpL4Protocol::m_congestionTypeId),
                          MakeTypeIdChecker())
            .AddAttribute("RecoveryType",
                          "Recovery type of TCP objects.",
                          TypeIdValue(TcpPrrRecovery::GetTypeId()),
                          MakeTypeIdAccessor(&TcpL4Protocol::m_recoveryTypeId),
                          MakeTypeIdChecker())
            .AddAttribute("SocketList",
                          "A container of sockets associated to this protocol. "
                          "The underlying type is an unordered map, the attribute name "
                          "is kept for backward compatibility.",
                          ObjectMapValue(),
                          MakeObjectMapAccessor(&TcpL4Protocol::m_sockets),
                          MakeObjectMapChecker<TcpSocketBase>());
    return tid;
}

TcpL4Protocol::TcpL4Protocol()
    : m_endPoints(new Ipv4EndPointDemux()),
      m_endPoints6(new Ipv6EndPointDemux())
{
    NS_LOG_FUNCTION(this);
}

TcpL4Protocol::~TcpL4Protocol()
{
    NS_LOG_FUNCTION(this);
}

void
TcpL4Protocol::SetNode(Ptr<Node> node)
{
    NS_LOG_FUNCTION(this);
    m_node = node;
}

void
TcpL4Protocol::NotifyNewAggregate()
{
    NS_LOG_FUNCTION(this);
    Ptr<Node> node = this->GetObject<Node>();
    Ptr<Ipv4> ipv4 = this->GetObject<Ipv4>();
    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();

    if (!m_node)
    {
        if (node && (ipv4 || ipv6))
        {
            this->SetNode(node);
            Ptr<TcpSocketFactoryImpl> tcpFactory = CreateObject<TcpSocketFactoryImpl>();
            tcpFactory->SetTcp(this);
            node->AggregateObject(tcpFactory);
        }
    }

    // We set at least one of our 2 down targets to the IPv4/IPv6 send
    // functions.  Since these functions have different prototypes, we
    // need to keep track of whether we are connected to an IPv4 or
    // IPv6 lower layer and call the appropriate one.

    if (ipv4 && m_downTarget.IsNull())
    {
        ipv4->Insert(this);
        this->SetDownTarget(MakeCallback(&Ipv4::Send, ipv4));
    }
    if (ipv6 && m_downTarget6.IsNull())
    {
        ipv6->Insert(this);
        this->SetDownTarget6(MakeCallback(&Ipv6::Send, ipv6));
    }
    IpL4Protocol::NotifyNewAggregate();
}

int
TcpL4Protocol::GetProtocolNumber() const
{
    return PROT_NUMBER;
}

void
TcpL4Protocol::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_sockets.clear();

    if (m_endPoints != nullptr)
    {
        delete m_endPoints;
        m_endPoints = nullptr;
    }

    if (m_endPoints6 != nullptr)
    {
        delete m_endPoints6;
        m_endPoints6 = nullptr;
    }

    m_node = nullptr;
    m_downTarget.Nullify();
    m_downTarget6.Nullify();
    IpL4Protocol::DoDispose();
}

Ptr<Socket>
TcpL4Protocol::CreateSocket(TypeId congestionTypeId)
{
    return CreateSocket(congestionTypeId, m_recoveryTypeId);
}

Ptr<Socket>
TcpL4Protocol::CreateSocket(TypeId congestionTypeId, TypeId recoveryTypeId)
{
    NS_LOG_FUNCTION(this << congestionTypeId.GetName());
    ObjectFactory rttFactory;
    ObjectFactory congestionAlgorithmFactory;
    ObjectFactory recoveryAlgorithmFactory;
    rttFactory.SetTypeId(m_rttTypeId);
    congestionAlgorithmFactory.SetTypeId(congestionTypeId);
    recoveryAlgorithmFactory.SetTypeId(recoveryTypeId);

    Ptr<RttEstimator> rtt = rttFactory.Create<RttEstimator>();
    Ptr<TcpSocketBase> socket = CreateObject<TcpSocketBase>();
    Ptr<TcpCongestionOps> algo = congestionAlgorithmFactory.Create<TcpCongestionOps>();
    Ptr<TcpRecoveryOps> recovery = recoveryAlgorithmFactory.Create<TcpRecoveryOps>();

    socket->SetNode(m_node);
    socket->SetTcp(this);
    socket->SetRtt(rtt);
    socket->SetCongestionControlAlgorithm(algo);
    socket->SetRecoveryAlgorithm(recovery);

    m_sockets[m_socketIndex++] = socket;
    return socket;
}

Ptr<Socket>
TcpL4Protocol::CreateSocket()
{
    return CreateSocket(m_congestionTypeId, m_recoveryTypeId);
}

Ipv4EndPoint*
TcpL4Protocol::Allocate()
{
    NS_LOG_FUNCTION(this);
    return m_endPoints->Allocate();
}

Ipv4EndPoint*
TcpL4Protocol::Allocate(Ipv4Address address)
{
    NS_LOG_FUNCTION(this << address);
    return m_endPoints->Allocate(address);
}

Ipv4EndPoint*
TcpL4Protocol::Allocate(Ptr<NetDevice> boundNetDevice, uint16_t port)
{
    NS_LOG_FUNCTION(this << boundNetDevice << port);
    return m_endPoints->Allocate(boundNetDevice, port);
}

Ipv4EndPoint*
TcpL4Protocol::Allocate(Ptr<NetDevice> boundNetDevice, Ipv4Address address, uint16_t port)
{
    NS_LOG_FUNCTION(this << boundNetDevice << address << port);
    return m_endPoints->Allocate(boundNetDevice, address, port);
}

Ipv4EndPoint*
TcpL4Protocol::Allocate(Ptr<NetDevice> boundNetDevice,
                        Ipv4Address localAddress,
                        uint16_t localPort,
                        Ipv4Address peerAddress,
                        uint16_t peerPort)
{
    NS_LOG_FUNCTION(this << boundNetDevice << localAddress << localPort << peerAddress << peerPort);
    return m_endPoints->Allocate(boundNetDevice, localAddress, localPort, peerAddress, peerPort);
}

void
TcpL4Protocol::DeAllocate(Ipv4EndPoint* endPoint)
{
    NS_LOG_FUNCTION(this << endPoint);
    m_endPoints->DeAllocate(endPoint);
}

Ipv6EndPoint*
TcpL4Protocol::Allocate6()
{
    NS_LOG_FUNCTION(this);
    return m_endPoints6->Allocate();
}

Ipv6EndPoint*
TcpL4Protocol::Allocate6(Ipv6Address address)
{
    NS_LOG_FUNCTION(this << address);
    return m_endPoints6->Allocate(address);
}

Ipv6EndPoint*
TcpL4Protocol::Allocate6(Ptr<NetDevice> boundNetDevice, uint16_t port)
{
    NS_LOG_FUNCTION(this << boundNetDevice << port);
    return m_endPoints6->Allocate(boundNetDevice, port);
}

Ipv6EndPoint*
TcpL4Protocol::Allocate6(Ptr<NetDevice> boundNetDevice, Ipv6Address address, uint16_t port)
{
    NS_LOG_FUNCTION(this << boundNetDevice << address << port);
    return m_endPoints6->Allocate(boundNetDevice, address, port);
}

Ipv6EndPoint*
TcpL4Protocol::Allocate6(Ptr<NetDevice> boundNetDevice,
                         Ipv6Address localAddress,
                         uint16_t localPort,
                         Ipv6Address peerAddress,
                         uint16_t peerPort)
{
    NS_LOG_FUNCTION(this << boundNetDevice << localAddress << localPort << peerAddress << peerPort);
    return m_endPoints6->Allocate(boundNetDevice, localAddress, localPort, peerAddress, peerPort);
}

void
TcpL4Protocol::DeAllocate(Ipv6EndPoint* endPoint)
{
    NS_LOG_FUNCTION(this << endPoint);
    m_endPoints6->DeAllocate(endPoint);
}

void
TcpL4Protocol::ReceiveIcmp(Ipv4Address icmpSource,
                           uint8_t icmpTtl,
                           uint8_t icmpType,
                           uint8_t icmpCode,
                           uint32_t icmpInfo,
                           Ipv4Address payloadSource,
                           Ipv4Address payloadDestination,
                           const uint8_t payload[8])
{
    NS_LOG_FUNCTION(this << icmpSource << (uint16_t)icmpTtl << (uint16_t)icmpType
                         << (uint16_t)icmpCode << icmpInfo << payloadSource << payloadDestination);
    uint16_t src;
    uint16_t dst;
    src = payload[0] << 8;
    src |= payload[1];
    dst = payload[2] << 8;
    dst |= payload[3];

    Ipv4EndPoint* endPoint = m_endPoints->SimpleLookup(payloadSource, src, payloadDestination, dst);
    if (endPoint != nullptr)
    {
        endPoint->ForwardIcmp(icmpSource, icmpTtl, icmpType, icmpCode, icmpInfo);
    }
    else
    {
        NS_LOG_DEBUG("no endpoint found source=" << payloadSource
                                                 << ", destination=" << payloadDestination
                                                 << ", src=" << src << ", dst=" << dst);
    }
}

void
TcpL4Protocol::ReceiveIcmp(Ipv6Address icmpSource,
                           uint8_t icmpTtl,
                           uint8_t icmpType,
                           uint8_t icmpCode,
                           uint32_t icmpInfo,
                           Ipv6Address payloadSource,
                           Ipv6Address payloadDestination,
                           const uint8_t payload[8])
{
    NS_LOG_FUNCTION(this << icmpSource << (uint16_t)icmpTtl << (uint16_t)icmpType
                         << (uint16_t)icmpCode << icmpInfo << payloadSource << payloadDestination);
    uint16_t src;
    uint16_t dst;
    src = payload[0] << 8;
    src |= payload[1];
    dst = payload[2] << 8;
    dst |= payload[3];

    Ipv6EndPoint* endPoint =
        m_endPoints6->SimpleLookup(payloadSource, src, payloadDestination, dst);
    if (endPoint != nullptr)
    {
        endPoint->ForwardIcmp(icmpSource, icmpTtl, icmpType, icmpCode, icmpInfo);
    }
    else
    {
        NS_LOG_DEBUG("no endpoint found source=" << payloadSource
                                                 << ", destination=" << payloadDestination
                                                 << ", src=" << src << ", dst=" << dst);
    }
}

IpL4Protocol::RxStatus
TcpL4Protocol::PacketReceived(Ptr<Packet> packet,
                              TcpHeader& incomingTcpHeader,
                              const Address& source,
                              const Address& destination)
{
    NS_LOG_FUNCTION(this << packet << incomingTcpHeader << source << destination);

    if (Node::ChecksumEnabled())
    {
        incomingTcpHeader.EnableChecksums();
        incomingTcpHeader.InitializeChecksum(source, destination, PROT_NUMBER);
    }

    packet->PeekHeader(incomingTcpHeader);

    NS_LOG_LOGIC("TcpL4Protocol " << this << " receiving seq "
                                  << incomingTcpHeader.GetSequenceNumber() << " ack "
                                  << incomingTcpHeader.GetAckNumber() << " flags "
                                  << TcpHeader::FlagsToString(incomingTcpHeader.GetFlags())
                                  << " data size " << packet->GetSize());

    if (!incomingTcpHeader.IsChecksumOk())
    {
        NS_LOG_INFO("Bad checksum, dropping packet!");
        return IpL4Protocol::RX_CSUM_FAILED;
    }

    return IpL4Protocol::RX_OK;
}

void
TcpL4Protocol::NoEndPointsFound(const TcpHeader& incomingHeader,
                                const Address& incomingSAddr,
                                const Address& incomingDAddr)
{
    NS_LOG_FUNCTION(this << incomingHeader << incomingSAddr << incomingDAddr);

    if (!(incomingHeader.GetFlags() & TcpHeader::RST))
    {
        // build a RST packet and send
        Ptr<Packet> rstPacket = Create<Packet>();
        TcpHeader outgoingTcpHeader;

        if (incomingHeader.GetFlags() & TcpHeader::ACK)
        {
            // ACK bit was set
            outgoingTcpHeader.SetFlags(TcpHeader::RST);
            outgoingTcpHeader.SetSequenceNumber(incomingHeader.GetAckNumber());
        }
        else
        {
            outgoingTcpHeader.SetFlags(TcpHeader::RST | TcpHeader::ACK);
            outgoingTcpHeader.SetSequenceNumber(SequenceNumber32(0));
            outgoingTcpHeader.SetAckNumber(incomingHeader.GetSequenceNumber() +
                                           SequenceNumber32(1));
        }

        // Remember that parameters refer to the incoming packet; in reply,
        // we need to swap src/dst

        outgoingTcpHeader.SetSourcePort(incomingHeader.GetDestinationPort());
        outgoingTcpHeader.SetDestinationPort(incomingHeader.GetSourcePort());

        SendPacket(rstPacket, outgoingTcpHeader, incomingDAddr, incomingSAddr);
    }
}

IpL4Protocol::RxStatus
TcpL4Protocol::Receive(Ptr<Packet> packet,
                       const Ipv4Header& incomingIpHeader,
                       Ptr<Ipv4Interface> incomingInterface)
{
    NS_LOG_FUNCTION(this << packet << incomingIpHeader << incomingInterface);

    TcpHeader incomingTcpHeader;
    IpL4Protocol::RxStatus checksumControl;

    checksumControl = PacketReceived(packet,
                                     incomingTcpHeader,
                                     incomingIpHeader.GetSource(),
                                     incomingIpHeader.GetDestination());

    if (checksumControl != IpL4Protocol::RX_OK)
    {
        return checksumControl;
    }

    Ipv4EndPointDemux::EndPoints endPoints;
    endPoints = m_endPoints->Lookup(incomingIpHeader.GetDestination(),
                                    incomingTcpHeader.GetDestinationPort(),
                                    incomingIpHeader.GetSource(),
                                    incomingTcpHeader.GetSourcePort(),
                                    incomingInterface);

    if (endPoints.empty())
    {
        if (this->GetObject<Ipv6L3Protocol>())
        {
            NS_LOG_LOGIC("  No Ipv4 endpoints matched on TcpL4Protocol, trying Ipv6 " << this);
            Ptr<Ipv6Interface> fakeInterface;
            Ipv6Header ipv6Header;
            Ipv6Address src;
            Ipv6Address dst;

            src = Ipv6Address::MakeIpv4MappedAddress(incomingIpHeader.GetSource());
            dst = Ipv6Address::MakeIpv4MappedAddress(incomingIpHeader.GetDestination());
            ipv6Header.SetSource(src);
            ipv6Header.SetDestination(dst);
            return (this->Receive(packet, ipv6Header, fakeInterface));
        }

        NS_LOG_LOGIC("TcpL4Protocol "
                     << this
                     << " received a packet but"
                        " no endpoints matched."
                     << " destination IP: " << incomingIpHeader.GetDestination()
                     << " destination port: " << incomingTcpHeader.GetDestinationPort()
                     << " source IP: " << incomingIpHeader.GetSource()
                     << " source port: " << incomingTcpHeader.GetSourcePort());

        NoEndPointsFound(incomingTcpHeader,
                         incomingIpHeader.GetSource(),
                         incomingIpHeader.GetDestination());

        return IpL4Protocol::RX_ENDPOINT_CLOSED;
    }

    NS_ASSERT_MSG(endPoints.size() == 1, "Demux returned more than one endpoint");
    NS_LOG_LOGIC("TcpL4Protocol " << this
                                  << " received a packet and"
                                     " now forwarding it up to endpoint/socket");

    (*endPoints.begin())
        ->ForwardUp(packet, incomingIpHeader, incomingTcpHeader.GetSourcePort(), incomingInterface);

    return IpL4Protocol::RX_OK;
}

IpL4Protocol::RxStatus
TcpL4Protocol::Receive(Ptr<Packet> packet,
                       const Ipv6Header& incomingIpHeader,
                       Ptr<Ipv6Interface> interface)
{
    NS_LOG_FUNCTION(this << packet << incomingIpHeader.GetSource()
                         << incomingIpHeader.GetDestination());

    TcpHeader incomingTcpHeader;
    IpL4Protocol::RxStatus checksumControl;

    // If we are receiving a v4-mapped packet, we will re-calculate the TCP checksum
    // Is it worth checking every received "v6" packet to see if it is v4-mapped in
    // order to avoid re-calculating TCP checksums for v4-mapped packets?

    checksumControl = PacketReceived(packet,
                                     incomingTcpHeader,
                                     incomingIpHeader.GetSource(),
                                     incomingIpHeader.GetDestination());

    if (checksumControl != IpL4Protocol::RX_OK)
    {
        return checksumControl;
    }

    Ipv6EndPointDemux::EndPoints endPoints =
        m_endPoints6->Lookup(incomingIpHeader.GetDestination(),
                             incomingTcpHeader.GetDestinationPort(),
                             incomingIpHeader.GetSource(),
                             incomingTcpHeader.GetSourcePort(),
                             interface);
    if (endPoints.empty())
    {
        NS_LOG_LOGIC("TcpL4Protocol "
                     << this
                     << " received a packet but"
                        " no endpoints matched."
                     << " destination IP: " << incomingIpHeader.GetDestination()
                     << " destination port: " << incomingTcpHeader.GetDestinationPort()
                     << " source IP: " << incomingIpHeader.GetSource()
                     << " source port: " << incomingTcpHeader.GetSourcePort());

        NoEndPointsFound(incomingTcpHeader,
                         incomingIpHeader.GetSource(),
                         incomingIpHeader.GetDestination());

        return IpL4Protocol::RX_ENDPOINT_CLOSED;
    }

    NS_ASSERT_MSG(endPoints.size() == 1, "Demux returned more than one endpoint");
    NS_LOG_LOGIC("TcpL4Protocol " << this
                                  << " received a packet and"
                                     " now forwarding it up to endpoint/socket");

    (*endPoints.begin())
        ->ForwardUp(packet, incomingIpHeader, incomingTcpHeader.GetSourcePort(), interface);

    return IpL4Protocol::RX_OK;
}

void
TcpL4Protocol::SendPacketV4(Ptr<Packet> packet,
                            const TcpHeader& outgoing,
                            const Ipv4Address& saddr,
                            const Ipv4Address& daddr,
                            Ptr<NetDevice> oif) const
{
    NS_LOG_FUNCTION(this << packet << saddr << daddr << oif);
    NS_LOG_LOGIC("TcpL4Protocol " << this << " sending seq " << outgoing.GetSequenceNumber()
                                  << " ack " << outgoing.GetAckNumber() << " flags "
                                  << TcpHeader::FlagsToString(outgoing.GetFlags()) << " data size "
                                  << packet->GetSize());
    // XXX outgoingHeader cannot be logged

    TcpHeader outgoingHeader = outgoing;
    /** @todo UrgentPointer */
    /* outgoingHeader.SetUrgentPointer (0); */
    if (Node::ChecksumEnabled())
    {
        outgoingHeader.EnableChecksums();
    }
    outgoingHeader.InitializeChecksum(saddr, daddr, PROT_NUMBER);

    packet->AddHeader(outgoingHeader);

    Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4>();
    if (ipv4)
    {
        Ipv4Header header;
        header.SetSource(saddr);
        header.SetDestination(daddr);
        header.SetProtocol(PROT_NUMBER);
        Socket::SocketErrno errno_;
        Ptr<Ipv4Route> route;
        if (ipv4->GetRoutingProtocol())
        {
            route = ipv4->GetRoutingProtocol()->RouteOutput(packet, header, oif, errno_);
        }
        else
        {
            NS_LOG_ERROR("No IPV4 Routing Protocol");
            route = nullptr;
        }
        m_downTarget(packet, saddr, daddr, PROT_NUMBER, route);
    }
    else
    {
        NS_FATAL_ERROR("Trying to use Tcp on a node without an Ipv4 interface");
    }
}

void
TcpL4Protocol::SendPacketV6(Ptr<Packet> packet,
                            const TcpHeader& outgoing,
                            const Ipv6Address& saddr,
                            const Ipv6Address& daddr,
                            Ptr<NetDevice> oif) const
{
    NS_LOG_FUNCTION(this << packet << saddr << daddr << oif);
    NS_LOG_LOGIC("TcpL4Protocol " << this << " sending seq " << outgoing.GetSequenceNumber()
                                  << " ack " << outgoing.GetAckNumber() << " flags "
                                  << TcpHeader::FlagsToString(outgoing.GetFlags()) << " data size "
                                  << packet->GetSize());
    // XXX outgoingHeader cannot be logged

    if (daddr.IsIpv4MappedAddress())
    {
        return (SendPacket(packet,
                           outgoing,
                           saddr.GetIpv4MappedAddress(),
                           daddr.GetIpv4MappedAddress(),
                           oif));
    }
    TcpHeader outgoingHeader = outgoing;
    /** @todo UrgentPointer */
    /* outgoingHeader.SetUrgentPointer (0); */
    if (Node::ChecksumEnabled())
    {
        outgoingHeader.EnableChecksums();
    }
    outgoingHeader.InitializeChecksum(saddr, daddr, PROT_NUMBER);

    packet->AddHeader(outgoingHeader);

    Ptr<Ipv6L3Protocol> ipv6 = m_node->GetObject<Ipv6L3Protocol>();
    if (ipv6)
    {
        Ipv6Header header;
        header.SetSource(saddr);
        header.SetDestination(daddr);
        header.SetNextHeader(PROT_NUMBER);
        Socket::SocketErrno errno_;
        Ptr<Ipv6Route> route;
        if (ipv6->GetRoutingProtocol())
        {
            route = ipv6->GetRoutingProtocol()->RouteOutput(packet, header, oif, errno_);
        }
        else
        {
            NS_LOG_ERROR("No IPV6 Routing Protocol");
            route = nullptr;
        }
        m_downTarget6(packet, saddr, daddr, PROT_NUMBER, route);
    }
    else
    {
        NS_FATAL_ERROR("Trying to use Tcp on a node without an Ipv6 interface");
    }
}

void
TcpL4Protocol::SendPacket(Ptr<Packet> pkt,
                          const TcpHeader& outgoing,
                          const Address& saddr,
                          const Address& daddr,
                          Ptr<NetDevice> oif) const
{
    NS_LOG_FUNCTION(this << pkt << outgoing << saddr << daddr << oif);
    if (Ipv4Address::IsMatchingType(saddr))
    {
        NS_ASSERT(Ipv4Address::IsMatchingType(daddr));

        SendPacketV4(pkt,
                     outgoing,
                     Ipv4Address::ConvertFrom(saddr),
                     Ipv4Address::ConvertFrom(daddr),
                     oif);

        return;
    }
    else if (Ipv6Address::IsMatchingType(saddr))
    {
        NS_ASSERT(Ipv6Address::IsMatchingType(daddr));

        SendPacketV6(pkt,
                     outgoing,
                     Ipv6Address::ConvertFrom(saddr),
                     Ipv6Address::ConvertFrom(daddr),
                     oif);

        return;
    }
    else if (InetSocketAddress::IsMatchingType(saddr))
    {
        InetSocketAddress s = InetSocketAddress::ConvertFrom(saddr);
        InetSocketAddress d = InetSocketAddress::ConvertFrom(daddr);

        SendPacketV4(pkt, outgoing, s.GetIpv4(), d.GetIpv4(), oif);

        return;
    }
    else if (Inet6SocketAddress::IsMatchingType(saddr))
    {
        Inet6SocketAddress s = Inet6SocketAddress::ConvertFrom(saddr);
        Inet6SocketAddress d = Inet6SocketAddress::ConvertFrom(daddr);

        SendPacketV6(pkt, outgoing, s.GetIpv6(), d.GetIpv6(), oif);

        return;
    }

    NS_FATAL_ERROR("Trying to send a packet without IP addresses");
}

void
TcpL4Protocol::AddSocket(Ptr<TcpSocketBase> socket)
{
    NS_LOG_FUNCTION(this << socket);

    for (auto& socketItem : m_sockets)
    {
        if (socketItem.second == socket)
        {
            return;
        }
    }
    m_sockets[m_socketIndex++] = socket;
}

bool
TcpL4Protocol::RemoveSocket(Ptr<TcpSocketBase> socket)
{
    NS_LOG_FUNCTION(this << socket);

    for (auto& socketItem : m_sockets)
    {
        if (socketItem.second == socket)
        {
            socketItem.second = nullptr;
            m_sockets.erase(socketItem.first);
            return true;
        }
    }

    return false;
}

void
TcpL4Protocol::SetDownTarget(IpL4Protocol::DownTargetCallback callback)
{
    m_downTarget = callback;
}

IpL4Protocol::DownTargetCallback
TcpL4Protocol::GetDownTarget() const
{
    return m_downTarget;
}

void
TcpL4Protocol::SetDownTarget6(IpL4Protocol::DownTargetCallback6 callback)
{
    m_downTarget6 = callback;
}

IpL4Protocol::DownTargetCallback6
TcpL4Protocol::GetDownTarget6() const
{
    return m_downTarget6;
}

} // namespace ns3
