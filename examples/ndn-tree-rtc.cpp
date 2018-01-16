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
  nodes.Create(7);

  // Create a simple binary tree
  PointToPointHelper p2p;
  p2p.Install(nodes.Get(0), nodes.Get(1));
  p2p.Install(nodes.Get(0), nodes.Get(2));
  p2p.Install(nodes.Get(1), nodes.Get(3));
  p2p.Install(nodes.Get(1), nodes.Get(4));
  p2p.Install(nodes.Get(2), nodes.Get(5));
  p2p.Install(nodes.Get(2), nodes.Get(6));

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
  ApplicationContainer consumer1 = consumerHelper1.Install(nodes.Get(3));
  consumer1.Start(Seconds(0)); // start consumer at 0s

  ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerRtc");
  consumerHelper2.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper2.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper2.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  ApplicationContainer consumer2 = consumerHelper2.Install(nodes.Get(4));
  consumer2.Start(Seconds(5)); // start consumer at 2s

  ndn::AppHelper consumerHelper3("ns3::ndn::ConsumerRtc");
  consumerHelper3.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper3.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper3.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  ApplicationContainer consumer3 = consumerHelper3.Install(nodes.Get(5));
  consumer3.Start(Seconds(10)); // start consumer at 4s

  ndn::AppHelper consumerHelper4("ns3::ndn::ConsumerRtc");
  consumerHelper4.SetAttribute("ConferencePrefix", StringValue("/conference"));
  consumerHelper4.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper4.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  ApplicationContainer consumer4 = consumerHelper4.Install(nodes.Get(6));
  consumer4.Start(Seconds(15)); // start consumer at 6s

  // Producer Rtc
  ndn::AppHelper producerHelper("ns3::ndn::ProducerRtc");
  producerHelper.SetAttribute("ConferencePrefix", StringValue("/conference"));
  producerHelper.SetAttribute("ProducerPrefix", StringValue("/producer"));
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  producerHelper.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
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
