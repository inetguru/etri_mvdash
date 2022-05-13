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

#ifndef MVDASH_ADAPTATION_ALGORITHM_H
#define MVDASH_ADAPTATION_ALGORITHM_H

#include "ns3/application.h"
#include "ns3/mvdash.h"

namespace ns3 {

class mvdashAdaptationAlgorithm : public Object
{
public:
  mvdashAdaptationAlgorithm ( const t_videoDataGroup &videoData,
                        const struct playbackDataGroup & playData,
                        const struct bufferData & bufferData,
                        const struct downloadDataGroup & downData  );

  virtual int64_t SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint, std::vector <int32_t> *pIndexes,  int32_t group) = 0; //TEST

protected:
  const t_videoDataGroup & m_videoData;
  const struct playbackDataGroup & m_playData;
  const struct bufferData & m_bufferData;
  const struct downloadDataGroup &m_downData;
};
} // namespace ns3

#endif /* MVDASH_ADAPTATION_ALGORITHM_H */