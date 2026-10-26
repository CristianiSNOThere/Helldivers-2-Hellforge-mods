# Helldivers 2 Hellforge mods

## Super Earth Arsenal Modpack v2.4.5

[Download v2.4.5](https://github.com/CristianiSNOThere/Helldivers-2-Hellforge-mods/releases/tag/hellforge-modpack-v2.4.5) | [Installation](hellforge/modpack/INSTALL.md) | [Complete player guide](hellforge/modpack/PLAYER-GUIDE.md)

26 independently selectable weapon and equipment options, plus 29 One True Flag damage presets. Includes two-round AP10 Railgun magazines with maximum damage at 80% charge, a 20-round Loyalist with 0.5 sway, backpackless AC-8, Rapid/Supercharge ARC-3, 150-round VG-70, dual-mode Punisher Plasma, Gas Overhaul, and selectable Sickle heatsinks.

Requires HellForge SDK 13 or newer, Helldivers 2 build 25480438, Python 3.10 or later and PowerShell 7. Follow the installation guide to choose options and prepare assets from your own HellForge installation. Reselect Sickle heatsinks after starting the game.

Compatible with Scope of Authority Backport 2.14.15 on HellForge engine 0.1.28. Gas Overhaul supports the separate 150-health Health Rework with either 2 or 2.8 health per second regeneration.

## Enemy Rebalance

A separate HellForge project. Its release will be published separately.

The [Arsenal selectable modpack](https://github.com/CristianiSNOThere/Super-Earth-Arsenal-Modpack) is available in its separate repository.

## Complete modpack changes

## 40-K Meltagun

Sets the handling rating, also called ergonomics, to 40. Removes the fire-panic effect that makes enemies panic when hit, while keeping its slowing effect. Beam range increases from 15 to 20 metres.

## A/GM-17 Gas Mortar Sentry

Sets the sentry's health to 600.

## AC-8 Autocannon Backpackless Rework

The call-in supplies only the weapon, leaving the backpack slot free. The gun carries 10 loaded rounds and 60 in reserve, and keeps its familiar five-round top reload while walking. Reloading is about 20% faster, and a resupply refills up to 60 reserve rounds. The handling rating increases from 17 to 30.

Recoil and camera kick are reduced by 25% per shot. Side-to-side recoil movement falls from 200 to 150, and up-and-down movement from 450 to 337.5. Side-to-side camera kick falls from 2 to 1.5, and vertical camera kick from 100 to 75. APHET direct hits deal 375 normal damage and 340 against durable parts, with heavy armor penetration at level 5; their impact explosion deals 180 normal damage. FLAK explosion damage becomes 210, with an outer damage radius of 8 metres; FLAK direct damage and shrapnel stay the same. JAR-5 Dominator High Explosive ammunition shares the explosion and receives the same buff.

## APW-1 Anti-Materiel Rifle

Sets the handling rating, also called ergonomics, to 40. Magazine capacity increases from 7 to 10 rounds, and the maximum number of spare magazines increases from 8 to 10. Starting and resupply magazine counts stay the same.

## AR-11 Arbitrator

Sets rifle damage to 80 and magazine capacity to 65 rounds. The underbarrel shotgun holds 4 shells, starts with 30 reserve shells and can carry up to 30; resupply provides 20 shells. Its stagger strength is set to 25 and knockback strength to 20. The handling rating is set to 41 before attachments, or 40 with the default optic.

## AR-32 Pacifier Tuning

Normal bullet damage increases from 55 to 75, and damage against durable parts increases from 10 to 15. Each hit builds up the medium-stun effect at strength 3 instead of 2, a 50% increase. The default magazine grows from 40 to 65 rounds. Aim sway falls from setting 1 to 0.6, giving 40% less weapon wobble.

## ARC-12 Blitzer

Sets normal damage to 100 and damage against durable parts to 45. Increases arc reach to 30 metres and sets the fire rate to 80 rounds per minute.

## ARC-3 Rapid/Supercharge

Adds two fire modes: hold R and choose Auto for Rapid or Semi for Supercharge. Rapid charges in roughly one-third of a second; its three charge thresholds are about 0.31, 0.34 and 0.37 seconds. Rapid deals 15% less than its base pulse damage: about 192 normal damage and 77 against durable parts, before armor and hit location. The base pulse values are 226 normal and 90 against durable parts.

Supercharge charges in about 3.4 to 3.75 seconds, with its middle threshold at 3.5 seconds. It deals 2,325 normal damage and 2,325 against durable parts, hits one enemy without chaining or splitting, and cannot explode from overcharging. Both modes have heavy armor penetration, level 7, keep the original Arc Thrower sounds and reach up to 45 metres.

## CQC-20 Breaching Hammer

Sets armor penetration to level 7 for direct hits and both the smaller and larger blast areas. Sets starting, resupply and maximum magazine counts to 21 each.

## Gas Overhaul

Gas damage over time increases from 25 to 60 per second, for both normal damage and damage against durable parts. Sterilizer direct contact damage increases from 1 to 2 for both values. Gas clouds keep their standard spread. The Sterilizer tank holds 175 rounds instead of 125, and its gas and confusion buildup per hit is 0.75 instead of 0.5. The incoming Helldiver gas-damage multiplier is 0.55, keeping nominal incoming gas damage at 33 per second before armour passives and other modifiers.

Gas confusion strength is doubled on susceptible enemies, producing stronger slowdown. When hosting a mission, gas-confused enemies can receive a 3-second Medium Stun, at most once every 6 seconds per enemy; existing active stuns are not refreshed and normal stun resistance applies. Gas also applies 15-second temporary armour weakening only to enemies, refreshed while their gas effects remain active. Gas no longer applies this armour debuff to Helldivers, and armour plates are not permanently destroyed. Up to four enemies per second can receive each of these effects.

These gas changes cover the Sterilizer, Dog Breath, Re-Educator, Speargun, gas grenades and mines, Gas Mortar Sentry, Eagle Gas Airstrike, Orbital Gas Strike and shared gas hazards. Advanced Filtration armour provides 100% immunity to gas damage and gas status effects, increased from 80%; other armour passives keep their existing bonuses. Natural Acid Storm weather still reduces Helldiver armour, and its shared effect duration is 15 seconds.

Works with the separate Health Rework mod: Normal supports 150 HP with either 2 or 2.8 health regenerated per second; Casual uses 160 HP and 3 per second. Keeps the selected preset's health, regeneration and stim settings. Use one health preset at a time.

## One True Flag

Choose damage from one dropdown with 29 presets. Normal-damage choices are 0, 100, 200, 300, 400, 500, 600, 750, 1,000, 1,500, 2,000, 3,000, 5,000, 7,500 and 9,999. Damage against durable parts can be half of normal damage, rounded down, or equal to it. The zero-damage preset sets both values to zero. The default is 300 normal damage and 150 against durable parts. The dropdown is inside the One True Flag option.

## P-34 Breacher

Sets the number of magazines received from resupply to 2.

## PLAS-1 Scorcher AR

Increases the magazine from 20 to 50 rounds and the fire rate from 350 to 540 rounds per minute. Normal damage decreases from 200 total to 150: 75 from the impact and 75 from the explosion, before armor, hit location and distance. Damage against durable parts is set to 38 on impact and 75 from the explosion, or 113 combined against a fully durable part. The full-damage blast radius increases from 1 to 1.25 metres, and the outer damage radius from 2 to 2.5 metres.

Starting spare magazines increase from 3 to 4, and maximum spare magazines from 5 to 6; resupply provides 5 magazines. Keeps the Semi and Auto selector: choose Auto in Weapon Functions for continuous fire. At 540 rounds per minute, a full magazine gives about 5.6 seconds of sustained fire. Recoil movement and camera kick fall by 25% per shot: side-to-side and up-and-down recoil settings decrease from 20 to 15, side-to-side camera kick from 2 to 1.5, and vertical camera kick from 20 to 15.

## PLAS-39 Accelerator Rifle

Sets magazine capacity to 21 rounds. Side-to-side and up-and-down recoil movement and camera kick are set to 2. Spread is set to 0.3 in both directions, and aim sway to 0.4, giving a tight shot grouping and steady aim. Stagger strength is set to 25. Direct hits and explosions use armor penetration level 4. The explosion's outer damage radius and shockwave radius are both set to 2.5 metres.

Use the left-side Weapon Functions selector to choose Semi or Burst. Semi fires one shot with an almost instant charge, at a base rate of 200 rounds per minute. Its minimum charge is 0.01 seconds, and its base interval between shots is 0.30 seconds; the normal fire-rate modifier can increase that interval to about 0.33 seconds. Burst charges for at least 0.45 seconds, then fires three rounds about 0.12 seconds apart. Both modes keep normal firing sounds and the game's fire-rate modifier.

## PLAS-45 Epoch Highest-Charge Rework

Highest-charge shots gain 30% more normal damage and damage against durable parts, on both direct hits and explosions. Blast hits also apply burning. Full blast damage reaches 5 metres, with an outer damage radius of 6 metres and shockwave radius of 7 metres. Magazine capacity increases from 3 to 4 rounds, and maximum spare magazines from 4 to 5. You can reload while walking using the existing reload animation.

Base highest-charge direct damage is 520 normal and 260 against durable parts; explosion damage is 1,040 for each. The existing charge damage multiplier still applies. The three charge thresholds stay at 1, 2.5 and 2.6 seconds, and lower-charge shots retain their damage. Starting spare magazines and magazines received from resupply remain at 2 each.

## R/40-K Hot-Shot

Sets magazine capacity to 20 rounds. Sets starting spare magazines, maximum spare magazines and magazines received from resupply to 5 each. Sets the handling rating to 45 and uses standard magazine reloads.

## RS-422 Railgun

Carries 2 rounds per magazine and 20 spare magazines (40 spare rounds). Each reload fills both rounds. Uses armor penetration 10. Deals 600 normal / 320 durable damage before charge scaling, reaching 1,500 normal / 800 durable damage at 80% charge. Damage stays at that maximum above 80%; the overcharge explosion limit is unchanged.

## S-11 Speargun

Sets starting spare magazines to 16, magazines received from resupply to 10 and maximum spare magazines to 20. Sets the weapon handling rating to 40.

## SAI Focus Precision

LAS-12 SAI deals 90 normal damage and 21 against durable parts as the shot leaves the muzzle, with either lens. Both lenses fire at 900 rounds per minute. Each lens has 50% less spread than its own stock setting: Standard's spread angle is about 4 degrees and Focus about 2.1 degrees, in both directions, so Focus remains the more precise option.

Projectile speed is set to 1,300 metres per second. The drag setting is about 1.24, making projectiles slow down and lose damage over distance. Recoil is reduced by one-third with either lens. Heat generated per shot is also reduced by one-third: from 2 to about 1.33 with Standard or cooldown heatsinks, and from 1.5 to 1 with the heat-capacity heatsink.

## Sickle Laser AR

LAS-16 Sickle deals 70 normal damage and 10 against durable parts. Heat generated per shot is reduced by 50%, from 1.15 to about 0.58, allowing longer firing before overheating. LAS-17 Double-Edge Sickle deals 65 normal damage and 10 against durable parts when cold or warm, rising to 83 normal and 14 against durable parts when hot or at maximum heat.

Both rifles fire immediately when the trigger is pressed, with no charge-up delay. Both have at least medium armor penetration, level 3; Double-Edge reaches level 4 at maximum heat. Both have a base fire rate of 700 rounds per minute, zero spread and projectile speed of 2,000 metres per second at every heat level, with no damage loss over distance.

Both rifles gain a magazine category in weapon customization with two heatsink choices:

- **Sickle Standard Heatsink:** 100 heat capacity and 8 heat units per second of cooling.
- **Sickle Rapid Cooling Heatsink:** keeps 100 heat capacity and increases cooling to 12 heat units per second, 50% faster than Standard.
- **Double-Edge Standard Heatsink:** 200 heat capacity, 8 heat units per second of cooling and heat-stage thresholds of 50, 100 and 190. Direct heat self-damage is reduced by 25%, to 15, 30 and 75 across its three stages.
- **Double-Edge Rapid Heat Heatsink:** reduces capacity to 150 and stage thresholds to 37.5, 75 and 142.5, reaching each stage 25% sooner. Cooling stays at 8 heat units per second. While equipped, direct heat self-damage is 50% of the original weapon's values: 10, 20 and 50 across its three stages.

Choose the desired heatsink in weapon customization before entering a mission. Reselect it after starting the game. Both choices keep the existing magazine model. Double-Edge's maximum-heat ignition and general fire damage are unchanged.

## Tenderizer Armor Softener

AR-61 Tenderizer magazine capacity increases from 35 to 70 rounds, and bullet speed from 820 to 1,000 metres per second. Direct and slightly angled hits gain medium armor penetration, level 3 instead of level 2; the other impact-angle penetration settings stay the same. Bullets apply Acid Storm, temporarily weakening enemy armor for 15 seconds.

Base aim sway is set to 0.9. Normal damage is set to 90 and damage against durable parts to 30, keeping the existing 600 and 850 rounds-per-minute settings, recoil and spread. The shared Acid Storm duration also affects its weather effect after exposure ends.

## VG-70 Variable Portable Maxigun Rework

Keeps its primary slot, reload while moving and backpack-free operation, with magazine capacity set to 150 rounds. Auto offers three rates: at 550 rounds per minute each bullet deals 110 normal damage and 29 against durable parts; at 750 it deals 90 and 24; at 1,000 it deals 75 and 20. Volley fires 15 bullets together, each dealing 40 normal damage and 11 against durable parts, using the selected 550, 750 or 1,000 rounds-per-minute setting. Total fires all remaining rounds at once, each dealing 28 normal damage and 8 against durable parts, with spread reduced to setting 0.75 in both directions. All three modes use medium armor penetration, level 3.

Spare magazines remain at 6 starting, 7 maximum and 7 from resupply. The handling rating is set to 40, and aim sway to 0.9. Recoil falls by 15% in every mode: side-to-side movement decreases from 5 to 4.25 and vertical movement from 10 to 8.5; side-to-side camera kick decreases from 4 to 3.4 and vertical camera kick from 8 to 6.8. Total keeps its original right-arm self-injury and knockdown. Muzzle flashes stay aligned during sustained Auto fire. Reload timing, projectile speed and sounds are retained.

## BR-14 Adjudicator Heavy Battle Rifle

BR-14 Adjudicator becomes a heavy battle rifle. Normal damage increases from 95 to 115, and damage against durable parts from 23 to 30. Stagger strength increases from 15 to 25, while knockback strength decreases from 12 to 10. Magazine capacity increases from 30 to 48 rounds, and fire rate decreases from 550 to 480 rounds per minute. Bullet speed increases from 820 to 1,000 metres per second, and speed lost each time a bullet penetrates decreases from 25% to 15%.

Side-to-side recoil movement decreases from 17 to 16.5, while vertical recoil movement increases from 35 to 57.5. Base aim sway decreases from setting 1 to 0.6, giving 40% less weapon wobble. Armor penetration, camera kick, spread, drag and spare-magazine counts stay the same.

## SG-8P Punisher Plasma

SG-8P Punisher Plasma gains Normal and Shotgun modes in the left-side Weapon Functions selector, plus a magazine-shaped undergrip with a matching white finish. Normal fires at 60 rounds per minute and deals 350 normal explosion damage and 225 against durable parts, with burning on hits. The blast deals full damage out to 3.5 metres, reaches an outer damage radius of 6.5 metres and has an 8-metre shockwave radius. Normal carries 17 loaded rounds and one spare 17-round magazine, has 50% less aim sway and a flatter projectile path. Its demolition rating is 30 for breaking destructible objects, with stagger strength 25 and knockback strength 30.

Shotgun fires nine explosive plasma pellets per shot at 100 rounds per minute, spending one round in total. Each pellet deals 25 normal damage and 9 against durable parts on impact, plus another 25 and 9 from the explosion. It has medium armor penetration, level 3, at every impact angle. Full-damage, outer damage and shockwave radii are all 0.5 metres, with stagger and knockback strength both set to 25. Shotgun uses a separate supply of 20 loaded rounds and 60 reserve rounds, and reloads twice as fast. Its spread angles are about 5.7 degrees side to side and 5.2 degrees up and down, giving a broad pellet pattern. Loyalist keeps its normal and charged plasma shots, and One-Two keeps its original grenade launcher and underbarrel appearance.

## GL-52 De-Escalator

Makes the GL-52 easier to aim by raising handling from 25 to 35. Increases maximum reserves and ammunition from a resupply from 40 to 70, and starting reserves from 32 to 70, while keeping its six-round cylinder. Reload speed increases by 15%. Electrical hits keep 100 normal damage, while damage against durable body parts rises from 70 to 100 and armor penetration stays at its stock level 4. Replaces its small-target stun with Stun Large, the effect used by stun grenades. Keeps its 200 rounds per minute and 10 m electrical reach.

## PLAS-15 Loyalist

Increases magazine capacity from 8 to 20 rounds and reduces sway from 1.0 to 0.5, giving 50% less weapon wobble. Each reload refills the 20-round magazine. Keeps its normal and charged shots and its existing spare-magazine counts.
