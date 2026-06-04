# ROS2 Scene Camera

Unreal Engine 5.7 plugin: a **normal scene camera** that captures the view, runs a **compute shader** to pack RGB8, publishes **`sensor_msgs/Image`** over **CycloneDDS**, and shows a **live preview window** when you press **Play**.

Repository: [github.com/flodhestus/Ros2SceneCamera](https://github.com/flodhestus/Ros2SceneCamera)

This is **not** a LiDAR plugin. It is standalone and does not use `Lidar360` naming.

## On Play (PIE)

| Role | Actor | DDS topic (default) | Window |
|------|--------|---------------------|--------|
| Publish | `ARos2SceneCameraPublisher` | `rt/sensor_image` | — |
| Subscribe | `ARos2SceneCameraSubscriber` | `rt/sensor_image` | **Scene Camera** |

Both actors are spawned automatically in the editor when you press **Play**.

## GPU path

1. **`USceneCaptureComponent2D`** renders **Final Color LDR** to a render target (default **960×540**).
2. **`SceneCameraCapture.usf`** compute shader (`8×8` threads) samples the RT and packs **RGB8**.
3. **`sensor_msgs/Image`** (`encoding: rgb8`) is written with CycloneDDS (default QoS).
4. Subscriber decodes the image and draws it in an OpenGL Win32 window.

If the compute shader is not available, a CPU **ReadPixels** fallback is used.

## Performance

| Setting | Default | Notes |
|---------|---------|--------|
| Resolution | 960×540 | Lower for lighter PIE |
| Publish rate | 15 Hz | Increase up to ~30 Hz on fast GPUs |
| Payload | W×H×3 bytes | Scales with resolution |

Scene capture cost dominates; DDS and the viewer are relatively cheap.

## Requirements

- **Win64**, UE **5.7**
- CycloneDDS binaries under `ThirdParty/cyclonedds`

## Quick start

1. Copy `Ros2SceneCamera` into your project `Plugins/` folder.
2. Enable **ROS2 Scene Camera**.
3. Press **Play** — publisher captures, subscriber opens **Scene Camera** when frames arrive.

## Related plugins

- [Lidar360GpuRayTracing](https://github.com/flodhestus/Lidar360GpuRayTracing) — 360° GPU ray tracing LiDAR  
- [Lidar360OptiX](https://github.com/flodhestus/Lidar360OptiX) — OptiX LiDAR  
