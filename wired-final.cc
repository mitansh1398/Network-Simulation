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
NS_LOG_COMPONENT_DEFINE ("wired-topology");

using namespace ns3;

int main(int argc, char *argv[]){

    std::string dataRate = "100Mbps"; //DataRate for application
    std::string tcpProtocol = "Vegas"; // TCP Protocol used as argument
    double simulationTime = 5;  // Simulation Time

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
    } else { //if tcp protocol argument doesn't match then return error and abort simulation
        TypeId tcpTid;
        NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpProtocol, &tcpTid), "TypeId " << tcpProtocol << " not found");
        Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcpProtocol)));
    }

    // GNU Plot data set declaration
    Gnuplot2dDataset dataset_src_dst;
    Gnuplot2dDataset dataset_jain_fairness;

    //creating topology for different packet size and running simulation on them
    for(int i=0; i<10; i++){
        uint32_t packetSize = packetSizeArray[i];

        // Setting MSS for TCP layer, Size heere is set without header  
        Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (packetSize));

        //created topology
        // Used dumbbell P2P topoloy with both leafs having 1 size
        PointToPointHelper routerToRouter;
        routerToRouter.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
        routerToRouter.SetChannelAttribute ("Delay", StringValue ("50ms"));
        routerToRouter.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue (QueueSize("62500B")));  
        PointToPointHelper nodeToRouter;
        nodeToRouter.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
        nodeToRouter.SetChannelAttribute ("Delay", StringValue ("20ms"));
        nodeToRouter.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", QueueSizeValue (QueueSize("250000B")));  
        PointToPointDumbbellHelper topology (1, nodeToRouter, 1, nodeToRouter, routerToRouter);

        //stack installation in entire topology
        InternetStackHelper stack;
        topology.InstallStack (stack);

        //subnet allocation to topology, left and right leaf seperately and bottleneck seperately
        topology.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"),
                            Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"));

        /*Installing application on server side i.e. Node2.
         Off time set to 0*/
        OnOffHelper serverApp ("ns3::TcpSocketFactory", 
                                (InetSocketAddress (topology.GetRightIpv4Address (0), 1000)));
        serverApp.SetAttribute ("PacketSize", UintegerValue (packetSize));
        serverApp.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
        serverApp.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
        serverApp.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        ApplicationContainer onOffApp = serverApp.Install(topology.GetLeft(0));

        //Installing data sink on destination Node 3
        PacketSinkHelper clientSink ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 1000));
        ApplicationContainer sinkApp = clientSink.Install (topology.GetRight(0));

        //started application on both sides
        onOffApp.Start(Seconds(1.0));
        sinkApp.Start(Seconds (0.0));

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
        
        Simulator::Destroy ();
    }

    // Plt file and Graph file for throughput vs packetsize
	std::string graphFileNameSD = tcpProtocol+"throughput_vs_packetSizeSD.png";
	std::string plotFileNameSD = tcpProtocol+"throughput_vs_packetSizeSD.plt";
    std::string plotTitleSD = "wired Throughput vs PacketSize in " + tcpProtocol;
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
    std::string plotTitleJFind = "wired JFind vs PacketSize in " + tcpProtocol;
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