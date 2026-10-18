# Driver lifecycle

States implemented by the bounded registry:

```text
DISCOVERED -> MATCHED -> STARTING -> ONLINE -> STOPPED
                              \-> FAILED
```

Registration rejects null callbacks, invalid/overlong names and duplicate
driver names. Device insertion rejects invalid bus identities, malformed
platform names, absent PCI vendor/device IDs, invalid device/function values,
excess BAR count, duplicate bus locations and full inventory. One device has
at most one active binding.

Start and cleanup callbacks execute outside the registry's interrupt-disabled
critical sections. If start fails, cleanup is called with any partial
driver-data value and the device is left FAILED and unbound. Stop is accepted
only from ONLINE and cleanup runs once. Retry, physical hot-unplug, concurrent
unbind, and asynchronous quiescence are not implemented. All current calls are
performed during single-core boot before general scheduler activity.
