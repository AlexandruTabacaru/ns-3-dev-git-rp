/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "tutorial-app.h"

#include "ns3/applications-module.h"

using namespace ns3;

TutorialApp::TutorialApp()
    : m_socket(nullptr),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
    //   m_dataRate(0),
    //   m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

TutorialApp::~TutorialApp()
{
    m_socket = nullptr;
}

/* static */
TypeId
TutorialApp::GetTypeId()
{
    static TypeId tid = TypeId("TutorialApp")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<TutorialApp>();
    return tid;
}

void
TutorialApp::Setup(Ptr<Socket> socket,
                   Address address,
                   uint32_t packetSize,
                   uint32_t nPackets)
{
    m_socket = socket;
    m_peer = address;
    m_packetSize = packetSize;
    m_nPackets = nPackets;
    // m_dataRate = dataRate;
}

void
TutorialApp::StartApplication()
{
    m_running = true;
    m_packetsSent = 0;

    m_socket->Bind();
    m_socket->Connect(m_peer);

    m_socket->SetSendCallback(MakeCallback(&TutorialApp::HandleSend, this));

    SendPacket();
}

void
TutorialApp::HandleSend(Ptr<Socket> socket, uint32_t availableBuffer)
{
    if (m_running)
    {
        SendPacket();
    }
}

void
TutorialApp::StopApplication()
{
    if (!m_running)
        return;

    m_running = false;

    // if (m_sendEvent.IsPending())
    // {
    //     Simulator::Cancel(m_sendEvent);
    // }

    if (m_socket)
    {
        m_socket->Close();
    }
}

void
TutorialApp::SendPacket()
{
    // Ptr<Packet> packet = Create<Packet>(m_packetSize);
    // m_socket->Send(packet);

    // if (++m_packetsSent < m_nPackets)
    // {
    //     ScheduleTx();
    // } else
    // {
    //     StopApplication();
    // }
    while (m_packetsSent < m_nPackets)
    {
        Ptr<Packet> packet = Create<Packet>(m_packetSize);
        int sent = m_socket->Send(packet);

        if (sent > 0)
        {
            ++m_packetsSent;
        }
        else
        {
            // Socket buffer is full; wait for space
            break;
        }
    }

    if (m_packetsSent >= m_nPackets)
    {
        StopApplication(); // We're done
    }
}

void
TutorialApp::ScheduleTx()
{
    if (m_running)
    {
        // Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
        // m_sendEvent = Simulator::Schedule(tNext, &TutorialApp::SendPacket, this);
    }
}
