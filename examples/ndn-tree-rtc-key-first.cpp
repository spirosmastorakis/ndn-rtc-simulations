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

#include <random>

namespace ns3 {

int
main(int argc, char* argv[])
{
  // setting default parameters for PointToPoint links and channels
  Config::SetDefault("ns3::PointToPointNetDevice::DataRate", StringValue("1Gbps"));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::QueueBase::MaxPackets", UintegerValue(50000));

  double freshness = 0.01;
  uint32_t fresh_data_num = 1;
  uint32_t sampling_rate = 30;
  double random_number_range = 1.00;
  double rtt = 80;
  bool force_to_retrieve_old_data = false;

  // Read optional command-line parameters (e.g., enable visualizer with ./waf --run=<> --visualize
  CommandLine cmd;
	cmd.AddValue("freshness", "Freshness of producer data", freshness);
	cmd.AddValue("fresh-data-num", "Number of fresh data packets to be retrieved", fresh_data_num);
  cmd.AddValue("rate", "Sampling Rate", sampling_rate);
  cmd.AddValue("random-number-range", "Random Number Range", random_number_range);
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
  double startTime = 0.0;

  std::random_device rd; // obtain a random number from hardware
  std::mt19937 eng(rd()); // seed the generator
  std::uniform_int_distribution<> distr(0, random_number_range * 1000); // define the range

  std::random_device rd2; // obtain a random number from hardware
  std::mt19937 eng2(rd2()); // seed the generator
  std::uniform_int_distribution<> distr2(random_number_range * 1000, (random_number_range + freshness) * 1000); // define the range

  std::random_device rd3; // obtain a random number from hardware
  std::mt19937 eng3(rd3()); // seed the generator
  std::uniform_int_distribution<> distr3(0, freshness * 1000); // define the range

  // Installing applications

  // Consumer Rtc at leaves
  ndn::AppHelper consumerHelper1("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper1.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper1.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper1.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper1.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper1.SetAttribute("Filename", StringValue("consumer1.csv"));
  consumerHelper1.SetAttribute("FilenameInterarrival", StringValue("consumer1-interarrival.csv"));
  ApplicationContainer consumer1 = consumerHelper1.Install(nodes.Get(3));
  if (force_to_retrieve_old_data)
    startTime = 1.00 + (1.0 * distr3(eng3) / 1000);
  else
    startTime = 1.00 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 1 start time: " << startTime << " sec\n";
  consumer1.Start(Seconds(startTime)); // start consumer at 0s
  // consumer1.Start(Seconds(1.181));

  ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper2.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper2.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper2.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper2.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper2.SetAttribute("Filename", StringValue("consumer2.csv"));
  consumerHelper2.SetAttribute("FilenameInterarrival", StringValue("consumer2-interarrival.csv"));
  ApplicationContainer consumer2 = consumerHelper2.Install(nodes.Get(4));
  if (force_to_retrieve_old_data)
    startTime = 1.00 + (1.0 * distr2(eng2) / 1000);
  else
    startTime = 1.00 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 2 start time: " << startTime << " sec\n";
  consumer2.Start(Seconds(startTime)); // start consumer at 5s
  // consumer2.Start(Seconds(1.508));

  ndn::AppHelper consumerHelper3("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper3.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper3.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper3.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper3.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper3.SetAttribute("Filename", StringValue("consumer3.csv"));
  consumerHelper3.SetAttribute("FilenameInterarrival", StringValue("consumer3-interarrival.csv"));
  ApplicationContainer consumer3 = consumerHelper3.Install(nodes.Get(5));
  if (force_to_retrieve_old_data)
    startTime = 1.00 + (1.0 * distr3(eng3) / 1000);
  else
    startTime = 1.00 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 3 start time: " << startTime << " sec\n";
  consumer3.Start(Seconds(startTime)); // start consumer at 10s
  // PIT collision
  consumer3.Start(Seconds(1.022));
  // Cache collition
  // consumer3.Start(Seconds(0.98));
  // PIT collision #2
  // consumer3.Start(Seconds(0.38));

  ndn::AppHelper consumerHelper4("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper4.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper4.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper4.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper4.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper4.SetAttribute("Filename", StringValue("consumer4.csv"));
  consumerHelper4.SetAttribute("FilenameInterarrival", StringValue("consumer4-interarrival.csv"));
  ApplicationContainer consumer4 = consumerHelper4.Install(nodes.Get(6));
  if (force_to_retrieve_old_data)
    startTime = 1.00 + (1.0 * distr2(eng2) / 1000);
  else
    startTime = 1.00 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 4 start time: " << startTime << " sec\n";
  consumer4.Start(Seconds(startTime)); // start consumer at 15s
  // PIT collision
  // consumer4.Start(Seconds(1.083));
  // Cache collition
  // consumer4.Start(Seconds(1.049));
  // PIT collision #2
  // consumer4.Start(Seconds(0.44));

  // Producer Rtc
  ndn::AppHelper producerHelper("ns3::ndn::ProducerRtc");
  producerHelper.SetAttribute("ConferencePrefix", StringValue("/conference"));
  producerHelper.SetAttribute("ProducerPrefix", StringValue("/producer"));
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  producerHelper.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  producerHelper.SetAttribute("Filename", StringValue("producer.csv"));
  producerHelper.SetAttribute("TweakFreshness", BooleanValue(true));
  producerHelper.Install(nodes.Get(0)); // install producer at the root of the tree

  Simulator::Stop(Seconds(5));

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
