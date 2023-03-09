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
#include "mvdash_client.h"
#include <ns3/core-module.h>
#include "ns3/tcp-socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "free_viewpoint_model.h"
#include "markovian_viewpoint_model.h"
#include "ns3/maximize_current_adaptation.h"
#include "ns3/adaptation_test.h" // dion
#include "ns3/adaptation_qmetric.h" // dion
#include "ns3/adaptation_tobasco.h" // dion
#include "ns3/adaptation_panda.h" // dion
#include "ns3/adaptation_mpc.h" // dion
#include <numeric>
#include <math.h>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("mvdashClient");

NS_OBJECT_ENSURE_REGISTERED (mvdashClient);

TypeId
mvdashClient::GetTypeId (void)
{
  static TypeId tid =
      TypeId ("ns3::mvdashClient")
          .SetParent<Application> ()
          .SetGroupName ("Applications")
          .AddConstructor<mvdashClient> ()
          .AddAttribute ("SimId", "The ID of the current simulation, for logging purposes",
                         UintegerValue (0), MakeUintegerAccessor (&mvdashClient::m_simId),
                         MakeUintegerChecker<uint32_t> ())
          .AddAttribute ("ServerAddress", "The Address of the DASH server", AddressValue (),
                         MakeAddressAccessor (&mvdashClient::m_serverAddress),
                         MakeAddressChecker ())
          .AddAttribute ("ClientId", "The ID of the this client object, for logging purposes",
                         UintegerValue (0), MakeUintegerAccessor (&mvdashClient::m_clientId),
                         MakeUintegerChecker<uint32_t> ())
          .AddAttribute ("VPInfo",
                         "The relative path to the file containing viewpoint switching info",
                         StringValue ("./contrib/etri_mvdash/viewpoint_transition.csv"),
                         MakeStringAccessor (&mvdashClient::m_vpInfoFilePath), MakeStringChecker ())
          .AddAttribute ("VPModel", "The View-point Switching Model",
                         StringValue ("free"), //free or markovian
                         MakeStringAccessor (&mvdashClient::m_vpModelName), MakeStringChecker ())
          .AddAttribute (
              "MVInfo", "The relative path to the file containing the Multi-View Video Source Info",
              StringValue ("./contrib/etri_mvdash/multiviewvideo.csv"),
              MakeStringAccessor (&mvdashClient::m_mvInfoFilePath), MakeStringChecker ())
          .AddAttribute (
              "mvVmaf", "The relative path to the file containing the Multi-View Video Source VMAF",
              StringValue ("./contrib/etri_mvdash/multiviewvideo.csv"),
              MakeStringAccessor (&mvdashClient::m_mvVmafFilePath), MakeStringChecker ())
          .AddAttribute ("MVAlgo", "The Multi-View Video Streaming Adaptation Algorithm", // dion
                         StringValue ("adaptation_flush"), //maximize_current or adaptation_flush
                         MakeStringAccessor (&mvdashClient::m_mvAlgoName), MakeStringChecker ())
          .AddAttribute ("reqType", "The request mechanism", // dion
                         StringValue ("hybrid"), //group or hybrid
                         MakeStringAccessor (&mvdashClient::m_reqType), MakeStringChecker ())
          .AddTraceSource ("ControllerTrace", "Tracing Controller related events",
                           MakeTraceSourceAccessor (&mvdashClient::m_ctrlTrace),
                           "ns3::mvdashClient::ControllerEventCallback")
          .AddTraceSource ("RequestTrace", "Tracing Request related events",
                           MakeTraceSourceAccessor (&mvdashClient::m_reqTrace),
                           "ns3::mvdashClient::RequestEventCallback")
          .AddTraceSource ("SegmentTrace", "A new packet is sent",
                           MakeTraceSourceAccessor (&mvdashClient::m_segTrace),
                           "ns3::mvdashClient::SegementEventCallback")
          .AddTraceSource ("Rx", "A packet has been received",
                           MakeTraceSourceAccessor (&mvdashClient::m_rxTrace),
                           "ns3::Packet::TracedCallback")
          .AddTraceSource ("Tx", "A packet has been sent",
                           MakeTraceSourceAccessor (&mvdashClient::m_txTrace),
                           "ns3::Packet::TracedCallback");
  return tid;
}

