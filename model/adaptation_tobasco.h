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

#ifndef ADAPTATION_TOBASCO_H
#define ADAPTATION_TOBASCO_H

#include "mvdash_adaptation_algorithm.h"

namespace ns3 {
class adaptationTobasco : public mvdashAdaptationAlgorithm
{
public:
  adaptationTobasco (const t_videoDataGroup &videoData, const struct playbackDataGroup &playData,
                    const bufferDataGroup &bufferData, const struct downloadData &downData);


  //isGroup : Request -> group true, single false
  //isVpChange : Viewpoint change true/false 
  mvdashAlgorithmReply SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                          std::vector<int32_t> *pIndexes, bool isGroup, bool isVpChange);

protected:
private:
  int32_t m_nViewpoints;

  /**
   * \brief Average segment throughput during the time interval [t1, t2]
   */
  double AverageSegmentThroughput (int32_t curViewpoint,int64_t t1, int64_t t2);
  /**
   * Was the minimum buffer level observed during a time interval with duration delta_beta
   * for a given t_n, at every point of time t_i <= t_(i+1) for every t_i < t_n ?
   *
   * \return if true is returned, then the above described situation holds, else false
   *
   */
  bool MinimumBufferLevelObserved ();

  const double m_a1;
  const double m_a2;
  const double m_a3;
  const double m_a4;
  const double m_a5;
  const int64_t m_bMin;
  const int64_t m_bLow;
  const int64_t m_bHigh;
  const int64_t m_bOpt;
  const int64_t m_deltaBeta;
  const int64_t m_deltaTime;
  int64_t m_lastRepIndex;
  bool m_runningFastStart;
  const int64_t m_highestRepIndex;

};
} // namespace ns3

#endif /* ADAPTATION_TOBASCO_H */