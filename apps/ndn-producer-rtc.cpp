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

#include "ndn-producer-rtc.hpp"
#include "ns3/log.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"

#include "model/ndn-l3-protocol.hpp"
#include "helper/ndn-fib-helper.hpp"

#include <memory>

NS_LOG_COMPONENT_DEFINE("ndn.ProducerRtc");

namespace ns3 {
namespace ndn {

NS_OBJECT_ENSURE_REGISTERED(ProducerRtc);

TypeId
ProducerRtc::GetTypeId(void)
{
  static TypeId tid =
    TypeId("ns3::ndn::ProducerRtc")
      .SetGroupName("Ndn")
      .SetParent<App>()
      .AddConstructor<ProducerRtc>()
      .AddAttribute("ConferencePrefix", "Conference Prefix", StringValue("/"),
                    MakeNameAccessor(&ProducerRtc::m_conferencePrefix), MakeNameChecker())
      .AddAttribute("ProducerPrefix", "Producer Prefix", StringValue("/"),
                    MakeNameAccessor(&ProducerRtc::m_producerPrefix), MakeNameChecker())
      .AddAttribute("SamplingRate", "Sampling Rate (frames per second)", UintegerValue(30),
                    MakeUintegerAccessor(&ProducerRtc::m_samplingRate),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute(
         "Postfix",
         "Postfix that is added to the output data (e.g., for adding producer-uniqueness)",
         StringValue("/"), MakeNameAccessor(&ProducerRtc::m_postfix), MakeNameChecker())
      .AddAttribute("PayloadSize", "Virtual payload size for Content packets", UintegerValue(1024),
                    MakeUintegerAccessor(&ProducerRtc::m_virtualPayloadSize),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("Freshness", "Freshness of data packets, if 0, then unlimited freshness",
                    TimeValue(Seconds(0)), MakeTimeAccessor(&ProducerRtc::m_freshness),
                    MakeTimeChecker())
      .AddAttribute(
         "Signature",
         "Fake signature, 0 valid signature (default), other values application-specific",
         UintegerValue(0), MakeUintegerAccessor(&ProducerRtc::m_signature),
         MakeUintegerChecker<uint32_t>())
      .AddAttribute("KeyLocator",
                    "Name to be used for key locator.  If root, then key locator is not used",
                    NameValue(), MakeNameAccessor(&ProducerRtc::m_keyLocator), MakeNameChecker())
      .AddAttribute("Filename", "Name of output .csv file", StringValue("default.csv"),
                    MakeStringAccessor(&ProducerRtc::m_filename), MakeStringChecker());
  return tid;
}

ProducerRtc::ProducerRtc()
{
  m_frameId = 0;
  NS_LOG_FUNCTION_NOARGS();
}

// inherited from Application base class.
void
ProducerRtc::StartApplication()
{
  NS_LOG_FUNCTION_NOARGS();
  App::StartApplication();

  m_outputFile.open(m_filename);
  m_outputFile << "Generation Time,Frame Name\n";

  m_samplePeriod = 1.0 / m_samplingRate;
  NS_LOG_INFO("Sampling Rate: " << m_samplingRate);
  NS_LOG_INFO("Sampling Period: " << m_samplePeriod);
  NS_LOG_INFO("Freshness Period: " << m_freshness.GetSeconds());
  FibHelper::AddRoute(GetNode(), Name(m_conferencePrefix.toUri() + m_producerPrefix.toUri()), m_face, 0);
  FibHelper::AddRoute(GetNode(), m_conferencePrefix, m_face, 0);
  Simulator::Schedule(Seconds(m_samplePeriod), &ProducerRtc::GenerateFrame, this);
}

void
ProducerRtc::StopApplication()
{
  NS_LOG_FUNCTION_NOARGS();

  App::StopApplication();

  m_outputFile.close();
}

void
ProducerRtc::GenerateFrame()
{
  // Generate a frame and push it to the queue of generated frames
  Name frameName = Name(m_conferencePrefix.toUri() + m_producerPrefix.toUri());
  frameName.append("delta");
  frameName.appendSequenceNumber(m_frameId);

  NS_LOG_INFO("Generating Frame: " << frameName);
  m_outputFile << Simulator::Now().GetSeconds() << "," << frameName << "\n";

  m_framesGenerated.push_back(frameName);
  m_frameId++;

  // make sure the queue does not get too long...
  if (m_framesGenerated.size() > 100) {
    NS_LOG_INFO("Erase the oldest element of the generated frame queue: " << m_framesGenerated.front());
    m_framesGenerated.erase(m_framesGenerated.begin());
  }

  NS_LOG_INFO("Generated frame queue size: " << m_framesGenerated.size());

  for (auto it = m_framesRequested.begin(); it != m_framesRequested.end(); it++) {
    if (*it == frameName) {
      NS_LOG_INFO("Generated frame has been already requested, sending data packet out");
      SendData(frameName);
      m_framesRequested.erase(it);
      break;
    }
  }
  Simulator::Schedule(Seconds(m_samplePeriod), &ProducerRtc::GenerateFrame, this);
}

void
ProducerRtc::OnInterest(shared_ptr<const Interest> interest)
{
  App::OnInterest(interest); // tracing inside

  NS_LOG_FUNCTION(this << interest);

  if (!m_active)
    return;

  Name interestName = interest->getName();

  if (interestName == m_conferencePrefix && interestName.size() == 1) {
    NS_LOG_INFO("Interest for conference prefix. Sending out latest frame: " << m_framesGenerated.back());
    SendData(m_framesGenerated.back());
    return;
  }

  // sanity check whether the name is already on the queue of requested frames
  for (auto it = m_framesRequested.begin(); it != m_framesRequested.end(); it++) {
    if (*it == interestName) {
      NS_LOG_ERROR("Interest name: " << interestName << " already requested");
      return;
    }
  }
  // check whether frame has alredy been generated
  for (auto it = m_framesGenerated.begin(); it != m_framesGenerated.end(); it++) {
    if (*it == interestName) {
      NS_LOG_INFO("Frame with name: " <<  interestName << " already generated, sending data packet out");
      SendData(interestName);
      return;
    }
  }
  // frame data not generated yet. Insert request to the queue
  NS_LOG_INFO("Frame with name: " <<  interestName << " not generated yet, pushing request to the queue");
  m_framesRequested.push_back(interestName);

  // TODO: Set timeout for a requested frame name in the queue based on the
  // Interest lifetime
}

void
ProducerRtc::SendData(Name& dataName)
{
  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(m_freshness.GetMilliSeconds()));

  data->setContent(make_shared< ::ndn::Buffer>(m_virtualPayloadSize));

  Signature signature;
  SignatureInfo signatureInfo(static_cast< ::ndn::tlv::SignatureTypeValue>(255));

  if (m_keyLocator.size() > 0) {
    signatureInfo.setKeyLocator(m_keyLocator);
  }

  signature.setInfo(signatureInfo);
  signature.setValue(::ndn::makeNonNegativeIntegerBlock(::ndn::tlv::SignatureValue, m_signature));

  data->setSignature(signature);

  NS_LOG_INFO("node(" << GetNode()->GetId() << ") responding with Data: " << data->getName());

  // to create real wire encoding
  data->wireEncode();

  m_transmittedDatas(data, this, m_face);
  m_appLink->onReceiveData(*data);
}

} // namespace ndn
} // namespace ns3
