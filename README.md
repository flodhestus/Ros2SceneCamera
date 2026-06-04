# ROS2 Scene Camera

Unreal Engine 5.7 plugin: a **normal scene camera** that captures the view, runs a **compute shader** to pack RGB8, publishes **`sensor_msgs/Image`** over **CycloneDDS**, and shows a **live preview window** when you press **Play**.

Repository: [github.com/flodhestus/Ros2SceneCamera](https://github.com/flodhestus/Ros2SceneCamera)

**Depends on:** [Lidar360GpuRayTracing](https://github.com/flodhestus/Lidar360GpuRayTracing) for the shared **`Ros2DdsShared`** module (single CycloneDDS participant). **Not** a LiDAR plugin — no `Lidar360` naming in camera actors.

Runs with **either** Gpu LiDAR or OptiX LiDAR on the same DDS stack. Only **three plugins** total in the stack.

## On Play (PIE)

| Role | Actor | DDS topic | Window |
|------|--------|-----------|--------|
| Publish | `ARos2SceneCameraPublisher` | `rt/sensor_image` | — |
| Subscribe | `ARos2SceneCameraSubscriber` | `rt/sensor_image` | **Scene Camera** |

## Defaults

- **1920×1080**, **20 Hz** publish, compute-shader RGB8 path with pooled readback

## Quick start

1. Enable **LiDAR 360 GPU Ray Tracing** (shared DDS) and **ROS2 Scene Camera**.
2. Optionally enable **LiDAR 360 OptiX** instead of using GPU LiDAR publisher.
3. Press **Play**.

## Related

- [Lidar360GpuRayTracing](https://github.com/flodhestus/Lidar360GpuRayTracing)  
- [Lidar360OptiX](https://github.com/flodhestus/Lidar360OptiX)  
