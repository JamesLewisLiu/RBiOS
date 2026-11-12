#include "rb/AvsReservedAlarmTask.hpp"
#include "rb/AvsThreadLocal.hpp"
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>

namespace {
void require(bool value) { if (!value) throw std::runtime_error("AVS reserved alarm task regression"); }
}
int main() { try {
    rb::AvsDescriptors descriptors(16); rb::AvsMutexes mutexes(descriptors,4);
    rb::AvsAlarms alarms(descriptors,2,50);
    rb::AvsReservedAlarmTask task(descriptors,mutexes,alarms);
    const auto mutex = mutexes.create();
    std::mutex testMutex; std::condition_variable changed; bool called = false, rejectedSelfWait = false;
    uintptr_t callbackIdentity = 0;
    const auto alarm = alarms.create([&](uint64_t) {
        callbackIdentity = rb::AvsThreadLocal::currentIdentity();
        require(callbackIdentity != 0 && rb::AvsThreadLocal::currentDescriptor() == 0x40003);
        mutexes.lock(mutex,callbackIdentity); mutexes.unlock(mutex);
        try { task.wait(); } catch (const std::logic_error &) { rejectedSelfWait = true; }
        try { task.stop(); require(false); } catch (const std::logic_error &) {}
        task.requestStop();
        { std::lock_guard<std::mutex> lock(testMutex); called = true; changed.notify_all(); }
        return false;
    });
    alarms.set(alarm,1); task.start();
    { std::unique_lock<std::mutex> lock(testMutex); require(changed.wait_for(lock,std::chrono::seconds(3),[&]{return called;})); }
    require(task.wait() == 0 && rejectedSelfWait);
    const auto running = task.status();
    require(running.taskEntered && running.completed && !running.failed && running.workerIdentity == callbackIdentity);
    require(running.requestedStack == 4096 && running.priority == 0 && running.affinity == -1);
    require(rb::AvsThreadLocal::currentIdentity() == 0);
    task.stop();
    const auto stopped = task.status(); require(stopped.joined && !stopped.nativeStarted && stopped.remainingUses == 0);
    uintptr_t payload = 1;
    require(descriptors.lookup(0x40003,&payload) && payload == 0);
    bool rejectedRestart = false;
    try { task.start(); } catch (const std::logic_error &) { rejectedRestart = true; }
    require(rejectedRestart);

    rb::AvsDescriptors cancelledDescriptors(8); rb::AvsMutexes cancelledMutexes(cancelledDescriptors,2);
    rb::AvsAlarms cancelledAlarms(cancelledDescriptors,1,50);
    rb::AvsReservedAlarmTask cancelled(cancelledDescriptors,cancelledMutexes,cancelledAlarms);
    cancelled.requestStop(); bool cancellation = false;
    try { cancelled.start(); } catch (const std::logic_error &) { cancellation = true; }
    require(cancellation && !cancelled.status().taskEntered && !cancelled.status().completed);
    require(cancelledDescriptors.lookup(0x40003,&payload) && payload == 0);

    rb::AvsDescriptors failedDescriptors(8); rb::AvsMutexes failedMutexes(failedDescriptors,2);
    rb::AvsAlarms failedAlarms(failedDescriptors,1,50);
    rb::AvsReservedAlarmTask failed(failedDescriptors,failedMutexes,failedAlarms);
    const auto held = failedMutexes.create();
    auto throwing = failedAlarms.create([&](uint64_t) -> bool {
        failedMutexes.lock(held,rb::AvsThreadLocal::currentIdentity());
        throw std::runtime_error("intentional alarm callback failure");
    });
    failedAlarms.set(throwing,1);
    try { failed.start(); } catch (const std::runtime_error &) {}
    bool failure = false;
    try { failed.wait(); } catch (const std::runtime_error &) { failure = true; }
    require(failure);
    try { failed.stop(); } catch (const std::runtime_error &) {}
    require(failed.status().failed && failed.status().joined && !failed.status().completed);
    require(failedDescriptors.lookup(0x40003,&payload) && payload == 0);
    int identityOwner = 0; const auto identity = reinterpret_cast<uintptr_t>(&identityOwner);
    rb::AvsThreadLocal::bind(identity); failedMutexes.registerCurrentThread(identity);
    failedMutexes.lock(held,identity); failedMutexes.unlock(held);
    failedMutexes.unregisterCurrentThread(); rb::AvsThreadLocal::unbind(identity);

    rb::AvsDescriptors occupiedDescriptors(8); rb::AvsMutexes occupiedMutexes(occupiedDescriptors,2);
    rb::AvsAlarms occupiedAlarms(occupiedDescriptors,1,50);
    occupiedDescriptors.replace(0x40003,nullptr,0x1234);
    {
        rb::AvsReservedAlarmTask occupied(occupiedDescriptors,occupiedMutexes,occupiedAlarms);
        bool rejected = false; try { occupied.start(); } catch (const std::logic_error &) { rejected = true; }
        require(rejected && occupied.status().failed && !occupied.status().nativeStarted);
    }
    require(occupiedDescriptors.lookup(0x40003,&payload) && payload == 0x1234);
    std::cout << "PASS reserved AVS alarm worker: real TLS/mutex identity, source settings, completion, cancellation, failure and teardown\n";
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }}
