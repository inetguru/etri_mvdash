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

#include "mvdash_adaptation_algorithm.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("mvdashAdaptationAlgorithm");

NS_OBJECT_ENSURE_REGISTERED (mvdashAdaptationAlgorithm);

mvdashAdaptationAlgorithm::mvdashAdaptationAlgorithm (
                        const t_videoDataGroup &videoData,
                        const struct playbackDataGroup & playData,
                        const struct bufferData & bufferData,
                        const struct downloadDataGroup & downData) 
: m_videoData (videoData),
  m_playData (playData),  
  m_bufferData (bufferData),
  m_downData (downData)
{}

} // namespace ns3