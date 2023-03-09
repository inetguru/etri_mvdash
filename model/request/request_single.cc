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
#include "request_single.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("requestSingle");

NS_OBJECT_ENSURE_REGISTERED (requestSingle);

requestSingle::requestSingle (const t_videoDataGroup &videoData, downloadData &downData,
                              downloadedSegment &downSegment, playbackDataGroup &playData,
                              bufferDataGroup &bufferData, mvdashAdaptationAlgorithm *pAlgorithm,
                              MultiView_Model *pViewModel, std::string reqType)
    : moduleRequest (videoData, downData, downSegment, playData, bufferData, pAlgorithm, pViewModel, reqType)
{
  NS_LOG_FUNCTION (this);
  m_nViewpoints = videoData.size ();
}

struct st_mvdashRequest *
requestSingle::prepareRequest (int tIndexReq, int m_sendRequestCounter, mvdashAlgorithmReply *abr)
{
  NS_LOG_FUNCTION (this);
  struct st_mvdashRequest *pReq = new st_mvdashRequest;

  int32_t vp = m_pViewModel->CurrentViewpoint ();
  // int32_t vp_prev = m_pViewModel->PrevViewpoint ();
  // int32_t playCurr = m_pViewModel->PrevViewpoint ();
  // if (vp != vp_prev)
  //   {
  //     NS_LOG_INFO ("------------------------" << playCurr << " "
  //                                             << m_playData.playbackIndex.back ());
  //     tIndexReq = m_playData.playbackIndex.back ();
  //   }

  std::vector<int32_t> qIndex (m_nViewpoints, -1); //other set to -1

  *abr = m_pAlgorithm->SelectRateIndexes (tIndexReq, vp, &qIndex, false, m_reqType);
  pReq->id = m_sendRequestCounter;
  pReq->group = 0;
  pReq->viewpoint = vp;
  pReq->timeIndex = tIndexReq;
  pReq->qualityIndex = qIndex[vp];
  pReq->segmentSize = m_videoData[vp].segmentSize[qIndex[vp]][tIndexReq];

  return pReq;
}

int
requestSingle::SendRequest (struct st_mvdashRequest *pMsg, Ptr<Socket> m_socket, bool m_connected,
                            std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexReqSent)
{
  NS_LOG_FUNCTION (this);
  if (m_connected)
    {
      Ptr<Packet> packet = Create<Packet> ((uint8_t const *) pMsg, sizeof (st_mvdashRequest));

      int actual = m_socket->Send (packet);

      if (actual == (int) packet->GetSize ())
        {
          // m_txTrace (this, packet);////////////////////////////////////
          std::vector<int32_t> qIndexes (m_nViewpoints, -1);
          m_requests->push (*pMsg);
          qIndexes[pMsg->viewpoint] = pMsg->qualityIndex;

          if (*m_tIndexReqSent < pMsg->timeIndex)
            {
              *m_tIndexReqSent = pMsg->timeIndex;
            }

          m_downData.id.push_back (pMsg->id);
          std::vector<int32_t> pIndex (1, pMsg->timeIndex);
          m_downData.playbackIndex.push_back (pIndex);
          struct st_requestTimeInfo tinfo = {Simulator::Now ().GetMicroSeconds (), 0, 0};
          m_downData.time.push_back (tinfo);
          m_downData.group.push_back (0); //set as single request
          m_downData.qualityIndex.push_back (qIndexes);
          m_downData.viewpointPriority.push_back (m_pViewModel->CurrentViewpoint ());

          //Playback
          m_downSegment.id.push_back (pMsg[0].timeIndex);
          m_downSegment.redownload.push_back (0);
          m_downSegment.qualityIndex.push_back (qIndexes);

          return 1;
        }
      else
        {
          NS_LOG_DEBUG ("  mvdashClient Unable to send a request packet" << actual);
        }
    }
  return 0;
}

int
requestSingle::readRequest (std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexDownloaded,
                            int32_t m_recvRequestCounter, int64_t timeNow, st_mvdashRequest curSeg)
{
  if (*m_tIndexDownloaded <= curSeg.timeIndex)
    *m_tIndexDownloaded = curSeg.timeIndex;

  m_downData.time.at (curSeg.id).downloadEnd = timeNow;

  if (*m_tIndexDownloaded <= curSeg.timeIndex)
    {
      for (int vp = 0; vp < m_nViewpoints; vp++)
        {
          g_bufferData[vp].timeNow.push_back (timeNow);
          g_bufferData[vp].segmentID.push_back (curSeg.timeIndex);

          int64_t bufferCurrent = 0;
          int64_t bufferNew = 0;
          if (curSeg.id == 0)
            {
              g_bufferData[vp].bufferLevelOld.push_back (bufferCurrent);
            }
          else
            {
              bufferCurrent =
                  std::max (g_bufferData[vp].bufferLevelNew.back () -
                                (timeNow - m_downData.time.at (curSeg.id - 1).downloadEnd),
                            (int64_t) 0);
              if (vp == curSeg.viewpoint)
                {
                  bufferNew = bufferCurrent + m_videoData[0].segmentDuration; // only for viewpoint
                }
            }

          g_bufferData[vp].bufferLevelOld.push_back (bufferCurrent);
          g_bufferData[vp].bufferLevelNew.push_back (bufferNew);
        }
      return 1;
    }
  return 0;
}

} // namespace ns3
