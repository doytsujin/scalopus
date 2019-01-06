/*
  Copyright (c) 2019, Ivor Wanders
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

#include "scalopus_catapult/lttng_provider.h"
#include "scalopus_catapult/lttng_source.h"

#include <sstream>

namespace scalopus
{

LttngProvider::LttngProvider(std::string path, EndpointManager::Ptr manager) : manager_(manager)
{
  // Start the tracing tool.
  tracing_tool_ = std::make_shared<BabeltraceTool>();
  tracing_tool_->init(path);
}

LttngProvider::~LttngProvider()
{
  tracing_tool_->halt();
}

TraceEventSource::Ptr LttngProvider::makeSource()
{
  return std::make_shared<LttngSource>(tracing_tool_, shared_from_this());
}

std::map<unsigned int, LttngProvider::ProcessMapping> LttngProvider::getMapping()
{
  std::lock_guard<decltype(mapping_mutex_)> lock(mapping_mutex_);
  return mapping_;
}

void LttngProvider::updateMapping()
{
  std::lock_guard<decltype(mapping_mutex_)> lock(mapping_mutex_);
  mapping_.clear();

  auto endpoints = manager_->endpoints();
  for (const auto& transport_endpoints : endpoints)
  {
    ProcessMapping remote_info;
    // try to find the EndpointProcessInfo
    auto endpoint_ptr = EndpointManager::findEndpoint<scalopus::EndpointProcessInfo>(transport_endpoints.second);
    if (endpoint_ptr != nullptr)
    {
      remote_info.info = endpoint_ptr->processInfo();
    }

    // Try to find the scope tracing endpoint and obtain its data.
    auto endpoint_scope_tracing = EndpointManager::findEndpoint<scalopus::EndpointScopeTracing>(transport_endpoints.second);
    if (endpoint_scope_tracing != nullptr)
    {
      remote_info.trace_ids = endpoint_scope_tracing->mapping();
    }
    mapping_[remote_info.info.id] = remote_info;
  }
}

bool LttngProvider::getScopeName(unsigned int pid, unsigned int trace_id, std::string& name)
{
  std::lock_guard<decltype(mapping_mutex_)> lock(mapping_mutex_);
  auto pid_info = mapping_.find(pid);
  if (pid_info != mapping_.end())
  {
    auto entry_mapping = pid_info->second.trace_ids.find(trace_id);
    if (entry_mapping != pid_info->second.trace_ids.end())
    {
      // yay! We found the appopriate mapping for this trace id.
      name = entry_mapping->second;
      return true;
    }
  }
  std::stringstream z;
  z << "Unknown 0x" << std::hex << trace_id;
  name = z.str();
  return false;
}

}  // namespace scalopus