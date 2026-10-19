#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "capability.h"
#include "../task/process.h"
#include "../task/scheduler.h"

// Mock implementations for testing
static uint8_t bootstrap_stack[32768] __attribute__((aligned(16)));

bool orion_x86_64_interrupts_enabled(void)
{
    return false;
}

bool orion_x86_64_interrupts_save_disable(void)
{
    return false;
}

void orion_x86_64_interrupts_restore(bool was_enabled)
{
    assert(!was_enabled);
}

void orion_x86_64_interrupts_disable(void)
{
}

void orion_x86_64_interrupts_enable(void)
{
    abort();
}

void orion_x86_64_context_switch(uint64_t **old_stack_pointer,
                                 uint64_t *new_stack_pointer)
{
    (void)old_stack_pointer;
    (void)new_stack_pointer;
    abort();
}

void orion_x86_64_thread_start_trampoline(void)
{
    abort();
}

void orion_x86_64_thread_returned(void)
{
    abort();
}

static bool test_basic_operations(void)
{
    // Initialize scheduler and capability system
    assert(orion_scheduler_init((uintptr_t)bootstrap_stack,
                                (uintptr_t)(bootstrap_stack +
                                            sizeof(bootstrap_stack))));
    assert(orion_capability_system_init());
    
    // Create a test process
    orion_process test_process;
    assert(orion_capability_process_init(&test_process));
    
    // Test creating a kernel object
    int test_object = 42;
    orion_capability_handle handle;
    assert(orion_capability_create_kernel_object(
        &test_process,
        ORION_OBJECT_DEVICE,
        &test_object,
        ORION_RIGHT_USE | ORION_RIGHT_MODIFY,
        NULL,  // No destructor for simple integer
        &handle) == ORION_CAPABILITY_OK);
    
    // Test describing the object
    orion_capability_info info;
    assert(orion_capability_describe(
        &test_process,
        handle,
        &info) == ORION_CAPABILITY_OK);
    
    assert(info.type == ORION_OBJECT_DEVICE);
    assert((info.rights & (ORION_RIGHT_USE | ORION_RIGHT_MODIFY)) == 
           (ORION_RIGHT_USE | ORION_RIGHT_MODIFY));
    assert((info.rights & ~(ORION_RIGHT_USE | ORION_RIGHT_MODIFY)) == 0);
    
    // Test acquiring a lease
    orion_capability_lease lease;
    assert(orion_capability_acquire(
        &test_process,
        handle,
        ORION_OBJECT_DEVICE,
        ORION_RIGHT_USE,
        &lease) == ORION_CAPABILITY_OK);
    
    assert(lease.object == &test_object);
    assert(lease.active == true);
    
    // Test releasing the lease
    assert(orion_capability_lease_release(
        &test_process,
        &lease) == ORION_CAPABILITY_OK);
    
    assert(lease.active == false);
    
    // Test closing the handle
    assert(orion_capability_close(
        &test_process,
        handle) == ORION_CAPABILITY_OK);
    
    // Clean up process
    assert(orion_capability_process_release(&test_process) == ORION_CAPABILITY_OK);
    
    // Check invariants
    assert(orion_capability_check_invariants());
    
    return true;
}

static void test_rights_management(void)
{
    // Initialize scheduler and capability system
    assert(orion_scheduler_init((uintptr_t)bootstrap_stack,
                                (uintptr_t)(bootstrap_stack +
                                            sizeof(bootstrap_stack))));
    assert(orion_capability_system_init());
    
    // Create a test process
    orion_process test_process;
    assert(orion_capability_process_init(&test_process));
    
    // Test creating an object with all rights
    int test_object = 123;
    orion_capability_handle handle;
    assert(orion_capability_create_kernel_object(
        &test_process,
        ORION_OBJECT_THREAD,
        &test_object,
        ORION_RIGHT_ALL,
        NULL,
        &handle) == ORION_CAPABILITY_OK);
    
    // Test deriving a capability with reduced rights
    orion_capability_handle derived_handle;
    assert(orion_capability_derive(
        &test_process,
        handle,
        ORION_RIGHT_USE | ORION_RIGHT_QUERY,  // Only use and query
        &derived_handle) == ORION_CAPABILITY_OK);
    
    // Verify the derived capability has the correct rights
    orion_capability_info info;
    assert(orion_capability_describe(
        &test_process,
        derived_handle,
        &info) == ORION_CAPABILITY_OK);
    
    assert(info.type == ORION_OBJECT_THREAD);
    assert((info.rights & (ORION_RIGHT_USE | ORION_RIGHT_QUERY)) == 
           (ORION_RIGHT_USE | ORION_RIGHT_QUERY));
    // Should NOT have modify, admin, etc.
    assert((info.rights & ~(ORION_RIGHT_USE | ORION_RIGHT_QUERY)) == 0);
    
    // Test transferring the derived capability
    orion_process target_process;
    assert(orion_capability_process_init(&target_process));
    
    orion_capability_handle transferred_handle;
    assert(orion_capability_transfer(
        &test_process,
        derived_handle,
        &target_process,
        ORION_RIGHT_USE,  // Further reduce to just use
        &transferred_handle) == ORION_CAPABILITY_OK);
    
    // Verify the transferred capability has the correct rights
    assert(orion_capability_describe(
        &target_process,
        transferred_handle,
        &info) == ORION_CAPABILITY_OK);
    
    assert(info.type == ORION_OBJECT_THREAD);
    assert(info.rights == ORION_RIGHT_USE);
    assert(info.owner == &target_process);
    
    // Clean up
    assert(orion_capability_close(&target_process, transferred_handle) == ORION_CAPABILITY_OK);
    assert(orion_capability_process_release(&target_process) == ORION_CAPABILITY_OK);
    assert(orion_capability_close(&test_process, handle) == ORION_CAPABILITY_OK);
    assert(orion_capability_process_release(&test_process) == ORION_CAPABILITY_OK);
    
    // Check invariants
    assert(orion_capability_check_invariants());
}

static void test_error_conditions(void)
{
    // Initialize scheduler and capability system
    assert(orion_scheduler_init((uintptr_t)bootstrap_stack,
                                (uintptr_t)(bootstrap_stack +
                                            sizeof(bootstrap_stack))));
    assert(orion_capability_system_init());
    
    // Create a test process
    orion_process test_process;
    assert(orion_capability_process_init(&test_process));
    
    // Test creating an object with invalid rights
    int test_object = 456;
    orion_capability_handle handle;
    assert(orion_capability_create_kernel_object(
        &test_process,
        ORION_OBJECT_DEVICE,
        &test_object,
        ORION_RIGHT_ALL | 0xFF00,  // Invalid rights
        NULL,
        &handle) == ORION_CAPABILITY_INVALID_ARGUMENT);
    
    // Test using a null process pointer
    assert(orion_capability_create_kernel_object(
        NULL,
        ORION_OBJECT_DEVICE,
        &test_object,
        ORION_RIGHT_USE,
        NULL,
        &handle) == ORION_CAPABILITY_INVALID_ARGUMENT);
    
    // Test using a null object pointer
    assert(orion_capability_create_kernel_object(
        &test_process,
        ORION_OBJECT_DEVICE,
        NULL,
        ORION_RIGHT_USE,
        NULL,
        &handle) == ORION_CAPABILITY_INVALID_ARGUMENT);
    
    // Clean up
    assert(orion_capability_process_release(&test_process) == ORION_CAPABILITY_OK);
    
    // Check invariants
    assert(orion_capability_check_invariants());
}

int main(void)
{
    test_basic_operations();
    test_rights_management();
    test_error_conditions();
    return 0;
}