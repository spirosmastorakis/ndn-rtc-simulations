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

#include "ndn-consumer-rtc-key-first.hpp"
#include "ns3/ptr.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/callback.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/uinteger.h"
#include "ns3/integer.h"
#include "ns3/double.h"

#include "utils/ndn-ns3-packet-tag.hpp"
#include "utils/ndn-rtt-mean-deviation.hpp"

#include <ndn-cxx/lp/tags.hpp>

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerRtcKeyFirst");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerRtcKeyFirst);

TypeId
ConsumerRtcKeyFirst::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerRtcKeyFirst")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerRtcKeyFirst>()

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&ConsumerRtcKeyFirst::m_seqMax), MakeIntegerChecker<uint32_t>())

      .AddAttribute("MustBeFreshNum", "Interests that will be sent with the MustBeFresh flag on",
                    IntegerValue(1),
                    MakeIntegerAccessor(&ConsumerRtcKeyFirst::m_mustBeFresh), MakeIntegerChecker<uint32_t>())

      .AddAttribute("ConferencePrefix", "Name of the conference", StringValue("/"),
                    MakeNameAccessor(&ConsumerRtcKeyFirst::m_conferencePrefix), MakeNameChecker())

      .AddAttribute("SamplingRate", "Sampling Rate (frames per second)", UintegerValue(30),
                    MakeUintegerAccessor(&ConsumerRtcKeyFirst::m_samplingRate),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&ConsumerRtcKeyFirst::m_freshness),
                    MakeTimeChecker())

      .AddAttribute("Filename", "Name of output .csv file", StringValue("default.csv"),
                    MakeStringAccessor(&ConsumerRtcKeyFirst::m_filename), MakeStringChecker())

      .AddAttribute("FilenameInterarrival", "Name of output .csv file for inter-arrival delays", StringValue("default-interarrival.csv"),
                    MakeStringAccessor(&ConsumerRtcKeyFirst::m_filenameInterarrival), MakeStringChecker())

      .AddAttribute("SegmentsPerDeltaFrame", "Segments per delta frame", UintegerValue(5),
                    MakeUintegerAccessor(&ConsumerRtcKeyFirst::m_segmentsPerDeltaFrame),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("SegmentsPerKeyFrame", "Segments per key frame", UintegerValue(30),
                    MakeUintegerAccessor(&ConsumerRtcKeyFirst::m_segmentsPerKeyFrame),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("PrintLambda", "Tweak the freshness period of the sent data", BooleanValue(false),
                    MakeBooleanAccessor(&ConsumerRtcKeyFirst::m_printLambda),
                    MakeBooleanChecker())

      .AddAttribute("Number", "Number of consumer App", UintegerValue(0),
                    MakeUintegerAccessor(&ConsumerRtcKeyFirst::m_num),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("RTT", "RTT (ms)", UintegerValue(0),
                    MakeUintegerAccessor(&ConsumerRtcKeyFirst::m_rtt_ideal),
                    MakeUintegerChecker<uint32_t>());
    ;

  return tid;
}

ConsumerRtcKeyFirst::ConsumerRtcKeyFirst()
  : m_DRD(0)
  , m_lambda(0)
  , m_inFlightDeltas(0)
  , m_inFlightKeys(0)
  , m_currentDeltaNum(0)
  , m_currentKeyNum(0)
  , m_currentKeyNumForDeltas(0)
  , m_segmentsReceived(0)
  , m_bootstrap_done(false)
  , m_inFlightFrames(0)
  , m_initialKeySegmentReceived(false)
  , globalSeqNum(0)
  , m_boostrapInterests(0)
  , m_initialLambda(0)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
}

ConsumerRtcKeyFirst::~ConsumerRtcKeyFirst()
{
}

void
ConsumerRtcKeyFirst::StartApplication()
{
  m_startTime = Simulator::Now();
  m_outputFile.open(m_filename);
  m_outputFile << "Time,RTT,Frame Name\n";

  m_outputFileInterarrival.open(m_filenameInterarrival);
  m_outputFileInterarrival << "Time,Darr,Frame Name\n";

  m_samplePeriod = 1.0 / m_samplingRate;

  NS_LOG_FUNCTION_NOARGS();
  NS_LOG_INFO("Interests for fresh data: " << m_mustBeFresh);

  // do base stuff
  App::StartApplication();
  Simulator::Schedule(Seconds(0), &ConsumerRtcKeyFirst::SendInitialInterest, this);
}

void
ConsumerRtcKeyFirst::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  // cleanup base stuff
  App::StopApplication();

  m_outputFile.close();
}

