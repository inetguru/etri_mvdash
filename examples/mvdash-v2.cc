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
#include "ns3/point-to-point-module.h"
#include "ns3/mvdash-helper.h"
#include "ns3/mvdash_client.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("mvdash");

// Service Function Declartions
void TxHandler (Ptr<const mvdashClient> c, Ptr<const Packet> p);
void RxHandler (Ptr<const mvdashClient> c, Ptr<const Packet> p);
void RequestTraceHandler (Ptr<const mvdashClient> client, requestEvent ev, int32_t id);
void SegmentTraceHandler (Ptr<const mvdashClient> client, segmentEvent ev, st_mvdashRequest sinfo);
void ControllerTraceHandler (Ptr<const mvdashClient> client, controllerState state,
                             controllerTraceEvent ev, int32_t tid);
int InitDynamicBandwidth (std::string bwTraceFile, Ptr<NetDevice> dev, uint64_t simTime);
void SetLogLevel (LogLevel log_precision);
void SetConfig ();

int
main (int argc, char *argv[])
{
  SetLogLevel (LOG_LEVEL_INFO); // LOG_LEVEL_INFO; LOG_LEVEL_LOGIC; LOG_ALL;
  SetConfig ();
  // ===========================================================================================
  // Simulation Parameters & Variables for Command Line Argumentsd
  uint32_t simId = 0; // The simulation id is used to distinguish log file
  double simTime =50.0; // Simulation Finish Time in seconds
  uint32_t useHttp3 = 0; // 0 - HTTP2/TCP, 1 - HTTP3/QUIC
  uint32_t useDynamicBW =0; // 0 - Dynamic Bandwidth Off, 1 - Dynamic Bandwidth On
  int nClients = 1;

  std::string bwInit = "5Mbps";
  std::string path = "./contrib/etri_mvdash/";
  std::string bwTrace = "sbwtrace_5Mbps_max.csv";
  std::string vpInfo = "viewpoint_transition.csv";
  std::string mvInfo = "dataset_size.csv";
  std::string vpModel = "free"; //[free or markovian]
  std::string mvAlgo = "adaptation_mpc"; //[maximize_current or adaptation_test or adaptation_tobasco or adaptation_panda]
  std::string reqType = "group"; //DION Test [group or hybrid]

  CommandLine cmd;
  cmd.Usage ("ETRI Multi-View Video DASH Streaming Simulation.\n");
  cmd.AddValue ("simId", "The simulation's index (for logging purposes)", simId);
  cmd.AddValue ("simTime", "The simulation Finish Time", simTime);
  cmd.AddValue ("useHttp3", "[0 - HTTP2/TCP, 1 - HTTP3/QUIC] ", useHttp3);
  cmd.AddValue ("useDynamicBW", "[0 - OFF, 1 - ON] ", useDynamicBW);
  cmd.AddValue ("nClients", "Number of Clients", nClients);
  cmd.AddValue ("bwInit", "The initial bandwidth for the bottleneck link", bwInit);
  cmd.AddValue ("bwTrace", "The name of the file containing bandwidth Traces", bwTrace);
  cmd.AddValue ("vpInfo", "The name of the file containing viewpoint switching info", vpInfo);
  cmd.AddValue ("vpModel", "[markovian, free]", vpModel);
  cmd.AddValue ("mvInfo", "The name of the file containing Multi-View video source info", mvInfo);
  cmd.AddValue ("mvAlgo", "[maximize_current,newone, adaptation_test, adaptation_tobasco, adaptation_panda]", mvAlgo);
  cmd.AddValue ("reqType", "[group, hybrid, single]", reqType);
  cmd.Parse (argc, argv);

  // ===========================================================================================
  /* Build Simulation Topology */
  NodeContainer routerNodes, serverNodes, clientNodes;
  routerNodes.Create (2);
  serverNodes.Create (1);
  clientNodes.Create (nClients);

  PointToPointHelper routerLink, serverLink, clientLinks;
  routerLink.SetDeviceAttribute ("DataRate", StringValue (bwInit));
  routerLink.SetChannelAttribute ("Delay", StringValue ("40ms"));
  serverLink.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  serverLink.SetChannelAttribute ("Delay", StringValue ("5ms"));
  clientLinks.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  clientLinks.SetChannelAttribute ("Delay", StringValue ("5ms"));

  NetDeviceContainer routerDevices, serverDevices;
  std::vector<NetDeviceContainer> clientDevices (nClients);

  routerDevices = routerLink.Install (routerNodes.Get (0), routerNodes.Get (1));
  serverDevices = serverLink.Install (serverNodes.Get (0), routerNodes.Get (0));
  for (int i = 0; i < nClients; i++)
    {
      clientDevices[i] = clientLinks.Install (routerNodes.Get (1), clientNodes.Get (i));
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
  for (int i = 0; i < nClients; i++)
    {
      address.Assign (clientDevices[i]);
      address.NewNetwork ();
    }

  // ===========================================================================================
  /* Handling Dynamic Bandwidth */
  if (useDynamicBW)
    InitDynamicBandwidth (path + bwTrace, routerDevices.Get (0), (uint64_t) (1000000 * simTime));

  // ===========================================================================================
  /* Install Server Application */
  uint16_t serverPort = 9;
  Address serverAddress = InetSocketAddress (serverInterfaces.GetAddress (0), serverPort);
  mvdashServerHelper serverHelper (InetSocketAddress (Ipv4Address::GetAny (), serverPort),
                                   useHttp3);
  ApplicationContainer serverApp = serverHelper.Install (serverNodes);
  serverApp.Start (Seconds (0.0));

  /* Install DASH Clients at clientNodes */
  mvdashClientHelper clientHelper (serverAddress, useHttp3);
  clientHelper.SetAttribute ("SimId", UintegerValue (simId));
  clientHelper.SetAttribute ("VPInfo", StringValue (path + vpInfo));
  clientHelper.SetAttribute ("VPModel", StringValue (vpModel));
  clientHelper.SetAttribute ("MVInfo", StringValue (path + mvInfo));
  clientHelper.SetAttribute ("MVAlgo", StringValue (mvAlgo));
  clientHelper.SetAttribute ("reqType", StringValue (reqType));

  //    ApplicationContainer clientApps = clientHelper.Install (clientNodes, mvAlgo);
  ApplicationContainer clientApps = clientHelper.Install (clientNodes);

  for (int i = 0; i < nClients; i++)
    {
      clientApps.Get (i)->SetStartTime (Seconds (0.1 + i * 0.45));
      Ptr<mvdashClient> mvclient = DynamicCast<mvdashClient> (clientApps.Get (i));
      mvclient->TraceConnectWithoutContext ("ControllerTrace",
                                            MakeCallback (&ControllerTraceHandler));
      // mvclient->TraceConnectWithoutContext ("RequestTrace", MakeCallback (&RequestTraceHandler));
        //  mvclient->TraceConnectWithoutContext ("SegmentTrace", MakeCallback (&SegmentTraceHandler));
      //    mvclient->TraceConnectWithoutContext ("Tx", MakeCallback (&TxHandler));
        //  mvclient->TraceConnectWithoutContext ("Rx", MakeCallback (&RxHandler));
    }

  clientApps.Stop (Seconds (simTime));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}

void
SetLogLevel (LogLevel log_precision)
{
  LogComponentEnable ("mvdash", log_precision);
  LogComponentEnable ("mvdashClient", log_precision);
  // LogComponentEnable ("mvdashServer", log_precision);
  // LogComponentEnable ("maximizeCurrentAdaptation", log_precision);
  // LogComponentEnable ("Free_Viewpoint_Model", log_precision);
  LogComponentEnable ("adaptationTest", log_precision);
  // LogComponentEnable ("adaptationTobasco", log_precision);
  // LogComponentEnable ("adaptationPanda", log_precision);
  LogComponentEnable ("adaptationMpc", log_precision);
  LogComponentEnable ("adaptationQmetric", log_precision);
  // LogComponentEnable ("MultiView_Model", log_precision);
}

void
SetConfig ()
{
  uint32_t nBufSize = 600000;
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (1446));
  Config::SetDefault ("ns3::TcpSocket::SndBufSize", UintegerValue (nBufSize));
}

