# envyd

## usage
Wherever 'nvmlDevice_t device' appears on the parameters of a function in the NVIDIA documentation,
substitute that parameter with `uuid`, which is the unique identifier for the specific GPU device.
Some actions might have slightly different names; actions will not have a version prefix, for example,
`nvmlDeviceGetMemoryInfo_v2` is a call to `"action": "nvmlDeviceGetMemoryInfo"`. The version is **dropped**.
This is done to ensure compatibility, in every case except the entire namespace drop from NVIDIA.
This also helps reduce complexity in client code! `envyd` will call the version that is best for your GPU.

When a parameter to an API is invalid, a detailed text will be provided.

### request template
```json
{
    "bearer": "token",
    "action": "nvmlDeviceGetMemoryInfo_v2",
    "uuid": "GPU-uuid-thingy-here"
}
```
Note:
- `bearer`: OPTIONAL. The `bearer` field is optional currently, and unused. It is planned for a future version, in order to keep track of access rights.
- `action`: REQUIRED. The `action` field is an internal `_handler` mapped name. Special `action`s exist, however most will be 1:1 with the [official NVIDIA documentation for NVML](https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries) function names.
- The `uuid` field is an argument that is endpoint-specific. 
  Wherever 'nvmlDevice_t device' appears on the parameters of a function in the NVIDIA documentation,
  substitute that parameter with `uuid`, which is the unique identifier for the specific GPU device.
- Other arguments might be required (or not), for example `nvmlDeviceGetThermalSettings` requires both a `uuid` and `sensorIndex` argument.
- Other endpoints might take no arguments at all, for example `nvmlDeviceGetDetailsAll` is a 'special' endpoint (`action`) that does not exist in the NVIDIA documentation; it conveniently groups multiple NVML calls that probably belong together.

The best thing you can do to learn this daemon, is to start experimenting with `netcat`. An example call is provided below. \
If you want to get into the nitty-gritty of this daemon, the collection of `actions` will be in `network.c -> assign_task`. Enjoy.

### response template
```json
{
  // - this will be null in case of errors
  // - this will also be null in cases where return type is 'void' (of the appropriate NVML API)
  "data" : ...,
  "status": "NVML_SUCCESS",  // or any other value from nvmlReturn_t, and/or special errors
  "description": "NVML operation was successful."  // some sort of helpful description, hopefully
}
```
Note:
- Any of these fields can be null; no guarantee of 'correctness' is given for any of these fields.
- The `data` field serves as the 'body' of the response. If you are expecting any response, this is it. This can be either a composite (json object, json array),
  or a primitive (number, boolean, etc). In essence, there are no guarantees what the type of this might be, it'll be what is considered most convenient for any specific field. 
- To check for success, verify that the `status` field evaluates to `NVML_SUCCESS`. Anything else is considered a **failure**.
  This field is almost 1:1 with `NVML`. However, since other types of errors can happen, that `NVML` could not possibly have any knowledge of,
  'special' errors exist here. They will be documented here, otherwise they will be in `network.h`.
- The description serves as a human-readable way to understand what went wrong. Excuse the generic error messages, if do you stumble upon any.  


## cookbook
Test actions via `netcat`, `jq` required for formatting purposes:
```bash
> echo '{"action": "nvmlDeviceGetDetailsAll"}' | jq . | nc -NU '/tmp/envyd.socket' | jq .
{
  "data": {
    "count": 1,
    "devices": [
      {
        "uuid": "GPU-06358cc0-eaaa-36de-0ec6-02c0be62ddef",
        "name": "NVIDIA GeForce RTX 2070 SUPER",
        "gsp_version": "560.35.03",
        "gsp_mode": 1,
        "gsp_default-mode": 1
      }
    ]
  },
  "status": "NVML_SUCCESS",
  "description": "Successfully successfully generated device list."
}
```

## special actions (i.e. not a 1:1 match with NVML documentation)
These are all accessible in the same way as any other `action`.
```
nvmlDeviceGetDetailsAll
```

## special statuses (i.e. not belonging to nvmlReturn_t)
```
JSON_PARSING_FAILED
INVALID_JSON_SCHEMA
UNDEFINED_INVALID_ACTION
```