void
ConsumerRtcKeyFirst::SendInitialInterest()
{
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  // Name keyName = Name(m_conferencePrefix.getPrefix(-1).toUri() + "/key");
  Name keyName = Name(m_conferencePrefix.getPrefix(-1).toUri() + "/discovery");
  interest->setName(keyName);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);
  // Set mustBeFresh selector to true if the consumer has been configured this way
  if (m_mustBeFresh > 0) {
    interest->setMustBeFresh(true);
    m_mustBeFresh--;
  }

  NS_LOG_INFO("> Interest for conference prefix: " << m_conferencePrefix.toUri());
  WillSendOutInterest(globalSeqNum);
  m_allOutstandingInterests.push_back(std::make_pair(globalSeqNum, interest->getName()));
  globalSeqNum++;
  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
  // record timestamp of initial interest
  m_DRD = Simulator::Now();
  m_firstInterestSent = Simulator::Now();
  // m_inFlightDeltas++;
}

void
ConsumerRtcKeyFirst::ScheduleNextDeltaFrame()
{
  if (m_lambda <= m_inFlightFrames)
    return;
  // Send as many Interests as we can
  int i = 0;
  for (; i < m_lambda - m_inFlightFrames; i++) {
    if (m_currentDeltaNum == 28) {
      m_currentDeltaNum = 0;
      m_currentKeyNumForDeltas++;
    }
    else {
      m_currentDeltaNum++;
    }
    shared_ptr<Name> nameToPrint = make_shared<Name>(m_exactDataName);
    nameToPrint->appendSequenceNumber(m_currentDeltaNum);
    nameToPrint->append("paired-key");
    nameToPrint->appendSequenceNumber(m_currentKeyNumForDeltas);

    NS_LOG_INFO("> Interests for frame: " << nameToPrint->toUri());

    for (int j = 0; j < m_segmentsPerDeltaFrame; j++) {
      shared_ptr<Name> nameWithSequence = make_shared<Name>(m_exactDataName);
      nameWithSequence->appendSequenceNumber(m_currentDeltaNum);
      nameWithSequence->append("paired-key");
      nameWithSequence->appendSequenceNumber(m_currentKeyNumForDeltas);
      nameWithSequence->appendSequenceNumber(j);

      shared_ptr<Interest> interest = make_shared<Interest>(*nameWithSequence);

      interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
      time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
      interest->setInterestLifetime(interestLifeTime);
      WillSendOutInterest(globalSeqNum);
      m_allOutstandingInterests.push_back(std::make_pair(globalSeqNum, interest->getName()));
      globalSeqNum++;
      m_transmittedInterests(interest, this, m_face);
      m_appLink->onReceiveInterest(*interest);
      m_outstandingDeltas.push_back(std::make_pair(interest->getName(), Simulator::Now()));
      m_inFlightDeltas++;
    }
  }
  m_inFlightFrames += i;
}

