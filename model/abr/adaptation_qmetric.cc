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
#include "adaptation_qmetric.h" // dion

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("adaptationQmetric");

NS_OBJECT_ENSURE_REGISTERED (adaptationQmetric);

// #define MY_NS_LOG_INFO

adaptationQmetric::adaptationQmetric (const t_videoDataGroup &videoData,
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
  mpc_future_count = 1;
  Qtarget = 90;
  Qsubtarget = 50;
}

mvdashAlgorithmReply
adaptationQmetric::SelectRateIndexes (int32_t tIndexReq, int32_t curViewpoint,
                                      std::vector<int32_t> *pIndexes, bool isGroup,
                                      std::string m_reqType)
{

  int64_t delay = 0;
  int skip_requestSegment = 0;
  int64_t down_predict = 0;
  int qIndexForCurView = 0;
  std::vector<std::vector<int>> bestCombo (m_nViewpoints,
                                           std::vector<int> (s_len, 0)); //get best combinations
  std::vector<int> bestComboF (m_nViewpoints);
  if (tIndexReq > 0)
    {
      const int64_t timeNow = Simulator::Now ().GetMicroSeconds ();

      //video data
      // int video_chunk_total = m_videoData[0].segmentSize[0].size () - 1;
      // int video_chunk_remain = video_chunk_total - tIndexReq;
      int64_t idLast = m_downData.id.back (); //last index of downloaded data
      int prev_mainView = m_downData.viewpointPriority[idLast];
      int32_t segmentLast =
          m_downData.playbackIndex[idLast][prev_mainView]; //chunk ID of the last downloaded data

      int64_t start_buffer = m_bufferData[curViewpoint].bufferLevelNew.back () -
                             (timeNow - m_downData.time.back ().downloadEnd);
      // //the first request after VP change should be finished before the playback end (1 segment duration)
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

      //Get previous bitrate
      std::vector<double> vmaf_defauld ((int) pIndexes->size ());

      for (int vp = 0; vp < (int) pIndexes->size (); vp++)
        {
          //The first segment after viewpoint change follows the previous view
          if (prev_mainView != curViewpoint && vp == curViewpoint)
            vmaf_defauld[vp] =
                m_videoData[prev_mainView]
                    .vmaf[m_downData.qualityIndex[idLast][prev_mainView]][segmentLast];
          else
            {
              int32_t bitrate_previous = m_downData.qualityIndex[idLast][vp];
              if (bitrate_previous == -1)
                bitrate_previous = 0;
              vmaf_defauld[vp] = m_videoData[vp].vmaf[bitrate_previous][segmentLast];
            }
        }

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

      //future chunks length
      // int last_index = (int) (video_chunk_total - video_chunk_remain);
      // int future_chunk_length = mpc_future_count;
      // if (video_chunk_total - last_index < mpc_future_count)
      //   {
      //     future_chunk_length = video_chunk_total - last_index;
      //   } //make sure the maximum is 5

      // --------------START MPC---------------
      //Combo options
      // std::vector<std::vector<int>> CHUNK_COMBO_OPTIONS =
      //     combinatorial (a_dim, future_chunk_length);

      double max_reward = 9 * 1e20; //Minimization
      int sgmt_idx = tIndexReq;
      std::vector<double> vmaf_last = vmaf_defauld;
      std::vector<std::vector<int>> CHUNK_COMBO_VP;
      if (m_reqType == "group")
        {
          CHUNK_COMBO_VP = combinatorial_Viewpoint (curViewpoint, a_dim, m_nViewpoints, sgmt_idx);
        }
      else
        {
          CHUNK_COMBO_VP = combinatorial (a_dim, mpc_future_count);

        }

      for (auto &combo : CHUNK_COMBO_VP)
        {
          double VP_curr_buffer = start_buffer;
          double VP_curr_rebuffer_time = 0;
          double VP_bitrate_sum = 0;
          double VP_smooth_diff = 0;
          double VP_reward = 0;

          double rate = 0;
          // iterate through quality combination for each Viewpoint
          for (auto pos = 0; pos < (int) combo.size (); pos++)
            {
              auto chunk_quality = combo[pos];
              double vmaf_score = m_videoData[pos].vmaf[chunk_quality][sgmt_idx];

              if (pos == curViewpoint)
                {
                  rate = Qtarget - vmaf_score;
                  VP_bitrate_sum += 1 * std::pow (rate, 2);
                  VP_smooth_diff += 1 * std::pow ((vmaf_score - vmaf_last[pos]), 2);
                }
              else
                {
                  rate = Qsubtarget - vmaf_score;
                  VP_bitrate_sum += 0.001 * std::pow (rate, 2);
                  VP_smooth_diff += 0.001 * std::pow ((vmaf_score - vmaf_last[pos]), 2);
                }

              vmaf_last[pos] = vmaf_score;
            }

          double chunk_size =
              predict_ChunkSize (isGroup, sgmt_idx, combo, pIndexes, curViewpoint, m_reqType);
          double download_time = (chunk_size * m_videoData[0].segmentDuration) / bw_future;
          if (VP_curr_buffer < download_time)
            {
              VP_curr_rebuffer_time += std::pow ((download_time - VP_curr_buffer), 2);
              VP_curr_buffer = 0;
            }
          else
            {
              VP_curr_buffer -= download_time;
            }

          VP_curr_buffer += m_videoData[0].segmentDuration;

          VP_reward = std::pow (VP_bitrate_sum, 2) + std::pow (VP_curr_rebuffer_time, 2) +
                      std::pow (VP_smooth_diff, 2);

          //save best reward
          if (VP_reward < max_reward)
            {
              max_reward = VP_reward;
              bestCombo[curViewpoint] = combo;
              bestComboF = combo;
              // NS_LOG_INFO (" Optimum  -R- " << reward << " combo " << bestComboF[0] << bestComboF[1]
              //                               << bestComboF[2] << bestComboF[3] << bestComboF[4]
              //                               << " Rebuffer " << curr_rebuffer_time);
            }
        }

      qIndexForCurView = bestCombo[curViewpoint][0];

      //Calculate delay to avoid buffer overflow
      int64_t chunk_size =
          predict_ChunkSize (isGroup, tIndexReq, bestComboF, pIndexes, curViewpoint, m_reqType);
      down_predict = chunk_size * m_videoData[0].segmentDuration / bw_future;
      int64_t Bf_predict = start_buffer - down_predict + m_videoData[curViewpoint].segmentDuration;
      if (start_buffer > m_bHigh)
        {
          delay = Bf_predict - m_bHigh;
        }

      //skip segment request
      if (!isGroup)
        {

          if (qIndexForCurView < 1)
            skip_requestSegment = 1;
        }
    }

  if (m_reqType == "group_sg")
    {
      for (int i = 0; i < (int) pIndexes->size (); i++)
        {
          (*pIndexes)[i] = bestCombo[curViewpoint][i];
        }
    }
  else if (m_reqType == "single")
    {
      (*pIndexes)[curViewpoint] = bestCombo[curViewpoint][0];
    }
  else
    {
      for (int vp = 0; vp < m_nViewpoints; vp++)
        (*pIndexes)[vp] = bestComboF[vp];
    }

  mvdashAlgorithmReply results;
  results.nextDownloadDelay = delay;
  results.nextRepIndex = tIndexReq;
  results.skip_requestSegment = skip_requestSegment;
  results.rate_group = *pIndexes;

  return results;
}

