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
#include "ns3/boolean.h"

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
      .AddAttribute("SegmentsPerDeltaFrame", "Segments per delta frame", UintegerValue(5),
                    MakeUintegerAccessor(&ProducerRtc::m_segmentsPerDeltaFrame),
                    MakeUintegerChecker<uint32_t>())
      .AddAttribute("SegmentsPerKeyFrame", "Segments per key frame", UintegerValue(30),
                    MakeUintegerAccessor(&ProducerRtc::m_segmentsPerKeyFrame),
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
      .AddAttribute("Filename", "Name of output .csv file", StringValue("default-producer.csv"),
                    MakeStringAccessor(&ProducerRtc::m_filename), MakeStringChecker())
      .AddAttribute("TweakFreshness", "Tweak the freshness period of the sent data", BooleanValue(false),
                    MakeBooleanAccessor(&ProducerRtc::m_tweakFreshness),
                    MakeBooleanChecker());
  return tid;
}

ProducerRtc::ProducerRtc()
{
  m_frameId = 0;
  m_keyFrameId = 0;
  m_keyFrameId--;
  m_deltaFrameId = 0;
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

Name
ProducerRtc::GenerateKeyFrame()
{
  m_keyFrameId++;
  Name frameName = Name(m_conferencePrefix.toUri() + m_producerPrefix.toUri());
  frameName.append("key");

  frameName.appendSequenceNumber(m_keyFrameId);

  m_deltaFrameId = 0;

  NS_LOG_INFO("Generating Key Frame: " << frameName);

  Name tempFrameName = frameName;

  for (int i = 0; i < m_segmentsPerKeyFrame; i++) {
    frameName.appendSequenceNumber(i);
    m_framesGenerated.push_back(frameName);
    m_outputFile << Simulator::Now().GetSeconds() << "," << frameName << "\n";
    m_outputFile.flush();
    frameName = tempFrameName;
  }

  return tempFrameName;
}

Name
ProducerRtc::GenerateDeltaFrame()
{
  Name frameName = Name(m_conferencePrefix.toUri() + m_producerPrefix.toUri());
  frameName.append("delta");

  frameName.appendSequenceNumber(m_deltaFrameId);

  frameName.append("paired-key");
  frameName.appendSequenceNumber(m_keyFrameId);

  m_deltaFrameId++;

  NS_LOG_INFO("Generating Delta Frame: " << frameName);

  Name tempFrameName = frameName;

  for (int i = 0; i < m_segmentsPerDeltaFrame; i++) {
    frameName.appendSequenceNumber(i);
    m_framesGenerated.push_back(frameName);
    m_outputFile << Simulator::Now().GetSeconds() << "," << frameName << "\n";
    m_outputFile.flush();
    frameName = tempFrameName;
  }

  return tempFrameName;
}

void
ProducerRtc::GenerateFrame()
{
  // Generate a frame and push it to the queue of generated frames

  // decide whether to generate a key or delta frame
  bool key_frame = false;
  Name frameName;
  if (m_frameId % m_samplingRate == 0) {
    key_frame = true;
    frameName = GenerateKeyFrame();
  }
  else {
    frameName = GenerateDeltaFrame();
  }

  m_frameId++;

  // make sure the queue does not get too long...
  // if (m_framesGenerated.size() > 1000) {
  //   NS_LOG_INFO("Erase the 5 first elements of the generated frame queue");
  //   for (int i = 1; i <= 5; i++) {
  //     NS_LOG_INFO("Element " << i << ": " <<  m_framesGenerated.front());
  //     m_framesGenerated.erase(m_framesGenerated.begin());
  //   }
  // }

  int segments_found = 0;
  std::vector<int> elementsToDelete;
  int elementIndex = 0;
  for (auto it = m_framesRequested.begin(); it != m_framesRequested.end(); it++) {
    if (frameName.isPrefixOf(*it)) {
      elementsToDelete.push_back(elementIndex);
      segments_found++;
      NS_LOG_INFO("Generated frame has been already requested, sending data packet out: " << *it);
      SendData(*it, m_freshness);
      if (key_frame) {
        if (segments_found == m_segmentsPerKeyFrame)
          break;
      }
      else {
        if (segments_found == m_segmentsPerDeltaFrame)
          break;
      }
    }
    elementIndex++;
  }

  // Erase elements from vector in a safe way
  for (int i = 0; i < elementsToDelete.size(); i++) {
    m_framesRequested.erase(m_framesRequested.begin() + elementsToDelete[i] - i);
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

  // Received Interest for exploration in delta namespace
  if (interestName == Name(m_conferencePrefix.toUri() + m_producerPrefix.toUri() + "/delta") && interestName.size() == 3) {
    Name frameToSend = m_framesGenerated.back();
    for (auto it = m_framesGenerated.end() - 1; it != m_framesGenerated.begin(); it--) {
      if (it->at(2).toUri() == "delta") {
        frameToSend = it->getPrefix(-1);
        frameToSend.appendSequenceNumber(0);
        break;
      }
    }
    NS_LOG_INFO("Interest for conference prefix in delta namespace. Sending out latest delta frame: " << frameToSend);
    SendData(frameToSend, m_freshness);
    return;
  }

  // Received Interest for exploration in key namespace
  if (interestName == Name(m_conferencePrefix.toUri() + m_producerPrefix.toUri() + "/key") && interestName.size() == 3) {
    Name frameToSend = m_framesGenerated.back();
    for (auto it = m_framesGenerated.end() - 1; it != m_framesGenerated.begin(); it--) {
      if (it->at(2).toUri() == "key") {
        frameToSend = it->getPrefix(-1);
        frameToSend.appendSequenceNumber(0);
        frameToSend.appendSequenceNumber(m_deltaFrameId);
        break;
      }
    }
    NS_LOG_INFO("Interest for conference prefix in key namespace. Sending out latest key frame: " << frameToSend);
    SendData(frameToSend, m_freshness);
    return;
  }

  // Received Interest for exploration in key namespace
  if (interestName == Name(m_conferencePrefix.toUri() + m_producerPrefix.toUri() + "/discovery") && interestName.size() == 3) {
    Name frameToSend = m_framesGenerated.back();
    for (auto it = m_framesGenerated.end() - 1; it != m_framesGenerated.begin(); it--) {
      if (it->at(2).toUri() == "key") {
        frameToSend = interestName;
        frameToSend.appendSequenceNumber(it->at(3).toSequenceNumber());
        frameToSend.appendSequenceNumber(m_deltaFrameId);
        break;
      }
    }
    NS_LOG_INFO("Interest for conference prefix in discovery namespace. Sending out latest key frame: " << frameToSend);
    Time t = MilliSeconds(90);
    SendData(frameToSend, t);
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
      NS_LOG_INFO("Frame segment with name: " <<  interestName << " already generated, sending data packet out");
      if (!m_tweakFreshness) {
        NS_LOG_INFO("Interest with regular lifetime");
        SendData(interestName, m_freshness);
        return;
      }
      if (interestName.at(2).toUri() == "key")
        SendData(interestName, m_freshness);
      else if (m_deltaFrameId == 0) {
        if (interestName.at(3).toSequenceNumber() == 28)
          SendData(interestName, m_freshness);
        else {
          NS_LOG_INFO("Data with 0 ms freshness");
          Time t = MilliSeconds(0);
          SendData(interestName, t);
        }
      }
      else if ((m_deltaFrameId - 1) == interestName.at(3).toSequenceNumber())
        SendData(interestName, m_freshness);
      else {
        NS_LOG_INFO("Data with 0 ms freshness");
        Time t = MilliSeconds(0);
        SendData(interestName, t);
      }
      return;
    }
  }
  // frame data not generated yet. Insert request to the queue
  NS_LOG_INFO("Frame segment with name: " <<  interestName << " not generated yet, pushing request to the queue");
  m_framesRequested.push_back(interestName);

}

void
ProducerRtc::SendData(Name& dataName, Time freshness)
{
  auto data = make_shared<Data>();
  data->setName(dataName);
  data->setFreshnessPeriod(::ndn::time::milliseconds(freshness.GetMilliSeconds()));
  NS_LOG_INFO("Freshness: " << freshness.GetMilliSeconds() << " ms");

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