void
ConsumerRtcKeyFirst::ScheduleSingleDeltaFrame()
{
  if (m_lambda <= m_inFlightFrames)
    return;
  // Send Interests for a single frame
  if (m_currentDeltaNum == 28) {
      m_currentDeltaNum = 0;
      m_currentKeyNumForDeltas++;
  }
  else {
      m_currentDeltaNum++;
  }
  shared_ptr<Name> nameToPrint = make_shared<Name>(m_exactDataName);
  nameToPrint->appendSequenceNumber(m_currentDeltaNum);
  nameToPrint->append("paired-key");
  nameToPrint->appendSequenceNumber(m_currentKeyNumForDeltas);

  NS_LOG_INFO("> Interests for frame: " << nameToPrint->toUri());

  for (int j = 0; j < m_segmentsPerDeltaFrame; j++) {
    shared_ptr<Name> nameWithSequence = make_shared<Name>(m_exactDataName);
    nameWithSequence->appendSequenceNumber(m_currentDeltaNum);
    nameWithSequence->append("paired-key");
    nameWithSequence->appendSequenceNumber(m_currentKeyNumForDeltas);
    nameWithSequence->appendSequenceNumber(j);

    shared_ptr<Interest> interest = make_shared<Interest>(*nameWithSequence);

    interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    interest->setInterestLifetime(interestLifeTime);
    WillSendOutInterest(globalSeqNum);
    m_allOutstandingInterests.push_back(std::make_pair(globalSeqNum, interest->getName()));
    globalSeqNum++;
    m_transmittedInterests(interest, this, m_face);
    m_appLink->onReceiveInterest(*interest);
    m_outstandingDeltas.push_back(std::make_pair(interest->getName(), Simulator::Now()));
    m_inFlightDeltas++;
  }
  m_inFlightFrames += 1;
}

void
ConsumerRtcKeyFirst::ScheduleNextPacket()
{
  ScheduleNextDeltaFrame();
}

void
ConsumerRtcKeyFirst::ScheduleInitialInterests()
{
  int delayFactor = 0;
  for (int i = 0; i < m_lambda - m_inFlightFrames; i++) {
    Simulator::Schedule(Seconds(delayFactor * m_samplePeriod), &ConsumerRtcKeyFirst::ScheduleSingleDeltaFrame, this);
    delayFactor++;
  }
}

void
ConsumerRtcKeyFirst::fetchPreviouslyGeneratedDeltas()
{
  // Retrieve previously generated Deltas
  for (int i = 0; i <= m_currentDeltaNum; i++) {
    Name nameToPrint = m_exactDataName;
    nameToPrint.appendSequenceNumber(i).append("paired-key").appendSequenceNumber(m_currentKeyNumForDeltas);
    NS_LOG_INFO("> Interests for previously generated frame: " << nameToPrint);
    for (int j = 0; j < m_segmentsPerDeltaFrame; j++) {
      shared_ptr<Name> nameWithSequence = make_shared<Name>(m_exactDataName);
      nameWithSequence->appendSequenceNumber(i);
      nameWithSequence->append("paired-key");
      nameWithSequence->appendSequenceNumber(m_currentKeyNumForDeltas);
      nameWithSequence->appendSequenceNumber(j);

      shared_ptr<Interest> interest = make_shared<Interest>(*nameWithSequence);

      interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
      time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
      interest->setInterestLifetime(interestLifeTime);
      WillSendOutInterest(globalSeqNum);
      m_allOutstandingInterests.push_back(std::make_pair(globalSeqNum, interest->getName()));
      globalSeqNum++;
      m_transmittedInterests(interest, this, m_face);
      m_appLink->onReceiveInterest(*interest);
      m_outstandingPreviousDeltas.push_back(std::make_pair(interest->getName(), Simulator::Now()));
      m_inFlightDeltas++;
    }
  }
}

