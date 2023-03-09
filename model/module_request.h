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

#ifndef MODULE_REQUEST
#define MODULE_REQUEST

#include "ns3/socket.h"
#include <queue>
#include "ns3/multiview-model.h"
#include "ns3/mvdash_adaptation_algorithm.h"
#include "ns3/adaptation_test.h" // dion

namespace ns3 {
class moduleRequest : public Object
{
public:
  moduleRequest (const t_videoDataGroup &videoData, downloadData &downData,
                 downloadedSegment &downSegment, playbackDataGroup &playData, bufferDataGroup &bufferData,
                 mvdashAdaptationAlgorithm *pAlgorithm, MultiView_Model *pViewModel, std::string reqType);

  virtual st_mvdashRequest *prepareRequest (int tIndexDownload, int m_sendRequestCounter,
                                            mvdashAlgorithmReply *abr) = 0;

  virtual int SendRequest (struct st_mvdashRequest *pMsg, Ptr<Socket> m_socket, bool m_connected,
                           std::queue<st_mvdashRequest> *m_requests, int32_t *m_tIndexReqSent) = 0;

  virtual int readRequest (std::queue<st_mvdashRequest> *m_requests,  int32_t *m_tIndexDownloaded, int32_t m_recvRequestCounter, int64_t timeNow,st_mvdashRequest curSeg)=0;
protected:
  const t_videoDataGroup &m_videoData;
  downloadData &m_downData;
  downloadedSegment &m_downSegment;
  bufferDataGroup &g_bufferData;
  playbackDataGroup &m_playData;
  mvdashAdaptationAlgorithm *m_pAlgorithm;
  MultiView_Model *m_pViewModel;
  std::string m_reqType;
};

} // namespace ns3

#endif /* MODULE_REQUEST */
