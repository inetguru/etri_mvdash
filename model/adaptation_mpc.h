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

#ifndef ADAPTATION_MPC_H
#define ADAPTATION_MPC_H

#include "mvdash_adaptation_algorithm.h" // dion
#include <queue>
#include <math.h>       /* log2 */

namespace ns3 {

class adaptationMpc : public mvdashAdaptationAlgorithm
{
public:
  adaptationMpc (const t_videoDataGroup &videoData, const struct playbackDataGroup &playData,
                 const bufferDataGroup &bufferData, const struct downloadData &downData);

  //isGroup : Request -> group true, single false
  //isVpChange : Viewpoint change true/false
  mvdashAlgorithmReply SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                          std::vector<int32_t> *pIndexes, bool isGroup,
                                          bool isVpChange);

protected:
private:
  int32_t m_nViewpoints;
  int default_quality;
  int s_info; //bit_rate, buffer_size, rebuffering_time, bandwidth_measurement, chunk_til_video_end
  int s_len; //how many frames in past
  int a_dim; //dimention of available bitrate
  int mpc_future_count; //future prediction
  int64_t m_bHigh; //max buffer

  std::vector<double> past_bandwidht;
  std::vector<double> past_errors;
  std::vector<double> past_bandwidth_ests;

  /**
 * @brief combinatorial like itertools.product in python
 * eg, 00, 01, 10 , 11
 * @param a_dim dimension (available representation)
 * @param mpc_future_count future chunk prediction 
 * @return std::vector<std::vector<int>> 
 */
  std::vector<std::vector<int>> combinatorial (int a_dim, int mpc_future_count);

  /**
   * @brief Get the Chunk Size between group or single request
   * 
   * @return int64_t 
   */
  int64_t predict_ChunkSize (bool isGroup, int segmentID, int chunk_quality, int m_nViewpoints,
                             int curViewpoint);
  
  /**
   * @brief Get the Chunk Size between group or single request
   * 
   * @param isGroup 
   * @param idLast ---last download ID to check quality
   * @param segmentLast --segment ID
   * @param m_nViewpoints 
   * @param curViewpoint 
   * @return int64_t 
   */
  int64_t
  past_ChunkSize (bool isGroup, int idLast, int segmentLast, int m_nViewpoints, int curViewpoint);

  double QOELog(int64_t bitrate_now, int Viewpoint);
};

} // namespace ns3

#endif