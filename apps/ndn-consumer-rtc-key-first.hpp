/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2011-2015  Regents of the University of California.
 *
 * This file is part of ndnSIM. See AUTHORS for complete list of ndnSIM authors and
 * contributors.
 *
 * ndnSIM is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * ndnSIM is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * ndnSIM, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 **/

#ifndef NDN_CONSUMER_RTC_KEY_FIRST_H
#define NDN_CONSUMER_RTC_KEY_FIRST_H

#include "ns3/ndnSIM/model/ndn-common.hpp"

#include "ndn-consumer.hpp"

#include <fstream>

namespace ns3 {
namespace ndn {

/**
 * @ingroup ndn-apps
 * @brief Ndn application for RTC consumer
 */
class ConsumerRtcKeyFirst : public Consumer {
public:
  static TypeId
  GetTypeId();

  /**
   * \brief Default constructor
   * Sets up randomizer function and packet sequence number
   */
  ConsumerRtcKeyFirst();
  virtual ~ConsumerRtcKeyFirst();

  // From App
  virtual void
  OnData(shared_ptr<const Data> contentObject);

  virtual void
  OnTimeout(uint32_t sequenceNumber);

protected:
  /**
   * \brief Constructs the Interest packet for an upcoming frame and sends it
   * using a callback to the underlying NDN protocol
   */
  virtual void
  ScheduleNextPacket();

  // from App
  virtual void
  StartApplication();

  virtual void
  StopApplication(); // Called at time specified by Stop

  /**
   * @brief Set type of frequency randomization
   * @param value Either 'none', 'uniform', or 'exponential'
   */
  void
  SetRandomize(const std::string& value);

  /**
   * @brief Get type of frequency randomization
   * @returns either 'none', 'uniform', or 'exponential'
   */
  std::string
  GetRandomize() const;

private:
  /**
   * @brief Send first interest with conference prefix
   */
  void
  SendInitialInterest();

  Time
  CheckIfDataFresh();

  void
  fetchCurrentKeyFrame();

  void
  fetchPreviouslyGeneratedDeltas();

  void
  ScheduleNextDeltaFrame();

  bool
  PreviouslyGeneratedDeltaFrameSegmentReceived(shared_ptr<const Data> data);

  void
  KeyFrameSegmentReceived(shared_ptr<const Data> data);

  void
  DeltaFrameSegmentReceived(shared_ptr<const Data> data);

  bool
  lastSegmentOfFrame(std::vector<std::pair<Name, Time>>::iterator it);

  void
  ScheduleSingleDeltaFrame();

  void
  ScheduleInitialInterests();

  void
  CancelTimers(uint32_t seq, uint32_t hopCount);

  void
  SendPacketAgain(Name interestName, uint32_t sequenceNumber);

protected:
  double m_frequency; // Frequency of interest packets (in hertz)
  bool m_firstTime;
  Ptr<RandomVariableStream> m_random;
  std::string m_randomType;
  uint32_t m_mustBeFresh;
  Name m_conferencePrefix;

  Time m_DRD;
  uint32_t m_lambda;
  uint32_t m_inFlightDeltas;
  uint32_t m_inFlightKeys;

  uint32_t m_samplingRate;
  float m_samplePeriod;

  uint64_t m_currentDeltaNum;
  uint64_t m_currentKeyNum;
  uint64_t m_currentKeyNumForDeltas;

  Name m_exactDataName;

  std::vector<std::pair<Name, Time>> m_outstandingDeltas;
  std::vector<std::pair<Name, Time>> m_outstandingKeys;
  std::vector<std::pair<Name, Time>> m_outstandingPreviousDeltas;
  uint64_t m_segmentsReceived;

  bool m_printLambda;

  bool m_bootstrap_done;

  Time m_previousDataArrival;

  Time m_freshness;
  std::string m_filename;
  std::ofstream m_outputFile;

  std::string m_filenameInterarrival;
  std::ofstream m_outputFileInterarrival;

  uint32_t m_segmentsPerDeltaFrame;
  uint32_t m_segmentsPerKeyFrame;

  uint32_t m_inFlightFrames;

  uint32_t m_initialKeyFrameId;

  bool m_initialKeySegmentReceived;

  Time m_startTime;

  uint32_t globalSeqNum;
  std::vector<std::pair<uint32_t, Name>> m_allOutstandingInterests;

  uint32_t m_boostrapInterests;

  Time m_firstInterestSent;
  uint32_t m_num;
  uint32_t m_initialLambda;
  uint32_t m_rtt_ideal;
};

} // namespace ndn
} // namespace ns3

#endif
