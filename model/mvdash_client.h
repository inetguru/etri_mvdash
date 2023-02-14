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

#ifndef MVDASH_CLIENT_H
#define MVDASH_CLIENT_H

#include "ns3/application.h"
#include "ns3/ptr.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include <queue>
#include "multiview-model.h"
#include "mvdash_adaptation_algorithm.h"
#include "mvdash.h"

namespace ns3 {

class Address;
class Socket;
class Packet;

/**
  * \brief This enum is used to define the states of the state machine which controls the behaviour of the client.
  */
enum controllerState { initial, downloading, downloadingPlaying, playing, terminal };

/**
  * \brief This enum is used to define the controller events of the state machine which controls the behaviour of the client.
  */
enum controllerEvent { downloadFinished, playbackFinished, irdFinished, init, viewChange };

enum controllerTraceEvent {
  cteSendRequest,
  cteDownloaded,
  cteAllDownloaded,
  cteStartPlayback,
  cteEndPlayback,
  cteBufferUnderrun
};

class mvdashClient : public Application
{
public:
  static TypeId GetTypeId (void);
  mvdashClient ();
  virtual ~mvdashClient ();
  void Initialize (void);

  uint32_t m_simId;
  uint32_t m_clientId;

  /**
   * Callback signature for `RequestTrace` trace source.
   * \param client Pointer to this instance of mvdashClient, which is where
   *                   the trace originated.
   * \param ev request event id.
   */
  typedef void (*RequestEventCallback) (Ptr<const mvdashClient> client, requestEvent ev,
                                        int32_t tid);
  /**
   * Callback signature for `SegmentTrace` trace source.
   * \param client Pointer to this instance of mvdashClient, which is where
   *                   the trace originated.
   * \param ev request event id.
   */
  typedef void (*SegmentEventCallback) (Ptr<const mvdashClient> client, segmentEvent ev,
                                        st_mvdashRequest sinfo);
  /**
   * Callback signature for `ControllerTrace` trace source.
   * \param client Pointer to this instance of mvdashClient, which is where
   *                   the trace originated.
   * \param ev request event id.
   */
  typedef void (*ControllerEventCallback) (Ptr<const mvdashClient> client, controllerState state,
                                           controllerTraceEvent ev, int32_t tid);

protected:
  virtual void DoDispose (void);

private:
  bool Req_buffering = false; //playback tracing : buffering or not
  bool Req_Type = false; // hybrid true, group false
  bool Req_single = false; //group request is pending or not
  int Req_m_tIndexReqSent; //Segment index
  int32_t Req_m_tIndexDownloaded; //Segment index

  int32_t indexDownload; //helper for single/group
  int32_t segment_LastBuffer;
  // inherited from Application base class.
  virtual void StartApplication (void); // Called at time specified by Start
  virtual void StopApplication (void); // Called at time specified by Stop

  /**
 * @brief Hybrid Request canceling
 * 
 *  case 1: current playback is the last buffered segment, continue group request
 *  case 2: when asking segment n+2 because not enough time to download n+1 >0 quality, but n+2 is not buffered yet
 */
  bool Req_HybridCanceling ();

  /**
 * @brief Hybrid request segment selection'
 * Due to not enough time to download choosed segment, skip to n+1
 * 
 * @return int 
 */
  int Req_HybridSegmentSelection ();

  /**
   * \brief Handle a packet received by the application
   * \param socket the receiving socket
   */
  void HandleRead (Ptr<Socket> socket);
  /**
   * \brief Connection Succeeded (called by Socket through a callback)
   * \param socket the connected socket
   */
  void ConnectionSucceeded (Ptr<Socket> socket);
  /**
   * \brief Connection Failed (called by Socket through a callback)
   * \param socket the connected socket
   */
  void ConnectionFailed (Ptr<Socket> socket);

  struct st_mvdashRequest *PrepareRequest (int tIndexDownload);
  int SendRequest (struct st_mvdashRequest *pMsg, int nReq);

  /**
 * @brief 
  * In the Request -> Response system, the most important part is m_downData.time
  * Because single request is redownload the previous segment, we only consider the segment index
  * and to update the quality of the downloaded segment
 * @param pMsg Mvdash request
 * @return int 
 */
  int Hybrid_SendRequest (struct st_mvdashRequest *pMsg); //DION Test

  /**
 * @brief create mvdash request for single messasge
 * 
 * @param tIndexReq Segment index
 * @return struct st_mvdashRequest* 
 */
  struct st_mvdashRequest *Hybrid_PrepareRequest (int tIndexDownload); //DION Test

  bool StartPlayback (void);

  void Controller (controllerEvent event);
  /**
   * \brief Read in bitrate values
   *
   */

  int ReadInBitrateValues (std::string segmentSizeFile);

  /**
   * @brief Calculate PSNR
   * 
   * @param segmentSizeFile 
   * @return int 
   */

  void read_VMAF ();
  void LogPlayback (void);
  void LogDownload (void);
  void LogBuffer (void);
  void SelectRateIndexes (int tIndexReq, std::vector<int32_t> *pIndexes);

  Ptr<Socket> m_socket; //!< Socket
  bool m_connected; //!< True if connected
  Address m_serverAddress; //!< Server address

  std::string m_vpInfoFilePath;
  std::string m_vpModelName;
  std::string m_mvInfoFilePath;
  std::string m_mvAlgoName;
  std::string m_reqType;

  controllerState m_state;

  st_mvdashRequest *pReq;
  int64_t m_delay; //delay due to buffer control
  int32_t m_tIndexLast; //last index by reading video files
  int32_t m_tIndexPlay;
  int32_t m_tIndexReqSent;
  int32_t m_tIndexDownloaded; //count downloaded segment
  int32_t m_bytesReceived;
  int32_t m_sendRequestCounter;
  int32_t m_recvRequestCounter;
  int skip_requestSegment; //skip request if not enough time to download > 0 quality
  bool m_segStarted;

  // int request_prevType; // group : 1, single : 0

  int32_t m_nViewpoints;

  MultiView_Model *m_pViewModel;
  mvdashAdaptationAlgorithm *m_pAlgorithm;

  //std::vector <videoData> m_videoData;
  t_videoDataGroup m_videoData;
  struct downloadData m_downData;
  struct downloadedSegment m_downSegment; //TEST --> Playback (merging group & single request)
  struct playbackDataGroup m_playData;
  // struct bufferData m_bufferData;

  bufferDataGroup g_bufferData;

  std::vector<int64_t> m_timeReqSent;
  std::queue<st_mvdashRequest> m_requests;

  // std::queue <st_mvdashRequest> m_pRequests;

  /// Traced Callback: The "RequestMessage" trace source
  TracedCallback<Ptr<const mvdashClient>, controllerState, controllerTraceEvent, int32_t>
      m_ctrlTrace;

  /// Traced Callback: The "RequestMessage" trace source
  TracedCallback<Ptr<const mvdashClient>, requestEvent, int32_t> m_reqTrace;
  /// Traced Callback: The "Segment" trace source
  TracedCallback<Ptr<const mvdashClient>, segmentEvent, st_mvdashRequest> m_segTrace;

  /// Traced Callback: received packets.
  TracedCallback<Ptr<const mvdashClient>, Ptr<const Packet>> m_rxTrace;
  /// Traced Callback: sent packets
  TracedCallback<Ptr<const mvdashClient>, Ptr<const Packet>> m_txTrace;
};

} // namespace ns3

#endif /* MVDASH_CLIENT_H */