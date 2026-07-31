#pragma once

// Reserves the Cardputer SMF task and bounded parser storage while internal
// RAM is still contiguous. Safe to call again after a successful start.
bool beginCardputerSmfPlayerService();
