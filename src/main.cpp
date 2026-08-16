#include <iostream>
#include <print>
#include <stop_token>
#include <thread>
#include <optional>

#include <Kurumi/RingBuffer.hpp>

#include <stdexec/execution.hpp>

int main() {
    stdexec::counting_scope _scope;
    kurumi::RingBuffer<std::string> _buffer(1024);

    stdexec::sender auto _sndr =
        stdexec::just(std::ref(_buffer))
        | stdexec::continues_on(stdexec::get_parallel_scheduler())
        | stdexec::then([](kurumi::RingBuffer<std::string>& buffer) {
            const auto _get_message = [&]() -> std::optional<std::string> {
                if (auto _message = buffer.acquire(); _message != "<END>") {
                    return std::optional<std::string>{ std::in_place, std::move(_message) };
                }

                return std::nullopt;
            };

            while (auto _message = _get_message()) {
                std::cout << std::format("[{}] Receive message: {}", std::this_thread::get_id(), _message.value()) << std::endl;
            }
        })
        | stdexec::let_error([](auto&&) noexcept {
            return stdexec::just();
        });

    stdexec::spawn(_sndr, _scope.get_token());

    do {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::string _message;
        std::cout << std::format("[{}] Send message: ", std::this_thread::get_id());
        std::cin >> _message;

        _buffer.force_emplace(std::move(_message));

        if (_message == "<END>") {
            break;
        }
    } while (true);

    stdexec::sync_wait(_scope.join());

    std::println("[{}] Broadcast ended", std::this_thread::get_id());

    return 0;
}