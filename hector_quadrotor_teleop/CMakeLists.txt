cmake_minimum_required(VERSION 2.8.3)
project(hector_quadrotor_teleop)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  sensor_msgs
  geometry_msgs
  hector_quadrotor_controller
  tf2_geometry_msgs
  visualization_msgs)
include_directories(include ${catkin_INCLUDE_DIRS})

catkin_package(
    INCLUDE_DIRS
    LIBRARIES
    CATKIN_DEPENDS roscpp sensor_msgs geometry_msgs
    DEPENDS
)

add_executable(quadrotor_teleop src/quadrotor_teleop.cpp)
target_link_libraries(quadrotor_teleop ${catkin_LIBRARIES})

install(TARGETS quadrotor_teleop
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY launch
  DESTINATION  ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
