//=================================================================================================
// Copyright (c) 2012, Johannes Meyer, TU Darmstadt
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Flight Systems and Automatic Control group,
//       TU Darmstadt, nor the names of its contributors may be used to
//       endorse or promote products derived from this software without
//       specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//=================================================================================================


#include <ros/ros.h>
#include <sensor_msgs/Joy.h>
#include <geometry_msgs/TwistStamped.h>
#include <geometry_msgs/PoseStamped.h>
#include <hector_uav_msgs/YawrateCommand.h>
#include <hector_uav_msgs/ThrustCommand.h>
#include <hector_uav_msgs/AttitudeCommand.h>
#include <hector_quadrotor_controller/limiters.h>
#include <tf2_geometry_msgs/tf2_geometry_msgs.h>

namespace hector_quadrotor
{

  class Teleop
  {
  private:
    ros::NodeHandle node_handle_;
    ros::Subscriber joy_subscriber_;

    ros::Publisher position_publisher_, velocity_publisher_, attitude_publisher_, yawrate_publisher_, thrust_publisher_;
    geometry_msgs::PoseStamped position_;

    double yaw;

    struct Axis
    {
      int axis;
      double max_;
      double min_;

      void setLimits(double min, double max)
      {
        min_ = min;
        max_ = max;
      }
    };

    struct Button
    {
      int button_;
    };

    struct
    {
      Axis x;
      Axis y;
      Axis z;
      Axis thrust;
      Axis yaw;
    } axes_;

    struct
    {
      Button slow;
      Button go;
    } buttons_;

    std::string robot_namespace_;
    double slow_factor_;
    std::string base_link_frame_, base_stabilized_frame_, world_frame_;
    double repeat_rate_;

