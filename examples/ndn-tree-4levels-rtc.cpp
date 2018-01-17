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

// ndn-simple.cpp

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

namespace ns3 {

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("100Mbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::QueueBase::MaxPackets", UintegerValue(20));

  double freshness = 0.1;
  uint32_t fresh_data_num = 1;
  uint32_t sampling_rate = 30;

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
	cmd.AddValue("freshness", "Freshness of producer data", freshness);
	cmd.AddValue("fresh-data-num", "Number of fresh data packets to be retrieved", fresh_data_num);
  cmd.AddValue("rate", "Sampling Rate", sampling_rate);
  cmd.Parse(argc, argv);

  // Creating nodes
  NodeContainer nodes;
  nodes.Create(15);

  // Create a binary tree of 4 levels
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(0), nodes.Get(2));
  p2p.Install(nodes.Get(1), nodes.Get(3));
  p2p.Install(nodes.Get(1), nodes.Get(4));
  p2p.Install(nodes.Get(2), nodes.Get(5));
  p2p.Install(nodes.Get(2), nodes.Get(6));
  // leaves
  p2p.Install(nodes.Get(3), nodes.Get(7));
  p2p.Install(nodes.Get(3), nodes.Get(8));
  p2p.Install(nodes.Get(4), nodes.Get(9));
  p2p.Install(nodes.Get(4), nodes.Get(10));
  p2p.Install(nodes.Get(5), nodes.Get(11));
  p2p.Install(nodes.Get(5), nodes.Get(12));
  p2p.Install(nodes.Get(6), nodes.Get(13));
  p2p.Install(nodes.Get(6), nodes.Get(14));

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/conference", "/localhost/nfd/strategy/best-route");

  // Installing applications

  // Consumer Rtc at leaves
  ndn::AppHelper consumerHelper1("ns3::ndn::ConsumerRtc");
  consumerHelper1.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper1.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper1.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper1.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper1.SetAttribute("Filename", StringValue("consumer1.csv"));
  ApplicationContainer consumer1 = consumerHelper1.Install(nodes.Get(7));
  consumer1.Start(Seconds(0)); // start consumer at 0s

  ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerRtc");
  consumerHelper2.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper2.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper2.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper2.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper2.SetAttribute("Filename", StringValue("consumer2.csv"));
  ApplicationContainer consumer2 = consumerHelper2.Install(nodes.Get(8));
  consumer2.Start(Seconds(2)); // start consumer at 2s

  ndn::AppHelper consumerHelper3("ns3::ndn::ConsumerRtc");
  consumerHelper3.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper3.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper3.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper3.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper3.SetAttribute("Filename", StringValue("consumer3.csv"));
  ApplicationContainer consumer3 = consumerHelper3.Install(nodes.Get(9));
  consumer3.Start(Seconds(4)); // start consumer at 4s

  ndn::AppHelper consumerHelper4("ns3::ndn::ConsumerRtc");
  consumerHelper4.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper4.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper4.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper4.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper4.SetAttribute("Filename", StringValue("consumer4.csv"));
  ApplicationContainer consumer4 = consumerHelper4.Install(nodes.Get(10));
  consumer4.Start(Seconds(6)); // start consumer at 6s

  ndn::AppHelper consumerHelper5("ns3::ndn::ConsumerRtc");
  consumerHelper5.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper5.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper5.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper5.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper5.SetAttribute("Filename", StringValue("consumer5.csv"));
  ApplicationContainer consumer5 = consumerHelper5.Install(nodes.Get(11));
  consumer5.Start(Seconds(8)); // start consumer at 8s

  ndn::AppHelper consumerHelper6("ns3::ndn::ConsumerRtc");
  consumerHelper6.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper6.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper6.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper6.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper6.SetAttribute("Filename", StringValue("consumer6.csv"));
  ApplicationContainer consumer6 = consumerHelper6.Install(nodes.Get(12));
  consumer6.Start(Seconds(10)); // start consumer at 10s

  ndn::AppHelper consumerHelper7("ns3::ndn::ConsumerRtc");
  consumerHelper7.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper7.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper7.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper7.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper7.SetAttribute("Filename", StringValue("consumer7.csv"));
  ApplicationContainer consumer7 = consumerHelper7.Install(nodes.Get(13));
  consumer7.Start(Seconds(12)); // start consumer at 12s

  ndn::AppHelper consumerHelper8("ns3::ndn::ConsumerRtc");
  consumerHelper8.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper8.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper8.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper8.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper8.SetAttribute("Filename", StringValue("consumer8.csv"));
  ApplicationContainer consumer8 = consumerHelper8.Install(nodes.Get(14));
  consumer8.Start(Seconds(14)); // start consumer at 14s

  // Producer Rtc
  ndn::AppHelper producerHelper("ns3::ndn::ProducerRtc");
  producerHelper.SetAttribute("ConferencePrefix", StringValue("/conference"));
  producerHelper.SetAttribute("ProducerPrefix", StringValue("/producer"));
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  producerHelper.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  producerHelper.SetAttribute("Filename", StringValue("producer.csv"));
  producerHelper.Install(nodes.Get(0)); // install producer at the root of the tree

  Simulator::Stop(Seconds(20.0));

  Simulator::Run();
  Simulator::Destroy();

  return 0;
}

} // namespace ns3

int
main(int argc, char* argv[])
{
  return ns3::main(argc, argv);
}
