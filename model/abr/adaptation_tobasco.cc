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
#include "adaptation_tobasco.h" // dion

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("adaptationTobasco");

NS_OBJECT_ENSURE_REGISTERED (adaptationTobasco);

// #define MY_NS_LOG_INFO

adaptationTobasco::adaptationTobasco (const t_videoDataGroup &videoData,
                                      const struct playbackDataGroup &playData,
                                      const bufferDataGroup &bufferData,
                                      const struct downloadData &downData)
    : mvdashAdaptationAlgorithm (videoData, playData, bufferData, downData),
      m_a1 (0.75),
      m_a2 (0.33),
      m_a3 (0.5),
      m_a4 (0.75),
      m_a5 (0.9),
      m_bMin (1000000),
      m_bLow (2000000),
      m_bHigh (4000000),
      m_bOpt ((int64_t) (0.5 * (m_bLow + m_bHigh))),
      m_deltaBeta (1000000),
      m_deltaTime (10000000),
      m_highestRepIndex (m_videoData[0].averageBitrate.size () - 1)
{
  NS_LOG_FUNCTION (this);
  m_nViewpoints = videoData.size ();
  m_runningFastStart = true;
}

mvdashAlgorithmReply
adaptationTobasco::SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                      std::vector<int32_t> *pIndexes, bool isGroup,
                                      std::string m_reqType)
{

  int64_t decisionCase = 0;
  int64_t delayDecision = 0;
  int64_t nextRepIndex = 0;
  int64_t bDelay = 0;

  // we use timeFactor, to divide the absolute size of a segment that is given in bits with the duration of a
  // segment in seconds, so that we get bitrate per seconds
  double timeFactor = m_videoData[0].segmentDuration / 1000000;
  const int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
  int64_t bufferNow = 0;

  if (tIndexReq > 0)
    {
      int32_t idLast = m_downData.id.back (); //last INDEX of download data

      int prevViewpoint = m_downData.viewpointPriority.at (idLast); //prev VP

      if (prevViewpoint != curViewpoint)
        {
          m_runningFastStart = true;
        }

      nextRepIndex = m_lastRepIndex;

      bufferNow = m_bufferData[curViewpoint].bufferLevelNew.back () -
                  (timeNow - m_bufferData[curViewpoint].timeNow.back ());

      double averageSegmentThroughput =
          AverageSegmentThroughput (curViewpoint, timeNow - m_deltaTime, timeNow);
      double nextHighestRepBitrate;

      if (m_lastRepIndex < m_highestRepIndex)
        {
          nextHighestRepBitrate =
              (m_videoData[curViewpoint].averageBitrate.at (m_lastRepIndex + 1)) / timeFactor;
        }
      else
        {
          nextHighestRepBitrate =
              (m_videoData[curViewpoint].averageBitrate.at (m_lastRepIndex)) / timeFactor;
        }

      //fast start?
      if (m_runningFastStart && m_lastRepIndex != m_highestRepIndex &&
          MinimumBufferLevelObserved () &&
          ((m_videoData[curViewpoint].averageBitrate.at (m_lastRepIndex)) / timeFactor <=
           m_a1 * averageSegmentThroughput))
        {

          /* --------- running fast start phase --------- */
          if (bufferNow < m_bMin)
            {
              if (nextHighestRepBitrate <= (m_a2 * averageSegmentThroughput))
                {
                  decisionCase = 1;
                  nextRepIndex = m_lastRepIndex + 1;
                }
            }
          else if (bufferNow < m_bLow)
            {
              if (nextHighestRepBitrate <= (m_a3 * averageSegmentThroughput))
                {
                  decisionCase = 2;
                  nextRepIndex = m_lastRepIndex + 1;
                }
            }
          else
            {
              if (nextHighestRepBitrate <= (m_a4 * averageSegmentThroughput))
                {
                  decisionCase = 3;
                  nextRepIndex = m_lastRepIndex + 1;
                }
              if (bufferNow > m_bHigh)
                {
                  delayDecision = 1;
                  bDelay = m_bHigh - m_videoData[curViewpoint].segmentDuration;
                }
            }
        }

      /* --------- --------- */
      else
        {
          m_runningFastStart = false;
          if (bufferNow < m_bMin)
            {
              decisionCase = 4;
              nextRepIndex = 0;
            }
          else if (bufferNow < m_bLow)
            {
              double lastSegmentThroughput =
                  (8.0 *
                   m_videoData[curViewpoint].segmentSize.at (m_lastRepIndex).at (tIndexReq - 1)) /
                  ((double) (m_downData.time[tIndexReq - 1].downloadEnd -
                             m_downData.time[tIndexReq - 1].requestSent) /
                   1000000.0);

              if ((m_lastRepIndex != 0) &&
                  ((8.0 *
                    m_videoData[curViewpoint].segmentSize.at (m_lastRepIndex).at (tIndexReq - 1)) /
                       timeFactor >=
                   lastSegmentThroughput))
                {
                  decisionCase = 5;
                  for (int i = m_highestRepIndex; i >= 0; i--)
                    {
                      if ((8.0 * m_videoData[curViewpoint].segmentSize.at (i).at (tIndexReq - 1)) /
                              timeFactor >=
                          lastSegmentThroughput)
                        {
                          continue;
                        }
                      else
                        {
                          nextRepIndex = i;
                          break;
                        }
                    }
                  if (nextRepIndex >= m_lastRepIndex)
                    {
                      nextRepIndex = m_lastRepIndex - 1;
                    }
                }
            }
          else if (bufferNow < m_bHigh)
            {
              if ((m_lastRepIndex == m_highestRepIndex) ||
                  (nextHighestRepBitrate >= m_a5 * averageSegmentThroughput))
                {
                  delayDecision = 2;
                  bDelay = (int64_t) (std::max (
                      bufferNow - m_videoData[curViewpoint].segmentDuration, m_bOpt));
                }
            }
          else
            {
              if ((m_lastRepIndex == m_highestRepIndex) ||
                  (nextHighestRepBitrate >= m_a5 * averageSegmentThroughput))
                {
                  delayDecision = 3;
                  bDelay = (int64_t) (std::max (
                      bufferNow - m_videoData[curViewpoint].segmentDuration, m_bOpt));
                }
              else
                {
                  decisionCase = 6;
                  nextRepIndex = m_lastRepIndex + 1;
                }
            }
        }
    }

  if (tIndexReq != 0 && delayDecision != 0)
    {
      if (bDelay > bufferNow)
        {
          bDelay = 0;
        }
      else
        {
          bDelay = bufferNow - bDelay;
        }
    }

  m_lastRepIndex = nextRepIndex;

  //   Set all view with rate index 0, except the current view
  int vp;
  for (vp = 0; vp < m_nViewpoints; vp++)
    if (vp != curViewpoint)
      (*pIndexes)[vp] = 0;

  (*pIndexes)[curViewpoint] = nextRepIndex;
  mvdashAlgorithmReply results;
  results.decisionCase = decisionCase;
  results.nextDownloadDelay = bDelay;
  results.nextRepIndex = tIndexReq;
  results.rate_group = *pIndexes;
  return results;
}

