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

#ifndef ADAPTATION_PANDA_H
#define ADAPTATION_PANDA_H

#include "mvdash_adaptation_algorithm.h"

namespace ns3 {
class adaptationPanda : public mvdashAdaptationAlgorithm
{
public:
  adaptationPanda (const t_videoDataGroup &videoData, const struct playbackDataGroup &playData,
                    const bufferDataGroup &bufferData, const struct downloadData &downData);


  //isGroup : Request -> group true, single false
  //isVpChange : Viewpoint change true/false 
  mvdashAlgorithmReply SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                          std::vector<int32_t> *pIndexes, bool isGroup, bool isVpChange);

protected:
private:
  int FindLargest (const double smoothBandwidthShare, const int32_t curViewpoint, const double delta);
  const double m_kappa;
  const double m_omega;
  const double m_alpha;
  const double m_beta;
  const double m_epsilon;
  const int64_t m_bMin;
  const int64_t m_highestRepIndex;
  int64_t m_lastBuffer;
  double m_lastTargetInterrequestTime;
  double m_lastBandwidthShare;
  double m_lastSmoothBandwidthShare;
  double m_lastVideoIndex;
  int32_t m_nViewpoints;
};

} // namespace ns3

#endif /* ADAPTATION_PANDA_H */