#include "../../src/internet/helper/ipv4-interface-container.h"
#include "../../src/internet/model/ipv4.h"
#include "tutorial-app.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <fstream>
#include <sys/stat.h>
#include <tuple>
#include <vector>
#include <filesystem> // Required for std::filesystem::create_directories in LogMetric

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpExample");

// global
PointToPointDumbbellHelper* dumbbell;

/**
 * Congestion window change callback
 *
 * @param oldCwnd Old congestion window.
 * @param newCwnd New congestion window.
 */
static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\t" << newCwnd);
    *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd
                         << std::endl;
}

/**
 * Logs a metric to a specified directory and file.
 *
 * @param directory Directory to write to (e.g., "metrics")
 * @param filename  Name of the file (e.g., "jfi.txt", "fct.txt")
 * @param message   The string message or data to log
 */
void LogMetric(const std::string& directory, const std::string& filename, const std::string& message)
{
    // Create the directory if it doesn't exist
    std::filesystem::create_directories(directory);

    // Full path to the output file
    std::string fullPath = directory + "/" + filename;

    // Open in append mode
    std::ofstream outFile(fullPath, std::ios_base::app);

    if (!outFile.is_open())
    {
        std::cerr << "Failed to open " + fullPath +" for appending.\n";
        return;
    }

    outFile << message << std::endl;
    outFile.close();
}

// Helper struct to manage throughput monitoring state (raw pointer management)
struct ThroughputMonitorState
{
    Ptr<OutputStreamWrapper> m_stream;
    double m_intervalSeconds;
    double m_lastTimeSeconds;
    uint64_t m_lastRxBytes;
    Ptr<PacketSink> m_sink;
    bool m_active; 
    uint32_t m_flowId;

    ThroughputMonitorState(Ptr<PacketSink> sink,
                           Ptr<OutputStreamWrapper> stream,
                           double interval,
                           double startTime,
                           uint32_t flowId)
        : m_stream(stream),
          m_intervalSeconds(interval),
          m_lastTimeSeconds(startTime),
          m_lastRxBytes(0),
          m_sink(sink),
          m_active(true),
          m_flowId(flowId)
    {

    }

    void StopMonitoringScheduled()
    {
        m_active = false;
    }
};

/**
 * Periodically logs the throughput for a flow.
 * Manages the lifetime of the 'state' object.
 *
 * @param state Raw pointer to the ThroughputMonitorState object holding context.
 */
static void
PeriodicThroughputLogger(ThroughputMonitorState* state) // Takes a raw pointer
{
    if (!state->m_active) // Primary condition for stopping and cleaning up
    {
        delete state; // Clean up the object
        return;
    }

    if (!state->m_sink || !state->m_stream || !state->m_stream->GetStream())
    {
        NS_LOG_WARN("PeriodicThroughputLogger: Invalid sink or stream for flow " << state->m_flowId << ". Deleting state.");
        delete state; // Clean up to prevent further issues or leaks
        return;
    }

    double currentTimeSeconds = Simulator::Now().GetSeconds();
    uint64_t currentRxBytes = state->m_sink->GetTotalRx();

    double throughputMbps = 0;
    if (currentTimeSeconds > state->m_lastTimeSeconds) // Avoid division by zero if time hasn't advanced
    {
        throughputMbps = (currentRxBytes - state->m_lastRxBytes) * 8.0 /
                         (currentTimeSeconds - state->m_lastTimeSeconds) / 1e6;
    }

    *state->m_stream->GetStream() << currentTimeSeconds << "\t" << throughputMbps << std::endl;

    state->m_lastTimeSeconds = currentTimeSeconds;
    state->m_lastRxBytes = currentRxBytes;

    // Reschedule ONLY if still active AND simulation is not finished
    if (state->m_active && !Simulator::IsFinished()) // Check m_active again before rescheduling
    {
        Simulator::Schedule(Seconds(state->m_intervalSeconds), &PeriodicThroughputLogger, state);
    }
    else
    {
        // Not rescheduling (either m_active became false, or simulation finished).
        // This is the point to clean up the state object.
        delete state;
    }
}

