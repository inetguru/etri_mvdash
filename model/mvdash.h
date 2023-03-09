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

#ifndef MVDASH_H
#define MVDASH_H

namespace ns3 {

enum requestEvent {
  reqev_reqMsgSent,
  reqev_reqMsgReceived,
  reqev_startReceiving,
  reqev_endReceiving,
  reqev_startTransmit,
  reqev_endTransmit
};

enum segmentEvent {
  segev_startTransmit,
  segev_endTransmit,
  segev_startReceiving,
  segev_endReceiving
};

struct st_mvdashRequest
{
  int32_t id; //request ID
  int32_t viewpoint;
  int32_t timeIndex; //segmentID
  int32_t qualityIndex;
  int32_t segmentSize;
  int32_t group = 1; //Group=1, 0 as single
  st_mvdashRequest (){};
  st_mvdashRequest (int32_t i, int32_t v, int32_t t, int32_t q, int32_t s)
      : id (i), viewpoint (v), timeIndex (t), qualityIndex (q), segmentSize (s){};
};

/*
struct st_mvdashRequestMessage {
    int32_t id;
    int32_t nRequests;
    struct st_mvdashRequest requests[];
};
*/
struct mvdashAlgorithmReply
{
  int64_t
      nextDownloadDelay; //!< delay time in microseconds when the next segment shall be requested from the server
  int64_t
      nextRepIndex; //!< representation level index of the next segement to be downloaded by the client
  int64_t
      decisionTime; //!< time in microsends when the adaptation algorithm decided which segment to download next, only for logging purposes
  int64_t
      decisionCase; //!< indicate in which part of the adaptation algorithm's code the decision was made, which representation level to request next, only for logging purposes
  int64_t
      delayDecisionCase; //!< indicate in which part of the adaptation algorithm's code the decision was made, how much time in microsends to wait until the segment shall be requested from server, only for logging purposes
  int skip_requestSegment; //skip download current segment (used by single request)
  std::vector<int32_t> rate_group; //get quality of n request
};

struct videoData
{
  std::vector<std::vector<int64_t>>
      segmentSize; //!< vector holding representation levels in the first dimension and their particular segment sizes in bytes in the second dimension
  std::vector<double>
      averageBitrate; //!< holding the average bitrate of a segment in representation i in bits
  std::vector<std::vector<double>> vmaf; //!<hold vmaf score
  int64_t segmentDuration; //!< duration of a segment in microseconds
};

typedef std::vector<struct videoData> t_videoDataGroup;

struct playbackDataGroup
{
  std::vector<int32_t> playbackIndex; //!< Index of the video segment
  std::vector<int32_t> mainViewpoint;
  std::vector<int32_t> buffering; //1 or 0
  std::vector<int64_t>
      playbackStart; //!< Point in time in microseconds when playback of this segment started
  std::vector<std::vector<int32_t>> qualityIndex; //!< Index of the video quality
};

struct st_requestTimeInfo
{
  int64_t
      requestSent; //!< Simulation time in microseconds when a segment was requested by the client
  int64_t
      downloadStart; //!< Simulation time in microseconds when the first packet of a segment was received
  int64_t
      downloadEnd; ///!< Simulation time in microseconds when the last packet of a segment was received
};

//For LOG donwload
struct downloadData
{
  std::vector<int32_t> id; //download id
  std::vector<std::vector<int32_t>> playbackIndex; //!< Index of the video segment, should be the primary key
  std::vector<struct st_requestTimeInfo> time;
  std::vector<std::vector<int32_t>> qualityIndex;
  std::vector<int32_t> group; // REQUEST : Group = 1 ,else 0

  // in future it should be a struct to hold multipriority with its probability
  std::vector<int32_t> viewpointPriority; // viewpoint priority
};

//For playback
struct downloadedSegment
{
  std::vector<int32_t> id;
  std::vector<int32_t> redownload; // redownload = 1 ,else 0
  std::vector<std::vector<int32_t>> qualityIndex;
};

struct bufferData
{

  std::vector<int64_t> timeNow; //!< current simulation time
  std::vector<int64_t>
      bufferLevelOld; //!< buffer level in microseconds before adding segment duration (in microseconds) of just downloaded segment
  std::vector<int64_t>
      bufferLevelNew; //!< buffer level in microseconds after adding segment duration (in microseconds) of just downloaded segment
  std::vector<int32_t> segmentID; //information on filled segment --> for single request

  std::vector<int64_t>
      hybrid_bufferLevelOld; //!< buffer level in microseconds after adding segment duration (in microseconds) of just downloaded segment
  std::vector<int64_t>
      hybrid_bufferLevelNew; //!< buffer level in microseconds after adding segment duration (in microseconds) of just downloaded segment
};

typedef std::vector<struct bufferData> bufferDataGroup; //hold bufferData for each viewpoint

// //For Bandwidht management Log
// struct bandwidthManagement{
//   std::vector<int32_t> id;
//   std::vector<double> bandwidth; // downloaded bandwidth
// };

} // namespace ns3

#endif /* MVDASH_H */