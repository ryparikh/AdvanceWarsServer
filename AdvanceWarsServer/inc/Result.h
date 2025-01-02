#pragma once

enum class Result : int
{
	Failed = -1,
	Succeeded = 0,
};

#define IfFailedReturn(r) \
	if (Result _result = (r); _result != Result::Succeeded) { \
		return _result; \
	}
