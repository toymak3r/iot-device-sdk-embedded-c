/* Copyright 2018-2019 Google LLC
 *
 * This is part of the Google Cloud IoT Device SDK for Embedded C,
 * it is licensed under the BSD 3-Clause license; you may not use this file
 * except in compliance with the License.
 *
 * You may obtain a copy of the License at:
 *  https://opensource.org/licenses/BSD-3-Clause
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "iotc_bsp_io_net.h"

#include <memory>
#include <stdio.h>

#include "gtest.h"
#include "iotc_test_echoserver.h"

namespace iotctest {
namespace {

typedef struct NetworkType_s {
  iotc_bsp_socket_type_t socket_type;
  iotc_bsp_protocol_type_t protocol_type;
  std::string listening_addr;

  NetworkType_s(iotc_bsp_socket_type_t socket_type_,
                iotc_bsp_protocol_type_t protocol_type_,
                std::string listening_addr_)
      : socket_type(socket_type_), protocol_type(protocol_type_),
        listening_addr(listening_addr_){};
} NetworkType;

class ServerTest : public ::testing::TestWithParam<NetworkType> {
  public:
    bool WaitUntilSocketReadyForWrite();
    bool WaitUntilSocketReadyForRead();
    iotc_bsp_socket_t test_socket_;
};

bool ServerTest::WaitUntilSocketReadyForWrite() {
  bool ready_to_write = false;
  iotc_bsp_socket_events_t socket_evts_[1] = {};
  socket_evts_[0].iotc_socket = test_socket_;
  socket_evts_[0].in_socket_want_write = 1;
  while (true) {
    if (iotc_bsp_io_net_select(socket_evts_, 1, kTimeoutSeconds) ==
            IOTC_BSP_IO_NET_STATE_OK &&
        socket_evts_[0].out_socket_can_write == 1) {
      ready_to_write = true;
      break;
    }
  }
  return ready_to_write;
}

bool ServerTest::WaitUntilSocketReadyForRead() {
  bool ready_to_read = false;
  iotc_bsp_socket_events_t socket_evts_[1] = {};
  socket_evts_[0].iotc_socket = test_socket_;
  socket_evts_[0].in_socket_want_read = 1;
  while (true) {
    if (iotc_bsp_io_net_select(socket_evts_, 1, kTimeoutSeconds) ==
            IOTC_BSP_IO_NET_STATE_OK &&
        socket_evts_[0].out_socket_can_read == 1) {
      ready_to_read = true;
      break;
    }
  }
  return ready_to_read;
}

TEST_P(ServerTest, EndToEndCommunicationWorks) {
  const NetworkType test_case = GetParam();
  const char* kTestMsg = "hello\n";
  const uint16_t kTestPort = 2000;
  std::string listening_addr_ = test_case.listening_addr;
  iotc_bsp_socket_type_t socket_type_ = test_case.socket_type;
  iotc_bsp_protocol_type_t protocol_type_ = test_case.protocol_type;
  int out_written_count_;

  // TODO(b/127770330)
  // std::unique_ptr<EchoTestServer> test_server =
  //     std::make_unique<EchoTestServer>(socket_type_, kTestPort, protocol_type_);

  std::unique_ptr<EchoTestServer> test_server(
      new EchoTestServer(socket_type_, kTestPort, protocol_type_));

  ASSERT_EQ(iotc_bsp_io_net_socket_connect(&test_socket_,
                                           listening_addr_.c_str(), kTestPort,
                                           socket_type_),
            IOTC_BSP_IO_NET_STATE_OK);
  ASSERT_EQ(iotc_bsp_io_net_connection_check(
                test_socket_, listening_addr_.c_str(), kTestPort),
            IOTC_BSP_IO_NET_STATE_OK);

  test_server->Run();
  ASSERT_TRUE(WaitUntilSocketReadyForWrite());
  EXPECT_EQ(iotc_bsp_io_net_write(test_socket_, &out_written_count_,
                                  (uint8_t*)kTestMsg, strlen(kTestMsg)),
            IOTC_BSP_IO_NET_STATE_OK);
  size_t test_msg_len_ = strlen(kTestMsg) + 1;
  char recv_buf_[test_msg_len_] = {0};
  int recv_len_ = 0;
  ASSERT_TRUE(WaitUntilSocketReadyForRead());
  EXPECT_EQ(iotc_bsp_io_net_read(test_socket_, &recv_len_,
                                 reinterpret_cast<uint8_t*>(recv_buf_),
                                 sizeof(recv_buf_)),
            IOTC_BSP_IO_NET_STATE_OK);
  EXPECT_STREQ(recv_buf_, kTestMsg);

  iotc_bsp_io_net_close_socket(&test_socket_);
  test_server->Stop();
}

INSTANTIATE_TEST_CASE_P(
    NetTestSuite, ServerTest,
    ::testing::Values(
        NetworkType(SOCKET_STREAM, PROTOCOL_IPV4,
                    const_cast<char*>("127.0.0.1")),
        NetworkType(SOCKET_STREAM, PROTOCOL_IPV6, const_cast<char*>("::1")),
        NetworkType(SOCKET_DGRAM, PROTOCOL_IPV4,
                    const_cast<char*>("127.0.0.1")),
        NetworkType(SOCKET_DGRAM, PROTOCOL_IPV6, const_cast<char*>("::1"))));

} // namespace
} // namespace iotctest
