// Copyright 2022 Proyectos y Sistemas de Mantenimiento SL (eProsima).
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
* @file monitor.cpp
*
*/

#include <chrono>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <regex>

#include <nlohmann/json.hpp>
#include <fastdds_statistics_backend/listener/DomainListener.hpp>
#include <fastdds_statistics_backend/StatisticsBackend.hpp>
#include <fastdds_statistics_backend/types/EntityId.hpp>
#include <fastdds_statistics_backend/types/types.hpp>
#include <fastdds_statistics_backend/types/JSONTags.h>

using namespace eprosima::statistics_backend;

class Monitor
{
public:
  Monitor(
    uint32_t domain,
    uint32_t n_bins,
    uint32_t t_interval)
  : domain_(domain)
    , n_bins_(n_bins)
    , t_interval_(t_interval)
    , node_names_()
    , monitor_id_(EntityId::invalid())
    , topic_id_(EntityId::invalid())
  {
  }

  ~Monitor()
  {
    StatisticsBackend::stop_monitor(monitor_id_);
  }

  bool init()
  {
    monitor_id_ = StatisticsBackend::init_monitor(domain_);
    if (!monitor_id_.is_valid()) {
      std::cout << "Error creating monitor" << std::endl;
      return 1;
    }

    StatisticsBackend::set_physical_listener(&physical_listener_);

    return true;
  }

  void run()
  {
    while (true) {
      get_participant_locators();
      std::this_thread::sleep_for(std::chrono::milliseconds(t_interval_ * 1000));
    }
  }

  //! Get the id of the topic searching by topic_name and data_type
  EntityId get_topic_id(
    std::string topic_name,
    std::string data_type)
  {
    // Get the list of all topics available
    std::vector<EntityId> topics = StatisticsBackend::get_entities(EntityKind::TOPIC);
    Info topic_info;
    // Iterate over all topic searching for the one with specified topic_name and data_type
    for (auto topic_id : topics) {
      topic_info = StatisticsBackend::get_info(topic_id);
      if (topic_info["name"] == topic_name && topic_info["data_type"] == data_type) {
        return topic_id;
      }
    }


    return EntityId::invalid();
  }

  // Print locators of participants
  void get_participant_locators()
  {
    // Vector of Statistics Data to store the latency data
    std::vector<StatisticsData> latency_data{};

    // Publishers on a specific topic
    std::vector<EntityId> participants = StatisticsBackend::get_entities(
      EntityKind::PARTICIPANT);

    std::cout << "Participants size: " << participants.size() << std::endl;
    // Iterate over the retrieve data
    for (auto participant : participants) {
      Info participant_info = StatisticsBackend::get_info(participant);
      std::string node_name = "";
      std::string participant_name_dds = "";
      if (node_names_.find(participant) == node_names_.end()) {
        std::vector<EntityId> topics = StatisticsBackend::get_entities(EntityKind::DATAREADER);
        Info topic_info;
        std::regex topic_regex(
          "^DataReader_rq/(.+)/get_parametersRequest_[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$");
        for (auto topic_id : topics) {
          try {
            topic_info = StatisticsBackend::get_info(topic_id);
            std::string topic_name = std::string(topic_info["name"]);
            std::smatch match;
            if (std::regex_search(topic_name, match, topic_regex) && match.size() > 1) {
              std::string name = match[1].str();
              // Extract the GUIDs
              std::string participant_guid = std::string(participant_info["guid"]);
              std::string topic_guid = std::string(topic_info["guid"]);
              participant_guid = participant_guid.substr(0, participant_guid.find("|"));
              topic_guid = topic_guid.substr(0, topic_guid.find("|"));

              // Check if the GUIDs match
              if (participant_guid == topic_guid) {
                std::cout << "Participant with GUID " << std::string(participant_info["guid"]) <<
                  " found with name " << name << std::endl;

                node_names_[participant] = name;
                node_name = name;
                break;
              }
            }

          } catch (const nlohmann::detail::type_error & e) {
            std::cout << "Error: " << e.what() << std::endl;
          }
        }
      } else {
        try {
          participant_name_dds = std::string(participant_info["name"]);
        } catch (const nlohmann::detail::type_error & e) {
          participant_name_dds = "";
        }
        node_name = node_names_[participant];
      }
      std::cout << "Participant with GUID " << std::string(participant_info["guid"]) << " (" <<
        node_name << ") " << "(" << participant_name_dds << ")" << std::endl;

      try {
        bool isFound = participant_info.contains("locators");
        if (isFound) {
          std::cout << "Participant locators: " << participant_info["locators"].dump(4) <<
            std::endl;
        } else {
          std::cout << "Participant locators: " << "Not found" << std::endl;
        }

      } catch (const nlohmann::detail::type_error & e) {
        std::cout << "Error: " << e.what() << std::endl;
      }
    }
  }


  //! Convert timestamp to string
  std::string timestamp_to_string(
    const Timestamp timestamp)
  {
    auto timestamp_t = std::chrono::system_clock::to_time_t(timestamp);
    auto msec =
      std::chrono::duration_cast<std::chrono::milliseconds>(timestamp.time_since_epoch()).count();
    msec %= 1000;
    std::stringstream ss;

    ss <<
      std::put_time(
      localtime(&timestamp_t),
      "%F %T") << "." << std::setw(3) << std::setfill('0') << msec;

    return ss.str();
  }

protected:
  class Listener : public eprosima::statistics_backend::PhysicalListener
  {
public:
    Listener()
    {
    }

