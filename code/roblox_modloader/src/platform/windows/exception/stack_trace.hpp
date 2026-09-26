#pragma once

#include <string_view>

namespace rml::exception_filter
{
	struct StackTraceLabels
	{
		std::string_view heading;
		std::string_view frame;
		std::string_view chain;
	};

	void log_stack_trace(const StackTraceLabels& labels);
}
