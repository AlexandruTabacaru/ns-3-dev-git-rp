#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-dumbbell.h"
#include "ns3/applications-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

using namespace ns3;

// global
PointToPointDumbbellHelper* dumbbell;

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

    // print routing tables to a file
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("routing-table.txt");
    Ipv4GlobalRoutingHelper::PrintRoutingTableAllAt(Seconds(0.5), stream);
}

void AddTcpFlow(uint32_t srcIndex, uint32_t dstIndex, std::string tcpType, double startTime, double stopTime) {
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

    // set up OnOff application
    OnOffHelper onoff("ns3::TcpSocketFactory", sinkAddr);
    onoff.SetAttribute("DataRate", StringValue("10Mbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));
    onoff.SetAttribute("StartTime", TimeValue(Seconds(startTime)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(stopTime)));

    onoff.Install(dumbbell->GetLeft(srcIndex));

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

    // enable logging
    // LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    // LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO); 
    // LogComponentEnable("TcpL4Protocol", LOG_LEVEL_INFO);

    Time::SetResolution(Time::NS);

    // set up topology and simulate flows
    SetupDumbbellTopology();
    AddTcpFlow(0, 1, "cubic", 2.0, 10.0);

    // set up flow monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();

    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    auto stats = flowMonitor->GetFlowStats();
    // print for debugging
    for (auto& flow : stats) {
        auto t = classifier->FindFlow(flow.first);
        std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Bytes:   " << flow.second.txBytes << "\n";
        std::cout << "  Rx Bytes:   " << flow.second.rxBytes << "\n";
        std::cout << "  Lost Packets: " << flow.second.lostPackets << "\n";
        std::cout << "  Throughput: " << (flow.second.rxBytes * 8.0 /
            (flow.second.timeLastRxPacket.GetSeconds() -
             flow.second.timeFirstTxPacket.GetSeconds()) / 1e6) << " Mbps\n\n";
    }

    Simulator::Destroy();
    delete dumbbell;  // cleanup
    return 0;
}
