#pragma once

#define ASSUME(x) do { if ((x)) __builtin_unreachable(); } while(0)
