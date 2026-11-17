# M4 acceptance

Headless Release build. Packets were received on UDP and decoded as OSC 1.0 bundles. `oscdump` is not on this machine; the decoded line uses its layout.

Trigger at u 0.5 on an 8-unit path, speed 40 (pass 0.100 s), one forward pass:

```text
/xen/trigger/T1 ssiifffd "C1" "P1" 1 1 0.500000 4.000000 0.000000 0.050000
```

The bundle timetag matched `unix at play + (event time − transport at play) + 10 ms` within 1 ms.

A following second at 1,000 passes per second (speed 4000) sent 997 `/xen/trigger/` packets. Transport advanced 1.00246 s while the wall clock advanced 1.00246 s, so the sender thread did not stall the control clock. The three missing packets were not retried.