mvdashClient::mvdashClient ()
    : m_socket (0),
      m_connected (false),
      m_state (initial),
      m_bytesReceived (0),
      m_sendRequestCounter (0),
      m_recvRequestCounter (-1),
      m_segStarted (false)
{
  NS_LOG_FUNCTION (this);
  m_delay = 0;
  skip_requestSegment = 0;
  m_tIndexLast = 0;
  m_tIndexPlay = 0;
  m_tIndexReqSent = -1;
  m_tIndexDownloaded = -1;
}

mvdashClient::~mvdashClient ()
{
  NS_LOG_FUNCTION (this);
}

void
mvdashClient::DoDispose (void)
{
  NS_LOG_FUNCTION (this);
  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

void
mvdashClient::Controller (controllerEvent event)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_INFO ("Controller S: " << m_state << " E:" << event << " at "
                                << Simulator::Now ().GetSeconds () << " --- " << m_tIndexLast);
  if (m_state == initial)
    {
      st_mvdashRequest *pReq = PrepareRequest (0);
      if (SendRequest (pReq, 5))
        {
          m_state = downloading;
        }
      free (pReq);
      return;
    }

  if (m_state == downloading) //Trigered when only download(playback freezing occured)
    {
      switch (event)
        {
        case downloadFinished:
          StartPlayback ();
          indexDownload = m_tIndexDownloaded;

          if (indexDownload >= m_tIndexLast) //finish to download final segment
            { // *e_df
              m_ctrlTrace (this, m_state, cteAllDownloaded, m_tIndexLast);
              m_state = playing;
            }
          else
            { // *e_d ----------------------------------------------------------------------------------------
              st_mvdashRequest *pReq = PrepareRequest (m_tIndexReqSent + 1);
              if (SendRequest (pReq, 5))
                {
                  m_state = downloadingPlaying;
                }
              free (pReq);
            }
          break;
        default:
          break;
        }
      return;
    }

  if (m_state == downloadingPlaying)
    {
      switch (event)
        {
        case viewChange: // case (viewChange || downloadFinished)
          case downloadFinished: {
            //prepare request first
            st_mvdashRequest *pReq = PrepareRequest (m_tIndexReqSent + 1);
            //Delay occurs
            if (m_delay > 0 && m_tIndexDownloaded <= m_tIndexLast)
              {
                /*  e_dirs */
                m_state = playing;
                NS_LOG_INFO ("***Group IRD Start at"
                             << Simulator::Now ().GetMicroSeconds () << " + " << m_delay << " finish at = "
                             << Simulator::Now ().GetMicroSeconds () + m_delay);
                controllerEvent ev = irdFinished;
                Simulator::Schedule (MicroSeconds (m_delay), &mvdashClient::Controller, this, ev);
              }
            else if (m_tIndexDownloaded >= m_tIndexLast) //Already download last segment
              { // *e_df
                m_ctrlTrace (this, m_state, cteAllDownloaded, m_tIndexLast);
                m_state = playing;
              }
            else //normal case, download new segment
              { // *e_d
                SendRequest (pReq, 5);
              }
            free (pReq);
          }
          break;
        case playbackFinished: //---------------------------------------
          m_ctrlTrace (this, m_state, cteEndPlayback, m_tIndexPlay - 1);
          if (!StartPlayback ())
            { // *e_pu
              m_state = downloading;
            }
          break;
        default:
          break;
        }
      return;
    }

  if (m_state == playing) //Playing the last segment
    {
      switch (event)
        {
          case irdFinished: /*  e_irc  */
          {
            m_state = downloadingPlaying;
            NS_LOG_INFO ("*** IRD finish" << Simulator::Now ().GetMicroSeconds ());
            st_mvdashRequest *pReq = PrepareRequest (m_tIndexReqSent + 1);
            SendRequest (pReq, 5);
            free (pReq);
          }
          break;
        case playbackFinished:
          m_ctrlTrace (this, m_state, cteEndPlayback, m_tIndexPlay - 1);
          if (m_tIndexPlay <= m_tIndexLast)
            {
              if (!StartPlayback ())
                { // Buffer Underrun // *e_pu
                  // m_state = downloading;
                  NS_LOG_INFO ("SOMETHING WRONG : S_P and Buffer Underrun");
                }
            }
          else
            { // *e_pf
              m_state = terminal;
              StopApplication ();
            }
          break;
        default:
          break;
        }
      return;
    }
}


