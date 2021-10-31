/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright 2021 ETRI (Electronic and Telecommunication Research Institute) KOREA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include "ns3/core-module.h"
#include "ns3/free_viewpoint_model.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/mvdash-helper.h"
#include "ns3/mvdash_client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("modeltest");


void TxHandler (Ptr<const mvdashClient> c, Ptr<const Packet> p) {
    NS_LOG_INFO(p->GetSize() << " Sent by Client " << c->m_clientId << " at " << Simulator::Now ().As (Time::S));
}

void RxHandler (Ptr<const mvdashClient> c, Ptr<const Packet> p) {
    NS_LOG_INFO(p->GetSize() << " Received by Client " << c->m_clientId << " at " << Simulator::Now ().As (Time::S));
}

void RequestTraceHandler(Ptr<const mvdashClient> client, requestEvent ev, int32_t id) {
    switch (ev) {
        case reqev_reqMsgSent: 
            NS_LOG_INFO("[REQ] Request Sent : " << id);
            break;
        case reqev_startReceiving:
            NS_LOG_INFO("[REQ] Start Receiving : " << id);
            break;
        case reqev_endReceiving:
            NS_LOG_INFO("[REQ] End Receiving : " << id);
            break;
        default :
            NS_LOG_INFO("[REQ] Improper Event" << ev);
            break; 
    }
}
void SegmentTraceHandler(Ptr<const mvdashClient> client, segmentEvent ev, st_mvdashRequest sinfo) {
    switch (ev) {
        case segev_startReceiving: 
            NS_LOG_INFO("Segment Start Receiving : <" << sinfo.viewpoint 
                << ", " << sinfo.timeIndex << ", " << sinfo.qualityIndex
                << ", " << sinfo.segmentSize << ">");
            break;
        case segev_endReceiving:
            NS_LOG_INFO("Segment End Receiving : <" << sinfo.viewpoint 
                << ", " << sinfo.timeIndex << ", " << sinfo.qualityIndex
                << ", " << sinfo.segmentSize << ">");
            break;
        default :
            NS_LOG_INFO("Improper Event" << ev);
            break; 
    }
}
void ControllerTraceHandler(Ptr<const mvdashClient> client, controllerState state, controllerTraceEvent ev, int32_t tid) {
    const char *state_names[] = {" [S_0]", " [S_D]", " [SDP]", " [S_P]", " [S_T]"};
    std::string str;
    
    switch (ev) {
        case cteSendRequest: 
            str = " : Send Request - ";    
            break;
        case cteDownloaded:
            str = " : Downloaded - ";
            break;
        case cteAllDownloaded:
            str = " : All segments downloaded";
            break;
        case cteStartPlayback:
            str = " : Start playback of - ";
            break;
        case cteEndPlayback:
            str = " : End playback of - ";
            break;
        case cteBufferUnderrun :
            str = " : Buffer Underrun Occurred - ";
        default :
            str = " : Undefined ";
            break; 
    };
    NS_LOG_INFO(Simulator::Now ().As (Time::S) << state_names[state] << str << tid);
}
int main(int argc, char *argv[]) {
    LogLevel log_precision = LOG_LEVEL_INFO; // LOG_LEVEL_INFO; LOG_LEVEL_LOGIC; LOG_ALL;
//    LogComponentEnable ("Free_Viewpoint_Model", log_precision);
    LogComponentEnable ("modeltest", log_precision);
    LogComponentEnable("mvdashClient", log_precision);
    LogComponentEnable("mvdashServer", log_precision);
    LogComponentEnable("maximizeCurrentAdaptation", log_precision);

    uint32_t useHttp3=0;      // 0 - HTTP2/TCP, 1 - HTTP3/QUIC    
    uint32_t nBufSize = 600000;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue (nBufSize));

/* Build Simulation Topology */
    int nClients = 1;

    NodeContainer routerNodes, serverNodes, clientNodes;
    routerNodes.Create (2);
    serverNodes.Create (1);
    clientNodes.Create (nClients);

    PointToPointHelper routerLink, serverLink, clientLinks;
    routerLink.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    routerLink.SetChannelAttribute ("Delay", StringValue ("40ms"));
    serverLink.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    serverLink.SetChannelAttribute ("Delay", StringValue ("5ms"));
    clientLinks.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    clientLinks.SetChannelAttribute ("Delay", StringValue ("5ms"));

    NetDeviceContainer routerDevices, serverDevices;
    std::vector <NetDeviceContainer> clientDevices(nClients);
    
    routerDevices = routerLink.Install(routerNodes.Get(0), routerNodes.Get(1));
    serverDevices = serverLink.Install(serverNodes.Get(0), routerNodes.Get(0));
    for (int i=0; i < nClients; i++) {
        clientDevices[i] = clientLinks.Install(routerNodes.Get(1), clientNodes.Get(i));
    }

    InternetStackHelper stack;
    stack.Install (routerNodes);
    stack.Install (serverNodes);
    stack.Install (clientNodes);

    /* Assign IP addresses */
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    address.Assign (routerDevices);
    address.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer serverInterfaces = address.Assign (serverDevices);
    address.SetBase ("10.1.3.0", "255.255.255.0");
    for (int i=0; i < nClients; i++) {
        address.Assign (clientDevices[i]);
        address.NewNetwork();
    }

    /* Install Server Application */
    uint16_t serverPort = 9;
    Address serverAddress = InetSocketAddress(serverInterfaces.GetAddress (0), serverPort);
    mvdashServerHelper serverHelper (InetSocketAddress(Ipv4Address::GetAny (), serverPort), useHttp3);
    ApplicationContainer serverApp = serverHelper.Install (serverNodes);
    serverApp.Start (Seconds (0.5));

    /* Install DASH Clients at clientNodes */
    mvdashClientHelper clientHelper (serverAddress, useHttp3);
    ApplicationContainer clientApps = clientHelper.Install (clientNodes, "test1");

    for (int i=0; i < nClients; i++) {
        clientApps.Get(i)->SetStartTime(Seconds(1.0+i*0.45));
        Ptr<mvdashClient> mvclient = DynamicCast<mvdashClient> (clientApps.Get (i));
        mvclient->TraceConnectWithoutContext ("ControllerTrace", MakeCallback (&ControllerTraceHandler));    
    //    mvclient->TraceConnectWithoutContext ("RequestTrace", MakeCallback (&RequestTraceHandler));
    //    mvclient->TraceConnectWithoutContext ("SegmentTrace", MakeCallback (&SegmentTraceHandler));    
    //    mvclient->TraceConnectWithoutContext ("Tx", MakeCallback (&TxHandler));
    //    mvclient->TraceConnectWithoutContext ("Rx", MakeCallback (&RxHandler));
    }

    clientApps.Stop(Seconds(50.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Run ();
    Simulator::Destroy ();
    NS_LOG_INFO ("Done."); 
}

/*    Free_Viewpoint_Model fview;

    int viewpoint;
    int64_t t_index = 0;

    for ( ; t_index < 100; t_index++) {
        viewpoint = fview.UpdateViewpoint(t_index);
        NS_LOG_INFO("viewpoint " << viewpoint);

    } */
