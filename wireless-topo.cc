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

    std::string dataRate = "100Mbps"; //DataRate for application
    std::string tcpProtocol = "Vegas"; // TCP Protocol used as argument
    double simulationTime = 6;  // Simulation Time

    uint32_t packetSizeArray[10] = {40, 44, 48, 52, 60, 552, 576, 628, 1420, 1500};
    
    /* taken arguments from cmd */
    CommandLine cmd;
    cmd.AddValue ("tcpProtocol", "Transport protocol to use:" "TcpVegas, TcpVeno, TcpWestwood", tcpProtocol);
    cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse (argc,argv);
    
    std::cout<<"Simulation time "<<simulationTime<<std::endl;
    std::cout<<"tcpProtocol"<<tcpProtocol<<std::endl;
    
    // Setting TCP Protocol according to given argument
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

    // GNU Plot data set declaration
    Gnuplot2dDataset dataset_src_dst;
    Gnuplot2dDataset dataset_dst_src;
    Gnuplot2dDataset dataset_jain_fairness;

    for(int i=0;i<10;i++){
        uint32_t packetSize = packetSizeArray[i];
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packetSize+20));
        /* Building topology */
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

        // Installing net device
        NetDeviceContainer bsDevices;
        bsDevices = bsHelper.Install (bs);
        NodeContainer wifiApNode0 = bs.Get (0);
        NodeContainer wifiApNode1 = bs.Get (1);

        //Creating Wifi
        //Creating Wifi Channel
        YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
        channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
        //Creating Physical helper
        YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
        phy.SetChannel (channel.Create ());
        WifiHelper wifi;
        //Using Constant Rate wifi manager
        wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
        WifiMacHelper mac;
        Ssid ssid = Ssid ("BS-0");
        mac.SetType ("ns3::StaWifiMac","Ssid", SsidValue (ssid),"ActiveProbing", BooleanValue (false));
        NetDeviceContainer staDevices0;
        //instaling wifi on Node0
        staDevices0 = wifi.Install (phy, mac, Node0);

        mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
        NetDeviceContainer apDevices0;
        apDevices0 = wifi.Install (phy, mac, wifiApNode0);

        //Creating Wifi Channel
        YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default ();
        channel1.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
        //Creating Physical helper
        YansWifiPhyHelper phy1 = YansWifiPhyHelper::Default ();
        phy1.SetChannel (channel1.Create ());
        WifiHelper wifi1;
        //Using Constant Rate wifi manager
        wifi1.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
        WifiMacHelper mac1;
        Ssid ssid1 = Ssid ("BS-1");
        mac1.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid1), "ActiveProbing", BooleanValue (false));
        NetDeviceContainer staDevices1;
        //instaling wifi on Node1
        staDevices1 = wifi1.Install (phy1, mac1, Node1);

        mac1.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid1));
        NetDeviceContainer apDevices1;
        apDevices1 = wifi1.Install (phy1, mac1, wifiApNode1);

        // Adding  constant moilty to nodes
        // Wireless nodes are kept 60m apart
        // They can't be kept 20ms(60Km) apart, that's out of wifi's range 
        // Routers kept 12Km apart, as they are connected by wired link
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
        positionAlloc->Add (Vector (0.0, 0.0, 0.0));
        positionAlloc->Add (Vector (60.0, 0.0, 0.0));
        positionAlloc->Add (Vector (12000.0, 0.0, 0.0));
        positionAlloc->Add (Vector (12060.0, 0.0, 0.0));
        mobility.SetPositionAllocator (positionAlloc);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (Node0.Get(0));
        mobility.Install (bs.Get(0));
        mobility.Install (bs.Get(1));
        mobility.Install (Node1.Get(0));

        //Installing Internet stack on all nodes and router
        InternetStackHelper stack;
        stack.Install (bs);
        stack.Install (Node0);
        stack.Install (Node1);


        Ipv4AddressHelper address;

        //Assigning adressrange to subnets
        // Routers
        address.SetBase ("10.1.2.0", "255.255.255.0");
        Ipv4InterfaceContainer p2pInterfaces;
        p2pInterfaces = address.Assign (bsDevices);
        // Left Nodes
        address.SetBase ("10.1.1.0", "255.255.255.0");
        address.Assign (apDevices0);
        Ipv4InterfaceContainer n0;
        n0 = address.Assign (staDevices0);
        // Right Nodes
        address.SetBase ("10.1.3.0", "255.255.255.0");
        address.Assign (apDevices1);
        Ipv4InterfaceContainer n1;
        n1 = address.Assign (staDevices1);

        // Populating Routing tables
        Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

        // Installing data sink on  Node 1
        PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9000));
        ApplicationContainer sinkApp = sinkHelper.Install (Node1);

        // Installing OnOff App on  Node 1
        OnOffHelper client ("ns3::TcpSocketFactory", (InetSocketAddress (n1.GetAddress(0), 9000)));
        client.SetAttribute ("PacketSize", UintegerValue (packetSize));
        client.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
        client.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        client.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer onOffApp = client.Install(Node0);

        onOffApp.Start(Seconds(1.0));
        sinkApp.Start(Seconds (0.0));


        Ptr<FlowMonitor> flowMonitor;
        FlowMonitorHelper flowHelper;
        flowMonitor = flowHelper.InstallAll();
        flowMonitor->CheckForLostPackets();

        Simulator::Stop (Seconds (simulationTime + 1));
        Simulator::Run ();

        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

        std::cout<<"Packet Size "<<packetSize<<std::endl;
        //display stats
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator j = stats.begin(); j != stats.end(); ++j){
            double throughput_tcp;
            throughput_tcp = j->second.rxBytes * 8.0 / (j->second.timeLastRxPacket.GetSeconds () - j->second.timeFirstRxPacket.GetSeconds ()) / 1000;
            
            //Adding packet to GNU dataset
            if(j->first==1){
                dataset_src_dst.Add (packetSize, throughput_tcp);
                dataset_jain_fairness.Add(packetSize, (throughput_tcp*throughput_tcp)/((throughput_tcp)*(throughput_tcp)*1));
            } else {
                dataset_dst_src.Add (packetSize, throughput_tcp);
            }

            // Getting tupple according to flow Number
            Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(j->first);
            std::cout << tcpProtocol <<" Flow " << j->first  << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "Throughput: " << throughput_tcp << " Kbps" << std::endl;
            std::cout<<std::endl;
        }
        Simulator::Destroy ();
    }

    // Plt file and Graph file for throughput vs packetsize
    std::string graphicsFileNameSD = tcpProtocol+"throughput_vs_packetSizeSDwifi.png";
	std::string plotFileNameSD = tcpProtocol+"throughput_vs_packetSizeSDwifi.plt";
    std::string plotTitleSD = "wifi-topo Throughput vs PacketSize in " + tcpProtocol;
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

    // Plt file and Graph file for throughput vs packetsize
    std::string graphicsFileNameDS = tcpProtocol+"throughput_vs_packetSizeDSwifi.png";
	std::string plotFileNameDS = tcpProtocol+"throughput_vs_packetSizeDSwifi.plt";
    std::string plotTitleDS = "wifi-topo Throughput vs PacketSize in " + tcpProtocol;
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


    // Plt file and Graph file for Jain Fairness Index vs packetsize
    std::string graphFileNameJFind = tcpProtocol+"JFind_vs_packetSizeSD.png";
	std::string plotFileNameJFind = tcpProtocol+"JFind_vs_packetSizeSD.plt";
    std::string plotTitleJFind = "wifi-topo JFind vs PacketSize in " + tcpProtocol;
    dataset_jain_fairness.SetTitle ("Source to Destination");
	dataset_jain_fairness.SetStyle (Gnuplot2dDataset::LINES_POINTS);
    Gnuplot plot3(graphFileNameJFind);

    plot3.SetTitle (plotTitleJFind);
    plot3.SetTerminal ("png");
    plot3.SetLegend ("Packet Size(in Bytes)", "JFind Values");
    plot3.AddDataset (dataset_src_dst);
    std::ofstream plotFileJFind (plotFileNameJFind.c_str());
    plot3.GenerateOutput (plotFileJFind);
    plotFileJFind.close ();

    return 0;
}