  public:
    Teleop()
    {
      ros::NodeHandle private_nh("~");

      private_nh.param<int>("x_axis", axes_.x.axis, 5);
      private_nh.param<int>("y_axis", axes_.y.axis, 4);
      private_nh.param<int>("z_axis", axes_.z.axis, 2);
      private_nh.param<int>("thrust_axis", axes_.thrust.axis, -3);
      private_nh.param<int>("yaw_axis", axes_.yaw.axis, 1);

      private_nh.param<double>("yaw_velocity_max", axes_.yaw.max_, 90.0 * M_PI / 180.0);
      private_nh.param<double>("yaw_velocity_min", axes_.yaw.min_, -axes_.yaw.max_);

      private_nh.param<int>("slow_button", buttons_.slow.button_, 1);
      private_nh.param<int>("go_button", buttons_.go.button_, 0);
      private_nh.param<double>("slow_factor", slow_factor_, 0.2);

      // TODO dynamic reconfig
      std::string control_mode;
      private_nh.param<std::string>("control_mode", control_mode, "twist");
      private_nh.param<std::string>("robot_namespace", robot_namespace_, "");
      if (!robot_namespace_.empty())
      { robot_namespace_ = robot_namespace_ + "/"; }

      ros::NodeHandle robot_nh(robot_namespace_);
      ros::NodeHandle limit_nh(robot_nh, "limits");

      // TODO factor out
      robot_nh.param<std::string>("base_link_frame", base_link_frame_, "base_link");
      robot_nh.param<std::string>("world_frame", world_frame_, "world");
      robot_nh.param<std::string>("base_stabilized_frame", base_stabilized_frame_, "base_stabilized");

      ros::NodeHandle joy_nh("joy");
      private_nh.param<double>("autorepeat_rate", repeat_rate_, 100.0);

      if (control_mode == "attitude")
      {

        hector_quadrotor_controller::AttitudeCommandLimiter attitude_limiter(limit_nh, "pose/orientation");
        hector_quadrotor_controller::YawrateCommandLimiter yawrate_limiter(limit_nh, "twist/angular");
        hector_quadrotor_controller::ThrustCommandLimiter thrust_limiter(limit_nh, "wrench/force");
        axes_.y.setLimits(attitude_limiter.roll_.min_, attitude_limiter.roll_.max_);
        axes_.x.setLimits(attitude_limiter.pitch_.min_, attitude_limiter.pitch_.max_);
        axes_.thrust.setLimits(thrust_limiter.thrust_.min_, thrust_limiter.thrust_.max_);
        axes_.yaw.setLimits(yawrate_limiter.turnrate_.min_, yawrate_limiter.turnrate_.max_);

        joy_subscriber_ = node_handle_.subscribe<sensor_msgs::Joy>("joy", 1,
                                                                   boost::bind(&Teleop::joyAttitudeCallback, this, _1));
        attitude_publisher_ = node_handle_.advertise<hector_uav_msgs::AttitudeCommand>(
            robot_namespace_ + "command/attitude", 10);
        yawrate_publisher_ = node_handle_.advertise<hector_uav_msgs::YawrateCommand>(
            robot_namespace_ + "command/yawrate", 10);
        thrust_publisher_ = node_handle_.advertise<hector_uav_msgs::ThrustCommand>(robot_namespace_ + "command/thrust",
                                                                                   10);
      }
      else if (control_mode == "velocity")
      {
        hector_quadrotor_controller::TwistLimiter twist_limiter(limit_nh, "twist");
        axes_.x.setLimits(twist_limiter.linear_.x_.min_, twist_limiter.linear_.x_.max_);
        axes_.y.setLimits(twist_limiter.linear_.y_.min_, twist_limiter.linear_.y_.max_);
        axes_.z.setLimits(twist_limiter.linear_.z_.min_, twist_limiter.linear_.z_.max_);
        axes_.yaw.setLimits(twist_limiter.angular_.z_.min_, twist_limiter.angular_.z_.max_);

        joy_subscriber_ = node_handle_.subscribe<sensor_msgs::Joy>("joy", 1,
                                                                   boost::bind(&Teleop::joyTwistCallback, this, _1));
        velocity_publisher_ = node_handle_.advertise<geometry_msgs::TwistStamped>(robot_namespace_ + "command/twist",
                                                                                  10);
      }
      else if (control_mode == "position")
      {
        hector_quadrotor_controller::TwistLimiter twist_limiter(limit_nh, "twist");
        axes_.x.setLimits(twist_limiter.linear_.x_.min_, twist_limiter.linear_.x_.max_);
        axes_.y.setLimits(twist_limiter.linear_.y_.min_, twist_limiter.linear_.y_.max_);
        axes_.z.setLimits(twist_limiter.linear_.z_.min_, twist_limiter.linear_.z_.max_);
        axes_.yaw.setLimits(twist_limiter.angular_.z_.min_, twist_limiter.angular_.z_.max_);

        joy_subscriber_ = node_handle_.subscribe<sensor_msgs::Joy>("joy", 1,
                                                                   boost::bind(&Teleop::joyPoseCallback, this, _1));
        position_publisher_ = node_handle_.advertise<geometry_msgs::PoseStamped>(robot_namespace_ + "command/pose", 10);
      }

    }

    ~Teleop()
    {
      stop();
    }

    void joyAttitudeCallback(const sensor_msgs::JoyConstPtr &joy)
    {

      hector_uav_msgs::AttitudeCommand attitude;
      hector_uav_msgs::ThrustCommand thrust;
      hector_uav_msgs::YawrateCommand yawrate;

      attitude.header.stamp = thrust.header.stamp = yawrate.header.stamp = ros::Time::now();
      attitude.header.frame_id = yawrate.header.frame_id = base_stabilized_frame_;
      thrust.header.frame_id = base_link_frame_;

      attitude.roll = -getAxis(joy, axes_.y);
      attitude.pitch = getAxis(joy, axes_.x);
      attitude_publisher_.publish(attitude);

      thrust.thrust = getAxis(joy, axes_.thrust);
      thrust_publisher_.publish(thrust);

      yawrate.turnrate = getAxis(joy, axes_.yaw);
      if (getButton(joy, buttons_.slow))
      {
        yawrate.turnrate *= slow_factor_;
      }
      yawrate_publisher_.publish(yawrate);
    }