// Application Methods
void
mvdashClient::StartApplication () // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);
  // Create the socket if not already
  //Initialize();
  if (!m_socket)
    {
      TypeId tid = TypeId::LookupByName ("ns3::TcpSocketFactory");
      m_socket = Socket::CreateSocket (GetNode (), tid);

      if (m_socket->Bind () == -1)
        {
          NS_FATAL_ERROR ("Failed to bind socket");
        }
      m_socket->Connect (m_serverAddress);
      m_socket->SetConnectCallback (MakeCallback (&mvdashClient::ConnectionSucceeded, this),
                                    MakeCallback (&mvdashClient::ConnectionFailed, this));
    }
}

void
mvdashClient::StopApplication () // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  LogDownload ();
  LogPlayback ();
  LogBuffer ();

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_socket->SetRecvCallback (MakeNullCallback<void, Ptr<Socket>> ());
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("mvdashClient found null socket to close in StopApplication");
    }
}

void
mvdashClient::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("mvdashClient Connection succeeded");
  m_connected = true;

  m_socket->SetRecvCallback (MakeCallback (&mvdashClient::HandleRead, this));

  controllerEvent ev = init;
  Controller (ev);
  //  Simulator::Schedule(MicroSeconds(0), &mvdashClient::Controller, this, ev);
}

void
mvdashClient::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("mvdashClient, Connection Failed");
}

void
mvdashClient::HandleRead (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Ptr<Packet> packet;

  while ((packet = socket->Recv ()))
    {
      int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
      if (packet->GetSize () == 0) // EOF
        break;

      st_mvdashRequest curSeg = m_requests.front ();

      if (m_recvRequestCounter < curSeg.id)
        {
          // the first of the request
          m_recvRequestCounter = curSeg.id;
          m_downData.time.at (curSeg.id).downloadStart = timeNow;
          m_reqTrace (this, reqev_startReceiving, m_recvRequestCounter);
        }

      if (!m_segStarted)
        {
          m_segTrace (this, segev_startReceiving, curSeg);
          m_segStarted = true;
        }
      m_rxTrace (this, packet);

      m_bytesReceived += packet->GetSize ();

      if (m_bytesReceived >= curSeg.segmentSize)
        {
          int success;
          success = m_pRequestModel->readRequest (&m_requests, &m_tIndexDownloaded,
                                                  m_recvRequestCounter, timeNow, curSeg);

          if (success)
            {
              // m_reqTrace (this, reqev_endReceiving, m_tIndexDownloaded);
              m_ctrlTrace (this, m_state, cteDownloaded, m_tIndexDownloaded);
              controllerEvent ev = downloadFinished;
              Controller (ev);
            }

          m_bytesReceived -= curSeg.segmentSize;
          m_segTrace (this, segev_endReceiving, curSeg);
          m_segStarted = false;
          m_requests.pop ();
        }
    }
} // namespace ns3

struct st_mvdashRequest *
mvdashClient::PrepareRequest (int tIndexReq)
{
  NS_LOG_FUNCTION (this);
  struct st_mvdashRequest *pReq;
  mvdashAlgorithmReply results;
  pReq = m_pRequestModel->prepareRequest (tIndexReq, m_sendRequestCounter, &results);
  m_delay = results.nextDownloadDelay;
  return pReq;
}