void
ConsumerRtcKeyFirst::fetchCurrentKeyFrame()
{
  shared_ptr<Name> nameWithSequence = make_shared<Name>(m_exactDataName.getPrefix(2));
  nameWithSequence->append("key");
  nameWithSequence->appendSequenceNumber(m_currentKeyNum);
  NS_LOG_INFO("Fetching current key frame: " << nameWithSequence->toUri());
  // fetch the current key frame
  for (int i = 0; i < m_segmentsPerKeyFrame; i++) {
    Name interestName = Name(*nameWithSequence);
    interestName.appendSequenceNumber(i);

    shared_ptr<Interest> interest = make_shared<Interest>(interestName);
    interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    interest->setInterestLifetime(interestLifeTime);
    WillSendOutInterest(globalSeqNum);
    m_allOutstandingInterests.push_back(std::make_pair(globalSeqNum, interest->getName()));
    globalSeqNum++;
    m_transmittedInterests(interest, this, m_face);
    m_appLink->onReceiveInterest(*interest);
    m_outstandingKeys.push_back(std::make_pair(interest->getName(), Simulator::Now()));

    m_inFlightKeys++;
  }
  m_currentKeyNum++;
}

bool
ConsumerRtcKeyFirst::lastSegmentOfFrame(std::vector<std::pair<Name, Time>>::iterator it)
{
  //print pipeline for debugging
  // for (auto iter = m_outstandingDeltas.begin(); iter != m_outstandingDeltas.end(); iter++) {
  //   std::cout << "Pipeline element: " << iter->first.toUri() << ", time:" << iter->second.GetSeconds() << std::endl;
  // }

  if (m_outstandingDeltas.size() == 1) {
    return true;
  }
  auto element_before = it;
  auto element_after = it;
  // segment at the beginning of the pipeline
  if (it == m_outstandingDeltas.begin()) {
    element_after++;
    if (element_after->first.at(2).toUri() == "delta" && element_after->first.at(5) != it->first.at(5))
      return true;
    else if (element_after->first.at(2).toUri() == "delta" && element_after->first.at(3) != it->first.at(3))
      return true;
    else
      return false;
  }
  // segment at the end of the pipeline
  else if (it == m_outstandingDeltas.end() - 1) {
    element_before--;
    if (element_before->first.at(2).toUri() == "delta" && element_before->first.at(5) != it->first.at(5))
      return true;
    else if (element_before->first.at(2).toUri() == "delta" && element_before->first.at(3) != it->first.at(3))
      return true;
    else
      return false;
  }
  // segment not at the beginning or end of pipeline
  else {
    element_before--;
    element_after++;
    if ((element_before->first.at(2).toUri() == "delta" && element_before->first.at(5) != it->first.at(5)) && (element_after->first.at(5).toUri() == "delta" && element_after->first.at(5) != it->first.at(5)))
      return true;
    else if ((element_before->first.at(2).toUri() == "delta" && element_before->first.at(3) != it->first.at(3)) && (element_after->first.at(2).toUri() == "delta" && element_after->first.at(3) != it->first.at(3)))
      return true;
    else
      return false;
  }
  return false;
}

bool
ConsumerRtcKeyFirst::PreviouslyGeneratedDeltaFrameSegmentReceived(shared_ptr<const Data> data)
{
  Time interArrivalDelay = Seconds(0);
  for (auto it = m_outstandingPreviousDeltas.begin(); it != m_outstandingPreviousDeltas.end(); it++) {
    if (it->first == data->getName()) {
      Time roundtrip = Simulator::Now() - it->second;
      //m_DRD = m_DRD + ((roundtrip - m_DRD) / m_segmentsReceived);
      m_outstandingPreviousDeltas.erase(it);
      NS_LOG_INFO("> Data packet received for previous frame segment: " << data->getName());

      // print to output file
      m_outputFile << Simulator::Now().GetSeconds() <<  "," << roundtrip.GetMilliSeconds() << "," << data->getName() << "\n";
      m_outputFile.flush();

      // if (data->getName().at(-1).toSequenceNumber() == 0) {
      //   // interArrival Delay only for first segment of key frame
      //   interArrivalDelay = this->CheckIfDataFresh();
      //   m_outputFileInterarrival << Simulator::Now().GetSeconds() <<  "," << interArrivalDelay.GetMilliSeconds() << "," << data->getName() << "\n";
      //   m_outputFileInterarrival.flush();
      // }
      //m_lambda = ceil(m_DRD.GetSeconds() / m_samplePeriod);
      // std::cerr << "Lambda: " << m_lambda << std::endl;
      return true;
    }
  }
  return false;
}

