# envyd
Daemon for `nvml`, to configure & monitor NVIDIA GPUs on the fly.

This is a (typically `systemd`) daemon that will serve the `nvml` API conveniently over a (unix, by default) socket. \
The service was initially created to serve as a backend for software like [MSI Afterburner](https://www.msi.com/Landing/afterburner/graphics-cards), albeit in Linux. 

* [usage](#usage)
  + [request template](#request-template)
  + [response template](#response-template)
* [cookbook / examples](#cookbook---examples)
  + [`nvmlDeviceGetDetailsAll` (starting point, `envyd` specific endpoint)](#-nvmldevicegetdetailsall---starting-point---envyd--specific-endpoint-)
    - [request](#request)
    - [response](#response)
  + [`nvmlDeviceGetPowerManagementLimitConstraints`](#-nvmldevicegetpowermanagementlimitconstraints-)
    - [request](#request-1)
    - [response](#response-1)
* [special actions (i.e. endpoints that do not match the `nvml` API)](#special-actions--ie-endpoints-that-do-not-match-the--nvml--api-)
  + [`nvmlDeviceGetDetailsAll`](#-nvmldevicegetdetailsall-)
* [special statuses (i.e. not belonging to nvmlReturn_t)](#special-statuses--ie-not-belonging-to-nvmlreturn-t-)
* [contributing](#contributing)
* [license](#license)

## usage
Wherever 'nvmlDevice_t device' appears on the parameters of a function in the NVIDIA documentation,
substitute that parameter with `uuid`, which is the unique identifier for the specific GPU device.
Some actions might have slightly different names; actions will not have a version prefix, for example,
`nvmlDeviceGetMemoryInfo_v2` is a call to `"action": "nvmlDeviceGetMemoryInfo"`. The version is **dropped**.
This is done to ensure compatibility, in every case except the entire namespace drop from NVIDIA.
This also helps reduce complexity in client code! `envyd` will call the version that is best for your GPU.

Some NVIDIA parameters might also be ignored, for the aforementioned reasons. 
For example, `nvmlDeviceGetThermalSettings` does not need the `sensorIndex`; 
it'll just provide all sensor information that are present in the current library.

When a parameter to an API is invalid, a detailed text will be provided.

### request template
```json
{
    "bearer": "token", // OPTIONAL
    "action": "nvmlDeviceGetMemoryInfo_v2",  // REQUIRED
    "uuid": "GPU-uuid-thingy-here" // OPTIONAL (required for anything that specifies a 'nvmlDevice_t' as an argument)
}
```
Note:
- `bearer`: OPTIONAL. The `bearer` field is optional currently, and unused. It is planned for a future version, in order to keep track of access rights.
- `action`: REQUIRED. The `action` field is an internal `_handler` mapped name. Special `action`s exist, however most will be 1:1 with the [official NVIDIA documentation for nvml](https://docs.nvidia.com/deploy/nvml-api/group__nvmlDeviceQueries.html#group__nvmlDeviceQueries) function names.
- The `uuid` field is an argument that is endpoint-specific. 
  Wherever 'nvmlDevice_t device' appears on the parameters of a function in the NVIDIA documentation,
  substitute that parameter with `uuid`, which is the unique identifier for the specific GPU device.
- Other arguments might be required (or not), for example `nvmlDeviceGetThermalSettings` requires both a `uuid` and `sensorIndex` argument.
- Other endpoints might take no arguments at all, for example `nvmlDeviceGetDetailsAll` is a 'special' endpoint (`action`) that does not exist in the NVIDIA documentation; it conveniently groups multiple `nvml` calls that probably belong together.

The best thing you can do to learn this daemon, is to start experimenting with `netcat`. An example call is provided below. \
If you want to get into the nitty-gritty of this daemon, the collection of `actions` will be in `network.c -> assign_task`. Enjoy.

### response template
```json
{
  // - this will be null in case of errors
  // - this will also be null in cases where return type is 'void' (of the appropriate `nvml` API)
  "data" : ...,
  "status": "NVML_SUCCESS",  // or any other value from nvmlReturn_t, and/or special errors
  "description": "NVML operation was successful."  // some sort of helpful description, hopefully
}
```
Note:
- Any of these fields can be null; no guarantee of 'correctness' is given for any of these fields.
- The `data` field serves as the 'body' of the response. If you are expecting any response, this is it. This can be either a composite (json object, json array),
  or a primitive (number, boolean, etc). In essence, there are no guarantees what the type of this might be, it'll be what is considered most convenient for any specific field. 
- To check for success, verify that the `status` field evaluates to `nvml_SUCCESS`. Anything else is considered a **failure**.
  This field is almost 1:1 with `nvml`. However, since other types of errors can happen, that `nvml` could not possibly have any knowledge of,
  'special' errors exist here. They will be documented here, otherwise they will be in `network.h`.
- The description serves as a human-readable way to understand what went wrong. Excuse the generic error messages, if do you stumble upon any.  


## cookbook / examples
Test actions via `netcat`, `jq` required for formatting purposes
### `nvmlDeviceGetDetailsAll` (starting point, `envyd` specific endpoint)
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
#### request 
```json
{"action": "nvmlDeviceGetDetailsAll"}
```
#### response
```json
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
### `nvmlDeviceGetPowerManagementLimitConstraints`
```bash
> echo '{"action": "nvmlDeviceGetPowerManagementLimitConstraints", "uuid": "GPU-06358cc0-eaaa-36de-0ec6-02c0be62ddef"}' | jq . | nc -NU '/tmp/envyd.socket' | jq .
{
  "data": {
    "minLimit": 125000,
    "maxLimit": 250000
  },
  "status": "NVML_SUCCESS",
  "description": null
}
```
#### request
original NVML signature was: 
```
nvmlReturn_t nvmlDeviceGetPowerManagementLimitConstraints (nvmlDevice_t device, unsigned int* minLimit, unsigned int* maxLimit)
```
... converted into the following `envyd` call:
```json
{
  "action": "nvmlDeviceGetPowerManagementLimitConstraints",
  "uuid": "GPU-06358cc0-eaaa-36de-0ec6-02c0be62ddef"
}
```
remember, we substitute any `nvmlDevice_t` with the `uuid` of said device, which you can get through `nvmlDeviceGetDetailsAll`.

#### response
```json
{
  "data": {
    "minLimit": 125000,
    "maxLimit": 250000
  },
  "status": "NVML_SUCCESS",
  "description": null
}
```

## special actions (i.e. endpoints that do not match the `nvml` API)
These are all accessible in the same way as any other `action`.
```
nvmlDeviceGetDetailsAll
```
Details:
### `nvmlDeviceGetDetailsAll`
- arguments: `N/A`
- returns (on success):
```json
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
- does: for every GPU found, combines the output of the following commands:
  1. `nvmlDeviceGetUUID` -> `uuid` for the device; used for every other call that requires a `nvmlDevice_t`
  2. `nvmlDeviceGetName` -> human-readable name
  3. `nvmlDeviceGetGspFirmwareVersion`
  4. `nvmlDeviceGetGspFirmwareMode`

## special statuses (i.e. not belonging to nvmlReturn_t)
```
JSON_PARSING_FAILED
INVALID_JSON_SCHEMA
UNDEFINED_INVALID_ACTION
```

## contributing
The official scope of the project, is to simplify the life of anyone who's managing GPUS through `nvml` on Linux. \
To do so successfully & you want to help, this project needs the following four things to succeed:
1. Feedback. Please report back w/ your experience using this service! we're looking for practical feedback regarding the following: (1) error codes (status messages), (2) descriptions, (3) cohesion.
   Feedback regarding `nvml` itself is out of scope, as it is ownership of NVIDIA.
2. Scope. This service needs to expand its scope to anything that the `nvml` library itself supports; this is possible only by other people contributing! Register your own `nvmlMethodName_handler` in `network.c`, it should only take a few minutes.
3. Documentation. This service needs to enhance its documentation; not everyone is gifted with the same skills of communication, and this project is certainly no exception. 
   If you have the time, kindly write some documentation that you think might be obscure to people w/o the necessary business knowledge!  
4. Adoption. In an ideal world, there are no competing standards: there is only **one**, well-supported, community implementation, backed by a strict, well-written standard (in this case, the latest `nvml.h` serves this purpose). 
  To achieve this goal, if you're thinking about creating a tool that interacts with `nvml`, aka manages an NVIDIA GPU, consider using this tool instead of the alternatives!

## license
MIT License

Copyright (c) 2024 gerelef

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
