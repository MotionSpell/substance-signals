#pragma once

#include "lib_modules/core/module.hpp" // KHost
#include "lib_modules/utils/helper.hpp" // span

// Single long running POST/PUT connection
struct HttpSender {
	virtual ~HttpSender() = default;
	virtual void send(span<const uint8_t> data) = 0;
	virtual void setPrefix(span<const uint8_t> prefix) = 0;
};

#include <string>
#include <vector>

// make an empty POST (used to check the end point exists)
void enforceConnection(std::string url, bool usePUT);

std::unique_ptr<HttpSender> createHttpSender(std::string url, std::string userAgent, bool usePUT, std::vector<std::string> extraHeaders, Modules::KHost* log);