void
ConsumerRtcKeyFirst::KeyFrameSegmentReceived(shared_ptr<const Data> data)
{
  Time interArrivalDelay = Seconds(0);
  for (auto it = m_outstandingKeys.begin(); it != m_outstandingKeys.end(); it++) {
    if (it->first == data->getName()) {
      Time roundtrip = Simulator::Now() - it->second;
      // m_DRD = m_DRD + ((roundtrip - m_DRD) / m_segmentsReceived);
      m_outstandingKeys.erase(it);
      // print to output file
      m_outputFile << Simulator::Now().GetSeconds() <<  "," << roundtrip.GetMilliSeconds() << "," << data->getName() << "\n";
      m_outputFile.flush();

      // if (data->getName().at(-1).toSequenceNumber() == 0) {
      //   // interArrival Delay only for first segment of key frame
      //   interArrivalDelay = this->CheckIfDataFresh();
      //   m_outputFileInterarrival << Simulator::Now().GetSeconds() <<  "," << interArrivalDelay.GetMilliSeconds() << "," << data->getName() << "\n";
      //   m_outputFileInterarrival.flush();
      // }

      NS_LOG_INFO("> Data packet received for key frame segment: " << data->getName());

      break;
    }
  }
}

void
ConsumerRtcKeyFirst::DeltaFrameSegmentReceived(shared_ptr<const Data> data)
{
  Time interArrivalDelay = Seconds(0);
  m_boostrapInterests++;
  if (m_boostrapInterests == m_initialLambda * 5) {
    Time bootstrapDone = Simulator::Now();
    bootstrapDone = bootstrapDone - m_firstInterestSent;
    std::cerr << "Consumer " << m_num << " :Initial lambda: " << m_initialLambda << "\n";
    std::cerr << "Consumer " << m_num << " :Bootstrap time (total): " << bootstrapDone.GetMilliSeconds() << " ms\n";
    std::cerr << "Consumer " << m_num << " :Bootstrap time: " << (1.0 * bootstrapDone.GetMilliSeconds()) / m_rtt_ideal << " xRTT" << std::endl;
  }
  for (auto it = m_outstandingDeltas.begin(); it != m_outstandingDeltas.end(); it++) {
    if (it->first == data->getName()) {
      Time roundtrip = Simulator::Now() - it->second;
      m_DRD = m_DRD + ((roundtrip - m_DRD) / m_segmentsReceived);

      // check if that was the last segment of this frame
      bool lastSegment = lastSegmentOfFrame(it);
      //std::cerr << "Frame name: " << it->first.toUri() << " , result: " << lastSegment << std::endl;
      if (lastSegment)
        m_inFlightFrames--;

      m_outstandingDeltas.erase(it);
      NS_LOG_INFO("> Data packet received for frame segment: " << data->getName());

      // print to output file
      m_outputFile << Simulator::Now().GetSeconds() <<  "," << roundtrip.GetMilliSeconds() << "," << data->getName() << "\n";
      m_outputFile.flush();

      if (data->getName().at(-1).toSequenceNumber() == 0) {
        // interArrival Delay only for first segment of key frame
        interArrivalDelay = this->CheckIfDataFresh();
        m_outputFileInterarrival << Simulator::Now().GetSeconds() <<  "," << interArrivalDelay.GetMilliSeconds() << "," << data->getName() << "\n";
        m_outputFileInterarrival.flush();
      }
      m_lambda = ceil(m_DRD.GetSeconds() / m_samplePeriod);
      if (m_printLambda)
        std::cerr << "Lambda: " << m_lambda << std::endl;
      break;
    }
  }
}

