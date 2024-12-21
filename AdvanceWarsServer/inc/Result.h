#pragma once

enum class Result : int
{
	Failed = -1,
	Succeeded = 0,
};

#define IfFailedReturn(r) \
	if ((r) != Result::Succeeded) { \
		return (r); \
	}
