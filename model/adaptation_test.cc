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
#include <ns3/core-module.h>
#include "adaptation_test.h" // dion

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("adaptationTest");

NS_OBJECT_ENSURE_REGISTERED (adaptationTest);

// #define MY_NS_LOG_INFO

adaptationTest::adaptationTest (const t_videoDataGroup &videoData,
                                const struct playbackDataGroup &playData,
                                const bufferDataGroup &bufferData,
                                const struct downloadData &downData)
    : mvdashAdaptationAlgorithm (videoData, playData, bufferData, downData)
{
  NS_LOG_FUNCTION (this);
  m_bHigh = 600000000000; //5 seconds -------------------------------------------------
  m_bLow = 2000000; //2 seconds minimum
  m_nViewpoints = videoData.size ();
}

mvdashAlgorithmReply
adaptationTest::SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                   std::vector<int32_t> *pIndexes, bool isGroup, bool isVpChange)
{
  //event controller

  int64_t delay = 0;
  int skip_requestSegment = 0;

  int vp;
  int qIndexForCurView = 0;

  //Group : set other vp to 0, single : set other -1
  for (vp = 0; vp < m_nViewpoints; vp++)
    {
      if (isGroup)
        (*pIndexes)[vp] = 0;
      else
        (*pIndexes)[vp] = -1;
    }
  if (tIndexReq > 0)
    {

      // Get Last Donwloaded SID
      int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
      int32_t idTimeLast = m_downData.time.size () - 1; //last time INDEX of download data

      //if the download time is -, set id last as previous
      if (m_downData.time[idTimeLast].downloadEnd <= 0)
        {
          idTimeLast -= 1;
        }

      int64_t idLast = m_downData.id.back (); //last INDEX of download data
      int32_t segment = m_downData.playbackIndex[idLast]; //last downloaded segment
      int prevViewpoint = m_downData.viewpointPriority.at (idLast);

      //====== Calculating previous download dataSize==========
      bool prev_single = (m_downData.group[idLast] == 0); //Previous is Group or single download
      int64_t dataSize = 0;
      //if previous download is group, we need to calculate other viewpoint segment size
      if (!prev_single)
        {
          for (vp = 0; vp < m_nViewpoints; vp++)
            {
              int q = m_downData.qualityIndex[idLast][vp];
              dataSize += m_videoData[vp].segmentSize[q][segment];
            }
        }
      else
        { //only consider single viewpoint of prev datasize
          int q = m_downData.qualityIndex[idLast][prevViewpoint];
          dataSize += m_videoData[prevViewpoint].segmentSize[q][segment];
        }

      //=========BW estimation=====================
      //Calculate delay or donwload time
      int64_t tDelay =
          m_downData.time[idTimeLast].downloadEnd - m_downData.time[idTimeLast].requestSent;
      //Estimate BW = data/delay * duration
      double bwBytesPerDuration = (double) dataSize * m_videoData[0].segmentDuration / tDelay;

      // //============Debug for previous information========================
      double bw_prev = dataSize * m_videoData[0].segmentDuration / tDelay;

      // NS_LOG_INFO ("Last >>>>>>>>>>>>>>>>>>>> Group?: "
      //              << m_downData.group[idLast] << ", [VP-sgm-Q]: [" << prevViewpoint << "-"
      //              << segment << "-" << m_downData.qualityIndex[idLast][prevViewpoint] << "]"
      //              << ", Buffer last: " << m_bufferData[curViewpoint].bufferLevelNew.back ()
      //              << " ,downTime: " << tDelay << " ,bw: " << bw_prev << " tA ");

      //=====Calculate other dataSize for current download===========
      //only for group request as it download all vp
      int64_t other_dataSizeToSend = 0;
      if (isGroup)
        //Calculate other datasize to Send
        for (vp = 0; vp < m_nViewpoints; vp++)
          {
            if (vp != curViewpoint)
              other_dataSizeToSend += m_videoData[vp].segmentSize[0][tIndexReq];
          }

      //====================Buffer control=============================
      //Calculate delay to avoid buffer overflow
      int64_t buffer_now = m_bufferData[curViewpoint].bufferLevelNew.back () -
                           (timeNow - m_downData.time.back ().downloadEnd);
      //Buffer control

      int64_t lowerBound = 0;
      // if (buffer_now <= m_bLow)
      //   {
      lowerBound = m_videoData[0].segmentDuration;
      // }

      //==========tAvailable calculation==========
      //group&single request is based on bufferLevel
      // int64_t tAvailable = m_bufferData[curViewpoint].bufferLevelNew.at (tIndexReq - 1) -
      //                      (timeNow - m_downData.time.back ().downloadEnd)-lowerBound;

      int64_t tAvailable = m_bufferData[curViewpoint].bufferLevelNew.back () -
                           (timeNow - m_downData.time.back ().downloadEnd) - lowerBound;

      /***
       * For Hybrid request
       *  - tAvailable is based on playing time
       *  - when download segment v+1, tAvailable is limited to 1 segment duration
      */

      if (!isGroup)
        {
          // int32_t skipCount = Req_HybridSegmentSelection (curViewpoint, tIndexReq);
          // NS_LOG_INFO (tIndexReq << " " << skipCount);
          // tIndexReq += skipCount;
          int32_t skipCount = tIndexReq - m_playData.playbackIndex.back ();
          // NS_LOG_INFO("sd-----------------"<<skipCount << " "<< m_playData.playbackStart.back ());

          if (prevViewpoint != curViewpoint)
            {
              tAvailable = ((skipCount) *m_videoData[0].segmentDuration) -
                           (timeNow - m_playData.playbackStart.back ());
            }
          else
            { //next segment based on temporary buffer

              tAvailable = m_bufferData[curViewpoint].hybrid_bufferLevelNew.back () -
                           (timeNow - m_downData.time.back ().downloadEnd);

              
            }
          //  int QualityMin = 1; //<1 is unnecessary to redownload
          //   int64_t targetQuality = m_videoData[curViewpoint].segmentSize[QualityMin][tIndexReq];
        }

      //====Calculate Quality for main View===============
      // data alowed for current view rate
      int64_t dataAllowed =
          (int64_t) bwBytesPerDuration * tAvailable / m_videoData[0].segmentDuration -
          other_dataSizeToSend;

      //set Best rate where it bellow data allowed
      if (dataAllowed > 0)
        {
          for (int qindex = m_videoData[curViewpoint].averageBitrate.size () - 1; qindex > 0;
               qindex--)
            {
              if (m_videoData[curViewpoint].segmentSize[qindex][tIndexReq] <= dataAllowed)
                {
                  qIndexForCurView = qindex;
                  break;
                }
            }
        }

      //Calculate delay
      int64_t chunk_size =
          other_dataSizeToSend + m_videoData[curViewpoint].segmentSize[qIndexForCurView][tIndexReq];
      int64_t down_predict = chunk_size * m_videoData[0].segmentDuration / bw_prev;
      int64_t Bf_predict = buffer_now - down_predict + m_videoData[curViewpoint].segmentDuration;
      if (buffer_now > m_bHigh)
        {
          delay = Bf_predict - m_bHigh;
        }

      // NS_LOG_INFO (tIndexReq-1 << " " << bw_prev << " - " << dataSize << " " << buffer_now);
    }

  (*pIndexes)[curViewpoint] = qIndexForCurView;

  if (!isGroup)
    {
      if (qIndexForCurView < 1)
        skip_requestSegment = 1;
    }
  // else
  // NS_LOG_INFO("================================"<<tIndexReq << " " << qIndexForCurView);
  // if (tIndexReq>1)
  //   (*pIndexes)[curViewpoint] = 3;

  mvdashAlgorithmReply results;
  results.nextDownloadDelay = delay;
  results.skip_requestSegment = skip_requestSegment;
  results.nextRepIndex = tIndexReq;

  return results;
}

int32_t
adaptationTest::Req_HybridSegmentSelection (int32_t curViewpoint, int32_t Req_m_tIndexReqSent)
{
  int32_t skipCount = 0;
  int32_t segment_LastBuffer = m_bufferData[curViewpoint].segmentID.back ();
  bool run = true;
  //Segment already in high quality --> skip

  while (run)
    {
      //mdowndata --> mdownSegment
      if (m_downData.qualityIndex[Req_m_tIndexReqSent][curViewpoint] >= 1 &&
          Req_m_tIndexReqSent < segment_LastBuffer)
        {
          NS_LOG_INFO (",,,,,,,,,," << Req_m_tIndexReqSent);
          skipCount += 1;
          Req_m_tIndexReqSent += 1;
        }
      else
        run = false;
    }
  // Segment already played --> skip
  run = true;
  while (run)
    {
      std::vector<int>::iterator it;
      std::vector<int32_t> myvector = m_playData.playbackIndex;
      it = std::find (myvector.begin (), myvector.end (), Req_m_tIndexReqSent);
      if (it != myvector.end ())
        {
          Req_m_tIndexReqSent += 1;
          skipCount += 1;
        }
      else
        run = false;
    }

  return skipCount;
}

} // namespace ns3