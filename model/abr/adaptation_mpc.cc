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
#include "adaptation_mpc.h" // dion
#include <numeric>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <math.h> /* log2 */

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("adaptationMpc");

NS_OBJECT_ENSURE_REGISTERED (adaptationMpc);

// #define MY_NS_LOG_INFO

adaptationMpc::adaptationMpc (const t_videoDataGroup &videoData,
                              const struct playbackDataGroup &playData,
                              const bufferDataGroup &bufferData,
                              const struct downloadData &downData)
    : mvdashAdaptationAlgorithm (videoData, playData, bufferData, downData)
{
  NS_LOG_FUNCTION (this);
  m_nViewpoints = videoData.size ();
  default_quality = 0;
  s_info = 5;
  s_len = 5;
  m_bHigh = 600000000000; //5 seconds -------------------------------------------------
  a_dim = videoData[0].segmentSize.size (); // repesentation count
  mpc_future_count = 5;
  QoeLog = 1;
}

mvdashAlgorithmReply
adaptationMpc::SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                  std::vector<int32_t> *pIndexes, bool isGroup,
                                  std::string m_reqType)
{

  // int vp;
  int64_t delay = 0;
  int skip_requestSegment = 0;
  int64_t down_predict = 0;
  int qIndexForCurView = 0;
  std::vector<std::vector<int>> bestCombo (m_nViewpoints,
                                           std::vector<int> (s_len, 0)); //get best combinations
  if (tIndexReq > 0)
    {

      const int64_t timeNow = Simulator::Now ().GetMicroSeconds ();

      //video data
      int video_chunk_total = m_videoData[0].segmentSize[0].size () - 1;
      int video_chunk_remain = video_chunk_total - tIndexReq;
      int64_t idLast = m_downData.id.back (); //last index of downloaded data
      int prev_mainView = m_downData.viewpointPriority[idLast];

      int64_t start_buffer = m_bufferData[curViewpoint].bufferLevelNew.back () -
                             (timeNow - m_downData.time.back ().downloadEnd); // buffer level
      //During viewpoint change, buffer level should follow previous viewpoint
      if (prev_mainView != curViewpoint)
        {
          start_buffer = m_bufferData[prev_mainView].bufferLevelNew.back () -
                       (timeNow - m_downData.time.back ().downloadEnd);
        }
      //the first request after VP change should be finished before the playback end (1 segment duration)
      // if (!isGroup)
      //   {
      //     int32_t skipCount = tIndexReq - m_playData.playbackIndex.back ();
      //     if (prev_mainView != curViewpoint)
      //       {

      //         start_buffer = ((skipCount) *m_videoData[0].segmentDuration) -
      //                        (timeNow - m_playData.playbackStart.back ());
      //         NS_LOG_INFO ("---------buffer start from 2s " << start_buffer);
      //       }
      //     else
      //       {
      //         start_buffer = m_bufferData[curViewpoint].hybrid_bufferLevelNew.back () -
      //                        (timeNow - m_downData.time.back ().downloadEnd);
      //         NS_LOG_INFO ("---------buffer continue from s "
      //                      << start_buffer << " "
      //                      << m_bufferData[curViewpoint].bufferLevelNew.back () -
      //                             (timeNow - m_downData.time.back ().downloadEnd));
      //       }
      //   }

      //bitrate
      int32_t bitrate_previous;
      if (m_reqType == "group_sg")
        bitrate_previous = m_downData.qualityIndex[idLast][((int) pIndexes->size ()) - 1];
      else
        bitrate_previous = m_downData.qualityIndex[idLast][prev_mainView];

      if (bitrate_previous == -1)
        bitrate_previous = m_downData.qualityIndex[idLast][curViewpoint]; // or 0?

      //downloadtime caluculation
      int32_t idTimeLast = m_downData.time.size () - 1; //last time index of download data
      int64_t tDelay = m_downData.time[idTimeLast].downloadEnd -
                       m_downData.time[idTimeLast].requestSent; //in ms of download time

      //-------------Bandwidth calculation-----------------------------------------------------
      //keep n past information only
      if ((int) past_errors.size () >= s_len)
        past_errors.erase (past_errors.begin ());
      if ((int) past_bandwidht.size () >= s_len)
        past_bandwidht.erase (past_bandwidht.begin ());

      //calculate past bandwidht in single or group request
      double bw_prev = past_ChunkSize (idLast, pIndexes, prev_mainView);
      bw_prev = bw_prev * m_videoData[0].segmentDuration / tDelay;
      past_bandwidht.push_back (bw_prev);

      //calculate error prediction : previous used BW vs actual BW
      double curr_error = 0; // we cannot predict the initial request
      if (past_bandwidth_ests.size () > 0)
        curr_error = std::abs (past_bandwidth_ests.back () - bw_prev) / bw_prev;
      past_errors.push_back (curr_error);

      //Calculate BW prediction
      double bw_sum = 0;
      for (auto &p : past_bandwidht)
        bw_sum += (1 / p);

      double harmonic_bandwidth = 1 / (bw_sum / past_bandwidht.size ());
      //get max error
      double bw_error = 0;
      for (auto &p : past_errors)
        bw_error = std::max (bw_error, p);

      double bw_future = harmonic_bandwidth / (1.0 + bw_error); //--> robust MPC
      bw_future = past_bandwidht.back (); //--> use previous BW
      past_bandwidth_ests.push_back (bw_future);

      //-----------------------
      // NS_LOG_INFO ("Pas e " << past_errors.size () << " Past BW " << past_bandwidht.size ()
      //                       << " Past Esti " << past_bandwidth_ests.size ());
      // std::string logStr;
      // for (auto &e : past_errors)
      //   {
      //     logStr.append (std::to_string (e) + " ");
      //   }
      // NS_LOG_INFO (logStr);
      // logStr = "";
      // for (auto &e : past_bandwidht)
      //   {
      //     logStr.append (std::to_string (e) + " ");
      //   }
      // NS_LOG_INFO (tIndexReq << " " << logStr << " " << start_buffer);
      // logStr = "";
      // for (auto &e : past_bandwidth_ests)
      //   {
      //     logStr.append (std::to_string (e) + " ");
      //   }
      // NS_LOG_INFO (logStr);
      // ------------------------------------------------------------------------------------

      //future chunks length
      int last_index = (int) (video_chunk_total - video_chunk_remain);
      int future_chunk_length = mpc_future_count;
      if (video_chunk_total - last_index < mpc_future_count)
        {
          future_chunk_length = video_chunk_total - last_index;
        } //make sure the maximum is 5

      // --------------START MPC---------------
      //Combo options
      std::vector<std::vector<int>> CHUNK_COMBO_OPTIONS = combinatorial (a_dim, mpc_future_count);
      double reward = 0;
      double max_reward = -999999999; //Maximization
      //iterate through each combination

      NS_LOG_INFO ("------------------------------------------------!");
      for (auto &combo : CHUNK_COMBO_OPTIONS)
        {
          double bitrate_last = bitrate_previous; //last downloaded chunk quality
          double curr_buffer = start_buffer;
          double curr_rebuffer_time = 0;
          double bitrate_sum = 0;
          double smooth_diff = 0;
          int last_VP = prev_mainView;

          // iterate through quality combination for each future chunk
          for (auto pos = 0; pos < future_chunk_length; pos++)
            {
              auto chunk_quality = combo[pos];
              auto index = tIndexReq + pos; //iterate through next chunk

              //calculate donwload time for single/group download
              double chunk_size = predict_ChunkSize (isGroup, index, chunk_quality, pIndexes,
                                                     curViewpoint, m_reqType);

              double download_time = (chunk_size * m_videoData[0].segmentDuration) / bw_future;

              // NS_LOG_INFO("----------------"<<chunk_quality<< " - "<<chunk_size << " " << download_time << " - " << curr_buffer);
              if (curr_buffer < download_time)
                {
                  curr_rebuffer_time += (download_time - curr_buffer);
                  curr_buffer = 0;

                  // break; //-------------- don't want to get buffering
                }
              else
                {
                  curr_buffer -= download_time;
                }
              if (QoeLog == 1)
                {
                  curr_buffer += m_videoData[0].segmentDuration;
                  bitrate_sum += QOELog (chunk_quality, curViewpoint);
                  smooth_diff += std::abs (QOELog (chunk_quality, curViewpoint) -
                                           QOELog (bitrate_last, last_VP));
                }
              else
                {
                  curr_buffer += m_videoData[0].segmentDuration;
                  bitrate_sum += m_videoData[curViewpoint].averageBitrate.at (chunk_quality);

                  smooth_diff +=
                      std::abs (m_videoData[curViewpoint].averageBitrate.at (chunk_quality) -
                                m_videoData[last_VP].averageBitrate.at (bitrate_last));
                }
              bitrate_last = chunk_quality;
              last_VP = curViewpoint;
            }

          reward = (bitrate_sum) - (35 * curr_rebuffer_time) - (smooth_diff);
          // NS_LOG_INFO(reward);
          bitrate_last = bitrate_previous;
          //save best reward
          if (reward > max_reward)
            {
              max_reward = reward;
              bestCombo[curViewpoint] = combo;

              // if (tIndexReq == 10 || tIndexReq == 177)
              //   {
              //     //calculate donwload time for single/group download
              //     double chunk_size =
              //         predict_ChunkSize (isGroup, tIndexReq, combo[0], pIndexes, curViewpoint);
              //     double download_time = (chunk_size * m_videoData[0].segmentDuration) / bw_future;

              //     NS_LOG_INFO (" R " << download_time << " combo " << combo[0] << combo[1]
              //                        << combo[2] << combo[3] << combo[4] << " R [" << bitrate_sum
              //                        << "-" << curr_rebuffer_time << "-" << smooth_diff << "] B ["
              //                        << start_buffer << " " << curr_buffer << "] T " << bw_future
              //                        << " " << bitrate_last);
              //   }
            }
        }

      qIndexForCurView = bestCombo[curViewpoint][0];

      //Calculate delay to avoid buffer overflow
      int64_t chunk_size = predict_ChunkSize (isGroup, tIndexReq, bestCombo[curViewpoint][0],
                                              pIndexes, curViewpoint, m_reqType);
      down_predict = chunk_size * m_videoData[0].segmentDuration / bw_future;
      int64_t Bf_predict = start_buffer - down_predict + m_videoData[curViewpoint].segmentDuration;
      if (start_buffer > m_bHigh)
        {
          delay = Bf_predict - m_bHigh;
        }


      //skip segment request for hybrid
      if (!isGroup)
        {

          if (qIndexForCurView < 1)
            skip_requestSegment = 1;
        }
    }

  // NS_LOG_INFO (" R   combo " << bestCombo[0] << bestCombo[1] << bestCombo[2] << bestCombo[3] << bestCombo[4]);
  if (m_reqType == "group_sg")
    {
      for (int i = 0; i < (int) pIndexes->size (); i++)
        {
          (*pIndexes)[i] = bestCombo[curViewpoint][i];
        }
    }
  else
    {
      (*pIndexes)[curViewpoint] = bestCombo[curViewpoint][0];
    }

  mvdashAlgorithmReply results;
  results.nextDownloadDelay = delay;
  results.nextRepIndex = tIndexReq;
  results.skip_requestSegment = skip_requestSegment;
  results.rate_group = *pIndexes;

  return results;
}

