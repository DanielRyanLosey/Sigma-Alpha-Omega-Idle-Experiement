First launch: Allow about a minute or longer for caches to build, depending on your computer. The character, animation, and Toth may not be ready immediately. Later launches reuse the prepared caches.

# Sigma Alpha Omega — Idle Experiment

**Created by Daniel Losey**  
**Release V1 · Windows · D.I. Architecture Demonstration**

Sigma Alpha Omega is a playable idle-game experiment built to demonstrate my **Deterministic Intelligence architecture, D.I.**

The game brings together detailed visual assets, animated scenes, harvesting, equipment, inventory, persistent progression, and a local text interface called **Toth**.

Its architecture uses explicit modules and conductors to prepare, group, sequence, and forward outputs. The game provides a visible demonstration of those relationships: change scenes, harvest resources, move items, equip a tool, and observe the resulting state and presentation.

This repository currently distributes the **compiled Windows game and its assets**.

## Download and play

1. Click the green **Code** button near the top of this repository.
2. Select **Download ZIP**.
3. When the download finishes, right-click the ZIP and select **Extract All**.
4. Open the extracted folder.
5. Run **Sigma Alpha Omega Idle Experiement.exe**.

**Extract the entire download before launching. Keep the executable beside the `SAO_I_E Assets` folder.**

Do not rename or rearrange the files inside the assets folder. Both the image assets and dictionary TSV files are required.

Visual Studio is not needed to play.

## First launch

The game creates its own prepared caches on your computer.

The startup sequence displays the initial screen, waits approximately two seconds, and begins bulk preparation. Animation starts when the required groups are ready.

**The first launch can take longer because the caches do not exist yet.** Allow preparation to finish. Later launches can reuse valid caches.

Use an extracted folder where your Windows account can create files. The game writes caches and player saves locally.

## Start here

1. Launch the game and allow preparation to finish.
2. Register your player profile through Toth:
   - Type `#Player Name` and press **Enter**.
   - When prompted, type `#YourChosenName` and press **Enter**.
3. Use the Start Game button or enter `#Start_Game`.
4. Open harvesting using its button or enter `#Harvest_Tall_Grass`.
5. Enter `#Inventory` or `#Gear` to access your items and equipment.
6. Enter `#Progress` to view character and harvesting progression.

You may use a nickname for your player profile. Use the same spelling and capitalization when returning to an existing profile.

> **Inventory and Gear currently require a typed command. Enter `#Inventory` or `#Gear` in Toth, then press Enter. Either command opens both panels.**

## Toth commands

Type these into Toth and press **Enter**. Command names are case-insensitive: `#Inventory` and `#inventory` perform the same action.

| Command | Action |
|---|---|
| `#Player Name` | Begins player-profile registration or selection. |
| `#YourChosenName` | Supplies your profile name after Toth asks for it. Replace `YourChosenName` with your chosen name. |
| `#Start_Game` | Opens the Settlement scene. |
| `#Harvest_Tall_Grass` | Opens Tall Grass / Straw harvesting. |
| `#Inventory` | Opens Inventory and Gear together and minimizes Toth. |
| `#Gear` | Opens the same combined Inventory and Gear view. |
| `#Progress` | Opens character stats and harvesting progress in Toth. |
| `#Back` | Closes Progress when selected; otherwise requests the contextual Back action, such as closing Inventory/Gear or returning to the previous scene. |
| `#Minimize` | Minimizes Toth. |
| `#Maximize` | Maximizes Toth. |
| `#Exit_Game` | Closes the game. |

**The profile-name entry only works after `#Player Name` has prompted you for a name.**

Example:

```text
#Player Name
```

Press Enter, wait for the prompt, then enter:

```text
#Explorer
```

Press Enter again.

## Inventory and equipment

Open the panels with **`#Inventory`** or **`#Gear`**.

- Hold the left mouse button on an item to drag it.
- Release the button over a valid location to drop it.
- Equipment belongs in its matching gear location.
- Drag the stone sickle into the **Tool** location to equip it.
- Equipping the stone sickle replaces the visible oak sickle.
- Move the stone sickle back into inventory to return to the oak sickle.
- Right-click an inventory stack to delete that entire stack.
- Use the **Back button** to close Inventory/Gear and reveal the scene underneath.

Resource stacks hold up to **10,000** items. Equipment stacks hold **one** item.

Each player receives one starter stone sickle. The grant is remembered, so deleting it does not grant another one.

## Harvesting and progression

The harvesting scene combines automatic character actions with additional click damage.

Harvested grass and straw are added to inventory. Progress is tracked separately for harvesting and the character.

The current progression rules are:

- Levels begin at **0**.
- Clearing all **10 bundles** awards **1 harvesting XP**.
- The first harvesting level requires **100 XP**.
- Each subsequent requirement increases by **0.005 XP**: 100, 100.005, 100.010, and so on.
- Each harvesting level adds **0.005 resource yield per harvested bundle**.
- Each earned harvesting level contributes **one percentage point** toward character-level progress.
- Each character level adds **0.01 character damage** and **0.01 click damage**.

Fractional resource rewards are retained and accumulate toward whole items.

Open the **Progress button** in Toth or enter **`#Progress`** to view the current values.

