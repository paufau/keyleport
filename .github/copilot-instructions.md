<instructions>
<build_instructions>
- Only use "./scripts/compile.sh" to build the project or to check if its compilable
</build_instructions>

<classes_structs_apis>
Use struct for simple data aggregates without invariants; use class for behavior.
Default constructor/destructor with = default; where no custom logic is needed.
Keep public API small; make helper functions private or file-static.
Provide minimal getters/setters; expose immutable views when possible (return by value for small types, by const-ref if large).
Mark trivial methods inline in headers only when necessary.
</classes_structs_apis>

<naming>
current code uses snake_case class names—choose one and be consistent; given existing code, keep snake_case for classes in this project until a global refactor.
Methods and functions: snake_case (e.g., poll_events, flush_pending_messages).
Member fields: trailing underscore (e.g., peer_, host_).
Constants: kPascalCase for file-scope or class-scope constants (e.g., kChannelReliable, kMaxPerPoll).
Namespaces: all lowercase.
</naming>

<error_handling_logging>
Use std::cerr for errors, std::cout for essential info. Avoid noisy logs in hot paths; gate verbose logs with a local constexpr bool kVerbose = false; and wrap debug prints.
Keep log lines single-line, prefixed by a component tag: [udp_client] ..., [udp_server] ..., [communication_service] ....
For recoverable issues, log and continue; for unrecoverable, return early with a clear error line.
</error_handling_logging>

<concurrency_threading>
Hold the smallest possible critical sections; prefer std::lock_guard<std::mutex> for short-lived locks.
Do not call out to user callbacks while holding internal locks; copy subscriber list first (as done in event_emitter).
Background service loop cadence: target 1ms sleep or tuned value; any blocking work must be segmented or offloaded.
Document preconditions for methods that assume external locking (e.g., flush_service_events() assumes mutex_ held).
</concurrency_threading>

<events_messaging>
Use utils::event_emitter<T> for intra-process pub/sub; copy callbacks under lock and invoke after releasing the lock.
Avoid blocking in event handlers; offload heavy work.
For cross-service events (e.g., networking to GUI), keep payloads minimal and typed (typed_package with JSON payload only when not performance critical).
</events_messaging>

<service_lifecycle>
All services implement service_lifecycle_listener with init(), update(), cleanup().
Do not hold locks across init/cleanup; ensure resources created in init are fully released in cleanup.
Background thread owned by main_loop iterates services; services should return quickly from update().
</service_lifecycle>

<data_json>
Use nlohmann::json minimally in hot paths; keep encode/decode methods in data types like typed_package.
Prefer strict parsing with sane defaults; handle parse errors gracefully.
</data_json>

<headers_vs_source>
Keep function bodies out of headers unless performance or templating requires it.
Avoid including .cpp files; compile units should be independent.
</headers_vs_source>

<platform_boundaries>
Place OS-specific implementations under macos/ and windows/ with a shared interface in the parent folder; choose at compile time or runtime via a small factory/helper.
</platform_boundaries>

<build_scripts>
Only build with: compile.sh
Keep third-party integrations isolated (ENet, SDL, ImGui) and configured via CMake’s FetchContent as currently done.
</build_scripts>

<misc_rules>
Avoid global state; if needed, use a service_locator with clear ownership semantics.
Keep magic numbers as named constants.
Prefer std::chrono for time and monotonic clocks for intervals.
Short, focused methods; early returns on error; consistent logging prefix.
</misc_rules>
</instructions>