int
mvdashClient::SendRequest (struct st_mvdashRequest *pMsg, int nReq)
{
  NS_LOG_FUNCTION (this);
  int success;

  success =
      m_pRequestModel->SendRequest (pMsg, m_socket, m_connected, &m_requests, &m_tIndexReqSent);

  if (success)
    {
      // m_txTrace (this, packet);
      m_sendRequestCounter++;
      // m_reqTrace (this, reqev_reqMsgSent, m_sendRequestCounter);
      m_ctrlTrace (this, m_state, cteSendRequest, pMsg->timeIndex);
    }
  return success;
}

bool
mvdashClient::StartPlayback (void)
{
  std::string change = "";
  NS_LOG_FUNCTION (this);
  // if we got called and there are no segments left in the buffer, there is a buffer underrun
  indexDownload = g_bufferData[m_pViewModel->CurrentViewpoint ()].segmentID.back ();
  if (m_tIndexPlay > indexDownload)
    {
      m_ctrlTrace (this, m_state, cteBufferUnderrun, m_tIndexPlay);
      NS_LOG_INFO ("Buffer Underrun " << m_tIndexPlay << "  " << indexDownload);
      Req_buffering = true;
      return false;
    }
  else
    {
      if (m_tIndexPlay > 0)
        {
          m_pViewModel->UpdateViewpoint (m_tIndexPlay); //change viewpoint each playback

          // Single request activated
          int32_t cVP = m_pViewModel->CurrentViewpoint ();
          int32_t pVP = m_pViewModel->PrevViewpoint ();
          //if request is hybrid and change viewpoint,
          if (m_reqType == "hybrid" && cVP != pVP)
            {
              if (m_state == playing)
                //already download last segment
                {
                  m_state = downloadingPlaying;
                  controllerEvent ev = viewChange;
                  Controller (ev);
                }
            }

          if (cVP != pVP)
            {
              change = "VPChanged " + std::to_string (m_pViewModel->PrevViewpoint ()) + "->" +
                       std::to_string (m_pViewModel->CurrentViewpoint ());
            }
        }

      controllerEvent ev = playbackFinished;
      Simulator::Schedule (MicroSeconds (m_videoData[0].segmentDuration), &mvdashClient::Controller,
                           this, ev);

      m_playData.playbackIndex.push_back (m_tIndexPlay);
      m_playData.mainViewpoint.push_back (m_pViewModel->CurrentViewpoint ());
      m_playData.playbackStart.push_back (Simulator::Now ().GetMicroSeconds ());

      m_playData.qualityIndex.push_back (
          m_downSegment.qualityIndex[m_tIndexPlay]); //played next segment at the downloaded quality

      if (Req_buffering)
        {
          m_playData.buffering.push_back (1);
        }
      else
        {
          m_playData.buffering.push_back (0);
        }
      Req_buffering = false;

      NS_LOG_INFO (" ----------------------------------------- : "
                   << " Play Playback " << m_tIndexPlay << " Q:"
                   << m_playData.qualityIndex[m_tIndexPlay][m_pViewModel->CurrentViewpoint ()]
                   << " " << change);

      m_ctrlTrace (this, m_state, cteStartPlayback, m_tIndexPlay);
      m_tIndexPlay++;

      return true;
    }
}

