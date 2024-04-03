#ifndef NARROW_PASSAGE_CONTROLLER_HPP
#define NARROW_PASSAGE_CONTROLLER_HPP

// Grid Map
#include <grid_map_msgs/GetGridMap.h>
#include <grid_map_msgs/ProcessFile.h>
#include <grid_map_msgs/SetGridMap.h>
#include <grid_map_ros/grid_map_ros.hpp>
#include <grid_map_cv/GridMapCvConverter.hpp>
// ROS
#include <geometry_msgs/PoseWithCovarianceStamped.h>
#include <message_filters/cache.h>
#include <message_filters/subscriber.h>
#include <ros/ros.h>
#include <ros/callback_queue.h>
#include <sensor_msgs/PointCloud2.h>
#include <std_srvs/Empty.h>
#include <std_srvs/Trigger.h>
#include <std_msgs/String.h>
#include <std_msgs/Float32.h>

#include <tf/transform_listener.h>
#include <nav_msgs/OccupancyGrid.h>

// Eigen
#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Dense>


//  file output
#include <iostream>
#include <fstream>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <nav_msgs/Odometry.h>
#include <tf/transform_datatypes.h>
#include <nav_msgs/Path.h>

#include <cmath>

#include <vector>

#include <algorithm>

#include <geometry_msgs/Twist.h>
#include <narrow_passage_detection_msgs/NarrowPassage.h>
#include <narrow_passage_detection_msgs/NarrowPassageController.h>


#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/PointStamped.h>

namespace narrow_passgae_controller
{
    ros::CallbackQueue queue_1;
    ros::CallbackQueue queue_2;
    class NarrowPassageController
    {
    protected:
        ros::Subscriber narrow_passage_sub;
        ros::Publisher  speed_pub;
        ros::Publisher  lqr_params_pub;
        ros::Subscriber stateSubscriber;

        void narrow_passage_messageCallback(const narrow_passage_detection_msgs::NarrowPassage msg);
        void speed_publisher(const narrow_passage_detection_msgs::NarrowPassage msg);
        void lqr_params_publisher(const narrow_passage_detection_msgs::NarrowPassage msg);
        void stateCallback(const nav_msgs::Odometry odom_state);
        void updateRobotState(const nav_msgs::Odometry odom_state);
        void predicteRobotState(float &x, float &y, float  &theta);

        geometry_msgs::PoseStamped pose;
        geometry_msgs::Vector3Stamped velocity_linear;
        geometry_msgs::Vector3Stamped velocity_angular;
        double dt;
    public:
        NarrowPassageController(ros::NodeHandle& nodeHandle);
        ros::NodeHandle nh;
        nav_msgs::Odometry latest_odom_;
    };
    

    
    
} // namespace narrow_passgae_controller


#endif