#include "ns3/core-module.h"
#include "ns3/global-route-manager.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/tcp-westwood.h"
#include "ns3/error-model.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/gnuplot.h"
#include <string>

// Network topology
//
//           100Mb/s, 20ms      10Mb/s, 50ms     100Mb/s, 20ms
//       n2-----------------r1-----------------r2-----------------n3
NS_LOG_COMPONENT_DEFINE ("dumbell-wired");

using namespace ns3;

int main(int argc, char *argv[]){
    uint32_t packetSize = 44;
    std::string dataRate = "100Mbps";
    std::string tcpProtocol = "Vegas";
    bool pcapTracing = false;
    double simulationTime = 5;
    std::string flowMonitorFile = "wired-fm";
    std::string pcapFile = "wired-pcap";
    uint32_t packetSizeArray[10] = {40, 44, 48, 52, 60, 552, 576, 628, 1420, 1500};
    //taken arguments from cmd
    CommandLine cmd;
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

    for(int i=0; i<10; i++){
        packetSize = packetSizeArray[i];
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packetSize));
        //xml output file name and pcap file output for traces
        flowMonitorFile = "wired-fm_" + std::to_string(packetSize) + "_" + tcpProtocol;
        pcapFile = "wired-pcap_" + std::to_string(packetSize) + "_" + tcpProtocol;

        //which tcp congestion algorithm to use.

        //created topology
        PointToPointHelper routerToRouter;
        routerToRouter.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        routerToRouter.SetChannelAttribute ("Delay", StringValue ("50ms"));
        routerToRouter.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue (QueueSize("62500B")));  
        PointToPointHelper nodeToRouter;
        nodeToRouter.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
        nodeToRouter.SetChannelAttribute ("Delay", StringValue ("20ms"));
        nodeToRouter.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue (QueueSize("250000B")));  
        PointToPointDumbbellHelper topology (1, nodeToRouter, 1, nodeToRouter, routerToRouter);

        //stack installation
        InternetStackHelper stack;
        topology.InstallStack (stack);

        //subnet allocation
        topology.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"));

        //adding application on server side
        OnOffHelper serverApp ("ns3::TcpSocketFactory", 
                                (InetSocketAddress (topology.GetRightIpv4Address (0), 1000)));
        serverApp.SetAttribute ("PacketSize", UintegerValue (packetSize));
        serverApp.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
        serverApp.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        serverApp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer onOffApp = serverApp.Install(topology.GetLeft(0));

        //adding sink on destination node
        // GetAny() returns 0.0.0.0 i.e. the listen address for the sink application.
        PacketSinkHelper clientSink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 1000));
        ApplicationContainer sinkApp = clientSink.Install (topology.GetRight(0));

        //started application on both sides
        onOffApp.Start(Seconds(1.0));
        sinkApp.Start(Seconds (0.0));
        // onOffApp.Stop(Seconds(simulationTime+1));
        // sinkApp.Stop(Seconds (simulationTime+1));

        //routuing tables 
        Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

        //added flow monitor to get stats
        Ptr<FlowMonitor> flowMonitor;
        FlowMonitorHelper flowHelper;
        flowMonitor = flowHelper.InstallAll();
        flowMonitor->CheckForLostPackets();
            
        //to get pacap output
        if (pcapTracing){
            nodeToRouter.EnablePcapAll(pcapFile);
        }

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
	std::string graphicsFileNameSD = tcpProtocol+"throughput_vs_packetSizeSD.png";
	std::string plotFileNameSD = tcpProtocol+"throughput_vs_packetSizeSD.plt";
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

    std::string graphicsFileNameDS = tcpProtocol+"throughput_vs_packetSizeDS.png";
	std::string plotFileNameDS = tcpProtocol+"throughput_vs_packetSizeDS.plt";
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
    return 0;
}       