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
  Config::SetDefault("ns3::PointToPointNetDevice::Mtu", UintegerValue(2000));
  Config::SetDefault("ns3::PointToPointChannel::Delay", StringValue("10ms"));
  Config::SetDefault("ns3::QueueBase::MaxPackets", UintegerValue(10000000));
  Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (0.001));
  Config::SetDefault ("ns3::RateErrorModel::ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));

  std::string errorModelType = "ns3::RateErrorModel";

  double freshness = 0.01;
  uint32_t fresh_data_num = 1;
  uint32_t sampling_rate = 30;
  double random_number_range = 0.95;

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
  NetDeviceContainer netDevCon;
  //Time t = MilliSeconds(40);
  // p2p.SetChannelAttribute ("Delay", TimeValue(t));
  // p2p.SetChannelAttribute ("Delay", TimeValue(t));
  // p2p.SetChannelAttribute ("Delay", TimeValue(t));
  netDevCon.Add(p2p.Install(nodes.Get(0), nodes.Get(1)));
  netDevCon.Add(p2p.Install(nodes.Get(0), nodes.Get(2)));
  //t = MilliSeconds(10);
  //p2p.SetChannelAttribute ("Delay", TimeValue(t));
  netDevCon.Add(p2p.Install(nodes.Get(1), nodes.Get(3)));
  netDevCon.Add(p2p.Install(nodes.Get(1), nodes.Get(4)));
  netDevCon.Add(p2p.Install(nodes.Get(2), nodes.Get(5)));
  netDevCon.Add(p2p.Install(nodes.Get(2), nodes.Get(6)));
  // leaves
  //t = MilliSeconds(40);
  //p2p.SetChannelAttribute ("Delay", TimeValue(t));
  netDevCon.Add(p2p.Install(nodes.Get(3), nodes.Get(7)));
  netDevCon.Add(p2p.Install(nodes.Get(3), nodes.Get(8)));
  netDevCon.Add(p2p.Install(nodes.Get(4), nodes.Get(9)));
  netDevCon.Add(p2p.Install(nodes.Get(4), nodes.Get(10)));
  netDevCon.Add(p2p.Install(nodes.Get(5), nodes.Get(11)));
  netDevCon.Add(p2p.Install(nodes.Get(5), nodes.Get(12)));
  netDevCon.Add(p2p.Install(nodes.Get(6), nodes.Get(13)));
  netDevCon.Add(p2p.Install(nodes.Get(6), nodes.Get(14)));

  // Install NDN stack on all nodes
  ndn::StackHelper ndnHelper;
  ndnHelper.SetDefaultRoutes(true);
  ndnHelper.InstallAll();

  // Choosing forwarding strategy
  ndn::StrategyChoiceHelper::InstallAll("/conference", "/localhost/nfd/strategy/best-route");

  // Installing global routing interface on all nodes
  ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
  ndnGlobalRoutingHelper.InstallAll();

  double startTime = 0.0;

  std::random_device rd; // obtain a random number from hardware
  std::mt19937 eng(rd()); // seed the generator
  std::uniform_int_distribution<> distr(0, random_number_range * 1000); // define the range

  // Installing applications

  // Consumer Rtc at leaves
  // ndn::AppHelper consumerHelper1("ns3::ndn::ConsumerRtc");
  ndn::AppHelper consumerHelper1("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper1.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper1.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper1.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper1.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper1.SetAttribute("Filename", StringValue("consumer1.csv"));
  consumerHelper1.SetAttribute("FilenameInterarrival", StringValue("consumer1-interarrival.csv"));
  ApplicationContainer consumer1 = consumerHelper1.Install(nodes.Get(7));
  startTime = 1.05 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 1 start time: " << startTime << " sec\n";
  //consumer1.Start(Seconds(startTime)); // start consumer at 0s
  // consumer1.Start(Seconds(1.234));
  consumer1.Start(Seconds(1.475));
  // consumer1.Start(Seconds(1.239));

  // ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerRtc");
  ndn::AppHelper consumerHelper2("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper2.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper2.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper2.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper2.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper2.SetAttribute("Filename", StringValue("consumer2.csv"));
  consumerHelper2.SetAttribute("FilenameInterarrival", StringValue("consumer2-interarrival.csv"));
  ApplicationContainer consumer2 = consumerHelper2.Install(nodes.Get(8));
  startTime = 1.05 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 2 start time: " << startTime << " sec\n";
  //consumer2.Start(Seconds(startTime)); // start consumer at 2s
  //consumer2.Start(Seconds(2.032));
  consumer2.Start(Seconds(1.511));
  //consumer2.Start(Seconds(1.399));

  // ndn::AppHelper consumerHelper3("ns3::ndn::ConsumerRtc");
  ndn::AppHelper consumerHelper3("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper3.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper3.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper3.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper3.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper3.SetAttribute("Filename", StringValue("consumer3.csv"));
  consumerHelper3.SetAttribute("FilenameInterarrival", StringValue("consumer3-interarrival.csv"));
  ApplicationContainer consumer3 = consumerHelper3.Install(nodes.Get(9));
  startTime = 1.05 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 3 start time: " << startTime << " sec\n";
  //consumer3.Start(Seconds(startTime)); // start consumer at 4s
  // consumer3.Start(Seconds(1.997));
  consumer3.Start(Seconds(1.311));
  //consumer3.Start(Seconds(1.251));

  //ndn::AppHelper consumerHelper4("ns3::ndn::ConsumerRtc");
  ndn::AppHelper consumerHelper4("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper4.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper4.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper4.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper4.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper4.SetAttribute("Filename", StringValue("consumer4.csv"));
  consumerHelper4.SetAttribute("FilenameInterarrival", StringValue("consumer4-interarrival.csv"));
  consumerHelper4.SetAttribute("PrintLambda", BooleanValue(true));
  ApplicationContainer consumer4 = consumerHelper4.Install(nodes.Get(10));
  startTime = 1.05 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 4 start time: " << startTime << " sec\n";
  //consumer4.Start(Seconds(startTime)); // start consumer at 6s
  // consumer4.Start(Seconds(1.587));
  consumer4.Start(Seconds(1.177));
  //consumer4.Start(Seconds(1.101));

  // ndn::AppHelper consumerHelper5("ns3::ndn::ConsumerRtc");
  ndn::AppHelper consumerHelper5("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper5.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper5.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper5.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper5.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper5.SetAttribute("Filename", StringValue("consumer5.csv"));
  consumerHelper5.SetAttribute("FilenameInterarrival", StringValue("consumer5-interarrival.csv"));
  ApplicationContainer consumer5 = consumerHelper5.Install(nodes.Get(11));
  startTime = 1.05 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 5 start time: " << startTime << " sec\n";
  //consumer5.Start(Seconds(startTime)); // start consumer at 8s
  // consumer5.Start(Seconds(1.332));
  consumer5.Start(Seconds(1.727));
  //consumer5.Start(Seconds(1.542));

  // ndn::AppHelper consumerHelper6("ns3::ndn::ConsumerRtc");
  ndn::AppHelper consumerHelper6("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper6.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper6.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper6.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper6.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper6.SetAttribute("Filename", StringValue("consumer6.csv"));
  consumerHelper6.SetAttribute("FilenameInterarrival", StringValue("consumer6-interarrival.csv"));
  ApplicationContainer consumer6 = consumerHelper6.Install(nodes.Get(12));
  startTime = 1.05 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 6 start time: " << startTime << " sec\n";
  //consumer6.Start(Seconds(startTime)); // start consumer at 10s
  // consumer6.Start(Seconds(1.999));
  consumer6.Start(Seconds(1.687));
  //consumer6.Start(Seconds(1.887));

  // ndn::AppHelper consumerHelper7("ns3::ndn::ConsumerRtc");
  ndn::AppHelper consumerHelper7("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper7.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper7.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper7.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper7.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper7.SetAttribute("Filename", StringValue("consumer7.csv"));
  consumerHelper7.SetAttribute("FilenameInterarrival", StringValue("consumer7-interarrival.csv"));
  ApplicationContainer consumer7 = consumerHelper7.Install(nodes.Get(13));
  startTime = 1.05 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 7 start time: " << startTime << " sec\n";
  //consumer7.Start(Seconds(startTime)); // start consumer at 12s
  // consumer7.Start(Seconds(1.546));
  consumer7.Start(Seconds(1.903));
  //consumer7.Start(Seconds(1.76));

  // ndn::AppHelper consumerHelper8("ns3::ndn::ConsumerRtc");
  ndn::AppHelper consumerHelper8("ns3::ndn::ConsumerRtcKeyFirst");
  consumerHelper8.SetAttribute("ConferencePrefix", StringValue("/conference/producer/delta"));
  consumerHelper8.SetAttribute("MustBeFreshNum", StringValue(std::to_string(fresh_data_num)));
  consumerHelper8.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  consumerHelper8.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  consumerHelper8.SetAttribute("Filename", StringValue("consumer8.csv"));
  consumerHelper8.SetAttribute("FilenameInterarrival", StringValue("consumer8-interarrival.csv"));
  ApplicationContainer consumer8 = consumerHelper8.Install(nodes.Get(14));
  startTime = 1.05 + (1.0 * distr(eng) / 1000);
  std::cerr << "Consumer 8 start time: " << startTime << " sec\n";
  //consumer8.Start(Seconds(startTime)); // start consumer at 14s
  // consumer8.Start(Seconds(1.304));
  consumer8.Start(Seconds(1.694));
  //consumer8.Start(Seconds(1.907));

  // Producer Rtc
  ndn::AppHelper producerHelper("ns3::ndn::ProducerRtc");
  producerHelper.SetAttribute("ConferencePrefix", StringValue("/conference"));
  producerHelper.SetAttribute("ProducerPrefix", StringValue("/producer"));
  producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
  producerHelper.SetAttribute("Freshness", StringValue(std::to_string(freshness)+"s"));
  producerHelper.SetAttribute("SamplingRate", StringValue(std::to_string(sampling_rate)));
  producerHelper.SetAttribute("Filename", StringValue("producer.csv"));
  producerHelper.Install(nodes.Get(0)); // install producer at the root of the tree

  std::string prefix = "/conference";
  // Add /prefix origins to ndn::GlobalRouter
  ndnGlobalRoutingHelper.AddOrigins(prefix, nodes.Get(0));

  // Calculate and install FIBs
  ndn::GlobalRoutingHelper::CalculateRoutes();


  // Error model
  // ObjectFactory factory;
  // factory.SetTypeId (errorModelType);
  // factory.Set("ErrorUnit", StringValue("ERROR_UNIT_PACKET"));
  // factory.Set("ErrorRate", DoubleValue(0.05));
  // for (int i = 0; i < netDevCon.GetN(); i++) {
  //   netDevCon.Get(i)->SetAttribute ("ReceiveErrorModel", PointerValue(factory.Create<ErrorModel>()));
  // }

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
