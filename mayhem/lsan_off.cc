// Leak detection is off for every target (SPEC §6.2 item 15): leaks are not the bug class we fuzz for,
// and zelda3 keeps config/shader strings alive for the process lifetime by design.
extern "C" int __lsan_is_turned_off() { return 1; }
