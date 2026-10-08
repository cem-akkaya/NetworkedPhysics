# Networked Physics – Chaos Modular Vehicle & Async Physics (UE 5.8)

This is my place for intense experimentation with Unreal physics. I follow engine updates, work through tutorials, and push those ideas into vehicles and network conditions that expose the hard parts of the simulation. I like tracing a behavior from Blueprint through C++ into Chaos, breaking assumptions, tuning the result, and coming back with a better understanding of how the engine works. This project is where I keep doing that and sharpening my skills.

> **Important Note**  
> This project currently targets a **custom Unreal Engine 5.8.0 source build**. The project code and assets are in this repository; the engine-side work is in my [Unreal Engine fork](https://github.com/cem-akkaya/UnrealEngine). Access to Unreal Engine source on GitHub requires an [Epic-linked GitHub account](https://dev.epicgames.com/documentation/en-us/unreal-engine/downloading-source-code-in-unreal-engine).

<img src="https://raw.githubusercontent.com/cem-akkaya/NetworkedPhysics/refs/heads/master/Source/5.gif" alt="networked-physics-splash" width="100%"/>

## About This Project

The experiments focus on **advanced networked physics** and **Chaos Modular Vehicle** in Unreal Engine 5.8. They build on ideas from the [Networked Physics Pawn Tutorial](https://dev.epicgames.com/community/learning/tutorials/MoBq/unreal-engine-networked-physics-pawn-tutorial), then push them into more complex modular vehicles and multi-body setups.

The experiments keep coming back to three areas:

- **Modular Vehicle Evolution**: Pushing the boundaries of the experimental Chaos Modular Vehicle plugin through custom C++ sub-modules.
- **Physics-Driven Mechanics**: Implementing real-world mechanical behaviors (like hydraulic arms and buckets) directly within the Physics Thread.
- **Networked Precision**: Investigating and solving the complexities of asynchronous physics synchronization in high-latency environments.

### Project and Engine Changes

The [NetworkedPhysics repository](https://github.com/cem-akkaya/NetworkedPhysics) contains the vehicle code, Blueprints, levels, and gameplay experiments. The [Unreal Engine fork](https://github.com/cem-akkaya/UnrealEngine) is the companion history for changes made inside Chaos Physics and Chaos Modular Vehicle. Together they show both the engine behavior and how the project uses it.

The [Pod Racer speed-scaling branch](https://github.com/cem-akkaya/NetworkedPhysics/tree/podracer-speed-scaling) shows this relationship in practice. Its history records [runtime chassis drag-area control](https://github.com/cem-akkaya/NetworkedPhysics/commit/a65ca33ed3bc630b933dfd1b26e2a542520a827a), [runtime aerofoil adjustment](https://github.com/cem-akkaya/NetworkedPhysics/commit/af9cc8814a9d03739a19a3b86fcfe56a1ccc0bf1), and [drag and lift multiplier changes](https://github.com/cem-akkaya/NetworkedPhysics/commit/40d6ad41cb22b213cbbab059b42d22d1b0f94114). Those commits contain Blueprint and level changes and describe engine experiments, but they do not contain the engine C++ changes. Runtime aerodynamic setters are now being moved into [project code](Source/NetworkedPhysics/VehicleAerodynamicsLibrary.cpp).

### A Deep Dive into Chaos Physics

Each vehicle gives me a different way to investigate what happens under the hood:

- **Low-Level Simulation**: Deciphering the interaction between Game Thread inputs and Physics Thread execution.
- **Structural Integrity**: Investigating how hierarchical simulation trees maintain stability under stress.
- **Experimental Boundaries**: Testing the limits of the new Modular Vehicle architecture to bridge the gap between "standard" vehicle sims and complex industrial machinery.

---

## Key Components

### 1. Pod Racer (Experimental Multi-Body Physics Vehicle)

The Pod Racer is an experimental, high-velocity vehicle setup designed to explore **force-driven, multi-body physics behavior** under extreme conditions. Its cockpit and two thrusters remain separate simulated bodies joined by soft constraints, while the thrusters use Chaos Modular Vehicle cluster actors and simulation components. This lets the vehicle draw on modular simulation without becoming a conventional single-body vehicle.

<img src="https://raw.githubusercontent.com/cem-akkaya/NetworkedPhysics/refs/heads/master/Source/6.gif" alt="networked-physics-splash" width="100%"/>

The setup consists of:
- A central pod acting as the control and stabilization body
- Two independent thrusters, each simulated as separate rigid bodies
- Motorcycle-like suspension systems on each thruster
- A chassis configuration responsible for aerodynamic drag
- Forward propulsion forces combined with upward aerofoil lift forces

All physics bodies are connected using soft, balanced constraints that allow controlled, chaotic motion while maintaining overall stability at very high speeds. The vehicle behaves almost like a low-altitude flying craft, while remaining constrained to ground-level interaction.

This setup runs through **async, networked physics** and is explicitly tuned to remain stable under extreme velocity, high force magnitudes, and aggressive player input. Rather than relying on heavy assists or self-correcting behavior, control responsiveness is preserved while requiring player mastery. The vehicle does not drive itself; understanding its dynamics is part of the experience.

The Pod Racer exists as a **physics and gameplay experiment**, focusing on:
- Multi-body force interaction at high velocity
- Stability and constraint behavior under stress
- Balancing realism with readable, skill-based control
- Exploring the limits of networked async physics with a multi-body modular vehicle

The Pod Racer complements the Loader and Mining Truck experiments: its behavior emerges from forces shared across separate bodies and their constraints, alongside modular suspension and aerodynamic simulation.

### 2. Loader Truck (LoaderPawn)
The `ALoaderPawn` demonstrates a significant extension of the Chaos Modular Vehicle architecture through specialized mechanical systems.

<img src="https://raw.githubusercontent.com/cem-akkaya/NetworkedPhysics/refs/heads/master/Source/3.gif" alt="networked-physics-splash" width="100%"/>

- **VehicleSimArmComponent & Follower**: A primary technical highlight. This is a custom-built simulation module that adds interactive arm manipulation (e.g., for loaders, cranes, or excavators). It demonstrates how to create custom PT (Physics Thread) simulation logic that integrates seamlessly with the modular vehicle tree.
- **LoaderSimComponent**: An extension of `UModularVehicleBaseComponent` that manages the core vehicle dynamics while coordinating the custom arm and bucket sub-modules.
- **Hierarchical Simulation**: Showcases complex part-to-part physics relationships and animation synchronization between the Physics Thread and Game Thread.

### 3. Mining Truck
This additional pawn is a second **Chaos Modular Vehicle–based implementation** designed to more closely resemble **real-life industrial machinery** rather than arcade-style vehicles.

<img src="https://raw.githubusercontent.com/cem-akkaya/NetworkedPhysics/refs/heads/master/Source/4.gif" alt="networked-physics-splash" width="100%"/>

- **Shared Arm System**: Uses the same `VehicleSimArmComponent` and follower setup as the LoaderPawn, validating that the arm simulation is reusable across different vehicle configurations.
- **Heavy Mass Configuration**: Significantly increased vehicle mass and inertia to emphasize momentum, stability, and resistance to sudden directional changes.
- **Low-Speed Dynamics**: Tuned for slow, deliberate movement typical of industrial vehicles rather than road cars.
- **High Gear Ratios**: Transmission and drivetrain configured with high gear ratios to provide strong torque at low speeds, closely matching real-world construction and utility vehicles.
- **Realistic Handling Feel**: Emphasizes correct torque distribution, reduced wheel slip, and physically plausible turning behavior under load.

This pawn exists primarily as a **physics validation case**, ensuring that the modular vehicle and arm systems behave correctly under heavy loads and realistic drivetrain constraints.

### 4. Wrecking Ball Controller (MyPhysicsPawn)
This component implements the manual asynchronous networked physics logic detailed in the community tutorial, serving as a robust base for state synchronization.

<img src="https://raw.githubusercontent.com/cem-akkaya/NetworkedPhysics/refs/heads/master/Source/2.gif" alt="networked-physics-splash" width="100%"/>

- **Tutorial Implementation**: Provides a clean, documented implementation of `Chaos::TSimCallbackObject` and `FNetworkPhysicsPayload`.
- **Synchronization Logic**: Demonstrates reliable input and state replication across the network with client-side prediction and server reconciliation.

---
<img src="https://raw.githubusercontent.com/cem-akkaya/NetworkedPhysics/refs/heads/master/Source/1.gif" alt="networked-physics-splash" width="100%"/>

## Technical Flow

### Modular Vehicle Simulation
1. `ALoaderPawn` initializes with `ULoaderSimComponent`.
2. `UVehicleSimArmComponent` is registered as a sub-module.
3. Chaos Physics handles the wheel/suspension simulation via the Modular Vehicle plugin's async tree.

### Async Networked Physics
1. Inputs are captured on the **Game Thread**.
2. Inputs are sent to the **Physics Thread** via `FAsyncInputPhysicsPawn`.
3. `FPhysicsPawnAsync` processes movement and forces during `OnPreSimulate_Internal`.
4. State is synchronized back using the `UNetworkPhysicsComponent`.