std::vector<std::vector<int>>
adaptationMpc::combinatorial (int a_dim, int mpc_future_count)
{

  std::vector<std::vector<int>> CHUNK_COMBO_OPTIONS;

  for (auto idx = 0; idx < std::pow (a_dim, mpc_future_count); idx++)
    {
      std::vector<int> vec;
      int j = idx;
      for (auto i = 0; i < mpc_future_count; ++i)
        {
          auto tmp = j % a_dim;
          vec.push_back (tmp);
          j /= a_dim;
        }
      CHUNK_COMBO_OPTIONS.push_back (vec);
    }
  return CHUNK_COMBO_OPTIONS;
}

int64_t
adaptationMpc::past_ChunkSize (int idLast, std::vector<int32_t> *pIndexes, int prev_mainView)
{
  int64_t chunk_size = 0;
  bool prev_single = (m_downData.group[idLast] == 0); //Previous is Group or single download

  if (prev_single)
    {
      int32_t segment = m_downData.playbackIndex[idLast][0];
      int q = m_downData.qualityIndex[idLast][prev_mainView];
      chunk_size += m_videoData[prev_mainView].segmentSize[q][segment];
    }
  else
    {

      for (int i = 0; i < (int) pIndexes->size (); i++)
        {
          int32_t segment = m_downData.playbackIndex[idLast][i]; //last index of segment
          int q = m_downData.qualityIndex[idLast][i];
          chunk_size += m_videoData[prev_mainView].segmentSize[q][segment];
        }
    }
  return chunk_size;
}

