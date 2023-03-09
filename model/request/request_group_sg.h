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
#ifndef REQUEST_GROUP_SG
#define REQUEST_GROUP_SG

#include "ns3/module_request.h"

namespace ns3 {
class requestGroupSg : public moduleRequest
{
public:
  requestGroupSg (const t_videoDataGroup &videoData, downloadData &downData,
                  downloadedSegment &downSegment, playbackDataGroup &playData,
                  bufferDataGroup &bufferData, mvdashAdaptationAlgorithm *pAlgorithm,
                  MultiView_Model *pViewModel, std::string reqType);

  int32_t segment_singleCounter; //donwload index during single request
  int32_t segment_lastGroup; //last segment of group request, to stop pending
  int32_t pending;

  struct st_mvdashRequest *prepareRequest (int tIndexDownload, int m_sendRequestCounter,
                                           mvdashAlgorithmReply *abr);
  int SendRequest (struct st_mvdashRequest *pMsg, Ptr<Socket> m_socket, bool m_connected,
                   std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexReqSent);
  int readRequest (std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexDownloaded,
                   int32_t m_recvRequestCounter, int64_t timeNow, st_mvdashRequest curSeg);

protected:
private:
  int32_t n_requests;
  int32_t m_nViewpoint;
  std::queue<st_mvdashRequest> readSegment_temp;
};
} // namespace ns3

#endif /* REQUEST_GROUP_SG */
