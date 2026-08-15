#include <print>
#include <thread>
#include <stop_token>

#include <stdexec/execution.hpp>

int main() {
    stdexec::run_loop _loop;

    std::jthread _thread([&](std::stop_token st) {
        std::stop_callback sc(st, [&]() {
            _loop.finish();
        });

        _loop.run();
    });

    stdexec::sender auto _sndr =
        stdexec::just("Hello Kurumi!")
        | stdexec::then([](std::string_view message) {
            std::println("[{}] Receive message: {}", std::this_thread::get_id(), message);
        });

    stdexec::sync_wait(stdexec::starts_on(_loop.get_scheduler(), _sndr));

    return 0;
}