    ~Listener() override
    {
    }

    void on_host_discovery(
      EntityId host_id,
      const DomainListener::Status & status) override
    {
      Info host_info = StatisticsBackend::get_info(host_id);

      if (status.current_count_change == 1) {
        std::cout << "Host " << std::string(host_info["name"]) << " discovered." << std::endl;
      } else {
        std::cout << "Host " << std::string(host_info["name"]) << " updated info." << std::endl;
      }
    }

    void on_user_discovery(
      EntityId user_id,
      const DomainListener::Status & status) override
    {
      Info user_info = StatisticsBackend::get_info(user_id);

      if (status.current_count_change == 1) {
        std::cout << "User " << std::string(user_info["name"]) << " discovered." << std::endl;
      } else {
        std::cout << "User " << std::string(user_info["name"]) << " updated info." << std::endl;
      }
    }

    void on_process_discovery(
      EntityId process_id,
      const DomainListener::Status & status) override
    {
      Info process_info = StatisticsBackend::get_info(process_id);

      if (status.current_count_change == 1) {
        std::cout << "Process " << std::string(process_info["name"]) << " discovered." << std::endl;
      } else {
        std::cout << "Process " << std::string(process_info["name"]) << " updated info." <<
          std::endl;
      }
    }

    void on_topic_discovery(
      EntityId domain_id,
      EntityId topic_id,
      const DomainListener::Status & status) override
    {
      Info topic_info = StatisticsBackend::get_info(topic_id);
      Info domain_info = StatisticsBackend::get_info(domain_id);

      if (status.current_count_change == 1) {
        std::cout << "Topic " << std::string(topic_info["name"])
                  << " [" << std::string(topic_info["data_type"])
                  << "] discovered." << std::endl;
      } else {
        std::cout << "Topic " << std::string(topic_info["name"])
                  << " [" << std::string(topic_info["data_type"])
                  << "] updated info." << std::endl;
      }
    }

    void on_participant_discovery(
      EntityId domain_id,
      EntityId participant_id,
      const DomainListener::Status & status) override
    {
      static_cast<void>(domain_id);
      Info participant_info = StatisticsBackend::get_info(participant_id);

      if (status.current_count_change == 1) {
        std::cout << "Participant with GUID " << std::string(participant_info["guid"]) <<
          " discovered." << std::endl;
      } else {
        std::cout << "Participant with GUID " << std::string(participant_info["guid"]) <<
          " updated info." << std::endl;
      }
      std::cout << "Participant id: " << participant_id.value() << std::endl;
      std::cout << "Participant kind: " << std::string(participant_info["kind"]) << std::endl;
      std::cout << "Participant name: " << std::string(participant_info["name"]) << std::endl;
      std::cout << "Participant alias: " << std::string(participant_info["alias"]) << std::endl;
      // std::cout << "Participant alive: " << std::string(participant_info["alive"]) << std::endl;
      // std::cout << "Participant metatraffic: " << std::string(participant_info["metatraffic"]) << std::endl;
      // std::cout << "Participant locators: " << participant_info["locators"].dump(4) << std::endl;
      bool isFound = participant_info.contains("locators");
      if (isFound) {
        std::cout << "Participant locators: " << participant_info["locators"].dump(4) << std::endl;
      } else {
        std::cout << "Participant locators: " << "Not found" << std::endl;
      }

    }

    void on_datareader_discovery(
      EntityId domain_id,
      EntityId datareader_id,
      const DomainListener::Status & status) override
    {
      static_cast<void>(domain_id);
      Info datareader_info = StatisticsBackend::get_info(datareader_id);

      if (status.current_count_change == 1) {
        std::cout << "DataReader with GUID " << std::string(datareader_info["guid"]) <<
          " discovered." << std::endl;
      } else {
        std::cout << "DataReader with GUID " << std::string(datareader_info["guid"]) <<
          " updated info." << std::endl;
      }
    }

    void on_datawriter_discovery(
      EntityId domain_id,
      EntityId datawriter_id,
      const DomainListener::Status & status) override
    {
      static_cast<void>(domain_id);
      Info datawriter_info = StatisticsBackend::get_info(datawriter_id);

      if (status.current_count_change == 1) {
        std::cout << "DataWriter with GUID " << std::string(datawriter_info["guid"]) <<
          " discovered." << std::endl;
      } else {
        std::cout << "DataWriter with GUID " << std::string(datawriter_info["guid"]) <<
          " updated info." << std::endl;
      }
    }

  } physical_listener_;

  DomainId domain_;
  uint32_t n_bins_;
  uint32_t t_interval_;
  std::map<EntityId, std::string> node_names_;
  EntityId monitor_id_;
  EntityId topic_id_;
};

int main()
{
  int domain = 0;
  int n_bins = 1;
  int t_interval = 5;

  Monitor monitor(domain, n_bins, t_interval);

  if (monitor.init()) {
    monitor.run();
  }

  return 0;
}
