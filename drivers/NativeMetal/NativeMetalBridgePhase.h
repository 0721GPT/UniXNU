// SPDX-License-Identifier: MIT
// Plugin-only phase after Turnip: the Metal bridge self-test (bridge/NativeMetalBridge.rs) on the same Turnip, on an 8 MiB worker under a
// watchdog. Markers 0x6501..0x650f come from the bridge itself; the result line goes to GPU7 ("M ..."), a stuck worker is reported there
// as "MW <last marker> <seconds>".
// After a passing self-test the worker runs the frontend checks (bridge/FrontendChecks.h) through the public Metal API on the native frontend
// (NativeBridgeMetalFrontend.mm) over the same Turnip: markers 0x6600 (device), 0x6601..0x660f, then 0x66f0 pass / 0x66f1 fail. Its verdict
// ends the same GPU7 line: "F0" (with the first unimplemented selector, if any), "F<failed> <marker> <first failure>", "Fo"/"Fx" + reason
// when the device or the fixtures were refused.
#pragma once
#include "bridge/NativeMetalBridge.h"
#include "bridge/NativeMetalObjects.h"
#include "bridge/FrontendChecks.h"
#include "NativeBridgeMetalFrontend.h"
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bridgephase {
struct Job {
    const char *turnip;
    char cache[512];
    NmbSelftest result;
    uint32_t code;
    frontendchecks::Report frontend;
    bool frontendRan, frontendOpened;
    char missing[64], error[48];
    volatile uint32_t last;
    volatile bool done;
};
static Gpu *gGpu;
static Job *gJob;
static void mark(uint32_t value) {
    if (gJob) gJob->last = value;
    if (gGpu) gGpu->mark(value);
}
static void missingSelector(const char *cls, const char *selector) {
    if (gJob && !gJob->missing[0]) snprintf(gJob->missing, sizeof gJob->missing, "FM %.20s %.36s", cls, selector);
}
// The frontend checks on the native frontend over Turnip (QuartzCore is already translated by the self-test, so it is not repeated).
static void frontend(Job *job) {
    mark(0x6600);
    id device = native_bridge_metal_open(job->turnip, job->cache, 0, missingSelector);
    // The runtime's reasons are per thread: keep this worker's for the report.
    if (!device) {strlcpy(job->error, nmb_last_error(), sizeof job->error); return;}
    job->frontendOpened = true;
    size_t bytes = 0;
    const void *air = nmb_fixture_air(&bytes);
    id library = native_bridge_metal_library_with_air(air, bytes);
    air = nmb_function_constant_air(&bytes);
    id fcLibrary = native_bridge_metal_library_with_air(air, bytes);
    if (!library || !fcLibrary) {strlcpy(job->error, nmb_last_error(), sizeof job->error); return;}
    job->frontend.mark = mark;
    job->frontendRan = true;
    frontendchecks::run(device, library, fcLibrary, nil, &job->frontend);
}
static void *worker(void *arg) {
    Job *job = static_cast<Job *>(arg);
    // The target's own compositor library, read in place (never copied anywhere).
    const uint8_t *qc = nullptr;
    size_t qcBytes = 0;
    const int fd = open("/System/Library/Frameworks/QuartzCore.framework/default.metallib", O_RDONLY);
    struct stat st = {};
    if (fd >= 0 && fstat(fd, &st) == 0 && st.st_size > 0) {
        void *m = mmap(nullptr, (size_t)st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (m != MAP_FAILED) {qc = static_cast<const uint8_t *>(m); qcBytes = (size_t)st.st_size;}
    }
    if (fd >= 0) close(fd);
    job->code = nmb_selftest(job->turnip, job->cache, qc, qcBytes, mark, &job->result);
    if (qc) munmap(const_cast<uint8_t *>(qc), qcBytes);
    if (!job->code) frontend(job);
    __atomic_store_n(&job->done, true, __ATOMIC_RELEASE);
    return nullptr;
}
static void run(Gpu &gpu, const char *turnipPath) {
    gpu.mark(0x6500);
    Job *job = new Job{};
    job->turnip = turnipPath;
    // A per-user cache directory of the host process (its sandbox allows it); the translated-shader cache goes in a subdirectory.
    if (!confstr(_CS_DARWIN_USER_CACHE_DIR, job->cache, sizeof job->cache - 16)) strlcpy(job->cache, "/tmp/", sizeof job->cache);
    strlcat(job->cache, "m2v", sizeof job->cache);
    mkdir(job->cache, 0700);
    gGpu = &gpu;
    gJob = job;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 8u << 20);
    pthread_t thread;
    const bool started = pthread_create(&thread, &attr, worker, job) == 0;
    pthread_attr_destroy(&attr);
    if (!started) {gpu.mark(0x65fd); return;}
    static const unsigned checkpoints[] = {30, 90, 180};
    unsigned next = 0;
    for (unsigned tenths = 0; !__atomic_load_n(&job->done, __ATOMIC_ACQUIRE); ++tenths) {
        usleep(100000);
        if (next < 3 && tenths == checkpoints[next] * 10) {
            char text[64];
            snprintf(text, sizeof text, "MW %x %us", job->last, checkpoints[next]);
            streamText7(gpu, text);
            ++next;
        }
    }
    pthread_join(thread, nullptr);
    const NmbSelftest &r = job->result;
    // One GPU7 line (the page keeps only the latest): the self-test's counts, then after a passing self-test the frontend's verdict.
    char text[64];
    const int at = snprintf(text, sizeof text, "M s%x c%u r%u i%u q%u k%u/%u p%u/%u", r.stage, r.compute_bad, r.render_bad, r.import_bad,
                            r.qc_functions, r.qc_kernels_ok, r.qc_kernels_failed, r.qc_pairs_ok, r.qc_pairs_failed);
    char *rest = text + (at > 0 && at < (int)sizeof text ? at : 0);
    const size_t room = sizeof text - (size_t)(rest - text);
    if (job->code) snprintf(rest, room, " %s", r.message);
    else if (!job->frontendOpened) snprintf(rest, room, " Fo %s", job->error);
    else if (!job->frontendRan) snprintf(rest, room, " Fx %s", job->error);
    else if (job->frontend.failed) snprintf(rest, room, " F%u %s", job->frontend.failed, job->frontend.first);
    else snprintf(rest, room, " F0%s%s", job->missing[0] ? " " : "", job->missing[0] ? job->missing + 3 : "");
    streamText7(gpu, text);
    const bool ok = !job->code && !r.compute_bad && !r.render_bad && !r.import_bad;
    gpu.mark(ok ? 0x65f0 : 0x65f1);
    if (!job->code) gpu.mark(job->frontendRan && !job->frontend.failed ? 0x66f0 : 0x66f1);
    gJob = nullptr;
    gGpu = nullptr;
    delete job;
}
}  // namespace bridgephase
