# Helldivers 2 Runtime Mods

This repository is the home for standalone runtime mods and weapon projects,
including Punisher Plasma. Current Arsenal weapon-pack development belongs in
the separate `Super-Earth-Arsenal-Modpack` repository. Keep this repository's
URL and its 22 earlier individual releases, ZIPs and source history intact.
`LEGACY-ARSENAL-MODS.md` preserves the former landing-page release index.

A new standalone project is not automatically an Arsenal pack addition. Follow
its own scoped compatibility contract and applicable root instructions. Merely
repurposing this repository does not authorize new Punisher gameplay work or
resuming a rejected native experiment. Deployment/settings stay with the user.

## Earlier Arsenal compatibility rules

The following requirements apply when maintaining an earlier Arsenal package or
when the user explicitly adds a standalone project to the selectable pack.

Every new mod in this set, including Arc Thrower and Autocannon work, must follow the current startup contract in `source/startup-initialization/` and the companion gate in `source/scorcher-ar/runtime.lua`. Use bounded waits with named failures; retain exact supported peer-order checks, fail-closed shared-table validation, and rollback. Update supported peer values when a new mod edits a shared table.

Keep each mod's identity and private resource keys distinct. Shared resource keys must have byte-identical payloads in all individual packages that include them. Keep Shared Loader Lua in a pure-Lua archive. Check the new individual ZIP against the complete current set for differing resource collisions and relevant startup orders, and verify a fresh startup before release. Make the matching mega pack addition from the same sources. Startup evidence does not prove gameplay compatibility for every combination.

Keep deployment and Arsenal settings with the user. Verify a published replacement before removing only its superseded individual release. Preserve mega pack release history.

Public release descriptions and player-facing READMEs must explain the complete player-visible changes without implementation details, test history, failed prototypes, or statements about which tests were or were not run. Keep validation evidence in internal work notes.
