#ifndef DDSC_F__BOOST_HIAB_UE53CHAOS_CUST_TRUCK_PLUGINS_LIDAR360GPURAYTRACING_THIRDPARTY_ROS2IDL_GENERATED_POINTFIELD_H_5A52FB02DA8337BCC71A0C4B8BCDCDA6
#define DDSC_F__BOOST_HIAB_UE53CHAOS_CUST_TRUCK_PLUGINS_LIDAR360GPURAYTRACING_THIRDPARTY_ROS2IDL_GENERATED_POINTFIELD_H_5A52FB02DA8337BCC71A0C4B8BCDCDA6

#include "dds/ddsc/dds_public_impl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define sensor_msgs_msg_FLOAT32 7
typedef struct sensor_msgs_msg_PointField
{
  char * name;
  uint32_t offset;
  uint8_t datatype;
  uint32_t count;
} sensor_msgs_msg_PointField;

extern const dds_topic_descriptor_t sensor_msgs_msg_PointField_desc;

#define sensor_msgs_msg_PointField__alloc() \
((sensor_msgs_msg_PointField*) dds_alloc (sizeof (sensor_msgs_msg_PointField)));

#define sensor_msgs_msg_PointField_free(d,o) \
dds_sample_free ((d), &sensor_msgs_msg_PointField_desc, (o))

#ifdef __cplusplus
}
#endif

#endif