void
mvdashClient::Initialize (void)
{
  NS_LOG_FUNCTION (this);

  ReadInBitrateValues (m_mvInfoFilePath);
  read_VMAF (m_mvVmafFilePath);

  // NS_LOG_ERROR ("Invalid view point switching Model name entered. Terminating" << m_nViewpoints);

  // initialize buffer data
  for (int i = 0; i < m_nViewpoints; i++)
    {
      g_bufferData.push_back (bufferData ());
    }

  // ===========================================================================================
  // Initialze View-Point Switching Model
  if (m_vpModelName == "markovian")
    {
      m_pViewModel = new Markovian_Viewpoint_Model (m_vpInfoFilePath);
      m_pViewModel->UpdateViewpoint (m_tIndexPlay);
    }
  else if (m_vpModelName == "free")
    {
      m_pViewModel = new Free_Viewpoint_Model ();
      m_pViewModel->m_nViews = m_nViewpoints;
      m_pViewModel->UpdateViewpoint (m_tIndexPlay);
    }
  else
    {
      NS_LOG_ERROR ("Invalid view point switching Model name entered. Terminating"
                    << m_vpModelName);
      StopApplication ();
      Simulator::Stop ();
    }

  // ===========================================================================================
  // Initialze Multi-View Adaptation Algorithm
  if (m_mvAlgoName == "maximize_current")
    {
      m_pAlgorithm =
          new maximizeCurrentAdaptation (m_videoData, m_playData, g_bufferData, m_downData);
    }
  else if (m_mvAlgoName == "adaptation_tobasco")
    {
      m_pAlgorithm = new adaptationTobasco (m_videoData, m_playData, g_bufferData, m_downData);
    }
  else if (m_mvAlgoName == "adaptation_panda") // DION
    {

      m_pAlgorithm = new adaptationPanda (m_videoData, m_playData, g_bufferData, m_downData);
    }
  else if (m_mvAlgoName == "adaptation_mpc") // DION
    {
      m_pAlgorithm = new adaptationMpc (m_videoData, m_playData, g_bufferData, m_downData);
    }
  else if (m_mvAlgoName == "adaptation_qmetric") // DION
    {
      m_pAlgorithm = new adaptationQmetric (m_videoData, m_playData, g_bufferData, m_downData);
    }
  else
    {
      NS_LOG_ERROR ("Invalid Adaptation Algorithm name entered. Terminating");
      StopApplication ();
      Simulator::Stop ();
    }

  // ================================================================ v===========================

  // Initialze Request Model
  if (m_reqType == "single")
    {
      m_pRequestModel = new requestSingle (m_videoData, m_downData, m_downSegment, m_playData,
                                           g_bufferData, m_pAlgorithm, m_pViewModel, m_reqType);
    }
  else if (m_reqType == "group")
    {
      m_pRequestModel = new requestGroupMv (m_videoData, m_downData, m_downSegment, m_playData,
                                            g_bufferData, m_pAlgorithm, m_pViewModel, m_reqType);
    }
  else if (m_reqType == "group_sg")
    {
      m_pRequestModel = new requestGroupSg (m_videoData, m_downData, m_downSegment, m_playData,
                                            g_bufferData, m_pAlgorithm, m_pViewModel, m_reqType);
    }
  else if (m_reqType == "hybrid")
    {
      m_pRequestModel = new requestHybrid (m_videoData, m_downData, m_downSegment, m_playData,
                                           g_bufferData, m_pAlgorithm, m_pViewModel, m_reqType);
    }
}

