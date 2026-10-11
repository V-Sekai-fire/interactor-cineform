// Submitting more frames than the encoder pool's job queue holds must not hang.
//
// The pool frees a queue slot only when the caller takes a finished sample out with
// CFHD_WaitForSample or CFHD_TestForSample. CFHD_EncodeAsyncSample on a full queue waits
// for a slot, and the only thread that could free one is the thread now waiting. So a
// submitter that does not drain before it submits deadlocks once the queue fills.
//
// One encoder thread gives a queue depth of two, so the third frame meets a full queue.
// Ten frames is enough to reach that state on every run.
//
// NEGATIVE CONTROL. The same program is built a second time with CINEFORM_PLANT_NO_CAP,
// which compiles the cap out of Encoder::submit. That build must hang, and the watchdog
// turns the hang into exit 1, so CTest runs it with WILL_FAIL. If the planted build passes,
// this proof cannot see the defect it is here for.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
#include "encoder.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uint32_t WIDTH = 3840;
constexpr uint32_t HEIGHT = 2160;
constexpr uint32_t FRAMES = 10;
constexpr uint32_t THREADS = 1;
constexpr auto WATCHDOG = std::chrono::seconds(20);

std::mutex mutex;
std::condition_variable done_signal;
bool done = false;
bool ok = false;
uint32_t samples = 0;
std::string error;

void encode() {
	std::vector<uint8_t> frame(size_t(WIDTH) * HEIGHT * 4u);
	cineform::Encoder encoder;
	std::string why;
	bool good = encoder.start(WIDTH, HEIGHT, 2, THREADS, false, &why);
	uint32_t count = 0;
	const cineform::Encoder::OnSample on_sample = [&](const void *, size_t len) {
		if (len > 0) {
			count++;
		}
	};
	for (uint32_t i = 0; good && i < FRAMES; i++) {
		// A different frame each time, so no layer can answer from a cache.
		for (size_t p = 0; p < frame.size(); p++) {
			frame[p] = uint8_t(p * 7u + i * 31u);
		}
		good = encoder.submit(frame.data(), on_sample, &why);
	}
	if (good) {
		encoder.drain(true, on_sample);
	}
	encoder.stop();

	std::lock_guard<std::mutex> lock(mutex);
	ok = good;
	samples = count;
	error = why;
	done = true;
	done_signal.notify_one();
}

} // namespace

int main() {
	std::printf("encoder queue: %u frames at %ux%u through %u encoder thread\n", FRAMES, WIDTH,
			HEIGHT, THREADS);

	std::thread(encode).detach();

	std::unique_lock<std::mutex> lock(mutex);
	if (!done_signal.wait_for(lock, WATCHDOG, [] { return done; })) {
		std::printf("  FAIL no result after %lld s: the submitter is stuck on a full queue\n",
				(long long)WATCHDOG.count());
		std::fflush(stdout);
		// The encoder thread is blocked inside the SDK and cannot be joined or stopped.
		std::_Exit(1);
	}
	if (!ok) {
		std::printf("  FAIL %s\n", error.c_str());
		return 1;
	}
	if (samples != FRAMES) {
		std::printf("  FAIL %u samples came back for %u frames\n", samples, FRAMES);
		return 1;
	}
	std::printf("  ok   %u samples for %u frames\n", samples, FRAMES);
	return 0;
}
