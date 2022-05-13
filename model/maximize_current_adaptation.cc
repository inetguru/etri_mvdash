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
#include "maximize_current_adaptation.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("maximizeCurrentAdaptation");

NS_OBJECT_ENSURE_REGISTERED (maximizeCurrentAdaptation);

// #define MY_NS_LOG_INFO

maximizeCurrentAdaptation::maximizeCurrentAdaptation (const t_videoDataGroup &videoData,
                                                      const struct playbackDataGroup &playData,
                                                      const struct bufferData &bufferData,
                                                      const struct downloadDataGroup &downData)
    : mvdashAdaptationAlgorithm (videoData, playData, bufferData, downData)
{
  NS_LOG_FUNCTION (this);
  m_nViewpoints = videoData.size ();
}

int64_t
maximizeCurrentAdaptation::SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                              std::vector<int32_t> *pIndexes,  int32_t group)
{
  int vp;
  int qIndexForCurView = 0;

  NS_LOG_INFO ("=========================== : " << tIndexReq <<" --> " << curViewpoint);

  //   Set all view with rate index 0, especialy not the current view
  for (vp = 0; vp < m_nViewpoints; vp++)
    if (vp != curViewpoint)
      (*pIndexes)[vp] = 0; 
      // (*pIndexes)[vp] = 1; //DION_TEST

  //sEGMENT INDEX 
  if (tIndexReq > 0)
    {

      // Estimate Avaiable Bandwidth
      int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
      int32_t idLast = m_downData.playbackIndex.back (); //last index od download data
    //   NS_LOG_INFO ("TEEEEEEEEEEEEEEEEEEEEE : " << m_downData.time[idLast].downloadEnd);

      //if the download time is -, set id last as previous
      if (m_downData.time[idLast].downloadEnd <= 0)
        {
          idLast -= 1;
        }

      //Calculate delay
      int64_t tDelay = m_downData.time[idLast].downloadEnd - m_downData.time[idLast].requestSent;
      int64_t dataSize = 0; 


      //Calculate Size of downloaded segment based on quality of each view point
      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          dataSize += m_videoData[vp].segmentSize[m_downData.qualityIndex[idLast][vp]][idLast];
        }

    //Estimate BW = data/(delay * duration)
      double bwBytesPerDuration = (double) dataSize / tDelay * m_videoData[0].segmentDuration;

      // #ifdef MY_NS_LOG_INFO
      //         NS_LOG_INFO("tDelay : " << tDelay
      //             << " dataSize : " << dataSize
      //             << " bwPerDuration : " << bwBytesPerDuration
      //         );

      //         NS_LOG_INFO(Simulator::Now ().As (Time::S)
      //             << " Buffer Status at : "  << m_bufferData.timeNow.back()
      //             << " Level Old " << m_bufferData.bufferLevelOld.back()
      //             << " Level New " << m_bufferData.bufferLevelNew.back()
      //         );

      //         NS_LOG_INFO("Last Downloaded id :" << idLast
      //             << " TSent : " << m_downData.time[idLast].requestSent
      //             << " TStart: " << m_downData.time[idLast].downloadStart
      //             << " TEnd  : " << m_downData.time[idLast].downloadEnd
      //         );

      //         NS_LOG_INFO("tIndex to Select : " << tIndexReq
      //             << " curViewpoint : " << curViewpoint
      //             << " v1 " << m_videoData[0].segmentSize[0][tIndexReq]
      //             << " v2 " << m_videoData[1].segmentSize[0][tIndexReq]
      //             << " v3 " << m_videoData[2].segmentSize[0][tIndexReq]
      //             << " v4 " << m_videoData[3].segmentSize[0][tIndexReq]
      //             << " v5 " << m_videoData[4].segmentSize[0][tIndexReq]
      //         );
      // #endif


      //Calculate other datasize
      int64_t dataSizeToSend = 0;
      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          if (vp != curViewpoint)
            dataSizeToSend += m_videoData[vp].segmentSize[0][tIndexReq];
        }

      //Available time = buffertime - margin
      int64_t tAvailable = m_bufferData.bufferLevelNew.back () -
                           (timeNow - m_bufferData.timeNow.back ()) -
                           m_videoData[0].segmentDuration;

      //mAXIMUM data alowed for current view rate
      int64_t dataAllowed =
          (int64_t) bwBytesPerDuration * tAvailable / m_videoData[0].segmentDuration -
          dataSizeToSend;

      // NS_LOG_INFO ("data allowed : " << dataAllowed);
      

// #ifdef MY_NS_LOG_INFO
//       NS_LOG_INFO ("dataSizeToSend : "
//                    << dataSizeToSend << " tAvailable : " << tAvailable << " dataAllowed : "
//                    << dataAllowed << " q4 : " << m_videoData[curViewpoint].segmentSize[3][tIndexReq]
//                    << " q3 : " << m_videoData[curViewpoint].segmentSize[2][tIndexReq]
//                    << " q2 : " << m_videoData[curViewpoint].segmentSize[1][tIndexReq]
//                    << " q1 : " << m_videoData[curViewpoint].segmentSize[0][tIndexReq]);
// #endif


      //set Best rate where it bellow data allowed
      if (dataAllowed > 0)
        {
          for (int qindex = m_videoData[curViewpoint].averageBitrate.size () - 1; qindex > 0;
               qindex--)
            {
              if (m_videoData[curViewpoint].segmentSize[qindex][tIndexReq] < dataAllowed)
                {
                  qIndexForCurView = qindex;
                  break;
                }
            }
        }

// #ifdef MY_NS_LOG_INFO
//       NS_LOG_INFO ("Q Index for MainView : "
//                    << qIndexForCurView << " segmentSize : "
//                    << m_videoData[curViewpoint].segmentSize[qIndexForCurView][tIndexReq]);
// #endif
      
      //Set current view rate
      // qIndexForCurView=1; //DION_TEST
      (*pIndexes)[curViewpoint] = qIndexForCurView;
    }
  return 0;
}

} // namespace ns3