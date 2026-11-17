# Contracts

Shared headers that more than one xPlugin may include. An xPlugin never includes another xPlugin; if two features must agree on a type, the type lives here and is versioned.

M1 ships no feature contracts. `TempoSource` is added with `xen.transport` in M2. `PositionSource` arrives with `xen.paths`.
