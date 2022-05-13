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
#include "maximize_current_adaptation.h"
#include "adaptation_test.h" // dion
#include <numeric>

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
  m_tIndexLast = 10;
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

  // NS_LOG_INFO("Controller S: " << m_state << " E:" << event << " at " << Simulator::Now ().GetSeconds ());
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
          if (m_tIndexDownloaded >= m_tIndexLast)
            { // *e_df
              m_ctrlTrace (this, m_state, cteAllDownloaded, m_tIndexLast);
              m_state = playing;
            }
          else
            { // *e_d
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
        case downloadFinished: //Last index
          //Test ---------------------------
          if (!Req_Type || !Req_pending)
            {
              if (m_tIndexDownloaded >= m_tIndexLast)
                { // *e_df
                  m_ctrlTrace (this, m_state, cteAllDownloaded, m_tIndexLast);
                  m_state = playing;
                }
              else
                { // *e_d
                  st_mvdashRequest *pReq = PrepareRequest (m_tIndexReqSent + 1);
                  SendRequest (pReq, 5);
                  free (pReq);
                }
            }

          // DION Test ----------------------------------------
          else
            {
              if (m_tIndexDownloaded >= m_tIndexLast)
                { // *e_df
                  m_ctrlTrace (this, m_state, cteAllDownloaded, m_tIndexLast);
                  m_state = playing;
                }
              else
                {
                  { // *e_d
                    st_mvdashRequest *pReq = Hybrid_PrepareRequest (Req_m_tIndexReqSent + 1);
                    Hybrid_SendRequest (pReq);
                    free (pReq);
                  }
                }
            }
          // Test ----------------------------------------
          break;
        case playbackFinished:
          m_ctrlTrace (this, m_state, cteEndPlayback, m_tIndexPlay - 1);
          if (!StartPlayback ())
            { // Buffer Underrun // *e_pu
              m_state = downloading;
            }
          break;
        default:
          break;
        }
      return;
    }

  if (m_state == playing)
    {
      switch (event)
        {
        case playbackFinished:
          m_ctrlTrace (this, m_state, cteEndPlayback, m_tIndexPlay - 1);
          if (m_tIndexPlay <= m_tIndexLast)
            {
              if (!StartPlayback ())
                { // Buffer Underrun // *e_pu
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
          m_bytesReceived -= curSeg.segmentSize;
          m_segTrace (this, segev_endReceiving, curSeg);
          m_segStarted = false;
          m_requests.pop ();

          bool bGroupReceived = false;
          if (m_requests.empty ())
            bGroupReceived = true;
          else if (m_requests.front ().id > m_recvRequestCounter)
            bGroupReceived = true;

          if (bGroupReceived)
            {

              if (curSeg.group == 1) //Test
                {
                  if (m_tIndexDownloaded <= curSeg.timeIndex)
                    m_tIndexDownloaded = curSeg.timeIndex;

                  m_downData.time.at (curSeg.id).downloadEnd = timeNow;

                  m_bufferData.timeNow.push_back (timeNow);
                  if (curSeg.id > 0)
                    {
                      m_bufferData.bufferLevelOld.push_back (
                          std::max (m_bufferData.bufferLevelNew.back () -
                                        (timeNow - m_downData.time.at (curSeg.id - 1).downloadEnd),
                                    (int64_t) 0));
                    }
                  else
                    { // first segment
                      m_bufferData.bufferLevelOld.push_back (0);
                    }
                  m_bufferData.bufferLevelNew.push_back (m_bufferData.bufferLevelOld.back () +
                                                         m_videoData[0].segmentDuration);

                  m_reqTrace (this, reqev_endReceiving, m_tIndexDownloaded);
                  m_ctrlTrace (this, m_state, cteDownloaded, m_tIndexDownloaded);
                  controllerEvent ev = downloadFinished;
                  Controller (ev);
                }
              else
                {
                  // Test ----------------------------------------

                  if (Req_m_tIndexDownloaded <= curSeg.timeIndex)
                    Req_m_tIndexDownloaded = curSeg.timeIndex;

                  m_downData.time.at (curSeg.id).downloadEnd = timeNow;

                  if (m_tIndexReqSent == curSeg.timeIndex)
                    {
                      int64_t diff =
                          std::max (m_bufferData.bufferLevelNew.back () -
                                        (timeNow - m_downData.time.at (curSeg.id - 1).downloadEnd),
                                    (int64_t) 0) -
                          m_bufferData.bufferLevelOld.back ();

                      //Test update buffer --> need further investigation
                      //Update buffer level due to time to download single request
                      m_bufferData.bufferLevelOld[m_bufferData.bufferLevelOld.size () - 1] =
                          m_bufferData.bufferLevelOld.back () - diff;
                      m_bufferData.bufferLevelNew[m_bufferData.bufferLevelNew.size () - 1] =
                          m_bufferData.bufferLevelNew.back () - diff;
                    }

                  //Playback --> set new quality
                  m_downSegment.redownload[curSeg.timeIndex] = 1;
                  m_downSegment.qualityIndex[curSeg.timeIndex][curSeg.viewpoint] =
                      curSeg.qualityIndex;

                  m_reqTrace (this, reqev_endReceiving, curSeg.timeIndex);
                  m_ctrlTrace (this, m_state, cteDownloaded, curSeg.timeIndex);
                  controllerEvent ev = downloadFinished;
                  Controller (ev);
                }
              // DION Test ----------------------------------------
            }
        }
    }
}

void
mvdashClient::SelectRateIndexes (int tIndexReq, std::vector<int32_t> *pIndexes)
{
  (*pIndexes)[0] = 1;
  (*pIndexes)[1] = 1;

  //  qIndexes = (std::vector <int32_t>) {1, 0, 0, 0, 0};
}

struct st_mvdashRequest *
mvdashClient::PrepareRequest (int tIndexReq)
{
  NS_LOG_FUNCTION (this);
  struct st_mvdashRequest *pReq =
      (struct st_mvdashRequest *) malloc (m_nViewpoints * sizeof (st_mvdashRequest));

  //std::vector <int32_t> qIndex = {1, 0, 0, 0, 0};
  std::vector<int32_t> qIndex (m_nViewpoints, 0);
  //SelectRateIndexes(tIndexReq, &qIndex);

  m_pAlgorithm->SelectRateIndexes (tIndexReq, m_pViewModel->CurrentViewpoint (), &qIndex, 1);

  for (int vp = 0; vp < m_nViewpoints; vp++)
    {
      pReq[vp].id = m_sendRequestCounter;
      pReq[vp].group = 1;
      pReq[vp].viewpoint = vp;
      pReq[vp].timeIndex = tIndexReq;
      pReq[vp].qualityIndex = qIndex[vp];
      pReq[vp].segmentSize = m_videoData[vp].segmentSize[qIndex[vp]][tIndexReq];
      // NS_LOG_INFO ("************** Ask : " << vp << "--" << qIndex[vp]);
    }

  return pReq;
}

int
mvdashClient::SendRequest (struct st_mvdashRequest *pMsg, int nReq)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    {
      Ptr<Packet> packet =
          Create<Packet> ((uint8_t const *) pMsg, nReq * sizeof (st_mvdashRequest));

      int actual = m_socket->Send (packet);

      if (actual == (int) packet->GetSize ())
        {
          m_txTrace (this, packet);
          std::vector<int32_t> qIndexes (m_nViewpoints, -1);
          for (int i = 0; i < nReq; i++)
            {
              m_requests.push (pMsg[i]);
              qIndexes[pMsg[i].viewpoint] = pMsg[i].qualityIndex;
            }
          if (m_tIndexReqSent < pMsg[0].timeIndex)
            m_tIndexReqSent = pMsg[0].timeIndex;

          //LOG Download
          m_downData.id.push_back (pMsg[0].id);
          m_downData.playbackIndex.push_back (pMsg[0].timeIndex);
          struct st_requestTimeInfo tinfo = {Simulator::Now ().GetMicroSeconds (), 0, 0};
          m_downData.time.push_back (tinfo);
          m_downData.qualityIndex.push_back (qIndexes);
          m_downData.group.push_back (1); //set as group request

          //Playback
          m_downSegment.id.push_back (pMsg[0].timeIndex);
          m_downSegment.redownload.push_back (0);
          m_downSegment.qualityIndex.push_back (qIndexes);

          m_reqTrace (this, reqev_reqMsgSent, m_sendRequestCounter++);
          m_ctrlTrace (this, m_state, cteSendRequest, m_tIndexReqSent);

      
          return 1;
        }
      else
        {
          NS_LOG_DEBUG ("  mvdashClient Unable to send a request packet" << actual);
        }
    }
  return 0;
}