## Talking to Toth

Toth also supports limited local language interactions. These entries do **not** use a `#`.

| Example | Response |
|---|---|
| `Hello` | A greeting. |
| `Who are you?` | The assistant’s identity. |
| `What is your name?` | The assistant’s current name. |
| `My name is Explorer` | Remembers a conversational name. |
| `Rename you` | Prompts you to supply a new assistant name. |
| `Save to memory` | Saves the conversational names. |
| `Define time` | Looks up a supported word. |
| `What is the definition of light?` | Looks up a supported word. |
| `What does learn mean?` | Looks up a supported word. |

**Conversational names and player profiles are separate.** Use `#Player Name` to establish the profile used for game saves.

Language support is currently limited to implemented query forms and the connected vocabulary. Some words have prepared example sentences; others return their definition and part of speech.

The broader mathematics conductor is not connected to this game’s text interface in this version.

## Saving your progress

Player data is stored locally under:

```text
SAO_I_E Assets\Player Saves
```

Inventory, equipment, unlocks, and progression are saved per player.

- Close the game normally when finishing a session.
- Keep your `Player Saves` folder when updating the game.
- Back up that folder if you want an additional copy of your progress.
- Deleting player saves removes the progress stored in those files.
- Do not replace a save with an empty file.

Generated caches are separate from player progress. Missing caches are rebuilt by the game.

## What the architecture demonstrates

D.I. separates source preparation, grouping, state transitions, and presentation.

In this implementation:

- Math modules prepare visual or data representations.
- Conductors establish explicit groups and sequence relationships.
- Prepared outputs move forward through the system.
- Equipment selection changes the participating tool group.
- Harvest stages connect visible bundle removal with resource consequences.
- Interface revisions identify when static presentation needs updating.
- Persistent numerical state retains progression and fractional rewards.

The distinction between **available content**, **active content**, and **changing content** is central to the design.

The development goal is to expand detailed content and interaction while keeping the active workload controlled. Storage, preparation time, and memory residency remain measurable parts of that expansion.

Beyond the game, the same structural principles suggest opportunities in interactive documentation, training, product configuration, education, and local knowledge tools.

## Observed performance

On the developer’s system, the current working version has shown approximately:

| Measurement | Developer observation |
|---|---|
| RAM | 20.6 MB during the observed session |
| CPU during ordinary operation | Near 0% |
| CPU during clicking or dragging | Peaks around 4% |
| Displayed GPU activity | At or near 0% |
| Displayed disk activity after preparation | Roughly 0% |

These are observations from the developer’s environment, not guaranteed requirements or standardized benchmark results. First-launch preparation performs additional work.

You can monitor your own results through Windows Task Manager. When sharing measurements, include your hardware, Windows version, scene, activity, and whether caches had already been built.

## Troubleshooting

**The game reports a missing asset**

Extract the complete ZIP. Confirm that `SAO_I_E Assets` is directly beside the executable and that its internal filenames and folders remain unchanged.

**Inventory does not have an opening button**

Type `#Inventory` or `#Gear` into Toth and press Enter.

**The first launch takes longer**

The game is preparing caches. Allow it to finish before judging normal operating performance.

**The game reports a missing DLL or runtime component**

Report the exact message and missing filename in a GitHub issue so the required dependency can be identified.

**A command does not work**

Include the leading `#` for game commands and press Enter. For profile creation, enter `#Player Name` first and wait for the name prompt.

## Feedback and project development

Use this repository’s **Issues** tab to report problems or share observations.

Helpful reports include:

- What you were doing.
- The exact error message, if any.
- A screenshot.
- Your Windows version and hardware.
- Whether this was the first launch.
- Steps that reproduce the behavior.

I am seeking investment and development partnerships to continue expanding Sigma Alpha Omega, formalize the D.I. architecture, improve its authoring tools, and explore additional commercial applications.

## Support Sigma Alpha Omega and D.I. Development

I’m Daniel Losey, the creator of Sigma Alpha Omega and the Deterministic Intelligence architecture behind it.

I built this working game to make the architecture tangible. You can explore its scenes, harvest resources, manage inventory, change equipment, and observe its resource usage on your own computer.

My goal is to keep expanding the game while developing the underlying architecture for broader real-world applications, including interactive education, training, technical documentation, and configurable visual systems.

If you enjoy the game or see potential in this work, please consider supporting its continued development.

Contributions help me dedicate more time and resources to:

- New environments, equipment, activities, and visual content.
- Improvements to the D.I. modules and conductors.
- Compatibility testing, bug fixes, and release quality.
- Documentation and tools that make the architecture easier to understand and build upon.
- Demonstrations of its capabilities beyond the game.

**[Support development through PayPal](https://paypal.me/daniellosey1)**

Contributions are voluntary support for development and do not purchase equity or promise a financial return.

If you cannot contribute financially, playing the game, sharing the repository, and reporting your experience are valuable ways to help.

Thank you for supporting independent development and giving this work the opportunity to grow.

**Daniel Losey**  
Creator of Sigma Alpha Omega and the D.I. architecture

The working game provides a practical starting point: download it, experience the interactions, and observe its behavior on your own system.

**Daniel Losey — Creator of Sigma Alpha Omega and the D.I. architecture.**