    void joyTwistCallback(const sensor_msgs::JoyConstPtr &joy)
    {
      geometry_msgs::TwistStamped velocity;
      velocity.header.frame_id = base_stabilized_frame_;
      velocity.header.stamp = ros::Time::now();

      velocity.twist.linear.x = getAxis(joy, axes_.x);
      velocity.twist.linear.y = getAxis(joy, axes_.y);
      velocity.twist.linear.z = getAxis(joy, axes_.z);
      velocity.twist.angular.z = getAxis(joy, axes_.yaw);
      if (getButton(joy, buttons_.slow))
      {
        velocity.twist.linear.x *= slow_factor_;
        velocity.twist.linear.y *= slow_factor_;
        velocity.twist.linear.z *= slow_factor_;
        velocity.twist.angular.z *= slow_factor_;
      }
      velocity_publisher_.publish(velocity);
    }

    void joyPoseCallback(const sensor_msgs::JoyConstPtr &joy)
    {
      position_.header.stamp = ros::Time::now();
      position_.header.frame_id = world_frame_;

      position_.pose.position.x += getAxis(joy, axes_.x) / repeat_rate_;
      position_.pose.position.y += getAxis(joy, axes_.y) / repeat_rate_;
      position_.pose.position.z += getAxis(joy, axes_.z) / repeat_rate_;
      yaw += getAxis(joy, axes_.yaw) / repeat_rate_;

      tf2::Quaternion q;
      q.setRPY(0.0, 0.0, yaw);
      position_.pose.orientation = tf2::toMsg(q);
      if (getButton(joy, buttons_.go))
      {
        position_publisher_.publish(position_);
      }
    }

    double getAxis(const sensor_msgs::JoyConstPtr &joy, Axis axis)
    {
      if (axis.axis == 0 || std::abs(axis.axis) > joy->axes.size())
      {
        return 0;
        ROS_ERROR_STREAM("Axis " << axis.axis << " out of range, joy has " << joy->axes.size() << " axes");
      }
      double output = std::abs(axis.axis) / axis.axis * joy->axes[std::abs(axis.axis) - 1];

      // Scale axis with min/max
      return (output + 1) * (axis.max_ - axis.min_) / 2 + axis.min_;
    }

    bool getButton(const sensor_msgs::JoyConstPtr &joy, Button button)
    {
      if (button.button_ <= 0 || button.button_ > joy->buttons.size())
      {
        ROS_ERROR_STREAM("Button " << button.button_ << " out of range, joy has " << joy->buttons.size() << " buttons");
        return false;
      }
      return joy->buttons[button.button_ - 1] > 0;
    }

    void stop()
    {
      if (velocity_publisher_.getNumSubscribers() > 0)
      {
        velocity_publisher_.publish(geometry_msgs::TwistStamped());
      }
      if (position_publisher_.getNumSubscribers() > 0)
      {
        velocity_publisher_.publish(geometry_msgs::PoseStamped());
      }
      if (attitude_publisher_.getNumSubscribers() > 0)
      {
        attitude_publisher_.publish(hector_uav_msgs::AttitudeCommand());
      }
      if (thrust_publisher_.getNumSubscribers() > 0)
      {
        thrust_publisher_.publish(hector_uav_msgs::ThrustCommand());
      }
      if (yawrate_publisher_.getNumSubscribers() > 0)
      {
        yawrate_publisher_.publish(hector_uav_msgs::YawrateCommand());
      }

    }
  };

} // namespace hector_quadrotor

int main(int argc, char **argv)
{
  ros::init(argc, argv, "quadrotor_teleop");

  hector_quadrotor::Teleop teleop;
  ros::spin();

  return 0;
}
