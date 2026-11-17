# __NAME__

Scaffolded xPlugin `__ID__`. Replace this note with what it provides, which contracts it uses, its properties, commands, events, and the JSON it saves.

## Provides

- Component `Settings` with property `__ID__.enabled`.

## Uses

Nothing yet.

## Real time

`compile` adds an empty `rt/Nop`. `tick` does not allocate.

## UI

`ui/` is a separate target and is omitted when `XEN_HEADLESS` is on. The CLI does not link it.