int
mvdashClient::ReadInBitrateValues (std::string segmentSizeFile)
{
  NS_LOG_FUNCTION (this);
  std::ifstream myfile;
  myfile.open (segmentSizeFile.c_str ());
  if (!myfile)
    {
      return -1;
    }

  // Local Variables for Iterations
  int vp, rindex; // viewpoints index, rate index
  int nRates, nSegments;
  int64_t averageByteSizeTemp = 0;

  std::string temp;
  std::getline (myfile, temp); // Get the first line
  std::istringstream buffer (temp);
  std::vector<int32_t> first_line ((std::istream_iterator<int32_t> (buffer)),
                                   std::istream_iterator<int32_t> ());

  m_nViewpoints = first_line[0];
  nSegments = first_line[1];
  m_tIndexLast = 0;
  for (vp = 0; vp < m_nViewpoints; vp++)
    {
      nRates = first_line[vp + 3];
      std::vector<std::vector<int64_t>> vals (nRates, std::vector<int64_t> (nSegments, 0));
      std::vector<std::vector<double>> vmaf (nRates, std::vector<double> (nSegments, 0));
      struct videoData v1 = {vals, std::vector<double> (nRates, 0.0), vmaf,
                             first_line[2]}; // firstline[2] --> Duration
      m_videoData.push_back (v1);
    }

  while (std::getline (myfile, temp) && m_tIndexLast < nSegments)
    {
      if (temp.empty ())
        break;
      std::istringstream buffer (temp);
      std::vector<int64_t> line ((std::istream_iterator<int64_t> (buffer)),
                                 std::istream_iterator<int64_t> ());
      int32_t i = 0;
      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          nRates = first_line[vp + 3];
          for (rindex = 0; rindex < nRates; rindex++)
            {
              m_videoData[vp].segmentSize[rindex][m_tIndexLast] = line[i++];
            }
        }
      m_tIndexLast++;
    }

  // Calculate Average Video Segment Size in Bytes and Video Rates in Kbps
  for (vp = 0; vp < m_nViewpoints; vp++)
    {
      nRates = first_line[vp + 3];
      for (rindex = 0; rindex < nRates; rindex++)
        {
          averageByteSizeTemp =
              (int64_t) std::accumulate (m_videoData[vp].segmentSize[rindex].begin (),
                                         m_videoData[vp].segmentSize[rindex].end (), 0.0) /
              nSegments;
          m_videoData[vp].averageBitrate[rindex] =
              8.0 * averageByteSizeTemp / m_videoData[vp].segmentDuration * 1000000;
        }
    }

  myfile.close ();
  return m_nViewpoints;
}

void
mvdashClient::read_VMAF (std::string segmentVmafFile)
{
  NS_LOG_FUNCTION (this);
  int nRates = m_videoData[0].averageBitrate.size ();
  //read
  std::ifstream myfile;
  myfile.open (segmentVmafFile.c_str ());
  if (!myfile)
    {
      StopApplication ();
      Simulator::Stop ();
    }

  std::string temp;
  int segId = 0;
  while (std::getline (myfile, temp))
    {
      std::istringstream buffer (temp);
      std::vector<double> line ((std::istream_iterator<double> (buffer)),
                                std::istream_iterator<double> ());

      for (int vp = 0; vp < m_nViewpoints; vp++)
        {
          int i = 0;
          for (int rindex = 0; rindex < nRates; rindex++)
            {
              m_videoData[vp].vmaf[rindex][segId] = line[i++];
            }
        }
      segId++;
    }

  myfile.close ();
  // NS_LOG_INFO (" " << m_videoData[1].vmaf[2][10]);
  // StopApplication ();
  // Simulator::Stop ();
}

void
mvdashClient::LogDownload (void)
{
  NS_LOG_FUNCTION (this);

  int vp, nReq = m_downData.id.size () - 1;
  std::string downloadLogFileName = "./contrib/etri_mvdash/downlog_sim" + std::to_string (m_simId) +
                                    "_cl" + std::to_string (m_clientId) + ".csv";
  std::ofstream downloadLog;
  //downloadLog.open("./contrib/etri_mvdash/downlog.csv");
  downloadLog.open (downloadLogFileName.c_str ());

  // CSV Columns
  // id, tIndex, tSent, tDownStart, tDownEnd, q_v0, q_v1, ...
  downloadLog << "id\tGroup?\tSegment\tmainView\tSent\tDownStart\tDownEnd";

  for (vp = 0; vp < m_nViewpoints; vp++)
    {
      downloadLog << "\tq_v" << vp;
    }
  downloadLog << "\n";

  for (int i = 0; i < nReq; i++)
    {
      std::string logStr = std::to_string (m_downData.id[i]);
      logStr.append ("\t" + std::to_string (m_downData.group[i]));
      logStr.append ("\t" + std::to_string (m_downData.playbackIndex[i][0]));
      logStr.append ("\t" + std::to_string (m_downData.viewpointPriority[i]));
      logStr.append ("\t" + std::to_string (m_downData.time[i].requestSent));
      logStr.append ("\t " + std::to_string (m_downData.time[i].downloadStart));
      logStr.append ("\t " + std::to_string (m_downData.time[i].downloadEnd));

      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          logStr.append ("\t  " + std::to_string (m_downData.qualityIndex[i][vp]));
        }

      downloadLog << logStr << "\n";
    }
  downloadLog.flush ();
  downloadLog.close ();
}

