// Copyright 2018 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "database/src/desktop/connection/connection.h"

#include <sstream>

#include "app/src/include/firebase/app.h"
#include "app/src/scheduler.h"
#include "app/src/semaphore.h"
#include "app/src/time.h"
#include "app/src/variant_util.h"
#include "app/tests/include/firebase/app_for_testing.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

static const char kDatabaseHostname[] = "cpp-database-test-app.firebaseio.com";
static const char kDatabaseNamespace[] = "cpp-database-test-app";

namespace firebase {
namespace database {
namespace internal {
namespace connection {

class ConnectionTest : public ::testing::Test, public ConnectionEventHandler {
 protected:
  ConnectionTest()
      : test_host_info_(nullptr),
        sem_on_cache_host_(0),
        sem_on_ready_(0),
        sem_on_data_message_(0),
        sem_on_disconnect_(0) {}

  void SetUp() override {
    testing::CreateApp();
    test_host_info_ = new HostInfo(kDatabaseHostname, kDatabaseNamespace, true);
  }

  void TearDown() override {
    delete firebase::App::GetInstance();
    delete test_host_info_;
  }

  void OnCacheHost(const std::string& host) override {
    LogDebug("OnCacheHost: %s", host.c_str());
    sem_on_cache_host_.Post();
  }

  void OnReady(int64_t timestamp, const std::string& sessionId) override {
    LogDebug("OnReady: %lld, %s", timestamp, sessionId.c_str());
    last_session_id_ = sessionId;
    sem_on_ready_.Post();
  }

  void OnDataMessage(const Variant& data) override {
    LogDebug("OnDataMessage: %s", util::VariantToJson(data).c_str());
    sem_on_data_message_.Post();
  }

  void OnDisconnect(Connection::DisconnectReason reason) override {
    LogDebug("OnDisconnect: %d", static_cast<int>(reason));
    sem_on_disconnect_.Post();
  }

  void OnKill(const std::string& reason) override {
    LogDebug("OnKill: %s", reason.c_str());
  }

  void ScheduledOpen(Connection* connection) {
    scheduler_.Schedule(new callback::CallbackValue1<Connection*>(
        connection, [](Connection* connection) { connection->Open(); }));
  }

  void ScheduledSend(Connection* connection, const Variant& message) {
    scheduler_.Schedule(new callback::CallbackValue2<Connection*, Variant>(
        connection, message, [](Connection* connection, Variant message) {
          connection->Send(message, false);
        }));
  }

  void ScheduledClose(Connection* connection) {
    scheduler_.Schedule(new callback::CallbackValue1<Connection*>(
        connection, [](Connection* connection) { connection->Close(); }));
  }

  HostInfo GetHostInfo() {
    assert(test_host_info_ != nullptr);
    if (test_host_info_) {
      return *test_host_info_;
    } else {
      return HostInfo();
    }
  }

  scheduler::Scheduler scheduler_;

  HostInfo* test_host_info_;

  std::string last_session_id_;

