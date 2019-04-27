#include "ns3/core-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/bridge-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/gnuplot.h"


// Network topology
//
//           100Mb/s, 20ms       10Mb/s, 100ms       100Mb/s, 20ms 
//       n0-----------------bs1-----------------bs2-----------------n1

using namespace ns3;

int main(int argc, char *argv[]){
    uint32_t packetSize = 44;
    std::string dataRate = "100Mbps";
    std::string tcpProtocol = "Westwood";
    bool pcapTracing = false;
    double simulationTime = 5;
    std::string flowMonitorFile = "wired-fm";
    std::string pcapFile = "wired-pcap";
    uint32_t packetSizeArray[10] = {40, 44, 48, 52, 60, 552, 576, 628, 1420, 1500};
    //taken arguments from cmd
    CommandLine cmd;
    cmd.AddValue ("packetSize", "Payload size in bytes", packetSize);
    cmd.AddValue ("tcpProtocol", "Transport protocol to use:" "TcpVegas, TcpVeno, TcpWestwood", tcpProtocol);
    cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue ("pcap", "Enable/disable PCAP Tracing", pcapTracing);
    cmd.Parse (argc,argv);
    std::cout<<"Simulation time "<<simulationTime<<std::endl;
    std::cout<<"tcpProtocol"<<tcpProtocol<<std::endl;
    if (tcpProtocol.compare("Westwood")==0){ 
        std::cout<<"Westwood running"<<std::endl;
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
        Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOOD));
    } else if(tcpProtocol.compare("Vegas")==0){
        std::cout<<"Vegas running"<<std::endl;
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVegas::GetTypeId ()));
    } else if(tcpProtocol.compare("Veno")==0){
        std::cout<<"Veno running"<<std::endl;
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpVeno::GetTypeId ()));
    } else {
        TypeId tcpTid;
        NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpProtocol, &tcpTid), "TypeId " << tcpProtocol << " not found");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcpProtocol)));
    }

    Gnuplot2dDataset dataset_src_dst;
    Gnuplot2dDataset dataset_dst_src;


    for(int i=0;i<10;i++){
        packetSize = packetSizeArray[i];
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packetSize+20));
        //xml output file name and pcap file output for traces
        flowMonitorFile = "wifi-fm_" + std::to_string(packetSize) + "_" + tcpProtocol;
        pcapFile = "wifi-pcap_" + std::to_string(packetSize) + "_" + tcpProtocol;
        /* Build nodes. */
        NodeContainer Node0;
        Node0.Create (1);
        NodeContainer Node1;
        Node1.Create (1);

        NodeContainer bs;
        bs.Create (2);

        //adding point to point channel to base stations
        PointToPointHelper bsHelper;
        bsHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        bsHelper.SetChannelAttribute ("Delay", StringValue ("100ms"));
        bsHelper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue (QueueSize("125000B")));

        NetDeviceContainer bsDevices;
        bsDevices = bsHelper.Install (bs);
        NodeContainer wifiApNode0 = bs.Get (0);
        NodeContainer wifiApNode1 = bs.Get (1);

        //for nodes
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
        phy.SetChannel (channel.Create ());
        WifiHelper wifi;
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
        WifiMacHelper mac;
        Ssid ssid = Ssid ("BS-0");
        mac.SetType ("ns3::StaWifiMac",
            "Ssid", SsidValue (ssid),
            "ActiveProbing", BooleanValue (false));
        NetDeviceContainer staDevices0;
        staDevices0 = wifi.Install (phy, mac, Node0);

        mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
        NetDeviceContainer apDevices0;
        apDevices0 = wifi.Install (phy, mac, wifiApNode0);



        YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default ();
        YansWifiPhyHelper phy1 = YansWifiPhyHelper::Default ();
        phy1.SetChannel (channel1.Create ());
        WifiHelper wifi1;
        wifi1.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
        WifiMacHelper mac1;
        Ssid ssid1 = Ssid ("BS-1");
        mac1.SetType ("ns3::StaWifiMac",
            "Ssid", SsidValue (ssid1),
            "ActiveProbing", BooleanValue (false));
        NetDeviceContainer staDevices1;
        staDevices1 = wifi1.Install (phy1, mac1, Node1);

        mac1.SetType ("ns3::ApWifiMac", 
            "Ssid", SsidValue (ssid1));
        NetDeviceContainer apDevices1;
        apDevices1 = wifi1.Install (phy1, mac1, wifiApNode1);

        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc =
        CreateObject<ListPositionAllocator> ();
        positionAlloc->Add (Vector (0.0, 0.0, 0.0));
        positionAlloc->Add (Vector (60.0, 0.0, 0.0));
        positionAlloc->Add (Vector (120.0, 0.0, 0.0));
        positionAlloc->Add (Vector (180.0, 0.0, 0.0));
        mobility.SetPositionAllocator (positionAlloc);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (Node0.Get(0));
        mobility.Install (bs.Get(0));
        mobility.Install (bs.Get(1));
        mobility.Install (Node1.Get(0));

        InternetStackHelper stack;
        stack.Install (bs);
        stack.Install (Node0);
        stack.Install (Node1);



        Ipv4AddressHelper address;

        address.SetBase ("10.1.2.0", "255.255.255.0");
        // address.Assign (bs);
        Ipv4InterfaceContainer p2pInterfaces;
        p2pInterfaces = address.Assign (bsDevices);

        address.SetBase ("10.1.1.0", "255.255.255.0");
        address.Assign (apDevices0);
        Ipv4InterfaceContainer n0;
        n0 = address.Assign (staDevices0);

        address.SetBase ("10.1.3.0", "255.255.255.0");
        address.Assign (apDevices1);
        Ipv4InterfaceContainer n1;
        n1 = address.Assign (staDevices1);

        /* Generate Route. */
        Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

        /* Generate Application. */
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory",
                                InetSocketAddress (Ipv4Address::GetAny (), 9000));
        ApplicationContainer sinkApp = sinkHelper.Install (Node1);

        // Install TCP sender on n0
        OnOffHelper client ("ns3::TcpSocketFactory",
                            (InetSocketAddress (n1.GetAddress(0), 9000)));
        client.SetAttribute ("PacketSize", UintegerValue (packetSize));
        client.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
        client.SetAttribute (
            "OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute (
            "OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer onOffApp = client.Install(Node0);

        onOffApp.Start(Seconds(1.0));
        sinkApp.Start(Seconds (0.0));


        Ptr<FlowMonitor> flowMonitor;
        FlowMonitorHelper flowHelper;
        flowMonitor = flowHelper.InstallAll();
        flowMonitor->CheckForLostPackets();
        // if (pcapTracing){
        //     nodeToRouter.EnablePcapAll(pcapFile);
        // }

        Simulator::Stop (Seconds (simulationTime + 1));
        Simulator::Run ();
        flowMonitor->SerializeToXmlFile(flowMonitorFile, true, true);

        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

        std::cout<<"Packet Size "<<packetSize<<std::endl;
        //display stats
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator j = stats.begin(); j != stats.end(); ++j){
            double throughput_tcp;
            throughput_tcp = j->second.rxBytes * 8.0 / (j->second.timeLastRxPacket.GetSeconds () - j->second.timeFirstRxPacket.GetSeconds ()) / 1000;
            if(j->first==1){
                dataset_src_dst.Add (packetSize, throughput_tcp);
            } else {
                dataset_dst_src.Add (packetSize, throughput_tcp);
            }
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(j->first);
            std::cout << tcpProtocol <<" Flow " << j->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "Throughput: " << throughput_tcp << " Kbps" << std::endl;
        }
        Simulator::Destroy ();
    }

    std::string graphicsFileNameSD = tcpProtocol+"throughput_vs_packetSizeSDwifi.png";
	std::string plotFileNameSD = tcpProtocol+"throughput_vs_packetSizeSDwifi.plt";
    std::string plotTitleSD = "Throughput vs PacketSize in " + tcpProtocol;
    dataset_src_dst.SetTitle ("Source to Destination");
	dataset_src_dst.SetStyle (Gnuplot2dDataset::LINES_POINTS);
    Gnuplot plot(graphicsFileNameSD);

    plot.SetTitle (plotTitleSD);
    plot.SetTerminal ("png");
    plot.SetLegend ("Packet Size(in Bytes)", "Throughput Values(in Kbps)");
    plot.AddDataset (dataset_src_dst);
    std::ofstream plotFileSD (plotFileNameSD.c_str());
    plot.GenerateOutput (plotFileSD);
    plotFileSD.close ();

    std::string graphicsFileNameDS = tcpProtocol+"throughput_vs_packetSizeDSwifi.png";
	std::string plotFileNameDS = tcpProtocol+"throughput_vs_packetSizeDSwifi.plt";
    std::string plotTitleDS = "Throughput vs PacketSize in " + tcpProtocol;

    dataset_dst_src.SetTitle ("Destination to Source");
	dataset_dst_src.SetStyle (Gnuplot2dDataset::LINES_POINTS);

    Gnuplot plot2(graphicsFileNameDS);

    plot2.SetTitle (plotTitleDS);
    plot2.SetTerminal ("png");
    plot2.SetLegend ("Packet Size(in Bytes)", "Throughput Values(in Kbps)");
    plot2.AddDataset (dataset_dst_src);
    std::ofstream plotFileDS (plotFileNameDS.c_str());
    plot2.GenerateOutput (plotFileDS);
    plotFileDS.close ();
}
