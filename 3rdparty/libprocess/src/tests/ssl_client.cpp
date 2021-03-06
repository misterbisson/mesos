/**
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License
*/

#include <iostream>

#include <glog/logging.h>

#include <gmock/gmock.h>

#include <gtest/gtest.h>

#include <process/gmock.hpp>
#include <process/gtest.hpp>
#include <process/socket.hpp>

#include <stout/gtest.hpp>
#include <stout/try.hpp>

#include "openssl.hpp"

using namespace process;
using namespace process::network;

using std::cout;
using std::endl;
using std::string;

// We only run these tests if we have configured with '--enable-ssl'.
#ifdef USE_SSL_SOCKET

/**
 * The flags that control the test SSL client behavior.
 *
 * These flags augment the environment variables prefixed by 'SSL_'
 * that are introduced by @see process::network::openssl::Flags.
 */
class Flags : public virtual flags::FlagsBase
{
public:
  Flags()
  {
    add(&Flags::use_ssl,
      "use_ssl",
      "Whether to try and connect using an SSL based socket. This is "
      "separate from whether SSL_ENABLED is set. We use this for "
      "testing failure cases.",
      true);

    add(&Flags::data,
      "data",
      "The message to send as the client.",
      "Hello World");

    add(&Flags::server, "server", "IP address of server", "127.0.0.1");

    add(&Flags::port, "port", "Port of server", 5050);
  }

  bool use_ssl;
  string data;
  string server;
  uint16_t port;
};


class SSLClientTest : public ::testing::Test
{
public:
  static Flags flags;
};


Flags SSLClientTest::flags;


int main(int argc, char** argv)
{
  // Load all the client flags.
  Try<Nothing> load = SSLClientTest::flags.load("", argc, argv);
  if (load.isError()) {
    EXIT(EXIT_FAILURE) << "Failed to load flags: " << load.error();
  }

  if (SSLClientTest::flags.help) {
    cout << SSLClientTest::flags.usage() << endl;
    return EXIT_SUCCESS;
  }

  process::initialize();

  openssl::initialize();

  // Initialize Google Mock/Test.
  testing::InitGoogleMock(&argc, argv);

  // Add the libprocess test event listeners.
  ::testing::TestEventListeners& listeners =
    ::testing::UnitTest::GetInstance()->listeners();

  listeners.Append(process::ClockTestEventListener::instance());
  listeners.Append(process::FilterTestEventListener::instance());

  return RUN_ALL_TESTS();
}


TEST_F(SSLClientTest, client)
{
  // Create the socket based on the 'use_ssl' flag. We use this to
  // test whether a regular socket could connect to an SSL server
  // socket.
  const Try<Socket> create =
    Socket::create(flags.use_ssl ? Socket::SSL : Socket::POLL);
  ASSERT_SOME(create);

  Socket socket = create.get();

  Try<net::IP> ip = net::IP::parse(flags.server, AF_INET);
  EXPECT_SOME(ip);

  // Connect to the server socket located at `ip:port`.
  const Future<Nothing> connect =
    socket.connect(Address(ip.get(), flags.port));

  // Verify that the client views the connection as established.
  AWAIT_EXPECT_READY(connect);

  // Send 'data' from the client to the server.
  AWAIT_EXPECT_READY(socket.send(flags.data));

  // Verify the client received the message back from the server.
  AWAIT_EXPECT_EQ(flags.data, socket.recv());
}

#endif // USE_SSL_SOCKET