void
TxHandler (Ptr<const mvdashClient> c, Ptr<const Packet> p)
{
  NS_LOG_INFO (p->GetSize () << " Sent by Client " << c->m_clientId << " at "
                             << Simulator::Now ().As (Time::S));
}

void
RxHandler (Ptr<const mvdashClient> c, Ptr<const Packet> p)
{
  NS_LOG_INFO (p->GetSize () << " Received by Client " << c->m_clientId << " at "
                             << Simulator::Now ().As (Time::S));
}

void
RequestTraceHandler (Ptr<const mvdashClient> client, requestEvent ev, int32_t id)
{
  switch (ev)
    {
    case reqev_reqMsgSent:
      NS_LOG_INFO ("[REQ] Request Sent : " << id);
      break;
    case reqev_startReceiving:
      NS_LOG_INFO ("[REQ] Start Receiving : " << id);
      break;
    case reqev_endReceiving:
      NS_LOG_INFO ("[REQ] End Receiving : " << id);
      break;
    default:
      NS_LOG_INFO ("[REQ] Improper Event" << ev);
      break;
    }
}
void
SegmentTraceHandler (Ptr<const mvdashClient> client, segmentEvent ev, st_mvdashRequest sinfo)
{
  switch (ev)
    {
    case segev_startReceiving:
      NS_LOG_INFO ("Segment Start Receiving : <" << sinfo.viewpoint << ", " << sinfo.timeIndex
                                                 << ", " << sinfo.qualityIndex << ", "
                                                 << sinfo.segmentSize << ">");
      break;
    case segev_endReceiving:
      NS_LOG_INFO ("Segment End Receiving : <" << sinfo.viewpoint << ", " << sinfo.timeIndex << ", "
                                               << sinfo.qualityIndex << ", " << sinfo.segmentSize
                                               << ">");
      break;
    default:
      NS_LOG_INFO ("Improper Event" << ev);
      break;
    }
}
void
ControllerTraceHandler (Ptr<const mvdashClient> client, controllerState state,
                        controllerTraceEvent ev, int32_t tid)
{
  const char *state_names[] = {" [S_0]", " [S_D]", " [SDP]", " [S_P]", " [S_T]"};
  std::string str;

  switch (ev)
    {
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
    case cteBufferUnderrun:
      str = " : Buffer Underrun Occurred - ";
    default:
      str = " : Undefined ";
      break;
    };
  NS_LOG_INFO (Simulator::Now ().As (Time::S) << state_names[state] << str << tid);
}

static void
bwevent_handler (Ptr<NetDevice> dev, uint64_t bps)
{
  Ptr<PointToPointNetDevice> mdev = DynamicCast<PointToPointNetDevice> (dev);
  mdev->SetDataRate (DataRate (bps));
}

int
InitDynamicBandwidth (std::string bwTraceFile, Ptr<NetDevice> dev, uint64_t simTime)
{
  NS_LOG_UNCOND ("Dynamic Bandwidth Enabled");
  std::ifstream myfile;
  myfile.open (bwTraceFile.c_str ());
  if (!myfile)
    {
      NS_LOG_ERROR ("Dynamic Bandwidth Trace File Open Error");
      return -1;
    }

  std::string temp;
  uint64_t time_change;
  uint64_t bps;
  while (std::getline (myfile, temp))
    {
      if (temp.empty ())
        break;
      std::stringstream buffer (temp);
      buffer >> time_change;
      buffer >> bps;
      if (time_change > simTime)
        break;
      //NS_LOG_INFO("Time " << (double)(time_change/1000000.0) << " BPS " << bps);
      Simulator::Schedule (MicroSeconds (time_change), &bwevent_handler, dev, bps);
    }
  return 1;
}