std::vector<std::vector<int>>
adaptationQmetric::combinatorial (int a_dim, int mpc_future_count)
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

std::vector<std::vector<int>>
adaptationQmetric::combinatorial_Viewpoint (int32_t curViewpoint, int a_dim, int mn_view, int index)
{
  std::vector<std::vector<int>> CHUNK_COMBO_OPTIONS;
  for (auto idx = 0; idx < std::pow (a_dim, mn_view); idx++)
    {
      std::vector<int> vec;
      int j = idx;
      bool filter = true;
      for (auto i = 0; i < mn_view; ++i)
        {
          auto tmp = j % a_dim;
          double vmaf_score = m_videoData[i].vmaf[tmp][index];
          // NS_LOG_INFO(i<<" " <<tmp << " " << vmaf_score << " "<< index);

          if (i == curViewpoint && vmaf_score >= (Qtarget))
            {
              filter = false;
              break;
            }

          else if (i != curViewpoint && vmaf_score >= (Qsubtarget))
            {
              if (tmp != 0)
                {
                  filter = false;
                  break;
                }
            }

          vec.push_back (tmp);
          j /= a_dim;
        }
      if (filter)
        {
          // NS_LOG_INFO ("VEC " << vec[0] << " " << vec[1] << " " << vec[2] << " " << vec[3] << " "
          //                     << vec[4]);

          CHUNK_COMBO_OPTIONS.push_back (vec);
        }
    }
  return CHUNK_COMBO_OPTIONS;
}

int64_t
adaptationQmetric::past_ChunkSize (int idLast, std::vector<int32_t> *pIndexes, int prev_mainView)
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
adaptationQmetric::predict_ChunkSize (bool isGroup, int segmentID, std::vector<int> chunk_quality,
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
              chunk_size += m_videoData[vp].segmentSize[chunk_quality[vp]][segmentID];
            }
        }
      else if (m_reqType == "group_sg")
        {

          chunk_size +=
              m_videoData[curViewpoint].segmentSize[chunk_quality[curViewpoint]][segmentID];
        }
    }
  else
    {
      chunk_size += m_videoData[curViewpoint].segmentSize[chunk_quality[curViewpoint]][segmentID];
    }

  return chunk_size;
}
} // namespace ns3