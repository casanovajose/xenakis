# Groups

Always-on xPlugin. Containment, visibility, and mute. Groups does not know paths, cursors, or OSC.

## Provides

- Component `Group` on a group entity.
- Component `GroupMember` on a child entity.
- Properties `xen.groups.name`, `xen.groups.visible`, `xen.groups.mute`.
- Commands `AddGroup`, `MoveToGroup`, `Reorder`.

## Uses

Nothing. `required` and `enhances` are empty.

## Commands

| Command | Arguments |
|---|---|
| `AddGroup` | `name` (string, default `Group`). Creates `G1`, `G2`, … |
| `MoveToGroup` | `entity`, `group`. Writes `GroupMember.parent` and the group's `children`. |
| `Reorder` | `group`, `children` (a permutation of the current child ids). |

`visible` and `mute` change through the kernel command `SetProperty`.

## Events

`group.created`, `group.removed`, `group.moved`, `group.reordered`.

## Saved schema (`xen.groups` version 1)

```json
"xen.groups": {
  "Group": { "name": "Intro", "visible": true, "mute": false, "children": ["P1"] },
  "GroupMember": { "parent": "G1" }
}
```

A child is muted for output when its group, or any ancestor group, has `mute: true`. Cursors still move; a later OSC xPlugin reads this JSON and drops the events. It must not include this folder.

## Real time

`compile` copies groups into `rt/GroupSnapshot`. `tick` does not allocate. Mute does not remove groups from the snapshot.
