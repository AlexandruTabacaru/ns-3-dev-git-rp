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

    // print routing tables to a file
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("routing-table.txt");
    Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt(Seconds(0.5), stream);
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
    app->Setup(ns3TcpSocket, sinkAddr, packetSize, nPackets, DataRate("1Mbps"));
    srcNode->AddApplication(app);
    app->SetStartTime(Seconds(startTime));
    app->SetStopTime(Seconds(stopTime));

    // set up congestion window tracing
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("flow-" + std::to_string(flowIndex) + ".cwnd");
    ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow",
                                             MakeBoundCallback(&CwndChange, stream));


    // debugging purposes
    std::cout << "Destination IP: " << dstAddr << std::endl;

    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(sinkApp.Get(0));
    Simulator::Schedule(Seconds(stopTime + 0.5), [=]() {
        std::cout << "Sink received: " << sink1->GetTotalRx() << " bytes\n";
    });
}
void AddP2PTcpFlow(Ptr<Node> srcNode, Ptr<Node> dstNode, Ipv4Address dstAddr,
                   std::string tcpType, uint32_t packetSize, uint32_t nPackets,
                   double startTime, double stopTime, uint32_t flowIndex) {
    // TCP Variant
    if (tcpType == "cubic") {
        Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpCubic::GetTypeId()));
    } else if (tcpType == "bbr") {
        Config::Set("/NodeList/*/$ns3::TcpL4Protocol/SocketType", TypeIdValue(TcpBbr::GetTypeId()));
    }

    // Set up sink
    uint16_t port = 50000;
    Address sinkAddr(InetSocketAddress(dstAddr, port));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(dstNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(stopTime + 1));

    // Application on sender
    Ptr<Socket> socket = Socket::CreateSocket(srcNode, TcpSocketFactory::GetTypeId());

    Ptr<TutorialApp> app = CreateObject<TutorialApp>();
    app->Setup(socket, sinkAddr, packetSize, nPackets, DataRate("1Mbps"));
    srcNode->AddApplication(app);
    app->SetStartTime(Seconds(startTime));
    app->SetStopTime(Seconds(stopTime));

    // cwnd tracing
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("flow-" + std::to_string(flowIndex) + ".cwnd");
    socket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream));
}
std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> SetupSingleFlow()
{
    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5ms"));

    NetDeviceContainer devices = p2p.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    Ipv4Address senderAddress = interfaces.GetAddress(0);
    Ipv4Address receiverAddress = interfaces.GetAddress(1);
    AddP2PTcpFlow(nodes.Get(0), nodes.Get(1), interfaces.GetAddress(1),
          "cubic", 1024, 1000, 2.0, 10.0, 0);
    std::vector<Ipv4Address> senderAddresses;
    std::vector<Ipv4Address> receiverAddresses;
    senderAddresses.push_back(senderAddress);
    receiverAddresses.push_back(receiverAddress);
    return std::make_tuple(senderAddresses, receiverAddresses);
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

    std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> addresses = SetupMultipleFlows();
    // std::tuple<std::vector<Ipv4Address>,std::vector<Ipv4Address>> addresses = SetupSingleFlow();
    std::vector<Ipv4Address> senderAddresses = std::get<0>(addresses);
    std::vector<Ipv4Address> receiverAddresses =  std::get<1>(addresses);

    // set up flow monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
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
            double throughput = (flow.second.rxBytes * 8.0 /
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