struct st_mvdashRequest *
mvdashClient::Hybrid_PrepareRequest (int tIndexReq)
{
  NS_LOG_FUNCTION (this);
  int32_t vp = m_pViewModel->CurrentViewpoint ();

  struct st_mvdashRequest *pReq = new st_mvdashRequest;

  std::vector<int32_t> qIndex (m_nViewpoints, 0);
  m_pAlgorithm->SelectRateIndexes (tIndexReq, m_pViewModel->CurrentViewpoint (), &qIndex, 0);

  // qIndex[vp] = 2; //DION TEST - Static index
  pReq->id = m_sendRequestCounter;
  pReq->group = 0;
  pReq->viewpoint = vp;
  pReq->timeIndex = tIndexReq;
  pReq->qualityIndex = qIndex[vp];
  pReq->segmentSize = m_videoData[vp].segmentSize[qIndex[vp]][tIndexReq];

  if (tIndexReq == m_tIndexDownloaded)
    {
      Req_pending = false;
    }
  return pReq;
}


int
mvdashClient::Hybrid_SendRequest (struct st_mvdashRequest *pMsg)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    {
      Ptr<Packet> packet = Create<Packet> ((uint8_t const *) pMsg, sizeof (st_mvdashRequest));

      NS_LOG_INFO ("************** Single Request -->SG :" << pMsg->timeIndex
                                                << " Vp :" << m_pViewModel->CurrentViewpoint ()
                                                << " Qt :" << pMsg->qualityIndex);

      int actual = m_socket->Send (packet);

      if (actual == (int) packet->GetSize ())
        {
          m_txTrace (this, packet);
          std::vector<int32_t> qIndexes (m_nViewpoints, 0);

          m_requests.push (*pMsg);
          qIndexes[pMsg->viewpoint] = pMsg->qualityIndex;

          if (Req_m_tIndexReqSent < pMsg->timeIndex)
            {
              Req_m_tIndexReqSent = pMsg->timeIndex;
            }

          m_downData.id.push_back (pMsg->id);
          m_downData.playbackIndex.push_back (pMsg->timeIndex);
          struct st_requestTimeInfo tinfo = {Simulator::Now ().GetMicroSeconds (), 0, 0};
          m_downData.time.push_back (tinfo);
          m_downData.group.push_back (0); //set as single request
          m_downData.qualityIndex.push_back (qIndexes);

          m_reqTrace (this, reqev_reqMsgSent, m_sendRequestCounter++);
          m_ctrlTrace (this, m_state, cteSendRequest, Req_m_tIndexReqSent);

          return 1;
        }
      else
        {
          NS_LOG_DEBUG ("  mvdashClient Unable to send a request packet" << actual);
        }
    }
  return 0;
}