void
mvdashClient::LogPlayback (void)
{
  NS_LOG_FUNCTION (this);
  int vp, nPlay = m_playData.playbackIndex.size () - 1;
  std::string playbackLogFileName = "./contrib/etri_mvdash/playback_sim" +
                                    std::to_string (m_simId) + "_cl" + std::to_string (m_clientId) +
                                    ".csv";

  std::ofstream playbackLog;
  //playbackLog.open("./contrib/etri_mvdash/playback.csv");
  playbackLog.open (playbackLogFileName.c_str ());

  // CSV Columns
  // tIndex, playStart, q_v0, q_v1, ...
  playbackLog << "Seg\tvp\tStart\tBffer?";
  for (vp = 0; vp < m_nViewpoints; vp++)
    playbackLog << "\tqV" << vp;
  for (vp = 0; vp < m_nViewpoints; vp++)
    playbackLog << "\tvV" << vp;
  playbackLog << "\n";

  for (int i = 0; i < nPlay; i++)
    {
      std::string logStr = std::to_string (m_playData.playbackIndex[i]);
      logStr.append ("\t" + std::to_string (m_playData.mainViewpoint[i]));
      logStr.append ("\t" + std::to_string (m_playData.playbackStart[i]));
      logStr.append ("\t" + std::to_string (m_playData.buffering[i]) + "");
      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          std::string c;
          if (m_playData.qualityIndex[i][vp] == -1)
            c = " ";
          else
            c = std::to_string (m_playData.qualityIndex[i][vp]);

          logStr.append ("\t" + c);
        }
      //VMAF print
      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          std::string c;
          if (m_playData.qualityIndex[i][vp] == -1)
            c = " ";
          else
            c = std::to_string (
                (int32_t) (m_videoData[vp].vmaf[m_playData.qualityIndex[i][vp]][i]));

          logStr.append ("\t" + c);
        }
      playbackLog << logStr << "\n";
    }
  playbackLog.flush ();
  playbackLog.close ();
} // Namespace ns3

void
mvdashClient::LogBuffer (void)
{
  NS_LOG_FUNCTION (this);
  std::string bufferLogFileName = "./contrib/etri_mvdash/buffer_sim" + std::to_string (m_simId) +
                                  "_cl" + std::to_string (m_clientId) + ".csv";

  std::ofstream bufferLog;
  bufferLog.open (bufferLogFileName.c_str ());

  for (int vp = 0; vp < m_nViewpoints; vp++)
    bufferLog << "\tnow_" << vp + 1 << "\tbufferOld_" << vp + 1 << "\tbufferNew_" << vp + 1;
  bufferLog << "\n";

  int isEmpty[m_nViewpoints] = {}; //check if all viewpoint buffer is already writen
  int i = 0;
  bool ends = false;
  while (!ends)
    {
      std::string logStr;

      for (int v = 0; v < m_nViewpoints; v++)
        {
          int length = g_bufferData[v].timeNow.size () - 1;

          if (i < length)
            {
              logStr.append ("\t" + std::to_string (g_bufferData[v].timeNow[i]));
              logStr.append ("\t" + std::to_string (g_bufferData[v].bufferLevelOld[i]));
              logStr.append ("\t" + std::to_string (g_bufferData[v].bufferLevelNew[i]));
            }
          else
            {
              isEmpty[v] = 1;
            }
        }

      i++;
      bufferLog << logStr << "\n";

      int al = sizeof (isEmpty) / sizeof (isEmpty[0]); //length calculation
      int count = std::count (isEmpty, isEmpty + al, 1);

      if (count == m_nViewpoints)
        {
          ends = true;
        }
    }

  bufferLog.flush ();
  bufferLog.close ();
}

} // Namespace ns3
