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
#include "adaptation_panda.h" // dion

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("adaptationPanda");

NS_OBJECT_ENSURE_REGISTERED (adaptationPanda);

// #define MY_NS_LOG_INFO

adaptationPanda::adaptationPanda (const t_videoDataGroup &videoData,
                                  const struct playbackDataGroup &playData,
                                  const bufferDataGroup &bufferData,
                                  const struct downloadData &downData)
    : mvdashAdaptationAlgorithm (videoData, playData, bufferData, downData),
      m_kappa (0.28), //convergence rate
      m_omega (0.3), //additive increase rate
      m_alpha (0.2),
      m_beta (0.2),
      m_epsilon (0.15),
      m_bMin (26),
      m_highestRepIndex (m_videoData[0].averageBitrate.size () - 1)
{
  NS_LOG_INFO (this);
  m_nViewpoints = videoData.size ();
  NS_ASSERT_MSG (m_highestRepIndex >= 0, "The highest quality representation index should be => 0");
}

mvdashAlgorithmReply
adaptationPanda::SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                    std::vector<int32_t> *pIndexes, bool isGroup,
                                    std::string m_reqType)
{
  const int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
  int64_t delay = 0;
  if (tIndexReq == 0)
    {

      int vp;
      for (vp = 0; vp < m_nViewpoints; vp++)
        (*pIndexes)[vp] = 0;

      m_lastVideoIndex = 0;
      m_lastBuffer = (m_videoData[0].segmentDuration) / 1e6;
      m_lastTargetInterrequestTime = 0;

      mvdashAlgorithmReply answer;
      answer.nextRepIndex = 0;
      answer.nextDownloadDelay = 0;
      answer.decisionTime = timeNow;
      answer.decisionCase = 0;
      answer.delayDecisionCase = 0;
      answer.rate_group=*pIndexes;

      return answer;
    }

  //TEST------------------------------------------
  int vp1;
  int prevIndex = m_downData.time.size () - 1;
  int previousRate = 0;

  for (vp1 = 0; vp1 < m_nViewpoints; vp1++)
    {
      if (vp1 != curViewpoint)
        previousRate +=
            m_videoData[vp1].averageBitrate.at (m_downData.qualityIndex[prevIndex].at (vp1));
    }

  previousRate = previousRate / (m_nViewpoints - 1) +
                 m_videoData[curViewpoint].averageBitrate.at (
                     m_downData.qualityIndex[prevIndex].at (curViewpoint));

  // estimate the bandwidth share
  double throughputMeasured =
      ((double) (previousRate * (m_videoData[0].segmentDuration / 1e6)) /
       (double) ((m_downData.time.back ().downloadEnd - m_downData.time.back ().requestSent) /
                 1e6)) /
      1e6;

  //TEST------------------------------------------

  // // estimate the bandwidth share
  // double throughputMeasured =
  //     ((double) (m_videoData[curViewpoint].averageBitrate.at (m_lastVideoIndex) *
  //                (m_videoData[0].segmentDuration / 1e6)) /
  //      (double) ((m_downData.time.back ().downloadEnd - m_downData.time.back ().requestSent) /
  //                1e6)) /
  //     1e6;

  if (tIndexReq == 1)
    {
      m_lastBandwidthShare = throughputMeasured;
      m_lastSmoothBandwidthShare = m_lastBandwidthShare;
    }

  double actualInterrequestTime;
  if (timeNow - m_downData.time.back ().requestSent > m_lastTargetInterrequestTime * 1e6)
    {
      actualInterrequestTime = (timeNow - m_downData.time.back ().requestSent) / 1e6;
    }
  else
    {
      actualInterrequestTime = m_lastTargetInterrequestTime;
    }

  //estimate BW w.r.t unpredictable condition
  double bandwidthShare =
      (m_kappa * (m_omega - std::max (0.0, m_lastBandwidthShare - throughputMeasured + m_omega))) *
          actualInterrequestTime +
      m_lastBandwidthShare;
  if (bandwidthShare < 0)
    {
      bandwidthShare = 0;
    }
  m_lastBandwidthShare = bandwidthShare;

  //smoothing
  double smoothBandwidthShare;
  smoothBandwidthShare =
      ((-m_alpha * (m_lastSmoothBandwidthShare - bandwidthShare)) * actualInterrequestTime) +
      m_lastSmoothBandwidthShare;

  m_lastSmoothBandwidthShare = smoothBandwidthShare;

  double deltaUp = m_omega + m_epsilon * smoothBandwidthShare;
  double deltaDown = m_omega;
  int rUp = FindLargest (smoothBandwidthShare, curViewpoint, deltaUp);
  int rDown = FindLargest (smoothBandwidthShare, curViewpoint, deltaDown);

  int rate_rUp = 0;
  int rate_rDown = 0;
  int rate_lastIndex = 0;

  for (vp1 = 0; vp1 < m_nViewpoints; vp1++)
    {
      if (vp1 != curViewpoint)
        {
          // NS_LOG_INFO ("=========================== : " << m_videoData[vp1].averageBitrate.at (0));
          rate_rUp += m_videoData[vp1].averageBitrate.at (0);
          rate_rDown += m_videoData[vp1].averageBitrate.at (0);
          rate_lastIndex += m_videoData[vp1].averageBitrate.at (0);
        }
    }

  rate_rUp += m_videoData[curViewpoint].averageBitrate.at (rUp);
  rate_rDown += m_videoData[curViewpoint].averageBitrate.at (rDown);
  rate_lastIndex += m_videoData[curViewpoint].averageBitrate.at (m_lastVideoIndex);

  int videoIndex;
  if ((rate_lastIndex) < (rate_rUp))
    {
      videoIndex = rUp;
    }
  else if ((rate_rUp <= rate_lastIndex) && (rate_lastIndex <= rate_rDown))
    {
      videoIndex = m_lastVideoIndex;
    }
  else
    {
      videoIndex = rDown;
    }
  m_lastVideoIndex = videoIndex;

  // int videoIndex;
  // if ((m_videoData[curViewpoint].averageBitrate.at (m_lastVideoIndex)) <
  //     (m_videoData[curViewpoint].averageBitrate.at (rUp)))
  //   {
  //     videoIndex = rUp;
  //   }
  // else if ((m_videoData[curViewpoint].averageBitrate.at (rUp)) <=
  //              (m_videoData[curViewpoint].averageBitrate.at (m_lastVideoIndex)) &&
  //          (m_videoData[curViewpoint].averageBitrate.at (m_lastVideoIndex)) <=
  //              (m_videoData[curViewpoint].averageBitrate.at (rDown)))
  //   {
  //     videoIndex = m_lastVideoIndex;
  //   }
  // else
  //   {
  //     videoIndex = rDown;
  //   }
  // m_lastVideoIndex = videoIndex;

  // schedule next download request

  int rate_target = 0;
  for (vp1 = 0; vp1 < m_nViewpoints; vp1++)
    {
      if (vp1 != curViewpoint)
        rate_target += m_videoData[vp1].averageBitrate.at (0);
    }

  rate_target += m_videoData[curViewpoint].averageBitrate.at (videoIndex);

  double targetInterrequestTime =
      std::max (0.0, ((double) ((rate_target * (m_videoData[0].segmentDuration / 1e6)) / 1e6) /
                      smoothBandwidthShare) +
                         m_beta * (m_lastBuffer - m_bMin));

  // double targetInterrequestTime =
  //     std::max (0.0, ((double) ((m_videoData[curViewpoint].averageBitrate.at (videoIndex) *
  //                                (m_videoData[curViewpoint].segmentDuration / 1e6)) /
  //                               1e6) /
  //                     smoothBandwidthShare) +
  //                        m_beta * (m_lastBuffer - m_bMin));

  if (m_downData.time.back ().downloadEnd - m_downData.time.back ().requestSent <
      m_lastTargetInterrequestTime * 1e6)
    {
      delay = 1e6 * m_lastTargetInterrequestTime -
              (m_downData.time.back ().downloadEnd - m_downData.time.back ().requestSent);
    }
  else
    {
      delay = 0;
    }

  m_lastTargetInterrequestTime = targetInterrequestTime;

  m_lastBuffer = (m_bufferData[curViewpoint].bufferLevelNew.back () -
                  (timeNow - m_downData.time.back ().downloadEnd)) /
                 1e6;

  //   Set all view with rate index 0, except the current view
  int vp;

  for (vp = 0; vp < m_nViewpoints; vp++)
    if (vp != curViewpoint)
      (*pIndexes)[vp] = 0;

  (*pIndexes)[curViewpoint] = videoIndex;

  mvdashAlgorithmReply results;
  results.nextRepIndex = videoIndex;
  results.decisionCase = 0;
  results.nextDownloadDelay = delay;
  results.rate_group = *pIndexes;

  return results;
}

int
adaptationPanda::FindLargest (const double smoothBandwidthShare, const int32_t curViewpoint,
                              const double delta)
{
  int64_t largestBitrateIndex = 0;

  //TEST------------------------------------------
  //Other vp set as 0 quality
  int vp;
  int otherRate = 0;

  for (vp = 0; vp < m_nViewpoints; vp++)
    {
      if (vp != curViewpoint)
        otherRate += m_videoData[vp].averageBitrate.at (0);
    }

  for (int i = 0; i <= m_highestRepIndex; i++)
    {
      int prevRate = otherRate;

      prevRate += m_videoData[curViewpoint].averageBitrate.at (i);

      int64_t currentBitrate = prevRate / 1e6;

      // int64_t currentBitrate = m_videoData[0].averageBitrate.at (i) / 1e6;
      if (currentBitrate <= (smoothBandwidthShare - delta))
        {
          largestBitrateIndex = i;
        }
    }
  return largestBitrateIndex;
}

} // namespace ns3