bool
mvdashClient::StartPlayback (void)
{
  NS_LOG_FUNCTION (this);
  // int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
  // if we got called and there are no segments left in the buffer, there is a buffer underrun
  if (m_tIndexPlay > m_tIndexDownloaded)
    {
      //m_bufferUnderrun = true;
      m_ctrlTrace (this, m_state, cteBufferUnderrun, m_tIndexPlay);
      NS_LOG_INFO ("Buffer Underrun" << m_tIndexPlay << "  " << m_tIndexDownloaded);
      Req_buffering = true;
      return false;
    }
  else
    {
      if (m_tIndexPlay > 0)
        {
          m_pViewModel->UpdateViewpoint (m_tIndexPlay); //change viewpoint each playback
          // Test ----------------------------------------
          // Single request activated
          int32_t cVP = m_pViewModel->CurrentViewpoint ();
          int32_t pVP = m_pViewModel->PrevViewpoint ();
          if (Req_Type && cVP != pVP)
            {
              Req_pending = true;
              Req_m_tIndexReqSent = m_tIndexPlay;
            }
          // DION Test ----------------------------------------
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
                   << m_playData.qualityIndex[m_tIndexPlay][m_pViewModel->CurrentViewpoint ()]);

      m_ctrlTrace (this, m_state, cteStartPlayback, m_tIndexPlay);
      m_tIndexPlay++;
      return true;
    }
}