  Semaphore sem_on_cache_host_;
  Semaphore sem_on_ready_;
  Semaphore sem_on_data_message_;
  Semaphore sem_on_disconnect_;
};

static const int kTimeoutMs = 5000;

TEST_F(ConnectionTest, DeleteConnectionImmediately) {
  Logger logger(nullptr);
  Connection connection(&scheduler_, GetHostInfo(), nullptr, this, &logger);
}

TEST_F(ConnectionTest, OpenConnection) {
  Logger logger(nullptr);
  Connection connection(&scheduler_, GetHostInfo(), nullptr, this, &logger);

  ScheduledOpen(&connection);
  EXPECT_TRUE(sem_on_cache_host_.TimedWait(kTimeoutMs));
  EXPECT_TRUE(sem_on_ready_.TimedWait(kTimeoutMs));
}

TEST_F(ConnectionTest, CloseConnection) {
  Logger logger(nullptr);
  Connection connection(&scheduler_, GetHostInfo(), nullptr, this, &logger);

  ScheduledOpen(&connection);
  EXPECT_TRUE(sem_on_cache_host_.TimedWait(kTimeoutMs));
  EXPECT_TRUE(sem_on_ready_.TimedWait(kTimeoutMs));

  ScheduledClose(&connection);
  EXPECT_TRUE(sem_on_disconnect_.TimedWait(kTimeoutMs));
}

TEST_F(ConnectionTest, MultipleConnections) {
  const int kNumOfConnections = 10;
  std::vector<Connection*> connections;

  Logger logger(nullptr);
  for (int i = 0; i < kNumOfConnections; ++i) {
    connections.push_back(
        new Connection(&scheduler_, GetHostInfo(), nullptr, this, &logger));
  }

  for (auto& itConnection : connections) {
    ScheduledOpen(itConnection);
  }

  for (int i = 0; i < connections.size(); ++i) {
    EXPECT_TRUE(sem_on_cache_host_.TimedWait(kTimeoutMs));
    EXPECT_TRUE(sem_on_ready_.TimedWait(kTimeoutMs));
  }

  for (auto& itConnection : connections) {
    ScheduledClose(itConnection);
  }

  for (int i = 0; i < connections.size(); ++i) {
    EXPECT_TRUE(sem_on_disconnect_.TimedWait(kTimeoutMs));
  }

  for (int i = 0; i < kNumOfConnections; ++i) {
    delete connections[i];
    connections[i] = nullptr;
  }
}

TEST_F(ConnectionTest, LastSession) {
  Logger logger(nullptr);
  Connection connection1(&scheduler_, GetHostInfo(), nullptr, this, &logger);

  ScheduledOpen(&connection1);
  EXPECT_TRUE(sem_on_cache_host_.TimedWait(kTimeoutMs));
  EXPECT_TRUE(sem_on_ready_.TimedWait(kTimeoutMs));

  Connection connection2(&scheduler_, GetHostInfo(), last_session_id_.c_str(),
                         this, &logger);

  ScheduledOpen(&connection2);
  EXPECT_TRUE(sem_on_cache_host_.TimedWait(kTimeoutMs));
  EXPECT_TRUE(sem_on_ready_.TimedWait(kTimeoutMs));

  // connection1 disconnected
  EXPECT_TRUE(sem_on_disconnect_.TimedWait(kTimeoutMs));

  ScheduledClose(&connection2);
  EXPECT_TRUE(sem_on_disconnect_.TimedWait(kTimeoutMs));
}

const char* const kWireProtocolClearRoot =
    "{\"r\":1,\"a\":\"p\",\"b\":{\"p\":\"/connection/ConnectionTest/"
    "\",\"d\": null}}";
const char* const kWireProtocolListenRoot =
    "{\"r\":2,\"a\":\"q\",\"b\":{\"p\":\"/connection/ConnectionTest/"
    "\",\"h\":\"\"}}";

TEST_F(ConnectionTest, SimplePutRequest) {
  Logger logger(nullptr);
  Connection connection(&scheduler_, GetHostInfo(), nullptr, this, &logger);

  ScheduledOpen(&connection);
  EXPECT_TRUE(sem_on_cache_host_.TimedWait(kTimeoutMs));
  EXPECT_TRUE(sem_on_ready_.TimedWait(kTimeoutMs));

  ScheduledSend(&connection, util::JsonToVariant(kWireProtocolClearRoot));
  EXPECT_TRUE(sem_on_data_message_.TimedWait(kTimeoutMs));
}

TEST_F(ConnectionTest, LargeMessage) {
  Logger logger(nullptr);
  Connection connection(&scheduler_, GetHostInfo(), nullptr, this, &logger);

  ScheduledOpen(&connection);
  EXPECT_TRUE(sem_on_cache_host_.TimedWait(kTimeoutMs));
  EXPECT_TRUE(sem_on_ready_.TimedWait(kTimeoutMs));

  ScheduledSend(&connection, util::JsonToVariant(kWireProtocolClearRoot));
  EXPECT_TRUE(sem_on_data_message_.TimedWait(kTimeoutMs));

  ScheduledSend(&connection, util::JsonToVariant(kWireProtocolListenRoot));
  EXPECT_TRUE(sem_on_data_message_.TimedWait(kTimeoutMs));

  // Send a long message
  std::stringstream ss;
  ss << "{\"r\":3,\"a\":\"p\",\"b\":{\"p\":\"/connection/ConnectionTest/"
        "\",\"d\":\"";
  for (int i = 0; i < 20000; ++i) {
    ss << "!";
  }
  ss << "\"}}";

  ScheduledSend(&connection, util::JsonToVariant(ss.str().c_str()));

  EXPECT_TRUE(sem_on_data_message_.TimedWait(kTimeoutMs));
  EXPECT_TRUE(sem_on_data_message_.TimedWait(kTimeoutMs));
}

TEST_F(ConnectionTest, TestBadHost) {
  HostInfo bad_host("bad-host-name.bad", "bad-namespace", true);
  Logger logger(nullptr);
  Connection connection(&scheduler_, bad_host, nullptr, this, &logger);
  connection.Open();
  EXPECT_TRUE(sem_on_disconnect_.TimedWait(kTimeoutMs));
}

TEST_F(ConnectionTest, TestCreateDestroyRace) {
  Logger logger(nullptr);
  // Test race when connecting to a valid host without sleep
  // Try this on real server less time or the server may block this client
  for (int i = 0; i < 10; ++i) {
    Connection* connection =
        new Connection(&scheduler_, GetHostInfo(), nullptr, this, &logger);
    connection->Open();
    delete connection;
  }

  // Test race when connecting to a valid host with sleep, to wait for websocket
  // thread to kick-in
  for (int i = 0; i < 10; ++i) {
    Connection* connection =
        new Connection(&scheduler_, GetHostInfo(), nullptr, this, &logger);
    connection->Open();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    delete connection;
  }

  // Test race when connecting to a bad host name without sleep
  HostInfo bad_host("bad-host-name.bad", "bad-namespace", true);
  for (int i = 0; i < 100; ++i) {
    Connection* connection =
        new Connection(&scheduler_, bad_host, nullptr, this, &logger);
    connection->Open();
    delete connection;
  }

  // Test race when connecting to a bad host name with sleep, to wait for
  // websocket thread to kick-in
  for (int i = 0; i < 100; ++i) {
    Connection* connection =
        new Connection(&scheduler_, bad_host, nullptr, this, &logger);
    connection->Open();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    delete connection;
  }
}

}  // namespace connection
}  // namespace internal
}  // namespace database
}  // namespace firebase
