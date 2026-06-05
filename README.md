# ROS2 Scene Camera

Standalone Unreal Engine 5.7 plugin: **scene camera** with **compute-shader** RGB8 capture, **`sensor_msgs/Image`** over **CycloneDDS**, and a **live preview window** on **Play**.

Repository: [github.com/flodhestus/Ros2SceneCamera](https://github.com/flodhestus/Ros2SceneCamera) · GitHub [@flodhestus](https://github.com/flodhestus)

**No dependency** on Lidar360GpuRayTracing or Lidar360OptiX. **Not** a LiDAR plugin — camera actors do not use `Lidar360` naming. Includes its own CycloneDDS stack and `Config/CycloneDDS.xml`.

## On Play

| Role | Actor | DDS topic (default) | Window |
|------|--------|---------------------|--------|
| Publish | `ARos2SceneCameraPublisher` | `rt/sensor_image` | — |
| Subscribe | `ARos2SceneCameraSubscriber` | `rt/sensor_image` | **Scene Camera** |

## GPU path

1. **`USceneCaptureComponent2D`** → **1920×1080** `PF_B8G8R8A8` render target (default).
2. **`SceneCameraCapture.usf`** compute shader (16×16 threads) packs **RGB8** with pooled readback.
3. **`sensor_msgs/Image`** (`rgb8`) published via CycloneDDS (default QoS).

## Defaults

| Setting | Value |
|---------|--------|
| Resolution | 1920×1080 |
| Publish rate | 20 Hz |
| Capture | Every frame (`bCaptureEveryFrame`) |

## Quick start

1. Copy this folder into your project `Plugins/` directory.
2. Enable **ROS2 Scene Camera**.
3. Press **Play**.

## Optional companions (separate repos, no plugin dependency)

- [Lidar360GpuRayTracing](https://github.com/flodhestus/Lidar360GpuRayTracing) — D3D12 RT LiDAR on `rt/sensor_pointcloud`  
- [Lidar360OptiX](https://github.com/flodhestus/Lidar360OptiX) — OptiX LiDAR on `rt/sensor_pointcloud`  
