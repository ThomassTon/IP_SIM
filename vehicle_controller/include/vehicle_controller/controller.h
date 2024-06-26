#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <nav_msgs/Path.h>
#include <nav_msgs/Odometry.h>
#include <geometry_msgs/PoseStamped.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <geometry_msgs/Twist.h>
#include <geometry_msgs/PointStamped.h>

#include <monstertruck_msgs/MotionCommand.h>
#include <monstertruck_msgs/SetAlternativeTolerance.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Empty.h>
#include <std_msgs/String.h>
#include <sensor_msgs/JointState.h>

#include <move_base_lite_msgs/FollowPathAction.h>
#include <actionlib/server/action_server.h>

#include <ros/ros.h>
#include <tf/transform_listener.h>
#include <tf/transform_datatypes.h>

// #include <vehicle_controller/controller_node.h>
#include <vehicle_controller/vehicle_control_interface.h>
#include <vehicle_controller/motion_parameters.h>
#include <vehicle_controller/ps3d.h>
#include <vehicle_controller/stuck_detector.h>
#include <vehicle_controller/four_wheel_steer_controller.h>
#include <vehicle_controller/differential_drive_controller.h>
#include <vehicle_controller/quaternions.h>
#include <vehicle_controller/utility.h>
#include <narrow_passage_detection_msgs/NarrowPassageDetection.h>

#include <memory>
#include <limits>

#include <algorithm>
#include <sstream>
#include <functional>
#include <grid_map_cv/GridMapCvConverter.hpp>
#include <grid_map_msgs/GetGridMap.h>
#include <grid_map_msgs/ProcessFile.h>
#include <grid_map_msgs/SetGridMap.h>
#include <grid_map_ros/grid_map_ros.hpp>

class Controller {
public:
  typedef enum { INACTIVE, VELOCITY, DRIVETO, DRIVEPATH } State;

  Controller(ros::NodeHandle& nh_);
  virtual ~Controller();

  virtual bool configure();

  virtual std::string getName() = 0;
  bool get_elevation_map= false;
  grid_map::GridMap elevation_map;
  bool followPathServerIsActive();
  virtual void followPathGoalCallback(actionlib::ActionServer<move_base_lite_msgs::FollowPathAction>::GoalHandle goal);
  virtual void followPathPreemptCallback(actionlib::ActionServer<move_base_lite_msgs::FollowPathAction>::GoalHandle preempt);
  ros::Subscriber controllerTypeSwitch;

    // action interface
  boost::shared_ptr<actionlib::ActionServer<move_base_lite_msgs::FollowPathAction> > follow_path_server_;
  actionlib::ActionServer<move_base_lite_msgs::FollowPathAction>::GoalHandle follow_path_goal_;

  void map_messageCallback2( const grid_map_msgs::GridMap &msg );
  ros::Subscriber map_sub2;
protected:
  virtual void computeMoveCmd() = 0;
  virtual void update();
  virtual void reset();
  virtual void stop();

  virtual bool driveto(const geometry_msgs::PoseStamped&, double speed);
  virtual bool drivepath(const nav_msgs::Path& path);

  virtual bool updateRobotState(const nav_msgs::Odometry& odom_state);
  virtual void stateCallback(const nav_msgs::OdometryConstPtr& odom_state);
  virtual void drivetoCallback(const ros::MessageEvent<geometry_msgs::PoseStamped>&);
  virtual void drivepathCallback(const ros::MessageEvent<nav_msgs::Path>&);
  virtual void cmd_velCallback(const geometry_msgs::Twist&);
  virtual void cmd_velTeleopCallback(const geometry_msgs::Twist&);
  virtual void speedCallback(const std_msgs::Float32&);
  virtual void poseCallback(const ros::MessageEvent<geometry_msgs::PoseStamped>&);

  virtual void stopVehicle();


  /**
   * @brief addLeg to current tracking path
   * @param pose to be added
   * @param speed 0 indicates that the speed from the configuration is used as
   *        desired linear speed in the lower controllers, otherwise the given
   *        value is used for this purpose
   */
  void addLeg(const geometry_msgs::PoseStamped &pose, double speed = 0.0);
  void removeLegsInThePast();

  virtual bool reverseAllowed();
  virtual bool usePathOrientation();
  virtual bool reverseForced();
  virtual bool pathToBeSmoothed(const std::deque<geometry_msgs::PoseStamped> &transformed_path, bool fixed_path);
  virtual bool createDrivepath2MapTransform(tf::StampedTransform  & transform, const nav_msgs::Path& path);

  ros::NodeHandle nh;
  tf::TransformListener listener;

  ros::Subscriber stateSubscriber;
  ros::Subscriber drivetoSubscriber;
  ros::Subscriber drivepathSubscriber;
  ros::Subscriber cmd_velSubscriber;
  ros::Subscriber cmd_velTeleopSubscriber;
  ros::Subscriber speedSubscriber;
  ros::Subscriber poseSubscriber;


  ros::Publisher endPosePublisher;
  ros::Publisher carrotPosePublisher;
  ros::Publisher lookatPublisher;
  ros::Publisher cameraOrientationPublisher;
  ros::Publisher drivepathPublisher;
  ros::Publisher diagnosticsPublisher;
  ros::Publisher speedPublisher;

  ros::Publisher smoothPathPublisher;
  ros::Publisher autonomy_level_pub_;
  ros::Publisher smoothPathPublisher_narrow;

  // // action interface
  // boost::shared_ptr<actionlib::ActionServer<move_base_lite_msgs::FollowPathAction> > follow_path_server_;
  // actionlib::ActionServer<move_base_lite_msgs::FollowPathAction>::GoalHandle follow_path_goal_;

  //Service Provider
  ros::ServiceServer alternative_tolerances_service;

  State state;
  std_msgs::Header  robot_state_header;
  RobotControlState robot_control_state;


  //monstertruck_msgs::MotionCommand drive;
  geometry_msgs::PoseStamped carrotPose;
  actionlib_msgs::GoalIDPtr goalID;

  nav_msgs::Path empty_path;

  unsigned int current;
  geometry_msgs::PoseStamped start;
  Legs legs;

  double flipper_state;

  // motion parameters (set at launch)
  MotionParameters mp_;
  // path-specific settings
  move_base_lite_msgs::FollowPathOptions default_path_options_;

  std::string map_frame_id;
  std::string base_frame_id;

  bool camera_control;
  double camera_lookat_distance;
  double camera_lookat_height;
  geometry_msgs::QuaternionStamped cameraDefaultOrientation;

  bool check_stuck;
  double dt;

  double velocity_error;
  std::string robot_description;
  geometry_msgs::PoseStamped current_pose;


  boost::shared_ptr<VehicleControlInterface> vehicle_control_interface_;

  std::string vehicle_control_type;
  int final_twist_trials;

  nav_msgs::OdometryConstPtr latest_odom_;

  double roll, pitch, yaw;

  nav_msgs::Path current_path;

  inline bool isDtInvalid()
  {
      return dt <= 0.0;
  }

  std::unique_ptr<StuckDetector> stuck;


public:





};

#endif // CONTROLLER_H
