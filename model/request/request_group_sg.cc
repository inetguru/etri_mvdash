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
#include "request_group_sg.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("requestGroupSg");

NS_OBJECT_ENSURE_REGISTERED (requestGroupSg);

requestGroupSg::requestGroupSg (const t_videoDataGroup &videoData, downloadData &downData,
                                downloadedSegment &downSegment, playbackDataGroup &playData,
                                bufferDataGroup &bufferData, mvdashAdaptationAlgorithm *pAlgorithm,
                                MultiView_Model *pViewModel, std::string reqType)
    : moduleRequest (videoData, downData, downSegment, playData, bufferData, pAlgorithm, pViewModel,
                     reqType)
{
  NS_LOG_FUNCTION (this);
  n_requests = 1;
  m_nViewpoint = videoData.size ();
}

struct st_mvdashRequest *
requestGroupSg::prepareRequest (int tIndexReq, int m_sendRequestCounter, mvdashAlgorithmReply *abr)
{

  NS_LOG_FUNCTION (this);
  struct st_mvdashRequest *pReq =
      (struct st_mvdashRequest *) malloc (n_requests * sizeof (st_mvdashRequest));

  std::vector<int32_t> qIndex (n_requests, -1);
  int32_t curVP = m_pViewModel->CurrentViewpoint ();

  *abr = m_pAlgorithm->SelectRateIndexes (tIndexReq, curVP, &qIndex, true, m_reqType);

  int n_index = tIndexReq;
  for (int p = 0; p < n_requests; p++)
    {
      abr->rate_group[p] = abr->rate_group[0];
      pReq[p].id = m_sendRequestCounter;
      pReq[p].group = 1;
      pReq[p].viewpoint = curVP;
      pReq[p].timeIndex = n_index;
      pReq[p].qualityIndex = abr->rate_group[p]; //download future at same Q
      pReq[p].segmentSize = m_videoData[curVP].segmentSize[abr->rate_group[p]][n_index];
      n_index++;
    }

  return pReq;
}

int
requestGroupSg::SendRequest (struct st_mvdashRequest *pMsg, Ptr<Socket> m_socket, bool m_connected,
                             std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexReqSent)
{
  NS_LOG_FUNCTION (this);
  if (m_connected)
    {

      Ptr<Packet> packet =
          Create<Packet> ((uint8_t const *) pMsg, n_requests * sizeof (st_mvdashRequest));

      int actual = m_socket->Send (packet);

      if (actual == (int) packet->GetSize ())
        {

          // m_txTrace (this, packet); /////////////////////////////////////
          std::vector<int32_t> qIndexes (m_nViewpoint, -1);
          std::vector<int32_t> pIndex (n_requests, -1);

          for (int i = 0; i < n_requests; i++)
            {
              m_requests->push (pMsg[i]);
              qIndexes[i] = pMsg[i].qualityIndex;
              pIndex[i] = pMsg[i].timeIndex;
            }

          if (*m_tIndexReqSent < pMsg[n_requests - 1].timeIndex)
            *m_tIndexReqSent = pMsg[n_requests - 1].timeIndex;

          //LOG Download
          m_downData.id.push_back (pMsg[n_requests - 1].id);
          m_downData.playbackIndex.push_back (pIndex);
          struct st_requestTimeInfo tinfo = {Simulator::Now ().GetMicroSeconds (), 0, 0};
          m_downData.time.push_back (tinfo);
          m_downData.qualityIndex.push_back (qIndexes);
          m_downData.group.push_back (1); //set as group request
          m_downData.viewpointPriority.push_back (m_pViewModel->CurrentViewpoint ());

          //Playback
          for (int i = 0; i < n_requests; i++)
            {
              std::vector<int32_t> playQuality (m_nViewpoint, -1);
              playQuality[pMsg[i].viewpoint] = pMsg[i].qualityIndex;
              m_downSegment.id.push_back (pMsg[i].timeIndex);
              m_downSegment.redownload.push_back (0);
              m_downSegment.qualityIndex.push_back (playQuality);
            }

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
requestGroupSg::readRequest (std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexDownloaded,
                             int32_t m_recvRequestCounter, int64_t timeNow, st_mvdashRequest curSeg)
{
  int success = 0;
  readSegment_temp.push (m_requests->front ()); //temporary

  bool bGroupReceived = false;
  //   if (m_requests->empty ())
  if (m_requests->size () == 1)
    bGroupReceived = true;
  else if (m_requests->front ().id > m_recvRequestCounter)
    bGroupReceived = true;

  // Update time as All segment are received at the same time (due to group req)
  if (bGroupReceived)
    {
      st_mvdashRequest currSeg = readSegment_temp.front ();
      m_downData.time.at (currSeg.id).downloadEnd = timeNow; //update receive time
      for (int vp = 0; vp < n_requests; vp++)
        {
          currSeg = readSegment_temp.front ();
          int32_t currVP = currSeg.viewpoint;
          if (*m_tIndexDownloaded <= currSeg.timeIndex)
            *m_tIndexDownloaded = currSeg.timeIndex;

          for (int mv = 0; mv < m_nViewpoint; mv++)
            {
              if (currVP != mv)
                {
                  g_bufferData[mv].timeNow.push_back (timeNow);
                  g_bufferData[mv].segmentID.push_back (currSeg.timeIndex);
                  g_bufferData[mv].bufferLevelOld.push_back (0);
                  g_bufferData[mv].bufferLevelNew.push_back (0);
                }
            }

          // NS_LOG_INFO ("++++++++ " << bGroupReceived << " " << currSeg.timeIndex << " "
          //                          << currSeg.id);

          g_bufferData[currVP].timeNow.push_back (timeNow);
          g_bufferData[currVP].segmentID.push_back (currSeg.timeIndex);

          if (currSeg.id == 0)
            {
              g_bufferData[currVP].bufferLevelOld.push_back (0);
            }
          else
            {
              int64_t currBuffer = g_bufferData[currVP].bufferLevelNew.back () -
                                   (timeNow - m_downData.time.at (currSeg.id - 1).downloadEnd);
              g_bufferData[currVP].bufferLevelOld.push_back (std::max (currBuffer, (int64_t) 0));
            }
          g_bufferData[currVP].bufferLevelNew.push_back (
              g_bufferData[currVP].bufferLevelOld.back () + m_videoData[0].segmentDuration);
          readSegment_temp.pop ();
        }
      success = 1;
    }
  return success;
}

} // namespace ns3
