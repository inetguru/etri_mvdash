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
#include "request_group_mv.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("requestGroupMv");

NS_OBJECT_ENSURE_REGISTERED (requestGroupMv);

requestGroupMv::requestGroupMv (const t_videoDataGroup &videoData, downloadData &downData,
                                downloadedSegment &downSegment, playbackDataGroup &playData,
                                bufferDataGroup &bufferData, mvdashAdaptationAlgorithm *pAlgorithm,
                                MultiView_Model *pViewModel, std::string reqType)
    : moduleRequest (videoData, downData, downSegment, playData, bufferData, pAlgorithm, pViewModel,
                     reqType)
{
  NS_LOG_FUNCTION (this);
  m_nViewpoints = videoData.size ();
}

struct st_mvdashRequest *
requestGroupMv::prepareRequest (int tIndexReq, int m_sendRequestCounter, mvdashAlgorithmReply *abr)
{
  NS_LOG_FUNCTION (this);
  struct st_mvdashRequest *pReq =
      (struct st_mvdashRequest *) malloc (m_nViewpoints * sizeof (st_mvdashRequest));

  std::vector<int32_t> qIndex (m_nViewpoints, 0);
  int32_t curVP = m_pViewModel->CurrentViewpoint ();

  *abr = m_pAlgorithm->SelectRateIndexes (tIndexReq, curVP, &qIndex, true, m_reqType);

  for (int vp = 0; vp < m_nViewpoints; vp++)
    {
      pReq[vp].id = m_sendRequestCounter;
      pReq[vp].group = 1;
      pReq[vp].viewpoint = vp;
      pReq[vp].timeIndex = tIndexReq;
      pReq[vp].qualityIndex = abr->rate_group[vp];
      pReq[vp].segmentSize = m_videoData[vp].segmentSize[abr->rate_group[vp]][tIndexReq];
    }

  return pReq;
}

int
requestGroupMv::SendRequest (struct st_mvdashRequest *pMsg, Ptr<Socket> m_socket, bool m_connected,
                             std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexReqSent)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    {

      Ptr<Packet> packet =
          Create<Packet> ((uint8_t const *) pMsg, m_nViewpoints * sizeof (st_mvdashRequest));

      int actual = m_socket->Send (packet);

      if (actual == (int) packet->GetSize ())
        {

          // m_txTrace (this, packet); /////////////////////////////////////
          std::vector<int32_t> qIndexes (m_nViewpoints, 0);
          for (int i = 0; i < m_nViewpoints; i++)
            {
              m_requests->push (pMsg[i]);
              qIndexes[pMsg[i].viewpoint] = pMsg[i].qualityIndex;
            }

          if (*m_tIndexReqSent < pMsg[0].timeIndex)
            *m_tIndexReqSent = pMsg[0].timeIndex;

          //LOG Download
          m_downData.id.push_back (pMsg[0].id);
          struct st_requestTimeInfo tinfo = {Simulator::Now ().GetMicroSeconds (), 0, 0};
          m_downData.time.push_back (tinfo);
          m_downData.qualityIndex.push_back (qIndexes);
          m_downData.group.push_back (1); //set as group request
          m_downData.viewpointPriority.push_back (m_pViewModel->CurrentViewpoint ());
          std::vector<int32_t> pIndex (m_nViewpoints, pMsg->timeIndex);
          m_downData.playbackIndex.push_back (pIndex);

          //Playback
          m_downSegment.id.push_back (pMsg[0].timeIndex);
          m_downSegment.redownload.push_back (0);
          m_downSegment.qualityIndex.push_back (qIndexes);

          // m_reqTrace (this, reqev_reqMsgSent, m_sendRequestCounter++); //////////////
          // m_ctrlTrace (this, m_state, cteSendRequest, m_tIndexReqSent); ////////////////
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
requestGroupMv::readRequest (std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexDownloaded,
                             int32_t m_recvRequestCounter, int64_t timeNow, st_mvdashRequest curSeg)
{
  bool bGroupReceived = false;

  // if (m_requests->empty ())
  if (m_requests->size () == 1)
    bGroupReceived = true;
  else if (m_requests->front ().id > m_recvRequestCounter)
    bGroupReceived = true;

  //All segment are received at the same time (due to group req)
  if (bGroupReceived)
    {

      if (*m_tIndexDownloaded <= curSeg.timeIndex)
        *m_tIndexDownloaded = curSeg.timeIndex;

      m_downData.time.at (curSeg.id).downloadEnd = timeNow;

      for (int vp = 0; vp < m_nViewpoints; vp++)
        {

          g_bufferData[vp].timeNow.push_back (timeNow);
          g_bufferData[vp].segmentID.push_back (curSeg.timeIndex);

          if (curSeg.id == 0)
            {
              g_bufferData[vp].bufferLevelOld.push_back (0);
            }
          else
            {
              int64_t currBuffer = g_bufferData[vp].bufferLevelNew.back () -
                                   (timeNow - m_downData.time.at (curSeg.id - 1).downloadEnd);
              g_bufferData[vp].bufferLevelOld.push_back (std::max (currBuffer, (int64_t) 0));
            }
          g_bufferData[vp].bufferLevelNew.push_back (g_bufferData[vp].bufferLevelOld.back () +
                                                     m_videoData[0].segmentDuration);
        }
      return 1;
    }
  return 0;
}

} // namespace ns3
