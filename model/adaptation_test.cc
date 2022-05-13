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
                                  const struct bufferData &bufferData,
                                  const struct downloadDataGroup &downData)
    : mvdashAdaptationAlgorithm (videoData, playData, bufferData, downData)
{
  NS_LOG_FUNCTION (this);
  m_nViewpoints = videoData.size ();
}

int64_t
adaptationTest::SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                    std::vector<int32_t> *pIndexes,  int32_t group)
{
  int vp;
  int qIndexForCurView = 0;


  //   Set all view with rate index 0, except the current view
  for (vp = 0; vp < m_nViewpoints; vp++)
    if (vp != curViewpoint)
      (*pIndexes)[vp] = 0;

  if (tIndexReq > 0)
    {

      // Get Last Donwloaded SID
      int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
      int32_t idTimeLast = m_downData.time.size () - 1; //last time index of download data

      //if the download time is -, set id last as previous
      if (m_downData.time[idTimeLast].downloadEnd <= 0)
        {
          idTimeLast -= 1;
        }

      //Calculate delay
      int64_t tDelay =
          m_downData.time[idTimeLast].downloadEnd - m_downData.time[idTimeLast].requestSent;
      int64_t dataSize = 0;

      int64_t idLast = m_downData.id.back (); //last ID of download data
      int32_t segment = m_downData.playbackIndex[idLast]; //last index od download data

      int64_t dataSizeToSend = 0;
      //Available time = buffertime - margin
      int64_t tAvailable = m_bufferData.bufferLevelNew.back () -
                           (timeNow - m_bufferData.timeNow.back ()) -
                           m_videoData[0].segmentDuration;
      bool prev_single = (m_downData.group[idLast] == 0); //Previous is Group or single download

      if (!prev_single)
        {
          //Calculate Size of previous downloaded segment based on quality of each view point
          for (vp = 0; vp < m_nViewpoints; vp++)
            {
              dataSize += m_videoData[vp].segmentSize[m_downData.qualityIndex[idLast][vp]][segment];
            }

          //Calculate other datasize
          for (vp = 0; vp < m_nViewpoints; vp++)
            {
              if (vp != curViewpoint)
                dataSizeToSend += m_videoData[vp].segmentSize[0][tIndexReq];
            }
        }
      else
        {
          dataSize += m_videoData[curViewpoint]
                          .segmentSize[m_downData.qualityIndex[idLast][curViewpoint]][segment];
        }

      //t available for next single segment
      if (group==0 && !prev_single){
          tAvailable -= m_videoData[0].segmentDuration; //-1 segment duration
      }

      //Estimate BW = data/(delay * duration)
      double bwBytesPerDuration = (double) dataSize / tDelay * m_videoData[0].segmentDuration;


      // //mAXIMUM data alowed for current view rate
      int64_t dataAllowed =
          (int64_t) bwBytesPerDuration * tAvailable / m_videoData[0].segmentDuration -
          dataSizeToSend;

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

    
      //Set current view rate
      // qIndexForCurView=5;  //DION_TEST
      (*pIndexes)[curViewpoint] = qIndexForCurView;
    }
  return 0;
}

} // namespace ns3