bool
adaptationTobasco::MinimumBufferLevelObserved ()
{
  if (m_downData.time.size () < 2)
    {
      return true;
    }
  int64_t lastPackage = m_downData.time.end ()[-1].downloadEnd;
  int64_t secondToLastPackage = m_downData.time.end ()[-2].downloadEnd;

  if (m_deltaBeta < m_videoData[0].segmentDuration)
    {
      if (lastPackage - secondToLastPackage < m_deltaBeta)
        {
          return true;
        }
      else
        {
          return false;
        }
    }
  else
    {
      if (lastPackage - secondToLastPackage < m_videoData[0].segmentDuration)
        {
          return true;
        }
      else
        {
          return false;
        }
    }
}

double
adaptationTobasco::AverageSegmentThroughput (int32_t curViewpoint, int64_t t_1, int64_t t_2)
{
  if (t_1 < 0)
    {
      t_1 = 0;
    }
  if (m_downData.time.size () < 1)
    {
      return 0;
    }

  // First, we have to find the index of the start of the download of the first downloaded segment in
  // the interval [t_1, t_2]
  uint index = 0;
  for (uint i = 0; i < m_downData.time.size (); i++)
    {
      if (m_downData.time.at (i).downloadEnd < t_1)
        {
          continue;
        }
      else
        {
          index = i;
          break;
        }
    }
  double lengthOfInterval;
  double sumThroughput = 0.0;
  double transmissionTime = 0.0;
  if (m_downData.time[index].requestSent < t_1)
    {
      lengthOfInterval = m_downData.time.at (index).downloadEnd - t_1;

      int vp;
      int dataSend = 0;

      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          if (vp != curViewpoint)
            dataSend += m_videoData[vp].averageBitrate.at (m_downData.qualityIndex[index].at (vp));
        }
      dataSend = dataSend / (vp) + m_videoData[curViewpoint].averageBitrate.at (
                                       m_downData.qualityIndex[index].at (curViewpoint));
      sumThroughput +=
          (dataSend *
           ((m_downData.time.at (index).downloadEnd - t_1) /
            (m_downData.time.at (index).downloadEnd - m_downData.time.at (index).requestSent))) *
          lengthOfInterval;

      transmissionTime += lengthOfInterval;
      index++;
      if (index >= m_downData.time.size ())
        {
          return (sumThroughput / (double) transmissionTime);
        }
    }

  // Compute the average download-time of all the fully completed segment downloads during [t_1, t_2].
  while (m_downData.time.at (index).downloadEnd <= t_2)
    {

      lengthOfInterval =
          m_downData.time.at (index).downloadEnd - m_downData.time.at (index).requestSent;

      //calculate other
      int vp;
      int dataSend = 0;
      for (vp = 0; vp < m_nViewpoints; vp++)
        {
          if (vp != curViewpoint)
            dataSend += m_videoData[vp].averageBitrate.at (m_downData.qualityIndex[index].at (vp));
        }

      dataSend = (dataSend / vp) + m_videoData[curViewpoint].averageBitrate.at (
                                       m_downData.qualityIndex[index].at (curViewpoint));

      sumThroughput +=
          (dataSend * m_videoData[0].segmentDuration / lengthOfInterval) * lengthOfInterval;

      transmissionTime += lengthOfInterval;
      index++;
      if (index > m_downData.time.size () - 1)
        {
          break;
        }
    }

  return (sumThroughput / (double) transmissionTime);
}

} // namespace ns3