/*
  Copyright (c) 2018, Ivor Wanders
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

  Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "scalopus_catapult/tracing_endpoint.h"

#include <seasocks/PrintfLogger.h>
#include <seasocks/Server.h>

#include <thread>
#include <chrono>

int main(int /* argc */, char** /* argv */)
{
  // retrieve the port number to bind the webserver on
  int port = 9222;  // 9222 is default chrom(e/ium) remote debugging port.
  std::cout << "Using port: " << port << ", 9222 is default, it is default remote debugging port" << std::endl;

  // Retrieve the path to run babeltrace on.
  std::string path = "";  // empty path defaults to 'lttng view'
  std::cout << "Using path: \"" << path << "\"  (empty defaults to lttng view scalopus_target_session)" << std::endl;

  // Create the devtools endpoint
  scalopus::TracingEndpoint::Ptr endpoint = std::make_shared<scalopus::TracingEndpoint>();

  // Start it on the path provided.
  endpoint->init(path);

  // Make a webserver and add the endpoints
  namespace ss = seasocks;
  auto logger = std::make_shared<ss::PrintfLogger>(ss::Logger::Level::WARNING);
  ss::Server server(logger);

  // Set the send buffer to 128 mb. At the end of the trace, the json representation needs to fit in this buffer.
  // This is a workaround for client buffer overflows, proper fix requires a substantial refactor (CORE-11125).
  server.setClientBufferSize(128 * 1024 * 1024u);

  server.addWebSocketHandler("/devtools/page/bar", endpoint);  // needed for chrom(e/ium) 65.0+
  server.addWebSocketHandler("/devtools/browser", endpoint);   // needed for chrome 60.0

  server.addPageHandler(endpoint);  // This is retrieved in the overview page.

  // Start the server in a separate thread.
  server.startListening(port);
  std::thread seasocksThread([&] { server.loop(); });

  std::cout << "Everything started, falling into idle loop. Use ctrl+c to quit." << std::endl;

  // block while we serve requests.
  {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(2000s);
  }

  std::cout << "Shutting down." << std::endl;
  server.terminate();  // send terminate signal to the server

  // Wait for the webserver thread to join.
  seasocksThread.join();

  return 0;
}