void
ConsumerRtcKeyFirst::OnData(shared_ptr<const Data> data)
{
  NS_LOG_FUNCTION_NOARGS();
  uint32_t seq = -1;

  for (auto it = m_allOutstandingInterests.begin(); it != m_allOutstandingInterests.end(); it++) {
    if (it->second.toUri() == data->getName().toUri()) {
      seq = it->first;
      break;
    }
    if (data->getName().at(2).toUri() == "discovery" && it->second.at(2).toUri() == "discovery") {
      seq = it->first;
      NS_LOG_INFO("> Discovery Interest Timeout cancelled ");
      break;
    }
  }

  int hopCount = 0;
  auto hopCountTag = data->getTag<lp::HopCountTag>();
  if (hopCountTag != nullptr) { // e.g., packet came from local node's cache
    hopCount = *hopCountTag;
  }

  if (seq != -1)
    this->CancelTimers(seq, hopCount);
  else
    NS_LOG_INFO("> Could not find retransmission timers for name: " << data->getName());

  // Data for initial Interest(s)
  if (!m_bootstrap_done) {
    Time interArrivalDelay = Seconds(0);
    if (m_lambda != 0)
      interArrivalDelay = this->CheckIfDataFresh();
    else
      m_previousDataArrival = Simulator::Now();

    m_DRD = Simulator::Now() - m_DRD;

    // print to output file
    m_outputFile << Simulator::Now().GetSeconds() <<  "," << m_DRD.GetMilliSeconds() << "," << data->getName() << "\n";
    m_outputFile.flush();

    m_outputFileInterarrival << Simulator::Now().GetSeconds() <<  "," << interArrivalDelay.GetMilliSeconds() << "," << data->getName() << "\n";
    m_outputFileInterarrival.flush();

    m_lambda = ceil(m_DRD.GetSeconds() / m_samplePeriod);
    m_initialLambda = m_lambda;
    NS_LOG_INFO("> Initial Data packet received for: " << data->getName());
    m_exactDataName = Name(data->getName().getPrefix(-3).toUri() + "/delta") ;
    // extract latest sequence number
    uint64_t currentDeltaNum = data->getName().at(-1).toSequenceNumber();
    uint64_t currentKeyNum = data->getName().at(-2).toSequenceNumber();
    if (m_currentDeltaNum <= currentDeltaNum && m_currentKeyNum <= currentKeyNum) { // && m_startTime.GetSeconds() > 1.5) {
      m_currentDeltaNum = 0;
      m_currentKeyNum = currentKeyNum + 1;
      m_currentKeyNumForDeltas = currentKeyNum + 1;
      m_initialKeyFrameId = m_currentKeyNum;
    }
    // else if (m_currentDeltaNum <= currentDeltaNum && m_currentKeyNum <= currentKeyNum && m_startTime.GetSeconds() <= 1.5) {
    //   m_currentDeltaNum = currentDeltaNum;
    //   m_currentKeyNum = currentKeyNum;
    //   m_currentKeyNumForDeltas = currentKeyNum;
    //   m_initialKeyFrameId = m_currentKeyNum;
    // }

    if (m_mustBeFresh > 0) {
      Simulator::Schedule(Seconds(m_freshness.GetSeconds() + 0.001), &ConsumerRtcKeyFirst::SendInitialInterest, this);
      return;
    }
    else {
      NS_LOG_INFO("Estimated request window: " << m_lambda);
      m_bootstrap_done = true;
      m_segmentsReceived++;
      fetchCurrentKeyFrame();
      if (m_startTime.GetSeconds() <= 1.5) {
        fetchPreviouslyGeneratedDeltas();
      }
      this->ScheduleNextPacket();
      //this->ScheduleInitialInterests();
      // m_lambda = ceil(m_DRD.GetSeconds() / m_samplePeriod);
      // Simulator::Schedule(Seconds(m_samplePeriod), &ConsumerRtcKeyFirst::ScheduleNextDeltaFrame, this);
    }
  }
  // Data for subsequent Interests
  else {
    NS_LOG_INFO("Bootstrap done");
    // key frame segment received
    if (data->getName().at(2).toUri() == "key") {
      KeyFrameSegmentReceived(data);
      NS_LOG_INFO("Key Received");
      if (m_initialKeyFrameId == data->getName().at(3).toSequenceNumber() && !m_initialKeySegmentReceived) {
        this->ScheduleNextPacket();
        //Simulator::Schedule(Seconds(m_samplePeriod), &ConsumerRtcKeyFirst::ScheduleNextDeltaFrame, this);
        m_initialKeySegmentReceived = true;
      }
      if (m_outstandingKeys.size() == 0) {
        // fetch next key frame when generated
        fetchCurrentKeyFrame();
      }
    }
    else {
      // check if there are outstanding Interests for segments that were previously generated
      bool foundMatchInPreviousDeltas = false;
      if (m_outstandingPreviousDeltas.size() != 0) {
        NS_LOG_INFO("Checking previously generated deltas");
        foundMatchInPreviousDeltas = PreviouslyGeneratedDeltaFrameSegmentReceived(data);
      }
      if (!foundMatchInPreviousDeltas) {
        // regular delta frame segment received
        m_segmentsReceived++;
        DeltaFrameSegmentReceived(data);
        this->ScheduleNextPacket();
        // Simulator::Schedule(Seconds(m_samplePeriod), &ConsumerRtcKeyFirst::ScheduleNextDeltaFrame, this);
      }
    }
  }
}

