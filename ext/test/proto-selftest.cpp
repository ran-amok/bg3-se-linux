// proto-selftest.cpp — PORT (linux).
//
// Does the pinned protobuf-lite runtime, built here in the game's libc++ ABI (std::__2,
// _LIBCPP_ABI_VERSION=2), actually work?  Linking is not the question -- a std::string ABI
// disagreement between the runtime and the generated code links clean and then corrupts at
// the first serialize, so the check has to run.  This is the only executable in the ext
// build; it is EXCLUDE_FROM_ALL, so `ninja -C build/ext proto-selftest && ...` is the run.
//
// Two-sided, like every other calibration here: the round trip must come back byte for byte,
// and malformed input must be refused.  What counts as malformed is narrower than it looks:
// a *wire type* that does not match the field (field 1 as a varint,
// "\x08\x01") parses fine, because proto3 skips a field it cannot interpret as unknown.  So the
// negative cases are the two the runtime really does refuse: an invalid tag, and a length
// prefix that runs off the end of the buffer.
#include <Extender/Shared/ExtenderProtocol.pb.h>

#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool ok, char const* what)
{
    std::printf("%-5s %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) failures++;
}

int main()
{
    std::printf("sizeof(std::string) = %zu (libc++ ABI v2, the game's)\n", sizeof(std::string));

    bg3se::net::MessageWrapper msg;
    auto* lua = msg.mutable_post_lua();
    lua->set_channel_name("bg3se_sweep");
    lua->set_payload(std::string("\x00\x01\xfe\xff", 4));   // bytes, embedded NUL on purpose
    lua->set_module("ExtenderSampleMod");
    lua->set_request_id(0x1234);
    lua->set_serializer(bg3se::net::SERIALIZER_BINARY);

    std::string wire;
    check(msg.SerializeToString(&wire), "MessageWrapper serialises");
    check(wire.size() > 40, "the wire form is the length the fields need");

    bg3se::net::MessageWrapper back;
    check(back.ParseFromString(wire), "and parses back");
    check(back.msg_case() == bg3se::net::MessageWrapper::kPostLua, "oneof comes back as post_lua");
    check(back.post_lua().channel_name() == lua->channel_name(), "channel_name survives");
    check(back.post_lua().payload() == lua->payload(), "payload survives, NUL and all");
    check(back.post_lua().module() == lua->module(), "module survives");
    check(back.post_lua().request_id() == 0x1234u, "request_id survives");
    check(back.post_lua().serializer() == bg3se::net::SERIALIZER_BINARY, "serializer survives");
    std::string again;
    check(back.SerializeToString(&again) && again == wire, "re-serialises byte for byte");

    // the other side
    bg3se::net::MessageWrapper bad;
    check(!bad.ParseFromString(std::string("\x00\x01", 2)), "an invalid tag (field 0) is refused");
    check(!bad.ParseFromString(std::string("\x0a\x7f\x01", 3)), "a truncated submessage is refused");
    check(bad.ParseFromString(std::string("\x08\x01", 2)), "but a wrong wire type is not: proto3 skips it as unknown");

    std::printf("%s\n", failures ? "proto-selftest: FAILED" : "proto-selftest: ok");
    return failures ? 1 : 0;
}
