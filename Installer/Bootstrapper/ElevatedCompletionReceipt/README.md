# Elevated completion receipt gate

`ElevatedCompletionReceipt/` is the read-only bridge between the Windows elevation request result and the controlled elevated bootstrapper completion receipt.

It consumes:

```text
ElevationHelper result
ControlledElevationBootstrapper completion receipt
```

and emits one canonical authenticated observation with:

```text
package-engine.observe-elevated-completion
```

The gate verifies that the completion receipt is bound to the exact elevation request, elevation grant, launch grant, bootstrap request, package-engine session and reviewed bootstrapper identity. It classifies the elevated helper result as `completed`, `blocked-nonzero-return`, `blocked-timeout` or `blocked-output-limit`.

This unit does not request elevation, start processes, copy payloads, coordinate lifecycle completion, publish installation state, mutate product or game directories, execute runtime adapters, mutate saves, sign artifacts, publish to the network, mutate catalogues or promote evidence.

Later `LifecycleCoordinator/` work can consume this observation to convert an elevation-pending lifecycle result into a completed or blocked lifecycle result.