void
ConsumerRtcKeyFirst::OnTimeout(uint32_t sequenceNumber)
{
  for (auto it = m_allOutstandingInterests.begin(); it != m_allOutstandingInterests.end(); it++) {
    if (it->first == sequenceNumber) {

      m_rtt->IncreaseMultiplier(); // Double the next RTO
      m_rtt->SentSeq(SequenceNumber32(sequenceNumber),
                 1); // make sure to disable RTT calculation for this sample
      m_retxSeqs.insert(sequenceNumber);

      NS_LOG_INFO("Timeout for name: " << it->second.toUri());
      this->SendPacketAgain(it->second, sequenceNumber);
      break;
    }
  }
}

void
ConsumerRtcKeyFirst::SendPacketAgain(Name interestName, uint32_t sequenceNumber)
{
  shared_ptr<Interest> interest = make_shared<Interest>(interestName);
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);
  WillSendOutInterest(sequenceNumber);
  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
}

Time
ConsumerRtcKeyFirst::CheckIfDataFresh()
{
  // check if we received cached data (bursty data arrival) or the inter-arrival delay is close to the sample period
  Time interArrivalDelay = Simulator::Now() - m_previousDataArrival;
  m_previousDataArrival = Simulator::Now();
  NS_LOG_INFO("Inter arrival delay: " << interArrivalDelay.GetSeconds());
  if (interArrivalDelay.GetSeconds() >= (80 * m_samplePeriod / 100))
    NS_LOG_INFO("Catching up with the producer...");
  else
    NS_LOG_INFO("Probably received cached data...");

  return interArrivalDelay;
}

void
ConsumerRtcKeyFirst::CancelTimers(uint32_t seq, uint32_t hopCount)
{
  SeqTimeoutsContainer::iterator entry = m_seqLastDelay.find(seq);
  if (entry != m_seqLastDelay.end()) {
    m_lastRetransmittedInterestDataDelay(this, seq, Simulator::Now() - entry->time, hopCount);
  }

  entry = m_seqFullDelay.find(seq);
  if (entry != m_seqFullDelay.end()) {
    m_firstInterestDataDelay(this, seq, Simulator::Now() - entry->time, m_seqRetxCounts[seq], hopCount);
  }

  m_seqRetxCounts.erase(seq);
  m_seqFullDelay.erase(seq);
  m_seqLastDelay.erase(seq);

  m_seqTimeouts.erase(seq);
  m_retxSeqs.erase(seq);

  m_rtt->AckSeq(SequenceNumber32(seq));
}

} // namespace ndn
} // namespace ns3
