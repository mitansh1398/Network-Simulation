#include "ns3/core-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/config-store-module.h"
#include "ns3/gnuplot.h"
#include "ns3/gnuplot-helper.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/nstime.h"
#include "ns3/propagation-delay-model.h"
NS_LOG_COMPONENT_DEFINE ("dumbell-wifi");

using namespace ns3;

/*
Network topology

          100Mb/s, 200ns       10Mb/s, 100ms       100Mb/s, 200ns 
      n0-----------------bs0-----------------bs1-----------------n1

Assumptions : n0-bs0 delay has been changed to 200ns to represent more practical case, by taking distance between those two as 60m.
              using ChannelWidth=40 and setting ShortGuardIntervalSupported=false and the using HtMcs5, we have datarate of approx 108 Mbps, much closer to defined 100 Mbps
*/



int main (int argc, char *argv[])
{
    
    std::string dataRate = "100Mbps"; //DataRate for application
    std::string tcpProtocol = "Vegas"; // TCP Protocol used as argument
    double simulationTime = 5;  // Simulation Time

    uint32_t packetSizeArray[10] = {40, 44, 48, 52, 60, 552, 576, 628, 1420, 1500};
    CommandLine cmd;

    //Taking TCp arguments and parsing it
    cmd.AddValue ("tcpProtocol", "Transport protocol to use:" "TcpVegas, TcpVeno, TcpWestwood", tcpProtocol);
    cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse (argc,argv);
    std::cout<<"Simulation time "<<simulationTime<<std::endl;
    std::cout<<"tcpProtocol"<<tcpProtocol<<std::endl;
    
    // comparing tcp agents
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

    // setting drop tail policy
    Config::SetDefault ("ns3::WifiMacQueue::DropPolicy", EnumValue(WifiMacQueue::DROP_NEWEST));
    
    Gnuplot2dDataset dataset_src_dst;
    Gnuplot2dDataset dataset_jain_fairness;

    for(int i=0; i<10; i++){
        // iterating over all packet sizes to plot the graph
        uint32_t packetSize = packetSizeArray[i];
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packetSize));
        WifiMacHelper wifiMacSTA;
        WifiMacHelper wifiMacBS;
        WifiHelper wifiHelper;
        wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211n_5GHZ);

        YansWifiChannelHelper wifiChannel;
        wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
        wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5e9));
        Ptr<YansWifiChannel> channel1 = wifiChannel.Create (); 
        Ptr<YansWifiChannel> channel2 = wifiChannel.Create ();

        // physical layer setup
        YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
        wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager", "DataMode", StringValue ("HtMcs5"), "ControlMode", StringValue ("HtMcs5"));

        NodeContainer node0;
        NodeContainer bs;
        NodeContainer node1;

        node0.Create (1);
        bs.Create (2);
        node1.Create (1);

        Ssid ssid = Ssid ("wifi-topology");

        wifiMacBS.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
        wifiMacSTA.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));

        // Node 0 
        wifiPhy.SetChannel (channel1);
        NetDeviceContainer node0NetDevice;
        node0NetDevice = wifiHelper.Install (wifiPhy, wifiMacSTA, node0);

        // Base station 1
        wifiPhy.SetChannel (channel1);
        NetDeviceContainer bs0NetDevice;
        bs0NetDevice = wifiHelper.Install (wifiPhy, wifiMacBS, bs.Get(0));

        // Base station 2
        wifiPhy.SetChannel (channel2);
        NetDeviceContainer bs1NetDevice;
        bs1NetDevice = wifiHelper.Install (wifiPhy, wifiMacBS, bs.Get(1));

        // Node 1
        wifiPhy.SetChannel (channel2);
        NetDeviceContainer node1NetDevice;
        node1NetDevice = wifiHelper.Install (wifiPhy, wifiMacSTA, node1);

        
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelWidth", UintegerValue (40));
        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (false));


        // Setting Position
        MobilityHelper mobility;
        Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
        positionAlloc->Add (Vector (0.0, 0.0, 0.0));
        positionAlloc->Add (Vector (60.0, 0.0, 0.0));
        positionAlloc->Add (Vector (12000.0, 0.0, 0.0));
        positionAlloc->Add (Vector (12060.0, 0.0, 0.0));

        mobility.SetPositionAllocator (positionAlloc);
        mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
        mobility.Install (node0.Get(0));
        mobility.Install (bs.Get(0));
        mobility.Install (bs.Get(1));
        mobility.Install (node1.Get(0));

        // Point to Point links between Base Station 1 and Base Station 2
        PointToPointHelper bsHelper;
        bsHelper.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        bsHelper.SetChannelAttribute ("Delay", StringValue ("100ms"));
        // bsHelper.SetQueue ("ns3::DropTailQueue","Mode",EnumValue (DropTailQueue::QUEUE_MODE_BYTES),"MaxBytes",UintegerValue (125000)) ;
        bsHelper.SetQueue ("ns3::DropTailQueue", "MaxSize",StringValue ("125000B")) ;

        NetDeviceContainer bsNetDevices = bsHelper.Install (bs.Get(0), bs.Get(1));

        // setting up internet stack
        InternetStackHelper stack;
        stack.Install (node0);
        stack.Install (bs);
        stack.Install (node1);

        // assignning IP addresses
        Ipv4AddressHelper address;
        address.SetBase ("10.1.1.0", "255.255.255.0");
        Ipv4InterfaceContainer n0_interface;
        n0_interface = address.Assign (node0NetDevice);
        Ipv4InterfaceContainer bs0_interface;
        bs0_interface = address.Assign (bs0NetDevice);

        address.SetBase ("10.2.1.0", "255.255.255.0");
        Ipv4InterfaceContainer bs1_interface;
        bs1_interface = address.Assign (bs1NetDevice);
        Ipv4InterfaceContainer n1_interface;
        n1_interface = address.Assign (node1NetDevice);

        address.SetBase ("10.3.1.0", "255.255.255.0");
        Ipv4InterfaceContainer p2pInterface;
        p2pInterface = address.Assign (bsNetDevices);

        // Sink on node1
        PacketSinkHelper clientSink ("ns3::TcpSocketFactory",InetSocketAddress (Ipv4Address::GetAny (), 1000));
        ApplicationContainer sinkApp = clientSink.Install (node1.Get(0));

        // Install TCP sender on node0
        OnOffHelper serverApp ("ns3::TcpSocketFactory", (InetSocketAddress (n1_interface.GetAddress (0), 1000)));
        serverApp.SetAttribute ("PacketSize", UintegerValue (packetSize));
        serverApp.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
        serverApp.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        serverApp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer onOffApp;
        onOffApp = serverApp.Install (node0.Get(0));

        // starting application
        onOffApp.Start (Seconds (1.0));
        sinkApp.Start (Seconds (0.0));

        Ptr<FlowMonitor> flowMonitor;
        FlowMonitorHelper flowHelper;
        flowMonitor = flowHelper.InstallAll();
        // Populating routuing tables 
        Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

        // Stop time of simulator~
        Simulator::Stop (Seconds (simulationTime+1));
        Simulator::Run ();
        Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
        std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();
        for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator j = stats.begin(); j != stats.end(); ++j){
            if(j->first==1){
                double throughput_tcp = j->second.rxBytes * 8.0 / (j->second.timeLastRxPacket.GetSeconds () - j->second.timeFirstRxPacket.GetSeconds ()) / 1000;
                dataset_src_dst.Add (packetSize, throughput_tcp);
                std::cout<<"Packet Size: "<<packetSize<<"; ";
                std::cout << "Throughput: " << throughput_tcp << " Kbps; ";
                dataset_src_dst.Add (packetSize, throughput_tcp);
                double fairness_index = (throughput_tcp)*(throughput_tcp)/(1*(throughput_tcp*throughput_tcp));
                dataset_jain_fairness.Add(packetSize, fairness_index);
                std::cout << "Fairness Index: " << fairness_index << std::endl;
            }
        }
        Simulator::Destroy();
    }

    // Plotting the graphs
    std::string graphFileNameSD = tcpProtocol+"throughput_vs_packetSizeSD.png";
    std::string plotFileNameSD = tcpProtocol+"throughput_vs_packetSizeSD.plt";
    std::string plotTitleSD = "wireless Throughput vs PacketSize in " + tcpProtocol;
    dataset_src_dst.SetTitle ("Source to Destination");
    dataset_src_dst.SetStyle (Gnuplot2dDataset::LINES_POINTS);
    Gnuplot plot(graphFileNameSD);

    plot.SetTitle (plotTitleSD);
    plot.SetTerminal ("png");
    plot.SetLegend ("Packet Size(in Bytes)", "Throughput Values(in Kbps)");
    plot.AddDataset (dataset_src_dst);
    std::ofstream plotFileSD (plotFileNameSD.c_str());
    plot.GenerateOutput (plotFileSD);
    plotFileSD.close ();

    // Plt file and Graph file for Jain Fairness Index vs packetsize
    std::string graphFileNameJFind = tcpProtocol+"JFind_vs_packetSizeSD.png";
    std::string plotFileNameJFind = tcpProtocol+"JFind_vs_packetSizeSD.plt";
    std::string plotTitleJFind = "wireless JFind vs PacketSize in " + tcpProtocol;
    dataset_jain_fairness.SetTitle ("Source to Destination");
    dataset_jain_fairness.SetStyle (Gnuplot2dDataset::LINES_POINTS);
    Gnuplot plot3(graphFileNameJFind);

    plot3.SetTitle (plotTitleJFind);
    plot3.SetTerminal ("png");
    plot3.SetLegend ("Packet Size(in Bytes)", "JFind Values");
    plot3.AddDataset (dataset_jain_fairness);
    std::ofstream plotFileJFind (plotFileNameJFind.c_str());
    plot3.GenerateOutput (plotFileJFind);
    plotFileJFind.close ();
    return 0;
}