std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> SetupDumbbellTopology() {
    // number of nodes on the left and right
    uint32_t nLeaf = 2;

    PointToPointHelper accessLink;
    accessLink.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    accessLink.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneckLink;
    bottleneckLink.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneckLink.SetChannelAttribute("Delay", StringValue("20ms"));

    dumbbell = new PointToPointDumbbellHelper(
        nLeaf, accessLink,
        nLeaf, accessLink,
        bottleneckLink
    );

    // install stack on all nodes
    InternetStackHelper stack;
    dumbbell->InstallStack(stack);

    Ipv4AddressHelper leftIp, rightIp, routerIp;
    leftIp.SetBase("10.1.1.0", "255.255.255.0");
    rightIp.SetBase("10.2.1.0", "255.255.255.0");
    routerIp.SetBase("10.3.1.0", "255.255.255.0");

    dumbbell->AssignIpv4Addresses (leftIp, rightIp, routerIp);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    std::vector<Ipv4Address> senderAddresses;
    std::vector<Ipv4Address> receiverAddresses;

    for (uint32_t i = 0; i<dumbbell->LeftCount();i++)
    {
        Ptr<Node> node = dumbbell->GetLeft(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        senderAddresses.push_back(ipv4->GetAddress(1,0).GetLocal());
    }
    for (uint32_t i = 0; i<dumbbell->RightCount();i++)
    {
        Ptr<Node> node = dumbbell->GetRight(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        receiverAddresses.push_back(ipv4->GetAddress(1,0).GetLocal());
    }
    return std::make_tuple(senderAddresses, receiverAddresses);
}

void AddTcpFlow(uint32_t flowIndex, uint32_t srcIndex, uint32_t dstIndex, std::string tcpType, uint32_t packetSize, uint32_t nPackets, double startTime, double stopTime) {
    // set the TCP variant
    if (tcpType == "cubic") {
        Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    } else if (tcpType == "bbr") {
        // BBRv1
        Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpBbr::GetTypeId()));
    } else if (tcpType == "jumpstart") {
        Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpCubicJumpstart::GetTypeId()));
    } else if (tcpType == "bbrv3") {
        Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpBbr3::GetTypeId()));
    } else {
        NS_LOG_UNCOND("Unknown TCP type: " << tcpType);
        return;
    }

    // get the destination node's address
    Ptr<Node> dstNode = dumbbell->GetRight(dstIndex);
    Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4>();
    Ipv4Address dstAddr = ipv4->GetAddress(1, 0).GetLocal(); 

    // set up port and sink application
    uint16_t port = 50000 + srcIndex;
    Address sinkAddr(InetSocketAddress(dstAddr, port));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(dstNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(stopTime + 1));
    
    // set up Application
    Ptr<Node> srcNode = dumbbell->GetLeft(srcIndex);

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(srcNode, TcpSocketFactory::GetTypeId());

    Ptr<TutorialApp> app = CreateObject<TutorialApp>();
    app->Setup(ns3TcpSocket, sinkAddr, packetSize, nPackets);
    srcNode->AddApplication(app);
    app->SetStartTime(Seconds(startTime));
    app->SetStopTime(Seconds(stopTime));

    // set up congestion window tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("metrics/flow-" + std::to_string(flowIndex) + ".cwnd");
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow",
                                             MakeBoundCallback(&CwndChange, stream));

    // Setup throughput monitoring
    Ptr<PacketSink> packetSink = DynamicCast<PacketSink>(sinkApp.Get(0));
    std::string throughputFileName = "metrics/flow-" + std::to_string(flowIndex) + "-throughput.txt";
    std::filesystem::create_directories("metrics"); 
    Ptr<OutputStreamWrapper> throughputStream = asciiTraceHelper.CreateFileStream(throughputFileName);
    
    double monitoringInterval = 0.01;
    
    ThroughputMonitorState* monitorStateRaw = 
        new ThroughputMonitorState(packetSink, throughputStream, monitoringInterval, startTime, flowIndex);

    if (stopTime > startTime) 
    {
        double firstLogTime = (startTime == 0.0) ? monitoringInterval : startTime + monitoringInterval;

        Simulator::Schedule(Seconds(firstLogTime), &PeriodicThroughputLogger, monitorStateRaw);
        Simulator::Schedule(Seconds(stopTime), &ThroughputMonitorState::StopMonitoringScheduled, monitorStateRaw);
    } else
    {
        delete monitorStateRaw;
    }

    // debugging purposes
    std::cout << "Destination IP: " << dstAddr << std::endl;

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp.Get(0));
    Simulator::Schedule(Seconds(stopTime + 0.5), [=]() {
        std::cout << "Sink received: " << sink1->GetTotalRx() << " bytes\n";
    });
}
std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> SetupSingleFlow()
{
    // set up topology and simulate flows
    std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> result = SetupDumbbellTopology();

    uint32_t flowCnt = 0;
    AddTcpFlow(flowCnt++, 0, 1, "jumpstart", 1024, 128, 2.0, 10.0);
    return result;
}
std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> SetupMultipleFlows()
{
    // set up topology and simulate flows
    std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> result = SetupDumbbellTopology();

    uint32_t flowCnt = 0;
    AddTcpFlow(flowCnt++, 0, 1, "cubic", 1024, 1000, 2.0, 10.0);
    AddTcpFlow(flowCnt++, 1, 0, "cubic", 1024, 1000, 3.0, 10.0);
    return result;
}

int main(int argc, char* argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // set up topology and simulate flows
   // std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> addresses = SetupMultipleFlows();
    std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> addresses = SetupSingleFlow();
    std::vector<Ipv4Address> senderAddresses = std::get<0>(addresses);
    std::vector<Ipv4Address> receiverAddresses =  std::get<1>(addresses);

    // set up flow monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    double sumThroughput = 0.0;
    double sumSquaredThroughput = 0.0;
    uint32_t flowCount = 0;
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    auto stats = flowMonitor->GetFlowStats();
    // print for debugging
    for (auto& flow : stats) {
        auto t = classifier->FindFlow(flow.first);
        if (std::find(senderAddresses.begin(), senderAddresses.end(), t.sourceAddress) != senderAddresses.end())
        {
            double throughput = (flow.second.txBytes * 8.0 /
                (flow.second.timeLastRxPacket.GetSeconds() -
                 flow.second.timeFirstTxPacket.GetSeconds()) / 1e6);
            double fct = (flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket).GetSeconds();
            std::cout << "Flow " << std::to_string(flowCount + 1)<< " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Tx Bytes:   " << flow.second.txBytes << "\n";
            std::cout << "  Rx Bytes:   " << flow.second.rxBytes << "\n";
            std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
            std::cout << "  Throughput: " << throughput << " Mbps\n";
            std::cout << "Flow Completion Time: " << fct << "\n\n";
            LogMetric("metrics", "fct.txt", "FCT for Flow " + std::to_string(flowCount + 1) + " = " + std::to_string(fct));
            sumThroughput += throughput;
            sumSquaredThroughput += throughput * throughput;
            flowCount++;
        }
    }
    double jfi = (sumThroughput * sumThroughput) / (flowCount * sumSquaredThroughput);
    std::cout << "Jain's Fairness Index: " << jfi << std::endl;
    LogMetric("metrics", "jfi.txt", "JFI = " + std::to_string(jfi));

    Simulator::Destroy();
    delete dumbbell;  // cleanup
    return 0;
}