int64_t
adaptationMpc::predict_ChunkSize (bool isGroup, int segmentID, int chunk_quality,
                                  std::vector<int32_t> *pIndexes, int curViewpoint,
                                  std::string m_reqType)
{
  int64_t chunk_size = 0;
  if (isGroup)
    {
      if (m_reqType == "group")
        {
          //calculate multiple view, set other to lowest quality
          for (int vp = 0; vp < (int) pIndexes->size (); vp++)
            {
              if (vp != curViewpoint)
                chunk_size += m_videoData[vp].segmentSize[0][segmentID];
            }
          chunk_size += m_videoData[curViewpoint].segmentSize[chunk_quality][segmentID];
        }
      else if (m_reqType == "group_sg")
        {

          chunk_size += m_videoData[curViewpoint].segmentSize[chunk_quality][segmentID];
        }
    }
  else
    {
      chunk_size += m_videoData[curViewpoint].segmentSize[chunk_quality][segmentID];
    }

  return chunk_size;
}

double
adaptationMpc::QOELog (int64_t Qnow, int VP)
{
  double minRate = m_videoData[VP].averageBitrate.at (0);
  double currRate = m_videoData[VP].averageBitrate.at (Qnow);
  return std::log2 (currRate / minRate);
}

} // namespace ns3