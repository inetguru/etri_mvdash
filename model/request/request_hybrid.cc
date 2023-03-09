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
#include "request_hybrid.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("requestHybrid");

NS_OBJECT_ENSURE_REGISTERED (requestHybrid);

requestHybrid::requestHybrid (const t_videoDataGroup &videoData, downloadData &downData,
                              downloadedSegment &downSegment, playbackDataGroup &playData,
                              bufferDataGroup &bufferData, mvdashAdaptationAlgorithm *pAlgorithm,
                              MultiView_Model *pViewModel, std::string reqType)
    : moduleRequest (videoData, downData, downSegment, playData, bufferData, pAlgorithm, pViewModel,
                     reqType)
{
  NS_LOG_FUNCTION (this);
  segment_singleCounter = 0;
  segment_lastGroup = 0;
  pending = 0;
  m_nViewpoints = videoData.size ();
  groupMv = new requestGroupMv (m_videoData, m_downData, m_downSegment, m_playData, g_bufferData,
                                m_pAlgorithm, m_pViewModel, reqType);
  single = new requestSingle (m_videoData, m_downData, m_downSegment, m_playData, g_bufferData,
                              m_pAlgorithm, m_pViewModel, reqType);
}

struct st_mvdashRequest *
requestHybrid::prepareRequest (int tIndexReq, int m_sendRequestCounter, mvdashAlgorithmReply *abr)
{
  NS_LOG_FUNCTION (this);
  struct st_mvdashRequest *pReq =
      (struct st_mvdashRequest *) malloc (m_nViewpoints * sizeof (st_mvdashRequest));

  int32_t vp = m_pViewModel->CurrentViewpoint ();
  int32_t vp_lastDownload = 0;
  if (tIndexReq != 0)
    vp_lastDownload = m_downData.viewpointPriority.back ();

  //start pending
  if (vp != vp_lastDownload)
    {
      pending = 1;
      segment_lastGroup = m_downData.playbackIndex.back ()[0];
      segment_singleCounter = m_playData.playbackIndex.back () + 1;
    }

  //stop pending
  if (pending == 1 && segment_singleCounter > segment_lastGroup)
    pending = 0;

  if (pending)
    {
      //download from p+1
      pReq = single->prepareRequest (segment_singleCounter, m_sendRequestCounter, abr);
    }
  else
    {
      pReq = groupMv->prepareRequest (tIndexReq, m_sendRequestCounter, abr);
    }

  return pReq;
}

int
requestHybrid::SendRequest (struct st_mvdashRequest *pMsg, Ptr<Socket> m_socket, bool m_connected,
                            std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexReqSent)
{
  NS_LOG_FUNCTION (this);
  int success = 0;
  if (pMsg->group == 0)
    {

      Ptr<Packet> packet = Create<Packet> ((uint8_t const *) pMsg, sizeof (st_mvdashRequest));

      int actual = m_socket->Send (packet);

      if (actual == (int) packet->GetSize ())
        {

          std::vector<int32_t> qIndexes (m_nViewpoints, -1);
          m_requests->push (*pMsg);
          qIndexes[pMsg->viewpoint] = pMsg->qualityIndex;

          m_downData.id.push_back (pMsg->id);
          struct st_requestTimeInfo tinfo = {Simulator::Now ().GetMicroSeconds (), 0, 0};
          m_downData.time.push_back (tinfo);
          m_downData.group.push_back (0); //set as single request
          m_downData.qualityIndex.push_back (qIndexes);
          m_downData.viewpointPriority.push_back (m_pViewModel->CurrentViewpoint ());

          if (pending)
            {
              std::vector<int32_t> pIndex (1, pMsg->timeIndex);
              m_downData.playbackIndex.push_back (pIndex);
            }
          else
            {
              std::vector<int32_t> pIndex (m_nViewpoints, pMsg->timeIndex);
              m_downData.playbackIndex.push_back (pIndex);
            }

          segment_singleCounter++;
          success = 1;
        }
      else
        {
          NS_LOG_DEBUG ("  mvdashClient Unable to send a request packet" << actual);
        }
    }
  else
    {
      success = groupMv->SendRequest (pMsg, m_socket, m_connected, m_requests, m_tIndexReqSent);
    }

  return success;
}

