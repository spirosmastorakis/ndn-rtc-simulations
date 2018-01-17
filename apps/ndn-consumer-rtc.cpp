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

#include "ndn-consumer-rtc.hpp"
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

NS_LOG_COMPONENT_DEFINE("ndn.ConsumerRtc");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ConsumerRtc);

TypeId
ConsumerRtc::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ConsumerRtc")
      .SetGroupName("Ndn")
      .SetParent<Consumer>()
      .AddConstructor<ConsumerRtc>()

      .AddAttribute("MaxSeq", "Maximum sequence number to request",
                    IntegerValue(std::numeric_limits<uint32_t>::max()),
                    MakeIntegerAccessor(&ConsumerRtc::m_seqMax), MakeIntegerChecker<uint32_t>())

      .AddAttribute("MustBeFreshNum", "Interests that will be sent with the MustBeFresh flag on",
                    IntegerValue(1),
                    MakeIntegerAccessor(&ConsumerRtc::m_mustBeFresh), MakeIntegerChecker<uint32_t>())

      .AddAttribute("ConferencePrefix", "Name of the conference", StringValue("/"),
                    MakeNameAccessor(&ConsumerRtc::m_conferencePrefix), MakeNameChecker())

      .AddAttribute("SamplingRate", "Sampling Rate (frames per second)", UintegerValue(30),
                    MakeUintegerAccessor(&ConsumerRtc::m_samplingRate),
                    MakeUintegerChecker<uint32_t>())

      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&ConsumerRtc::m_freshness),
                    MakeTimeChecker())

      .AddAttribute("Filename", "Name of output .csv file", StringValue("default.csv"),
                    MakeStringAccessor(&ConsumerRtc::m_filename), MakeStringChecker())

    ;

  return tid;
}

ConsumerRtc::ConsumerRtc()
  : m_DRD(0)
  , m_lambda(0)
  , m_inFlight(0)
  , m_currentSeqNum(0)
  , m_frames(0)
  , m_bootstrap_done(false)
{
  NS_LOG_FUNCTION_NOARGS();
  m_seqMax = std::numeric_limits<uint32_t>::max();
}

ConsumerRtc::~ConsumerRtc()
{
}

void
ConsumerRtc::StartApplication()
{
  m_outputFile.open(m_filename);
  m_outputFile << "Time,RTT,Darr,Frame Name\n";

  m_samplePeriod = 1.0 / m_samplingRate;

  NS_LOG_FUNCTION_NOARGS();
  NS_LOG_INFO("Interests for fresh data: " << m_mustBeFresh);

  // do base stuff
  App::StartApplication();
  Simulator::Schedule(Seconds(1.0), &ConsumerRtc::SendInitialInterest, this);
}

void
ConsumerRtc::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  // cleanup base stuff
  App::StopApplication();

  m_outputFile.close();
}

void
ConsumerRtc::SendInitialInterest()
{
  shared_ptr<Interest> interest = make_shared<Interest>();
  interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
  interest->setName(m_conferencePrefix);
  time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
  interest->setInterestLifetime(interestLifeTime);
  // Set mustBeFresh selector to true if the consumer has been configured this way
  if (m_mustBeFresh > 0) {
    interest->setMustBeFresh(true);
    m_mustBeFresh--;
  }

  NS_LOG_INFO("> Interest for conference prefix: " << m_conferencePrefix.toUri());

  m_transmittedInterests(interest, this, m_face);
  m_appLink->onReceiveInterest(*interest);
  // record timestamp of initial interest
  m_DRD = Simulator::Now();
  m_inFlight++;
}

void
ConsumerRtc::ScheduleNextPacket()
{
  // Send as many Interests as we can
  int i = 0;
  for (; i < m_lambda - m_inFlight; i++) {
    shared_ptr<Name> nameWithSequence = make_shared<Name>(m_exactDataName);
    m_currentSeqNum++;
    nameWithSequence->appendSequenceNumber(m_currentSeqNum);

    shared_ptr<Interest> interest = make_shared<Interest>(*nameWithSequence);

    interest->setNonce(m_rand->GetValue(0, std::numeric_limits<uint32_t>::max()));
    time::milliseconds interestLifeTime(m_interestLifeTime.GetMilliSeconds());
    interest->setInterestLifetime(interestLifeTime);

    NS_LOG_INFO("> Interest for frame: " << interest->getName());

    m_transmittedInterests(interest, this, m_face);
    m_appLink->onReceiveInterest(*interest);
    m_outstandingInterests.push_back(std::make_pair(interest->getName(), Simulator::Now()));
  }
  m_inFlight += i;
}

void
ConsumerRtc::OnData(shared_ptr<const Data> data)
{
  m_inFlight--;
  Time interArrivalDelay = Seconds(0);
  // Data for initial Interest(s)
  if (!m_bootstrap_done) {
    if (m_lambda != 0)
      interArrivalDelay = this->CheckIfDataFresh();
    else
      m_previousDataArrival = Simulator::Now();

    m_DRD = Simulator::Now() - m_DRD;

    // print to output file
    m_outputFile << Simulator::Now().GetSeconds() <<  "," << m_DRD.GetMilliSeconds() << "," << interArrivalDelay.GetMilliSeconds() << "," << data->getName() << "\n";
    m_outputFile.flush();

    m_lambda = ceil(m_DRD.GetSeconds() / m_samplePeriod);
    NS_LOG_INFO("> Initial Data packet received for: " << data->getName());
    m_exactDataName = data->getName().getPrefix(-1);
    // extract latest sequence number
    uint64_t currentSeqNum = data->getName().at(-1).toSequenceNumber();
    if (m_currentSeqNum < currentSeqNum)
      m_currentSeqNum = currentSeqNum;

    if (m_mustBeFresh > 0) {
      Simulator::Schedule(Seconds(m_freshness.GetSeconds() + 0.001), &ConsumerRtc::SendInitialInterest, this);
      return;
    }
    else {
      NS_LOG_INFO("Estimated request window: " << m_lambda);
      m_bootstrap_done = true;
      m_frames++;
    }
  }
  // Data for subsequent Interests
  else {
    m_frames++;
    for (auto it = m_outstandingInterests.begin(); it != m_outstandingInterests.end(); it++) {
      if (it->first == data->getName()) {
        Time roundtrip = Simulator::Now() - it->second;
        m_DRD = m_DRD + ((roundtrip - m_DRD) / m_frames);
        m_outstandingInterests.erase(it);
        NS_LOG_INFO("> Data packet received for frame: " << data->getName());
        interArrivalDelay = this->CheckIfDataFresh();

        // print to output file
        m_outputFile << Simulator::Now().GetSeconds() <<  "," << roundtrip.GetMilliSeconds() << "," << interArrivalDelay.GetMilliSeconds() << "," << data->getName() << "\n";
        m_outputFile.flush();

        break;
      }
    }
  }

  this->ScheduleNextPacket();
}

void
ConsumerRtc::OnTimeout(uint32_t sequenceNumber)
{
  // do nothing
}

Time
ConsumerRtc::CheckIfDataFresh()
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

} // namespace ndn
} // namespace ns3