void
mvdashClient::Initialize (void)
{
  NS_LOG_FUNCTION (this);

  //ReadInBitrateValues("./contrib/etri_mvdash/multiviewvideo.csv");
  ReadInBitrateValues (m_mvInfoFilePath);

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
      NS_LOG_ERROR ("Invalid view point switching Model name entered. Terminating");
      StopApplication ();
      Simulator::Stop ();
    }

  // ===========================================================================================
  // Initialze Multi-View Adaptation Algorithm
  if (m_mvAlgoName == "maximize_current")
    {
      m_pAlgorithm =
          new maximizeCurrentAdaptation (m_videoData, m_playData, m_bufferData, m_downData);
    }
  else if (m_mvAlgoName == "newone")
    {
      m_pAlgorithm =
          new maximizeCurrentAdaptation (m_videoData, m_playData, m_bufferData, m_downData);
    }
  else if (m_mvAlgoName == "adaptation_test") // DION
    {
      m_pAlgorithm = new adaptationTest (m_videoData, m_playData, m_bufferData, m_downData);
    }
  else
    {
      NS_LOG_ERROR ("Invalid Adaptation Algorithm name entered. Terminating");
      StopApplication ();
      Simulator::Stop ();
    }

  // ===========================================================================================
  // Initialze Request Model
  if (m_reqType == "hybrid")
    {
      Req_Type = true;
    }
  else if (m_reqType == "group")
    {
      Req_Type = false;
    }
  else
    {
      NS_LOG_ERROR ("Invalid view point switching Model name entered. Terminating");
      StopApplication ();
      Simulator::Stop ();
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
      struct videoData v1 = {vals, std::vector<double> (nRates, 0.0),
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
  downloadLog << "id\tGroup?\tIndex\tSent\tDownStart\tDownEnd";

  for (vp = 0; vp < m_nViewpoints; vp++)
    {
      downloadLog << "\tq_v" << vp + 1;
    }
  downloadLog << "\n";

  for (int i = 0; i < nReq; i++)
    {
      std::string logStr = std::to_string (m_downData.id[i]);
      logStr.append ("\t" + std::to_string (m_downData.group[i]));
      logStr.append ("\t" + std::to_string (m_downData.playbackIndex[i]));
      logStr.append ("\t" + std::to_string (m_downData.time[i].requestSent));
      logStr.append ("\t " + std::to_string (m_downData.time[i].downloadStart));
      logStr.append ("\t " + std::to_string (m_downData.time[i].downloadEnd));
      // if (m_downData.group[i] == 1)
      //   {
      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          logStr.append ("\t  " + std::to_string (m_downData.qualityIndex[i][vp]));
          // std::to_string (m_downData.qualityIndex[m_downData.playbackIndex[i]][vp]));
        }
      // NS_LOG_INFO(logStr);
      // }

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
  playbackLog << "tIndex\tvp\tStart\tBuffering?";
  for (vp = 0; vp < m_nViewpoints; vp++)
    playbackLog << "\t  q_v" << vp + 1;
  playbackLog << "\n";

  for (int i = 0; i < nPlay; i++)
    {

      std::string logStr = std::to_string (m_playData.playbackIndex[i]);
      logStr.append ("\t" + std::to_string (m_playData.mainViewpoint[i]));
      logStr.append ("\t" + std::to_string (m_playData.playbackStart[i]));
      logStr.append ("\t" + std::to_string (m_playData.buffering[i]) + "  ");
      for (vp = 0; vp < m_nViewpoints; vp++)
        logStr.append ("\t  " + std::to_string (m_playData.qualityIndex[i][vp]));
      // NS_LOG_INFO(logStr);
      playbackLog << logStr << "\n";
    }
  playbackLog.flush ();
  playbackLog.close ();
} // Namespace ns3

void
mvdashClient::LogBuffer (void)
{
  NS_LOG_FUNCTION (this);
  int nBuffer = m_bufferData.timeNow.size () - 1;
  std::string bufferLogFileName = "./contrib/etri_mvdash/buffer_sim" + std::to_string (m_simId) +
                                  "_cl" + std::to_string (m_clientId) + ".csv";

  std::ofstream bufferLog;
  //bufferLog.open("./contrib/etri_mvdash/buffer.csv");
  bufferLog.open (bufferLogFileName.c_str ());

  // CSV Columns
  // now, bufferOld, bufferNew
  bufferLog << "now\tbufferOld\tbufferNew\n";

  for (int i = 0; i < nBuffer; i++)
    {
      std::string logStr = std::to_string (m_bufferData.timeNow[i]);
      logStr.append ("\t" + std::to_string (m_bufferData.bufferLevelOld[i]));
      logStr.append ("\t" + std::to_string (m_bufferData.bufferLevelNew[i]));
      // NS_LOG_INFO(logStr);
      bufferLog << logStr << "\n";
    }
  bufferLog.flush ();
  bufferLog.close ();
}
} // Namespace ns3