int
requestHybrid::readRequest (std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexDownloaded,
                            int32_t m_recvRequestCounter, int64_t timeNow, st_mvdashRequest curSeg)
{
  int success = 0;
  // int32_t vp = m_pViewModel->CurrentViewpoint ();
  // int32_t vp_prev = m_pViewModel->PrevViewpoint ();

  if (curSeg.group == 0)
    {

      m_downData.time.at (curSeg.id).downloadEnd = timeNow;

      //Consume buffer level
      for (int vp = 0; vp < m_nViewpoints; vp++)
        {
          g_bufferData[vp].timeNow.push_back (timeNow);
          g_bufferData[vp].bufferLevelOld.push_back (
              std::max (g_bufferData[vp].bufferLevelNew.back () -
                            (timeNow - m_downData.time.at (curSeg.id - 1).downloadEnd),
                        (int64_t) 0));
          g_bufferData[vp].bufferLevelNew.push_back (g_bufferData[vp].bufferLevelOld.back ());
        }

      //=====================================
      //Temporary buffer for hybrid request to prevent greedy quality selection
      //======================================

      int64_t bufferCurrent;
      if (m_downData.group.at (curSeg.id - 1))
        { //If prev is group, Based on time available to play the next segment
          bufferCurrent = std::max (m_videoData[0].segmentDuration -
                                        (timeNow - m_playData.playbackStart.back ()),
                                    (int64_t) 0);
        }
      else
        { //based on last buffer level
          bufferCurrent = std::max (g_bufferData[curSeg.viewpoint].hybrid_bufferLevelNew.back () -
                                        (timeNow - m_downData.time.at (curSeg.id - 1).downloadEnd),
                                    (int64_t) 0);
        }

      //buffer handle
      g_bufferData[curSeg.viewpoint].hybrid_bufferLevelOld.push_back (bufferCurrent);
      g_bufferData[curSeg.viewpoint].hybrid_bufferLevelNew.push_back (
          bufferCurrent + m_videoData[0].segmentDuration);

      //Redownload handle
      m_downSegment.redownload[curSeg.timeIndex] = 1;
      m_downSegment.qualityIndex[curSeg.timeIndex][curSeg.viewpoint] = curSeg.qualityIndex;

      success = 1;
    }
  else
    {
      success = groupMv->readRequest (m_requests, m_tIndexDownloaded, m_recvRequestCounter, timeNow,
                                      curSeg);
    }
  return success;
}

// bool
// requestHybrid::Req_HybridCanceling ()
// {
//   segment_LastBuffer = g_bufferData[m_pViewModel->CurrentViewpoint ()].segmentID.back ();
//   if (Req_m_tIndexReqSent + 1 > segment_LastBuffer)
//     {
//       NS_LOG_INFO ("------------------------------------------------------------------ Cancel S "
//                    << Req_m_tIndexReqSent + 1);
//       Req_single = false;
//     }

//   return Req_single;
// }

// int
// requestHybrid::Req_HybridSegmentSelection (int Req_m_tIndexReqSent)
// {
//   NS_LOG_INFO ("--------------------------- Base Segment " << Req_m_tIndexReqSent + 1);

//   int segment_LastBuffer = g_bufferData[m_pViewModel->CurrentViewpoint ()].segmentID.back ();
//   bool run = true;

//   //Segment already downloaded in high quality --> skip

//   while (run)
//     {
//       if (Req_m_tIndexReqSent + 1 <= segment_LastBuffer)
//         {
//           if (m_downSegment
//                   .qualityIndex[Req_m_tIndexReqSent + 1][m_pViewModel->CurrentViewpoint ()] >= 1)
//             Req_m_tIndexReqSent += 1;
//           else
//             run = false;
//         }
//       else
//         {
//           Req_single = false;
//           run = false;
//         }
//     }

//   NS_LOG_INFO ("--------------------------- Already downloaded? " << Req_m_tIndexReqSent + 1);

//   // Segment already played --> skip
//   run = true;
//   while (run)
//     {
//       std::vector<int>::iterator it;
//       it = find (m_playData.playbackIndex.begin (), m_playData.playbackIndex.end (),
//                  Req_m_tIndexReqSent + 1);
//       if (it != m_playData.playbackIndex.end ())
//         Req_m_tIndexReqSent += 1;
//       else
//         run = false;
//     }

//   NS_LOG_INFO ("--------------------------- Already played? " << Req_m_tIndexReqSent + 1);

//   // Not enough time --> skip
//   run = true;
//   while (run)
//     {
//       if (Req_m_tIndexReqSent + 1 > segment_LastBuffer)
//         {
//           Req_single = false;
//           run = false;
//         }
//       st_mvdashRequest *pReq = Hybrid_PrepareRequest (Req_m_tIndexReqSent + 1);
//       free (pReq);
//       if (skip_requestSegment == 1)
//         {
//           if (Req_m_tIndexReqSent + 2 > segment_LastBuffer)
//             {
//               Req_single = false;
//               run = false;
//             }
//           else
//             Req_m_tIndexReqSent += 1;
//         }
//       else
//         run = false;
//     }
//   NS_LOG_INFO ("--------------------------- Skip? " << Req_m_tIndexReqSent + 1 << " Req Single? "
//                                                     << Req_single);

//   return Req_m_tIndexReqSent;
//   return 0;
// }

} // namespace ns3