#include <fstream>
#include "tutorial-app.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include <sys/stat.h>

using namespace ns3;

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
void SetupDumbbellTopology() {
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


    // debugging purposes
    std::cout << "Destination IP: " << dstAddr << std::endl;

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp.Get(0));
    Simulator::Schedule(Seconds(stopTime + 0.5), [=]() {
        std::cout << "Sink received: " << sink1->GetTotalRx() << " bytes\n";
    });
}

int main(int argc, char* argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    // set up topology and simulate flows
    SetupDumbbellTopology();

    uint32_t flowCnt = 0;
    AddTcpFlow(flowCnt++, 0, 1, "jumpstart", 1024, 1000, 2.0, 50.0);
    // AddTcpFlow(flowCnt++, 1, 0, "cubic", 1024, 1000, 3.0, 10.0);

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
        double throughput = (flow.second.rxBytes * 8.0 /
            (flow.second.timeLastRxPacket.GetSeconds() -
             flow.second.timeFirstTxPacket.GetSeconds()) / 1e6);
        double fct = (flow.second.timeLastRxPacket - flow.second.timeFirstTxPacket).GetSeconds();
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << flow.second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << flow.second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Throughput: " << throughput << " Mbps\n";
        std::cout << "Flow Completion Time: " << fct << "\n\n";
        LogMetric("metrics", "fct.txt", "FCT for Flow " + std::to_string(flow.first) + " = " + std::to_string(fct));
        sumThroughput += throughput;
        sumSquaredThroughput += throughput * throughput;
        flowCount++;
    }
    double jfi = (sumThroughput * sumThroughput) / (flowCount * sumSquaredThroughput);
    std::cout << "Jain's Fairness Index: " << jfi << std::endl;
    LogMetric("metrics", "jfi.txt", "JFI = " + std::to_string(jfi));
    Simulator::Destroy();
    delete dumbbell;  // cleanup
    return 0;
}
